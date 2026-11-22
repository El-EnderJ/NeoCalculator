#!/usr/bin/env python3
"""Close post-review paired measurements with fresh linked-resource identities."""
import hashlib
import json
import statistics
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESOURCE = ROOT / 'out/math-tex-parity-02/resources'
OUT = RESOURCE / 'final-performance'
read = lambda path: json.loads(path.read_text(encoding='utf-8'))
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
old = read(RESOURCE / 'aftercritic1/RESOURCE_REPORT.json')
pairs = read(OUT / 'paired/comparison.json')
post = read(OUT / 'post-verification/prepared-identities.json')
assert pairs['pairs'] == 5 and len(pairs['runs']) == 10 and len(pairs['rows']) == 96
assert post['workloadsExecuted'] is False and post['identities'] == pairs['identities']
native = Path('C:/.piobuild/numOS-parity-02/native/emulator_pc/program.exe')
pool = Path('C:/.piobuild/numOS-parity-02/pool/emulator_pc/program.exe')
assert sha(native) == old['native']['sha256'] == '4987f4ea928aaa138c90ca3aecdb92bd1a6c5d6edbf28046a17633d03b7f3058'
assert sha(pool) == old['poolBinary']['sha256']
retained = read(RESOURCE / 'aftercritic1/retained-executables.json')
for record in retained['records']:
    assert sha(Path(record['executable'])) == record['sha256']
refresh_calls = read(OUT / 'resource-refresh-executions.json')
assert len(refresh_calls) == 7 and all(call['exitCode'] == 0 for call in refresh_calls)
for call in refresh_calls:
    assert sha(OUT / call['log']) == call['logSha256']
counts = {key: 0 for key in old['counters']}
per_run = []
for run in pairs['runs']:
    log = OUT / 'paired' / run['log']
    assert run['exitCode'] == 0 and sha(log) == run['logSha256']
    rows = [json.loads(line) for line in log.read_text(encoding='utf-8').splitlines() if line.startswith('{')]
    assert rows[-1]['summary'] and rows[-1]['failures'] == 0
    if run['label'] != 'candidate':
        continue
    totals = {key: sum(row[key] for row in rows[:-1]) for key in counts}
    assert totals == old['counters'], ('counters changed', run['pair'])
    assert all(row['cAllocations'] == 0 for row in rows[:-1] if row['phase'] == 'layout-denied')
    per_run.append(totals)
    for key in counts:
        counts[key] += totals[key]

boards = []
for prior in old['boards']:
    name, target = prior['name'], prior['resources']['target']
    raw = read(OUT / 'firmware' / (target + '.json'))
    types = read(OUT / 'firmware' / (name + '-types.json'))
    stack = read(OUT / 'firmware' / (name + '-stack-model.json'))
    assert raw['elfSha256'] == types['elfSha256'] == stack['elfSha256'] == prior['resources']['elfSha256']
    assert raw['imageSha256'] == prior['resources']['imageSha256']
    assert raw['marginPass'] and raw['appBytes'] == 6553600
    assert types['types'] == prior['types']['types'] and stack['provisionalUniformDepthHasReserve']
    assert {symbol['section'] for symbol in types['hotpathCodeSymbols']} == {'.flash.text'}
    boards.append(dict(name=name, resources=raw, types=types, stack=stack, before=prior['before']))

reviews = []
for iteration in (2, 3):
    path = ROOT / 'out/math-tex-parity-02/reviews' / ('iteration-' + str(iteration)) / 'CRITICAL_REVIEW.md'
    reviews.append(dict(path=path.relative_to(ROOT).as_posix(), sha256=sha(path)))
