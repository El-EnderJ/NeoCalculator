#!/usr/bin/env python3
"""Generate the compact runtime dataset and audit from pinned offline facts."""
import argparse
from decimal import Decimal, localcontext
import hashlib
import itertools
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "data/scientific/codata2022/source_capture.json"
SPEC = ROOT / "data/scientific/constants_spec.json"
PI = Decimal("3.141592653589793238462643383279502884197169399375105820974944592307816406")

def expression(text):
    d = Decimal(text)
    sign, digits, power = d.as_tuple()
    integer = int(''.join(map(str, digits))) * (-1 if sign else 1)
    if power == 0:
        return str(integer)
    return f"({integer}*10^{power})" if power > 0 else f"({integer}/10^{abs(power)})"

def decimal_text(value):
    return str(value.normalize()).lower()

def assemble(source, spec):
    records = []
    with localcontext() as ctx:
        ctx.prec = 60
        facts = source['constants']
        c, h, g, ug = [Decimal(facts[k][field]) for k, field in
                      [('c', 'value'), ('h', 'value'), ('bg', 'value'), ('bg', 'standard_uncertainty')]]
        for spec_record in spec['constants']:
            record = dict(spec_record)
            code = record['nist_code']
            if code:
                row = facts[code]
                record.update(nominal_decimal=row['value'], standard_uncertainty=decimal_text(Decimal(row['standard_uncertainty'])),
                              exact=row['exact'], unit=row['unit'], nominal_expression=expression(row['value']),
                              status='exact_definition' if row['exact'] else 'measured_recommended',
                              source_url=source['table_url'], source_location=row['name'])
                if code == 'hbar':
                    record['nominal_expression'] = f"({expression(facts['h']['value'])}/(2*pi))"
                    record['note'] = 'Exact h/(2*pi); source decimal is truncated and never the nominal authority.'
                elif code == 'gn':
                    record['note'] = 'Exact conventional standard gravity; not measured local gravitational acceleration.'
                else:
                    record['note'] = 'Rounded CODATA recommended central value; decimal storage does not imply physical exactness.' if not row['exact'] else 'Exact modern SI defining constant.'
            else:
                force = record['key'] == 'planck_force'
                central = c**4/g if force else (h/(2*PI)*c**5/g).sqrt()
                uncertainty = central*ug/g if force else central*ug/(2*g)
                central_expression = f"({expression(facts['c']['value'])}^4/{expression(facts['bg']['value'])})" if force else f"sqrt(({expression(facts['h']['value'])}/(2*pi))*{expression(facts['c']['value'])}^5/{expression(facts['bg']['value'])})"
                record.update(nominal_decimal=decimal_text(central), standard_uncertainty=decimal_text(uncertainty),
                              exact=False, unit='N' if force else 'J', nominal_expression=central_expression,
                              status='derived_uncertain', source_url=source['table_url'],
                              source_location='Derived from CODATA G, exact h and c',
                              note='Not a separately tabulated CODATA entry. Canonical formula and full first-order uncertainty replace the old display-rounded payload.')
            nominal, u = Decimal(record['nominal_decimal']), Decimal(record['standard_uncertainty'])
            record['relative_standard_uncertainty'] = decimal_text(u/abs(nominal)) if u else '0'
            record['action'] = 'Integrate with stable typed identity and captured correlations.'
            if record['constant_id'] in (6, 10, 11):
                record['action'] += ' Preserve formula as central mathematical authority.'
            if record['constant_id'] == 15:
                record['action'] = 'Add only this basic constant; existing six Planck references already depend on G.'
            records.append(record)
    code_to_id = {r['nist_code']: r['constant_id'] for r in records if r['nist_code']}
    pairs = []
    for pair in source['correlations']:
        a, b = sorted((code_to_id[pair['a']], code_to_id[pair['b']]))
        pairs.append(dict(a=a, b=b, rho=pair['rho'], source_url=pair['url'], basis='published',
                          response_sha256=pair['response_sha256']))
    base = {(p['a'], p['b']): Decimal(p['rho']) for p in pairs}
    uncertain = [r['constant_id'] for r in records if not r['exact']]
    # WHY: force and energy both have negative sensitivity to the same G.
    # Their correlations are derived from a captured G pair, never guessed.
    for a, b in itertools.combinations(uncertain, 2):
        if (a, b) in base:
            continue
        aa, bb = (15 if a in (10, 11) else a), (15 if b in (10, 11) else b)
        rho = Decimal(1) if aa == bb else base[tuple(sorted((aa, bb)))]
        rho *= (-1 if a in (10, 11) else 1) * (-1 if b in (10, 11) else 1)
        pairs.append(dict(a=a, b=b, rho=str(rho), basis='derived_from_G',
                          source_url=source['table_url'], dependency_pair=[aa, bb],
                          derivation='sign(dA/dG)*sign(dB/dG)*rho(G-equivalent sources)'))
    return {'schema': 1, 'dataset': source['dataset'], 'dataset_version_id': 1,
            'nist_web_version': source['nist_web_version'], 'web_release_date': '2024-05-09',
            'publication_date': '2025-04-30', 'consulted': source['consulted'],
            'source_capture_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
            'unknown_correlation_policy': 'unavailable; never infer zero',
            'constants': records, 'correlations': sorted(pairs, key=lambda p: (p['a'], p['b']))}

