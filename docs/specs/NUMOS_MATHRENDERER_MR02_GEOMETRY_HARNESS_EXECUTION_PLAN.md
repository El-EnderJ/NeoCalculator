# NumOS — MR-02 Execution Plan: Host Math-Layout Harness + Geometry Snapshots

> **Exact implementation plan** for ticket MR-02 (`NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md:66-81`)
> — the pixel-independent geometry oracle (test plan layer **O-B**). Written for mechanical
> execution by an Opus/Sonnet-class implementer. Verified against `b234e15` (2026-07-03).
>
> **Empirical grounding (this audit, on this machine)**: the exact two-TU closure
> `src/math/MathAST.cpp` + `src/math/font/MathGlyphAssembly.cpp` was compiled with plain
> `g++ -std=c++17 -O1 -I src` (no LVGL, no Arduino, no SDL, no PlatformIO) and executed: a
> nested-fraction tree laid out successfully (`w=42 asc=50 desc=15` with `defaultFontMetrics()`),
> a second `calculateLayout` pass over the same tree produced **identical** results, and
> `dumpTree` already annotates geometry (`Row  [42x65 a50/d15]`, suffix built at
> `MathAST.cpp:1563-1570`). One pre-existing warning: `-Wunused-variable` at `MathAST.cpp:1112`.
> The harness design below is therefore proven feasible, not predicted.
>
> Governing decisions: **ADR-MR00-06** (snapshot policy, fixture metrics), ADR-MR00-03 (budgets),
> ADR-MR00-07 (golden policy) in `NUMOS_MATHRENDERER_MR00_DECISION_RECORDS.md`.
> Build/CI model: `NUMOS_CAS_HOST_HARNESS_SPEC.md` (standalone g++, forked isolation not needed
> here — layout cannot loop; XFAIL not needed — no known-bad rows; footer style reused).
> Dependency: MR-01 must land first (it ships `dump_math_metrics`, the fixture source — MR-01
> plan §H.11).

---

## A. Executive summary

MR-02 adds a **host-only** test target that runs the production layout code
(`calculateLayout` and everything it calls) over a fixed ≥80-case expression corpus with
**committed fixture FontMetrics**, dumps a machine-readable per-node geometry snapshot (JSONL),
and compares it byte-for-byte against a committed baseline. It touches **zero files under
`src/**`** — it is pure test infrastructure:

- `tests/host/mathlayout_geometry_test.cpp` — harness (corpus + walker + emit + compare).
- `tests/host/mathlayout_fixed_metrics.h` — the FontMetrics fixture (captured via MR-01's
  `dump_math_metrics`, procedure §D.4).
- `tests/host/golden_geometry/mathv2_geometry.jsonl` — the committed snapshot baseline.
- `scripts/build-mathlayout-host-tests.sh` — build+run driver (single source of truth for TUs).
- `scripts/check-math-metrics-fixture.py` — emulator↔fixture parity checker.
- `.github/workflows/emulator-build.yml` — two appended steps (may be split to PR-D).

Payoff: every later renderer ticket — above all the MR-05 freeze-refactor — can **prove**
bit-identical geometry instead of asserting it; layout drift is caught per-node, per-case,
before any PPM changes; and the orphaned intent of `tests/MathEnginePhaseRegression.cpp`
(2,123 lines referenced by nothing — GT §K) is revived in a maintainable form.

---

## B. Why the geometry harness must precede the renderer refactor

1. **MR-05 is unreviewable without it.** MR-05 moves geometry policy out of `MathAST.h`
   (`fractionPartMetrics` `MathAST.h:314-326`, `fractionBarGaps` `:569-588`,
   `superscriptShiftMetrics` `:484-513`, large-op helpers `:353-415`) and wraps layout in
   memoization. Its contract is "bit-identical geometry" (tickets MR-05 invariants). PPM goldens
   test only ~7 showcase expressions' pixels (`kShowcaseIds`, `NativeHal.cpp:1103-1113`) plus
   two calc screens; the snapshot corpus tests ~90 expressions × every node — that is the
   difference between "the 9 golden screens didn't change" and "layout didn't change".
2. **Snapshots localize.** A golden diff says "some pixels differ somewhere"; a snapshot diff
   says "case `deep_nested_exponent`, node `c0/exp/c0`, ascent 21→22" — reviewable in seconds.
3. **Snapshots are font-rasterization-free**, so they run on any host in <1 s and can gate PRs
   *before* the multi-minute emulator build (CAS-harness placement precedent,
   `NUMOS_CAS_HOST_HARNESS_SPEC.md` §E).
4. **Every MR-03+ ticket needs the corpus anyway** (round-trip oracles will reuse the same
   builders; new node kinds add cases additively).

---

## C. Current layout data available today (what the snapshot can read)