layout_ratio = statistics.median(row['medianRatio'] for row in pairs['rows'] if row['phase'] == 'layout-denied')
refresh_ratio = statistics.median(row['medianRatio'] for row in pairs['rows'] if row['phase'] == 'draw-cursor-denied')
result = dict(closedAtUtc=datetime.now(timezone.utc).isoformat(), phase='post-review-final',
              status='Post-review performance and resource measurement complete; no publication performed.',
              reviews=reviews, native=old['native'], poolBinary=old['poolBinary'], boards=boards,
              timing=dict(pairs=5, layoutMedianRatio=layout_ratio, refreshNavigationMedianRatio=refresh_ratio,
                          interpretation='Refresh/navigation includes one checked layout per frame inside its timed interval. It is not pure painting. Cursor time is separately measured inside cursor computation.',
                          source='paired/comparison.json', sourceSha256=sha(OUT/'paired/comparison.json'), runs=pairs['runs'],
                          maximumObservedLayoutScopeDepth={label:max(row[label]['maxLayoutDepth'] for row in pairs['rows']) for label in ('baseline','candidate')},
                          reportingCorrections=pairs.get('reportingCorrections',[])),
              candidateCountersPerRun=per_run[0], candidateCountersAcrossFiveRuns=counts,
              reusedLifecycleEvidence=dict(source='../aftercritic1/RESOURCE_REPORT.json',
                                          executionBoundary='Earlier complete tests on the exact same corrected binary; not rerun or relabeled as post-review timings.',
                                          lifecycle=old['lifecycle'], ownership=old['ownership'], recovery=old['recovery']),
              sourceAndLinkedPostVerification=post,
              resourceRefreshExecutions=refresh_calls,
              retainedPrivateExecutablesRehashed=retained['records'],
              limitations=old['limitations'] + [
                  'Instrumented native O1 timings are host observations, not ESP32 latency predictions.',
                  'The candidate includes structural admission, new source-derived typography and changed physical styles; the before/after ratio is not a like-for-like isolated algorithm speedup.',
                  'The aggregate is the median of 48 per-case/style ratios, not a total-workload ratio or performance acceptance threshold.',
                  'This probe does not separately measure exclusive painting time; subtracting independently measured layout medians would not establish it.',
                  'The quadratic Tutor fixture is representative geometry; actual Tutor page/action profiling belongs to the product evidence.'])