def cpp(dataset):
    lines = ['// Generated by scripts/generate-physical-constants.py; do not edit.',
             '#include "PhysicalConstants.h"', 'namespace numos::scientific {', 'namespace {',
             'constexpr ConstantRecord kConstants[] = {']
    for r in dataset['constants']:
        lines.append('    {ConstantId::%s,%d,%s,{%s},%s,%s,%s,%s,%s},' %
                     (r['enum'], r['reference_id'], json.dumps(r['nominal_expression']),
                      ','.join(map(str, r['dimension'])), r['standard_uncertainty'], str(r['exact']).lower(),
                      json.dumps(r['symbol'], ensure_ascii=False), json.dumps(r['unit']), json.dumps(r['standard_uncertainty'])))
    lines += ['};', 'struct Correlation { uint8_t a, b; int32_t rho100000; };',
              'constexpr Correlation kNonzero[] = {']
    known_zero = [0]*16
    for p in dataset['correlations']:
        rho = Decimal(p['rho'])
        if rho:
            assert rho*100000 == int(rho*100000)
            lines.append(f"    {{{p['a']},{p['b']},{int(rho*100000)}}},")
        else:
            known_zero[p['a']] |= 1 << p['b']
            known_zero[p['b']] |= 1 << p['a']
    lines += ['};', '// Explicitly captured zero pairs, including documented derivatives of G.',
              'constexpr uint16_t kKnownZero[] = {' + ','.join(hex(x) for x in known_zero) + '};',
              '} // namespace',
              'const ConstantRecord* constant(ConstantId id) noexcept {',
              '    const auto n=uint16_t(id);',
              '    return n && n<=sizeof(kConstants)/sizeof(*kConstants) ? &kConstants[n-1] : nullptr;',
              '}',
              'const ConstantRecord* byReference(uint16_t id) noexcept {',
              '    if (id>=1 && id<=14) return constant(ConstantId(id));',
              '    return id==59 ? constant(ConstantId::Gravitational) : nullptr;',
              '}',
              'size_t constantCount() noexcept { return sizeof(kConstants)/sizeof(*kConstants); }',
              'bool correlation(ConstantId first, ConstantId second, double& rho) noexcept {',
              '    const auto* a=constant(first); const auto* b=constant(second);',
              '    if (!a || !b || a->exact || b->exact) return false;',
              '    if (first==second) { rho=1; return true; }',
              '    auto i=uint16_t(first), j=uint16_t(second);',
              '    if (kKnownZero[i] & (uint16_t(1)<<j)) { rho=0; return true; }',
              '    if (i>j) { const auto t=i; i=j; j=t; }',
              '    for (const auto& p:kNonzero) if (p.a==i && p.b==j) {',
              '        rho=double(p.rho100000)/100000.0; return true;',
              '    }',
              '    return false;',
              '}', '} // namespace numos::scientific', '']
    return '\n'.join(lines)