| Data | Where | Exposed? |
|---|---|---|
| `LayoutResult{width, ascent, descent, inkAscent, inkDescent}` per node | cached in every node after `calculateLayout` (`MathAST.h:447-459`, field `:646`, accessor `layout()` `:627`) | public getter [V] |
| Root layout consumed by MathCanvas | `onDraw` reads `_root->layout()` after recompute (`MathRenderer.cpp:737-744`) | emulator-side; host reads the same cache |
| Baseline conventions | `layoutBaselineFromTop/layoutTopFromBaseline/rowChildTopFromRowTop` (`MathAST.h:521-538`); ascent/descent positive from baseline (Y-2) | pure helpers [V] |
| Ink vs line metrics | `inkAscent/inkDescent` per node + `layoutInkAscentPx/DescentPx` helpers (`MathAST.h:461-471`) | public [V] |
| Per-node script level | `MathNode::scriptLevel()` — **written during layout** by `applyScriptLevel` (`MathAST.cpp:80-82`, called from each `calculateLayout`) | public [V] — snapshot records it |
| Node-kind extras cached by layout | fraction: `ruleThicknessPx/barHalfUpPx/barHalfDownPx/numeratorShiftPx/denominatorShiftPx/ruleOverhangPx` (`MathAST.h:785-790`); paren: `parenWidth()` (`:937`); power: `italicCorrectionPx()` (`:836`); root: `radicalRuleThickness/VerticalGap/ExtraAscender/KernBefore/KernAfter` (`:886-890`); function/logbase: `labelWidth/parenWidth/innerPad/parenAscent/parenDescent` (`:976-980`, `:1028-1032`) | public getters [V] |
| Structure walk | `childCount()/child(i)` generic (`MathAST.h:637-638`) + typed accessors for slot naming | public [V] |
| **Not exposed / not recorded** | per-child x-offsets inside rows (computed transiently in draw and in `childXOffset`, `MathRenderer.h:392`); absolute screen positions; the `FontMetrics`/`MathStyle` actually applied per node (only `scriptLevel` is stored); draw-time geometry (cursor, scroll, delimiter stroke shapes) | snapshot design works around each — §E (gapSum) and §J (goldens cover draw) |

---

## D. Host harness design

### D.1 Build model: standalone g++, not a PlatformIO env

Same verdict and reasons as the CAS harness (`NUMOS_CAS_HOST_HARNESS_SPEC.md` §C.4): a
PlatformIO `native` env needs the `lib_ignore` dance for `lib/giac`, trips over the Windows
`build_dir = C:/.piobuild/numOS` (`platformio.ini:6`), and hides the TU list. One shell script is
the single source of truth for what links.

### D.2 Exact translation units (empirically verified closure)

```
tests/host/mathlayout_geometry_test.cpp    # NEW — harness main
src/math/MathAST.cpp                       # production layout (the thing under test)
src/math/font/MathGlyphAssembly.cpp        # DelimiterAssembler + g_delimiterAssemblyRenderable
src/math/MathRenderVisualCases.cpp         # 21 accepted case builders (compiles when !ARDUINO — guard at :1)
src/math/MathStressExpressions.cpp         # 17 stress case builders (no guard; pure vpam factories — verified)
```

Flags: `g++ -std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -I src`
(the `-Wno-unused-variable` covers the pre-existing warning at `MathAST.cpp:1112`; do **not**
"fix" that variable in this ticket — src is frozen).
No `NATIVE_SIM`, no defines at all: none of these TUs needs one (verified).
`g_delimiterAssemblyRenderable` stays at its default `false`
(`MathGlyphAssembly.cpp:15-18`) — **matching the device/emulator reality** (the stix subset
lacks U+239C, so the boot probe leaves it false, `MathRenderer.cpp:431-435`). The harness must
assert this at startup and record it in the snapshot header; when MR-04/MR-11 flip it, the flip
is an explicit re-baseline event.

### D.3 How LVGL is avoided (the fixture-metrics mechanism, ADR-MR00-06)

The layout core is LVGL-free by design (`MathTypography.h:23` — "PURE MATH layer"; B-2), but the
*inputs* — `FontMetrics` — are produced on-device by probing LVGL glyphs
(`metricsFromFont`, `MathRenderer.cpp:446-535`). The harness therefore uses a **committed
fixture** instead of the probe:

```cpp
// tests/host/mathlayout_fixed_metrics.h — GENERATED BY PROCEDURE §D.4; do not hand-edit values.
// Captured from: [MATHMETRICS] dump, emulator build <git-sha>, fonts stix_math_{18,12,8}.c.
#pragma once
#include "math/MathAST.h"
namespace mathlayout_fixture {
// fields in metricsFromFont population order: charWidth, ascent, descent,
// lineAscent, lineDescent, capHeight, scriptLevel, script*, emSize, style,
// numberAscent, numberDescent
inline vpam::FontMetrics normalMetrics();        // em 18, scriptLevel 0
inline vpam::FontMetrics scriptMetrics();        // em 12, scriptLevel 1
inline vpam::FontMetrics scriptScriptMetrics();  // em  8, scriptLevel 2
// wire exactly like MathCanvas ctor (MathRenderer.cpp:417-425):
//   normal.script → script; script.script → scriptscript; scriptscript.script → nullptr
vpam::FontMetrics wiredNormal(vpam::MathStyle style);  // returns normal with .style set
inline constexpr const char* kFixtureHash = "<sha256-8 of the three value rows>";
}
```

The three profile value rows are transcribed **verbatim** from the `[MATHMETRICS]` lines
(MR-01 plan §H.11). Style handling per case mirrors the two shipped consumers: showcase sets the
case's own style (`NativeHal.cpp:1145`), CalculationApp pins TEXT (`CalculationApp.cpp:161`);
the harness uses each corpus case's declared `MathStyle` (visual/stress cases carry one —
`MathRenderVisualCases.h:9-14`).

### D.4 Fixture capture procedure (verbatim; rerun on any font/metrics change)

```bash
export PLATFORMIO_BUILD_DIR=.pio/build
pio run -e emulator_pc
SDL_VIDEODRIVER=dummy .pio/build/emulator_pc/program --headless --deterministic \
  --script tests/emulator/scripts/mathv2_metrics_dump.numos --frames 800 --quiet \
  | grep '^\[MATHMETRICS\]' > /tmp/metrics.txt
# transcribe the three lines into tests/host/mathlayout_fixed_metrics.h,
# recompute kFixtureHash (sha256 of the three lines, first 8 hex chars),
# and record the emulator git sha in the header comment.
python scripts/check-math-metrics-fixture.py /tmp/metrics.txt   # must print OK
```

`scripts/check-math-metrics-fixture.py` (NEW, stdlib-only): parses `[MATHMETRICS]` lines from a
file or stdin, parses the committed header's value rows (regex on the generated section), exits
0 iff every field matches, exit 1 with a per-field diff otherwise, exit 2 on malformed input.
This same script is the **CI parity gate** (§I step 2) — fixture drift can never be silent.

### D.5 Portability