(OUT / 'RESOURCE_REPORT.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
md = '# Final post-review performance and resource evidence\n\n## Architecture Assessment\n\n'
md += 'The approved corrected candidate `4987f4ea928aaa138c90ca3aecdb92bd1a6c5d6edbf28046a17633d03b7f3058` was measured after full review 2 and the separate challenge 3. Five alternating-order pairs completed in the coordinated exclusive timing window. Ten raw logs retain UTC/order, executable/source identity and hashes. Fresh read-only ELF/DWARF profiles and post-run identities close the same binary. This work did not publish an emulator or flash hardware.\n\n'
md += '## Solution\n\n'
md += f'The median of per-case/style layout ratios is **{layout_ratio:.5f}**, and refresh/navigation ratios **{refresh_ratio:.5f}**. These report increased measured host work honestly; the individual absolute values and regressions are retained below. They supersede the timing observations in the loaded-host e019 checkpoint, which remains preserved.\n\n'
md += '**Refresh/navigation includes checked layout.** Its timer surrounds pan/cursor movement, invalidation and the entire LVGL refresh; each candidate frame includes one observed admission and root layout inside that interval, plus drawing, LVGL tasks and cursor work. It is not exclusive paint time. Cursor duration alone is separately accumulated inside `computeCursorPosition`.\n\n'
md += 'Text-style absolute medians (microseconds per layout/frame; cursor per actual call):\n\n| Case | Layout before | Layout after | Ratio | Refresh/navigation before | Refresh/navigation after | Ratio | Cursor after |\n|---|---:|---:|---:|---:|---:|---:|---:|\n'
lookup = {(row['case'], row['style'], row['phase']): row for row in pairs['rows']}
for row in pairs['rows']:
    if row['style'] != 1 or row['phase'] != 'layout-denied':
        continue
    frame = lookup[(row['case'], 1, 'draw-cursor-denied')]
    md += f"| {row['case']} | {row['baseline']['medianNs']/1000:.3f} | {row['candidate']['medianNs']/1000:.3f} | {row['medianRatio']:.3f} | {frame['baseline']['medianNs']/1000:.3f} | {frame['candidate']['medianNs']/1000:.3f} | {frame['medianRatio']:.3f} | {frame['candidate']['medianCursorNs']/1000:.4f} |\n"
md += '\nEvery style and minimum/maximum sample is retained in [paired/comparison.json](paired/comparison.json), with the complete ratio table in [paired/comparison.md](paired/comparison.md). Candidate work includes checked admission, authentic italic/script kern and operator metrics, and the corrected physical style transitions. These observations do not isolate the cost of any one change or predict device latency. No cache or metric approximation was added.\n\n'
md += 'Maximum observed instrumented layout-scope depth is 72 in both versions. It is a maximum, not a per-invocation event count or a raw AST-depth measurement. A [summary-only normalization correction](paired/DEPTH_REPORTING_CORRECTION.md) moved this value out of normalized counters; all raw logs, times and additive counts remain unchanged.\n\n'
md += 'Across the five candidate runs, 240,000 checked layouts plus 4,800 actual frames passed allocation denial. Counters are exactly equal in all five runs and to the corrected pre-review run. Linked C allocation requests during layout/cursor and all own C++/AST requests remain zero; existing LVGL C draw-task requests remain explicitly counted.\n\n| Counter | Per candidate run | Across five runs |\n|---|---:|---:|\n'
for key, value in counts.items():
    md += f'| {key} | {per_run[0][key]:,} | {value:,} |\n'
md += '\n## Memory & Typographic Profile\n\n| Target / metric | Received | Final | Delta |\n|---|---:|---:|---:|\n'
for board in boards:
    for key in ('imageBytes', 'freeBytes', 'linkedFlash', 'staticRam', 'iram'):
        before, after = board['before'][key], board['resources'][key]
        md += f"| {board['name']} / {key} | {before:,} | {after:,} | {after-before:+,} |\n"
md += '\nBoth app partitions remain 6,553,600 bytes and exceed the 131,072-byte free margin. All enumerated layout/delimiter/canvas/cursor code remains in `.flash.text`; no new IRAM placement is claimed. Target ABI sizes are unchanged by the critic corrections. Full initial-to-final per-type deltas are in the fresh firmware type JSON files.\n\n'
stack = boards[0]['stack']
md += f"The largest recursive own-frame envelope remains {stack['uniformRecursiveOwnEnvelopePerRawEdge']} B. At 192 raw edges, the conservative contribution including a leaf is {stack['rawEdgeEnvelopeIncludingLeaf']:,} B, with {stack['slackAfterKnownEnvelope']:,} B left in the configured 65,536-byte task stack. Ninety root/row pairs model {stack['ninetyRootRowPairsPlusRootRowAndNumber']:,} B; the alternating fraction/row/root/row/power/row cycle contributes {stack['alternatingFractionRootPowerWithThreeRowsPerCycle']} B. These are simultaneous-own-frame models, not measured physical high-water marks.\n\n"
md += 'Earlier full tests on this exact corrected native/pool binary remain applicable: fixed-pool 100 cycles after four warmups with 6,128/5,520 B minimum sampled free/largest block; ownership 100 cycles after 54 warmups with stable live totals; 31 recovery journeys (27 persistent failures and four allocation-free controls). They were not rerun during the timed window. Their actual scopes, identities and host-only ownership totals are preserved in [the corrected-candidate checkpoint](../aftercritic1/RESOURCE_REPORT.md), and both exact private EXEs are rehashable via [retention verification](../aftercritic1/RETAINED_EXECUTABLES.md).\n\n'
md += '## Risks & Edge Cases\n\n' + ''.join('- ' + line + '\n' for line in result['limitations'])
md += '\n## Alternatives Considered\n\nNo global geometry cache, memory-pool expansion, font approximation, partition change or productive profiling instrumentation was introduced. Resource costs and host timing regressions remain explicit; firmware peak/fragmentation and device performance require separate hardware measurement.\n'
(OUT / 'RESOURCE_REPORT.md').write_text(md, encoding='utf-8')
print('PASS final post-review closure:', OUT / 'RESOURCE_REPORT.md')