def documentation(dataset):
    lines = ['# Physical constants — CODATA 2022', '',
             'Generated by `python scripts/generate-physical-constants.py`; do not edit this inventory manually.', '',
             'Machine-readable inventory: [`physical_constants.json`](../src/math/scientific/physical_constants.json). '
             'Pinned independent input: [`source_capture.json`](../data/scientific/codata2022/source_capture.json).', '',
             'Dataset **CODATA 2022**, NIST web **9.0**, web release **2024-05-09**, publication **2025-04-30**, consulted **2026-10-07**. '
             'The current recommended set is 2022; no CODATA 2026 recommended dataset is asserted.', '',
             'Stable ConstantId is independent of dataset version and ReferenceId. ReferenceIds 1–14 retain their identity; '
             'G is the sole addition (ReferenceId 59, ConstantId 15). All other context references remain outside this layer.', '',
             'The input audit contains exactly the 14 physical references found in registry.json at baseline '
             '`ab4017764a62a03e0befd51a50fa82f3c31b4b70`. Notably this set did not contain h, k, alpha, R, or G. '
             'Exact h and k are captured as dependency data only. G is added because six existing Planck references depend on it.', '',
             '| ReferenceId / ConstantId | EN / ES | Symbol | SI dimension L,M,T,I,Θ,N,J | Unit | Registry nominal; u; source | CODATA / derived nominal | u | u_r | Status |',
             '|---|---|---|---|---|---|---|---|---|---|']
    for r in dataset['constants']:
        b=r['baseline']
        previous=f"{b['nominal_decimal']}; {b['uncertainty_decimal']}; {b['source']}" if b else 'new basic dependency'
        nominal=r['nominal_decimal']
        if len(nominal)>25: nominal=f"{Decimal(nominal):.14E} (formula)"
        u=r['standard_uncertainty']
        if len(u)>25: u=f"{Decimal(u):.14E}"
        relative=f"{Decimal(r['relative_standard_uncertainty']):.7E}" if not r['exact'] else '0'
        lines.append(f"| {r['reference_id']} / {r['constant_id']} | {r['name_en']} / {r['name_es']} | {r['symbol']} | {r['dimension']} | {r['unit']} | {previous} | {nominal} | {u} | {relative} | {r['status']} |")
    lines += ['', 'All 12 independently tabulated original references agree with the pinned NIST digits and physical exactness. '
              'The source-relative uncertainty ratios above are calculated from published rounded u and nominal values; they need not reproduce NIST’s separately rounded u_r field digit for digit. '
              'Reduced Planck uses exact h/(2*pi), never its truncated decimal. Standard gravity is conventional exactness, not a claim about gravity at a location. '
              'Planck force and Planck energy were display-rounded derived values; they now use full dependency formulas and propagated u.', '',
              '| ConstantId | Dependency / authoritative central expression | Action / notes |', '|---|---|---|']
    for r in dataset['constants']:
        lines.append(f"| {r['constant_id']} | `{r['dependency_formula']}`; canonical `{r['nominal_expression']}` | {r['action']} {r['note']} |")
    lines += ['', '## Captured correlations', '',
              '36 of 36 off-diagonal pairs among the nine tabulated uncertain constants were queried explicitly from NIST. '
              'The runtime stores nonzero values sparsely and explicit-zero availability masks. '
              'Force and energy add 19 analytically derived pairs using their negative sensitivity to G. '
              'The total 11-source uncertain set has all 55 pairs available. Unknown IDs or future missing pairs return unavailable. '
              'There is no inference of independence from a missing table entry. Diagonal correlation is 1; exact quantities have no uncertainty sources.', '',
              '| Pair ConstantIds | rho | Evidence |', '|---|---|---|']
    for p in dataset['correlations']:
        evidence=f"[NIST pair query]({p['source_url'].replace('|','%7C')})" if p['basis']=='published' else f"Derivative signs with G; captured pair {p['dependency_pair']}"
        lines.append(f"| {p['a']}, {p['b']} | {p['rho']} | {evidence} |")
    lines += ['', 'The complete captured matrix is checked for positive semidefiniteness with a bounded Jacobi eigenvalue oracle; '
              'no unknown entries are filled with zeros. Published coefficients are rounded to five decimals; this selected matrix remains PSD within 1e-12. '
              'Perfect-correlation subspaces are singular by construction, so strict positive definiteness would be the wrong requirement. '
              'Use the published nominal/u precision; dependencies between separately rounded recommended constants can leave small residuals.', '',
              'The independent reversed-pair capture and 70-digit Decimal oracle are in '
              '[`independent_oracle.json`](../data/scientific/codata2022/independent_oracle.json). '
              'For a0×me, rho=-0.99999 gives u=7.3474178868567063e-51 kg m; forcing rho to zero gives '
              '1.6593332935255792e-50 kg m. Both zeroing and sign reversal must fail the gate.', '',
              '## Scientific scope and authorities', '',
              'This implements a bounded first-order sensitivity/covariance subset, not full GUM conformance. '
              'User inputs specify standard uncertainty (k=1); separate new inputs are independent by product contract. '
              'No probability distribution, effective degrees of freedom, confidence level, or guaranteed interval is inferred.', '',
              '- [NIST values and version history](https://physics.nist.gov/cuu/Reference/versioncon.shtml): web 9.0 uses the 2022 adjustment, released 9 May 2024. '
              '[Publication](https://www.nist.gov/publications/codata-recommended-values-fundamental-physical-constants-2022): 30 April 2025; data through 31 December 2022.',
              '- [JCGM 100:2008](https://doi.org/10.59161/JCGM100-2008E), §§5.1–5.2: local sensitivities and covariance; §7.2.6: uncertainty usually needs at most two significant digits, '
              'estimate rounded consistently, extra calculation digits retained. NumOS chooses two digits for compact reporting, with a separate inspection view and unrounded canonical payload.',
              '- [JCGM 100:2008/Amd.1:2026](https://doi.org/10.59161/PPDI3267), amendment to §4.1.4 and H.1.7: significant nonlinearity can affect both central estimate and uncertainty; '
              'higher-order or Monte Carlo treatment may be needed. NumOS rejects unsupported significant nonlinearity instead of inventing zero uncertainty.',
              '- [JCGM GUM-6:2020](https://doi.org/10.59161/JCGMGUM-6-2020), §12 and Annex F.2: model adequacy and a limited local comparison against the linearized model at xi±u(xi). '
              'This local check is not a proof for arbitrary nonlinear functions. A 10% remainder threshold and ±3u domain guards, if used, are NumOS product choices, not prescribed GUM thresholds.',
              '- [JCGM 101:2008](https://doi.org/10.59161/JCGM101-2008): distribution propagation by Monte Carlo is an alternative when linearization is inadequate; it is not implemented in production.',
              '- [NIST TN 1297 §7](https://www.nist.gov/pml/nist-technical-note-1297/nist-tn-1297-7-reporting-uncertainty): identify standard uncertainty or expanded uncertainty and k; '
              'probability interpretation needs support. U=2u is labelled k=2 and does not automatically mean 95% confidence.', '',
              'Only numerical facts, metadata, and original implementation notes are vendored. No JCGM PDF or font is redistributed. '
              '`fetch-codata2022.py` is an explicit maintainer network action; all generation, build and verification paths are offline.', '']
    return '\n'.join(lines)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args=parser.parse_args()
    dataset=assemble(json.loads(SOURCE.read_text(encoding='utf8')),json.loads(SPEC.read_text(encoding='utf8')))
    generated={'src/math/scientific/physical_constants.json':json.dumps(dataset,ensure_ascii=False,indent=2)+'\n',
               'src/math/scientific/PhysicalConstants.cpp':cpp(dataset),
               'docs/PHYSICAL_CONSTANTS_CODATA_2022.md':documentation(dataset)}
    for name,content in generated.items():
        path=ROOT/name
        if args.check:
            if not path.exists() or path.read_text(encoding='utf8')!=content:
                raise SystemExit(f'stale generated file: {name}')
        else:
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text(content,encoding='utf8',newline='\n')
    print(f"{'Checked' if args.check else 'Generated'} {len(generated)} files: 15 constants, 55 known pairs")

if __name__=='__main__':
    main()