Linux (CI) and macOS: `g++`/`clang++` via `CXX` env var. Windows: MinGW g++ (same command; the
script uses `#!/usr/bin/env bash` — run under Git-Bash/MSYS, mirroring
`run-emulator-windows.ps1` users' setup). No SDL, no display, no filesystem writes outside
`$OUT` (default `/tmp/mathlayout-host` via `MATHLAYOUT_HOST_OUT`, CAS-harness pattern).
Determinism: pure integer math over committed constants (Y-6) ⇒ identical output on all
platforms/compilers; the double-pass check (§H) enforces it per run.

### D.6 Harness CLI contract (`tests/host/mathlayout_geometry_test.cpp`)

```
mathlayout_geometry_test [--emit PATH]      # write candidate JSONL (default $OUT/candidate.jsonl)
                         [--baseline PATH]  # committed baseline (default tests/host/golden_geometry/mathv2_geometry.jsonl)
                         [--update]         # overwrite baseline with candidate (HUMAN-ONLY; CI never passes this)
                         [--case ID]        # run one case (debugging)
                         [--list]           # print corpus ids + status, exit 0
```

Exit codes: `0` all cases match baseline (or `--update`/`--list`); `1` any mismatch/missing/extra
line; `2` internal error (fixture assert, I/O, malformed baseline). Footer (grep-stable):
`[MATH-GEO] cases=<n> built=<n> skipped_future=<n> mismatched=<n> exit=<code>`.
Mismatch output: for each differing case, print `case=<id>` plus the first 10 differing
node records as `path=<p> field=<f> baseline=<x> candidate=<y>` — then the unified requirement:
the raw candidate JSONL is always written so CI uploads it as an artifact.

---

## E. Snapshot format (normative)

**File**: one JSONL file, `tests/host/golden_geometry/mathv2_geometry.jsonl` — one line per
expression case, ASCII, keys in the fixed order below (the emitter is deterministic; JSON is
never reformatted by tooling). Line-diffable per case in PRs.

**Header line** (first line of the file):

```json
{"v":"mrgeo1","fixture":"<kFixtureHash>","assembly_renderable":false,"cases":<n>}
```

**Case line schema** (`v` implied by header):

```json
{"id":"<case id>","src":"visual|stress|mrv2","style":"display|text","stats":{"depth":D,"nodes":N,"frac":F,"script":S,"saturated":false},
 "root":{"w":W,"a":A,"d":D,"ia":IA,"idc":IDC},
 "nodes":[{"p":"<path>","k":"<kind>","sl":L,"dp":DP,"w":..,"a":..,"d":..,"ia":..,"idc":..,"x":{<extras>}}, …]}
```

- `id`: corpus id (§F). `src`: which builder family. `style`: the `MathStyle` the case laid
  out under.
- `stats`: `mathTreeStats` output (MR-01 helper — same walker, one truth).
- `nodes`: **every node**, preorder. `p` = slash-joined path from root: NodeRow children are
  `c<i>`; named slots use the frozen labels
  `num den base exp rad deg content arg lo hi body var sub` (matching the typed accessors in
  `MathAST.h`). Root row is path `""` (emitted as `"/"`). Example:
  `/c2/num/c0` = root row → 3rd child → numerator row → 1st child.
- `k`: NodeType name (`Row Number Operator Empty Fraction Power Root Paren Function LogBase
  Constant Variable PeriodicDecimal DefIntegral Summation Subscript BigOp` — the 17 kinds,
  `MathAST.h:72-90`; future kinds append).
- `sl`: `node->scriptLevel()` (written by layout — `MathAST.cpp:80-82`); `dp`: depth.
- `w/a/d/ia/idc`: `LayoutResult` width/ascent/descent/inkAscent/inkDescent.
- `x` extras by kind (omit key when kind has none):
  - `Row`: `{"gapSum":G}` where `G = w − Σ(children w)` — pure arithmetic on captured values;
    captures inter-atom spacing drift without re-implementing the spacing walk
    (ADR-MR00-06 / Special Question 9).
  - `Fraction`: `{"rt":..,"bu":..,"bd":..,"ns":..,"ds":..,"ov":..}` (the six getters,
    `MathAST.h:785-790`).
  - `Paren`: `{"pw":..}`; `Power`: `{"ic":..}`;
    `Root`: `{"rt":..,"vg":..,"ea":..,"kb":..,"ka":..}`;
    `Function`/`LogBase`: `{"lw":..,"pw":..,"ip":..,"pa":..,"pd":..}`;
    `BigOp`: `{"disp":0|1}` (`useDisplayLimits()`, `MathAST.h:1328`).
- Glyph-fallback data: none at layout level (resolution happens at draw). The header's
  `assembly_renderable` plus `Paren.pw`/ink fields carry everything layout knows; draw-side
  fallback is MR-01's counter + goldens (§J).
- Warning flags: `stats.saturated` (walker cap) — a saturated case is a harness **error**
  (exit 2), not a snapshot row.

**Example case lines** (illustrative values — real ones pinned at first `--update`):

```json
{"id":"visual/nested_fraction","src":"visual","style":"text","stats":{"depth":5,"nodes":14,"frac":2,"script":0,"saturated":false},"root":{"w":58,"a":34,"d":21,"ia":34,"idc":21},"nodes":[{"p":"/","k":"Row","sl":0,"dp":0,"w":58,"a":34,"d":21,"ia":34,"idc":21,"x":{"gapSum":0}},{"p":"/c0","k":"Fraction","sl":0,"dp":1,"w":58,"a":34,"d":21,"ia":34,"idc":21,"x":{"rt":1,"bu":1,"bd":0,"ns":6,"ds":9,"ov":1}}, …]}
{"id":"mrv2/frac_ladder_d7","src":"mrv2","style":"text","stats":{"depth":15,"nodes":22,"frac":7,"script":0,"saturated":false},"root":{"w":…},"nodes":[…]}
```

---

## F. Geometry corpus (exact case list, ≥80)

Three sources. Existing builders are linked, not copied (§D.2). Every case id is frozen once the
baseline is blessed; new cases append.

### F.1 `visual/*` — the 21 accepted visual cases [current]

All of `mathRenderVisualCases()` (`MathRenderVisualCases.cpp:157-179`), each with its declared
style. Why: these are the *accepted geometry* the golden `math_showcase_smoke` shows 7 of; the
snapshot freezes all 21. Representable today: yes.

### F.2 `stress/*` — the 17 stress cases [current]

All of `kStressCases` (`MathStressExpressions.cpp:220-238`): quadratic formula, Gauss-law
display/text, deep nested exponent, elastic matrix brackets (the faked-with-fractions matrix),
nested radicals, Cauchy integral display/text, four-level fraction, summation/product/union
display+text, log/trig composite, determinant bars, brace system, bracketed binomial stack.
Why: widest current construct coverage incl. big operators, tall delimiters, integrals.
Representable today: yes (they are built and rendered on the validate firmware today).

### F.3 `mrv2/*` — 52 new targeted cases [current unless marked]

Defined as builder functions inside the harness using the public factories
(`MathAST.h:1343-1406`). Notation below is NumIR-flavored shorthand for the builder recipe.

| # | id | Recipe | Style | Why it matters | Status |
|---|---|---|---|---|---|
| 1 | `mrv2/int_single` | `7` | text | SymbolBox floor case; charWidth anchor | current |
| 2 | `mrv2/int_multi` | `1234567890` | text | digit-run advance | current |
| 3 | `mrv2/dec` | `3.1415` | text | decimal point advance | current |
| 4 | `mrv2/var_x` | `id(x)` | text | italic variable box | current |
| 5 | `mrv2/var_ans` | `Ans` (`makeVariable('#')`) | text | word-chip label width | current |
| 6 | `mrv2/const_pi_e` | `pi*e` row | text | constant glyph boxes + BIN spacing | current |
| 7 | `mrv2/ops_spacing_bin` | `1+2-3*4` | text | BINARY spacing (4 mu) | current |
| 8 | `mrv2/ops_spacing_rel` | `1=2<3` (relations) | text | REL spacing (5 mu) | current |
| 9 | `mrv2/ops_pm` | `1±2` | text | ± axis centering | current |
| 10 | `mrv2/frac_simple` | `frac(1;2)` | text | FractionBox anchor (calc golden twin) | current |
| 11 | `mrv2/frac_display` | `frac(1;2)` | display | display vs text gap constants | current |
| 12 | `mrv2/frac_asym_ink` | `frac(1;pow(id(y);2))` | text | ink-derived shifts (`fractionBarGaps`, `MathAST.h:569-588`) | current |
| 13-19 | `mrv2/frac_ladder_d1…d7` | nested `frac(1; frac(1; …))` ladders, 1–7 fraction levels | text | the cfrac tower policy: full glyph size at L0 (`fractionPartMetrics`, `MathAST.h:314-326`); depth ramp toward the budget; d7 ⇒ structural depth 15 | current |
| 20 | `mrv2/frac_wide_num` | `frac(1+2+3+4+5;2)` | text | rule overhang + centering | current |
| 21 | `mrv2/pow_simple` | `pow(2;2)` | text | superscript shift anchor | current |
| 22 | `mrv2/pow_italic_base` | `pow(id(f)?…` — use `pow(id(x);2)` | text | italic correction (`MathAST.cpp:482-506`, getter `MathAST.h:836`) | current |
| 23-27 | `mrv2/pow_ladder_d1…d5` | `pow(2;pow(2;…))` towers | text | script-level demotion L0→L1→L2 clamp (`FontMetrics::superscript`, `MathAST.h:249-290`) | current |
| 28 | `mrv2/pow_of_paren` | `pow(paren(id(x)+1);2)` | text | power-of-delimited-base — exemplar-B superscripted bracket precursor | current |
| 29 | `mrv2/pow_frac_exp` | `pow(2;frac(1;2))` | text | fraction demoted inside exponent | current |
| 30 | `mrv2/sub_simple` | `sub(id(x);1)` (`makeSubscript`) | text | SubscriptShiftDown floor (`MathAST.cpp:1395`) | current |
| 31 | `mrv2/root_simple` | `root(2)` | display | RadicalBox anchor | current |
| 32 | `mrv2/root_degree` | `root(8;3)` | display | degree raise 55 % + kerns (`MathAST.cpp:563-617`) | current |
| 33-35 | `mrv2/root_ladder_d2…d4` | `root(2+root(2+…))` chains | display | exemplar-B radical-chain tail (finite prefix) | current |
| 36 | `mrv2/root_tall_content` | `root(frac(1;2))` | display | radical over tall ink | current |
| 37 | `mrv2/func_sin` | `sin(id(x))` | text | label + auto-paren metrics (`MathAST.h:976-980`) | current |
| 38 | `mrv2/func_arcsin` | `arcsin(id(x))` | text | widest label | current |
| 39 | `mrv2/logb` | `logb(2;8)` | text | subscripted log (`MathAST.cpp:778-816`) | current |
| 40 | `mrv2/func_tall_arg` | `sin(frac(1;2))` | text | auto-paren growth around tall arg | current |
| 41 | `mrv2/paren_simple` | `paren(1+2)` | text | DelimitedBox floor | current |
| 42-44 | `mrv2/paren_ladder_d2,d3,d5` | nested parens ×2/×3/×5 | text | delimiter growth per nesting (4.6) | current |
| 45 | `mrv2/paren_tall_frac` | `paren(frac(frac(1;2);3))` | text | symmetric-axis delimiter geometry (`MathAST.cpp:97-155`) | current |
| 46 | `mrv2/brackets_braces_bars` | `bracket(bar(brace(id(x))))` | text | all DelimKinds in one tree (`MathAST.h:164-191`) | current |
| 47 | `mrv2/sum_display` | `sum(id(n)=1;10;pow(id(n);2)+1)` | display | display limits stacked (`shouldUseDisplayLimits`, `MathAST.h:410-415`) | current |
| 48 | `mrv2/sum_text_inline` | same | text | inline-limit variant — the style-demotion tripwire for the future `@display` flag | current |
| 49 | `mrv2/sum_in_frac_num` | `frac(sum(id(k)=1;10;id(k)); 2)` | display | **exemplar-B numerator shape**: Σ demotes inside the fraction today; this case freezes the *current* (pre-`@display`) geometry so MR-12's flag lands as an explicit new case, not silent drift | current |
| 50 | `mrv2/prod_display` | `bigop(prod; id(i)=1; id(n); id(x))` | display | NodeBigOp variant selection (`stix_math_variants` tables) | current |
| 51 | `mrv2/union_inline` | `bigop(union;…)` | text | set-op inline limits | current |
| 52 | `mrv2/integral_bounds_frac` | `integral(0; frac(pi;2); pow(id(x);2); id(x))` | display | **exemplar-B ∫₀^{π/2} exactly**; differential tail gap (`integralDifferentialGapPx`, `MathAST.h:381-384`) | current |
| 53 | `mrv2/pdec` | `pdec(0;1;6;neg)` (`makePeriodicDecimal`) | display | overline metrics (`MathAST.cpp:960-1013`) | current |
| 54 | `mrv2/wide_row_scroll` | the `wide_scroll` shape ×2 concatenated | display | int16 accumulation headroom; widest committed case | current |
| 55 | `mrv2/relation_row` | `frac(1;pi) = frac(2*root(2);9801)` | display | **exemplar-A LHS + prefactor exactly** | current |
| 56 | `mrv2/ramanujan_body_frac` | `frac( paren(1103+26390*id(k)) ; pow(396; 4*id(k)) )` — factorials omitted (no node yet) | display | exemplar-A body, buildable subset | current |
| 57 | `mrv2/boss_bracket_power` | `pow(bracket(frac(1;2)+root(2)); 2)` | display | exemplar-B giant-bracket^exponent skeleton | current |
| 58 | `mrv2/boss_cfrac_in_root` | `root(1 + frac(pow(id(x);2); 1 + frac(pow(id(x);2); 1+1)))` | display | exemplar-B √(cfrac tower) finite prefix | current |
| 59 | `mrv2/depth15_tower` | fraction ladder to structural depth 15 (editor-cap twin of MR-01 script §I.5) | text | deepest hand-typable tree; budget-adjacent | current |
| 60 | `mrv2/empty_slots` | `frac(_;_)` + `root(_)` row (NodeEmpty everywhere) | text | placeholder box metrics (`NodeEmpty::MIN_*`, `MathAST.h:750-751`) | current |
| 61-64 | `future/matrix_2x2`, `future/binom`, `future/lim_underscript`, `future/cfrac_ddots` | — | — | exemplar-B constructs with **no node kind today** (`NodeMatrix`/`NodeBinom`/`NodeLimit`/`NodeEllipsis`: P2, notation spec §M/§H/§K/§P) | **placeholder** — corpus rows exist with `status:future`; harness prints `SKIP future/<id> (node kind lands MR-13/14/12/14)` and counts them in the footer (`skipped_future=4`) so absence is visible, never silent |

**Count**: 21 + 17 + 60 built + 4 placeholders = **98 rows, 94 built** (≥80 required).
Category rationale requested by the task (integers, variables, fractions, nested fractions,
powers, subscripts, roots, nested radicals, functions, parentheses, tall delimiters,
relations, matrices-future, sum/product, cfrac-future, Ramanujan pieces, boss pieces) is covered
by the mapping above; subscripts exist today via `NodeSubscript` (tutor-produced,
`MathAST.h:1246-1279`) — case 30.

---

## G. Snapshot comparison policy

- **Exact equality** on every field of every line (string compare of canonicalized lines is
  sufficient because the emitter is deterministic; the comparator still parses JSON to produce
  per-field diagnostics). **No tolerances** — layout is integer math (Y-6); banded asserts exist
  only in the emulator command surface (MR-01 §H.3), never in committed snapshots
  (ADR-MR00-06).
- **Change classification** (the comparator prints which one):
  1. `fixture-mismatch` — header `fixture` differs from `kFixtureHash` ⇒ metrics changed;
     re-run §D.4 and re-baseline **in the same PR as the font/metrics change**.
  2. `corpus-change` — case ids added/removed ⇒ expected only in PRs that add cases.
  3. `geometry-drift` — same ids, differing node fields ⇒ a layout change. Either a bug
     (freeze-refactor context: always a bug) or an intentional metric change (must be listed in
     the PR description case-by-case).
- **Intentional update**: human runs `scripts/build-mathlayout-host-tests.sh --update`,
  reviews `git diff tests/host/golden_geometry/`, commits with a message naming the cause.
  Exact analog of golden promotion (golden README flow).
- **Accidental-rebaseline prevention**: CI never passes `--update`; `--update` refuses to run
  when the candidate's fixture hash ≠ the committed header's **unless** `--update` is combined
  with a fresh fixture (the script re-runs the parity check first); baseline file changes
  require PR review like goldens (recommend a CODEOWNERS line for
  `tests/host/golden_geometry/**` — maintainer decision).

---

## H. Cache-off/cache-on gate (the MR-05 hook, built now)

The harness runs every case **twice** on the same tree and compares the two snapshot records
in-memory before emitting:

1. Pass 1: fresh tree → `calculateLayout(fm)` → record.
2. Pass 2: same tree → `calculateLayout(fm)` again → record; **must be byte-identical**
   (verified feasible in this audit's probe — second pass identical).

Today this proves layout is a pure function of (tree, metrics) — idempotent, no hidden state
mutation (failure mode §K.8). After MR-05:

- Pass 2 becomes a **memo-hit** pass (layout valid ⇒ near-zero work) — identity now proves
  Y-7 (memoization is semantics-free) at the geometry level.
- MR-05 additionally builds the harness twice — once normal, once with
  `-DNUMOS_MATH_LAYOUT_CACHE_OFF=1` (the MR-05 kill-switch flag) — and byte-compares the two
  full JSONL outputs. The build script gains a `--cache-off` flag **now** (it simply appends
  the define; harmless no-op until MR-05 introduces the flag) so MR-05's CI wiring is one line.
- MR-05 will append mutate-then-relayout scenario cases (edit a subtree, relayout, compare to a
  fresh build of the post-edit tree) — the harness's case API must accept a
  `mutate(MathNode* root)` hook from day one (empty for all Phase-0 cases).

Failure contract: any pass-1 vs pass-2 divergence is exit 1 with `field=… pass1=… pass2=…`
diagnostics — this is a **hard error even in `--update` mode** (a nondeterministic layout must
never be baselined).

---

## I. CI integration plan

Two appended steps in `.github/workflows/emulator-build.yml` (may be deferred to PR-D per the
Phase-0 acceptance plan; commands are identical either way):

1. **`Math geometry harness (MR-02)`** — inserted immediately after the KeyCode guard
   (`emulator-build.yml:96-104`), before "Build emulator": fails in ~30 s.

```yaml
- name: Math geometry harness (MR-02)
  run: scripts/build-mathlayout-host-tests.sh
- name: Upload geometry candidate
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: mathlayout-geometry-candidate
    path: /tmp/mathlayout-host/candidate.jsonl
    if-no-files-found: warn
```

2. **`Math metrics fixture parity (MR-02)`** — after the emulator build (needs the binary),
   e.g. right after the Phase 9F step:

```yaml
- name: Math metrics fixture parity (MR-02)
  run: |
    set -euo pipefail
    SDL_VIDEODRIVER=dummy timeout 60s .pio/build/emulator_pc/program \
      --headless --deterministic \
      --script tests/emulator/scripts/mathv2_metrics_dump.numos \
      --frames 800 --quiet | tee /tmp/metrics.txt
    python scripts/check-math-metrics-fixture.py /tmp/metrics.txt
```

- Expected runtime: compile ≈5-15 s (five TUs, `-O1`), run <1 s, parity step ≈10 s — total
  well under the 30 s/invocation budget.
- Failure format: §D.6 footer + per-field lines; candidate JSONL uploaded always.
- OS coverage: Linux gates; macOS/Windows are documented-portable, not CI-gated (matches the
  golden pipeline's Linux-only rule).
- **Gating**: hard gate, and it gates **before** the emulator goldens by construction
  (step order) — layout drift fails before pixels are even generated.

---

## J. Relationship to PPM goldens

| Property | Geometry snapshots (O-B) | PPM goldens (O-E) |
|---|---|---|
| What they see | every node's measured box for ~94 expressions | final composited pixels of ~19 screens |
| Measurement drift (shifts/gaps/sizes) | ✔ per-node, localized | shadowed (only if a covered screen shows it) |
| Draw/placement bugs (`drawNodeWithLayout` conversion `MathRenderer.cpp:1481-1493`, stroke shapes, cursor, clipping, scroll, LVGL compositing, font rasterization) | ✖ invisible | ✔ |
| Missing-glyph/tofu regressions | ✖ (layout doesn't resolve glyphs) | ✔ + MR-01 counters |
| Cost / locality of failure | <1 s, names the node | minutes, names the screen |
| Update ceremony | `--update` + line diff review | human promotion + image review |
| **When both are required** | every geometry-visible change (MR-11 wave; any spacing fix) and every freeze-refactor (MR-05/06 — both must be **zero-diff**) | same |

Rule of thumb (extends GHOOK §G): every renderer change needs the snapshot verdict first;
pixels are the last, most expensive lock.

---

## K. Failure modes and how the harness catches (or explicitly doesn't catch) them

| # | Failure mode | Caught by |
|---|---|---|
| 1 | Line-height / font-metric drift (font regen, LVGL glyph-descriptor change, `metricsFromFont` edit) | fixture parity step (§I.2) — fails at the *source*, before stale snapshots mislead |
| 2 | Baseline drift (ascent/descent redistribution with same height) | per-node `a`/`d` fields |
| 3 | Delimiter-sizing drift (growth formula, pad, assembly floor) | `Paren` `pw` + node `a/d/ia/idc` on cases 41-46, 57 |
| 4 | Missing-glyph drift (glyph silently vanishes) | **not here** — layout doesn't resolve glyphs; covered by MR-01 `assert_math_glyph_fallback missing=0` + goldens (documented no-silent-gap) |
| 5 | Style-demotion drift (script-level lattice, readable-fraction policy) | per-node `sl` + sizes on ladders 13-19, 23-27, 48-49 |
| 6 | Root-size drift (radical gaps/kerns/ascender) | `Root` extras on 31-36 |
| 7 | Fraction-bar drift (rule thickness, gap-derived shifts) | `Fraction` extras on 10-20 |
| 8 | Unexpected cache/state mutation (layout not idempotent; future memo bug) | double-pass identity (§H) — hard error |
| 9 | Inter-atom spacing drift (table/mu conversion) | `Row.gapSum` on 7-9 and every multi-child row |
| 10 | int16 overflow on wide rows | case 54 + saturation behavior review at MR-05 (which adds saturating adds) |

---

## L. Acceptance tests (MR-02 PR gate)

1. `scripts/build-mathlayout-host-tests.sh` exits 0; footer reports
   `cases=98 built=94 skipped_future=4 mismatched=0`.
2. **Negative control (T6)**: flip one digit in a committed baseline line locally → run → exit 1
   with a correct `path/field/baseline/candidate` diagnostic; revert. (Paste output in the PR.)
3. **Double-pass identity**: enabled on every run by construction; grep the log for
   `pass2=identical` summary line.
4. **Fixture parity**: §I.2 step green; corrupt one field in `/tmp/metrics.txt` locally →
   checker exits 1 with the field named. (Paste output.)
5. Runtime: harness step wall time <30 s in CI logs.
6. Baseline file committed exactly once, generated by `--update`, hand-verified spot checks:
   `visual/power_2_squared` root box plausibly matches the golden screenshot's glyph sizes
   (human sanity, recorded in PR).
7. Emulator/firmware binaries **byte-identical** — MR-02 touches no `src/**` file (the diff
   proves it; that *is* the no-golden-impact proof).

---

## M. Rollback

Delete the five new paths + the two CI steps; `git revert` of the single PR does exactly that.
No `src/**` edits exist to revert; no golden, mask, script, or generated file changes.
MR-05's cache gate loses its vehicle if MR-02 is reverted — revert MR-05 planning accordingly
(dependency graph: MR-01 → MR-02 → … → MR-05).

---

## N. PR checklist (copy into the PR description)

- [ ] Diff touches only: `tests/host/mathlayout_geometry_test.cpp`,
      `tests/host/mathlayout_fixed_metrics.h`, `tests/host/golden_geometry/mathv2_geometry.jsonl`,
      `scripts/build-mathlayout-host-tests.sh`, `scripts/check-math-metrics-fixture.py`,
      (optionally) `.github/workflows/emulator-build.yml`.
- [ ] **Zero `src/**` changes** (`git diff --stat` pasted).
- [ ] Fixture header carries the capture git-sha + hash; §D.4 procedure output pasted.
- [ ] Harness exits 0 locally; footer pasted; `--list` output pasted (98 ids).
- [ ] Negative control (§L.2) and fixture-corruption control (§L.4) outputs pasted.
- [ ] Double-pass identity confirmed.
- [ ] `mathv2_metrics_dump.numos` exists (MR-01 dependency) and the parity step passes locally.
- [ ] Baseline JSONL reviewed: header line correct (`assembly_renderable:false`), 94 case
      lines, 4 future SKIPs in the run log.
- [ ] Full golden sweep still identical (nothing should have changed — prove it anyway,
      MR-01 plan §J.4).
- [ ] CI green including the new step(s), artifacts present.

*End of MR-02 execution plan.*
