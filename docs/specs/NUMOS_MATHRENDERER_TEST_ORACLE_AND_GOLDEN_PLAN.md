# NumOS — MathRenderer Test Oracle and Golden Plan

> **Normative test plan** for MathRenderer v2. Defines the oracle stack (geometry, serialization,
> semantic, visual), golden strategy, emulator script surface, performance guardrails, and failure
> injection. Verified against `main @ 2d6b796` (2026-07-02).
> Conventions inherited: T1–T7 taxonomy and mask policy from
> `NUMOS_GRAPHER_TEST_ORACLE_AND_VISUAL_COVERAGE_PLAN.md`; assert-hook pattern from
> `NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md` (NATIVE_SIM `debug*` accessors, append-only `.numos`
> grammar, exit codes 0/2/3/4); golden mechanics from `tests/emulator/golden/README.md`
> (P6 PPM 320×240 = 230,415 B, mismatch fails / missing warns, human-only promotion).
> Markers: **[V]** exists, **[P]** proposed.

---

## 1. Oracle stack (layered, cheapest-first)

| Layer | Oracle | Font-dep? | Vehicle | Exists |
|---|---|---|---|---|
| O-A **Structure** | NumIR round-trip `parse(emit(t))≡t`; capability totality; tree well-formedness (parent ptrs, slot arity) | no | host g++ + emulator | [P] |
| O-B **Geometry** | `LayoutResult` snapshots per case: `{w, ascent, descent, inkAsc, inkDesc}` + per-child x-offsets, compared to committed JSON | metrics-dep, pixel-indep | host g++ (layout core is LVGL-free [V] `MathTypography.h:23` — but `metricsFromFont` needs LVGL fonts ⇒ host harness links the three `stix_math_*.c` + a font-probe shim, same trick as emulator build) | [P] |
| O-C **Semantic** | `evaluate()`/refusal verdicts via `.numos` `assert_result` [V] + new `assert_math_*` [P] | no | emulator (CI-gated) | partial [V] |
| O-D **Behavioral** | cursor navigation traces, depth-refusal, glyph-plan kinds | no | emulator asserts | [P] |
| O-E **Visual** | byte-exact PPM goldens + warning-only candidates | yes | emulator (Linux only) | [V] |
| O-F **Perf/mem** | frame budget, zero-heap-delta, stack HWM, layout-call counter | no | device stress diag [V] + emulator counters [P] |

Design rule (from grapher plan experience): **every new feature must land with O-A…O-D coverage
before any golden** — pixels are the last, most expensive lock, and REN-1 makes them churn-prone.

## 2. New assert hooks (GHOOK pattern; NATIVE_SIM only, zero firmware footprint)

Accessors [P] (MR-01):

```cpp
#ifdef NATIVE_SIM   // on MathCanvas / CalculationApp
const LayoutResult& debugRootLayout() const;
int  debugNodeCount() const;         int  debugMaxDepth() const;
int  debugLayoutCallsLastFrame() const;   // memoization oracle (Y-7)
bool debugLastEvalRefused() const;   const char* debugLastRefusalName() const;
bool debugSerializeCurrent(std::string& numirOut) const;
int  debugGlyphPlanCounts(GlyphPlanKind k) const;  // Exact/Variant/Assembly/Vector/Tofu tally
#endif
```

`.numos` commands [P] (append-only, parse-validated at load, new `ScriptCmd` fields not packed
ints — GHOOK §16):

```
type_numir "frac(1;2)+frac(1;3)"          # build editor tree from NumIR
insert_template TEMPLATE_NAME             # palette path
assert_math_serialized "frac(1;2)+frac(1;3)"
assert_math_layout w_min=.. w_max=.. h_min=.. h_max=..   # tolerance-banded, NOT exact px
assert_math_layout_exact w=.. ascent=.. descent=..       # exact, for frozen contract cases
assert_math_depth n=..        assert_math_nodes n=..
assert_math_refusal name=DisplayOnly|UnboundedDomain|DepthExceeded|None
assert_math_glyphplan tofu=0                             # "no tofu on this screen"
assert_math_layout_calls max=0                           # steady-state frame, cache proof
assert_cursor_role role=fraction_numerator               # uses classifyCursorTargetRow [V] MathAST.h:1437-1463
```

Verdict lines keep the frozen `[ASSERT] <script>:<line>: PASS/FAIL - msg` grammar [V].

## 3. Host geometry harness (MR-02) [P]

`tests/host/mathlayout_geometry_test.cpp` + `scripts/build-mathlayout-host-tests.sh`, modeled on
the CAS harness (`NUMOS_CAS_HOST_HARNESS_SPEC.md`: plain g++, forked-child timeouts, XFAIL list,
`[MATH-HOST]` footer). Contents:

- Builds every `MathStressExpressions` case [V 17 cases] + every `MathRenderVisualCases` case
  [V 21 cases] + new MRV2 cases; runs `calculateLayout` with `defaultFontMetrics()` and the real
  font metrics; dumps geometry JSON; compares to committed snapshots
  (`tests/host/golden_geometry/*.json`).
- Unit tests proper: spacing-table lookups (`interAtomSpacingPx` cases), `fractionBarGaps`,
  `superscriptShiftMetrics`, delimiter FSM (`validateAndSplit` piece/extender constraints [V
  behavior `MathGlyphAssembly.cpp:30-86`]), NumIR emit/parse property tests, capability totality
  (static_assert-style exhaustive switch), budget refusal at depth 25 / 4097 nodes.
- This harness is what lets MR-05's freeze-refactor prove **bit-identical** geometry: snapshot
  diff must be empty before/after.

## 4. Test categories (complete matrix)

Per category: primary oracle(s) → vehicle. All `.numos` scripts run `--deterministic` [V rule].

| # | Category | Cases (representative) | Oracles |
|---|---|---|---|
| 4.1 | Baseline/simple | `1+2`, `2x`, relation rows | O-C [V exists: `calc_1_plus_2`], O-B |
| 4.2 | Fractions | simple, 4-deep nested [V case], mixed with powers, readable-policy sizes | O-B snapshots; golden `calc_fraction_sum` [V] |
| 4.3 | Powers/subscripts | nested exponents [V case], italic-correction bases, combined `NodeScripts` (P2) | O-B; O-A |
| 4.4 | Roots | √, ⁿ√ with degree kerns, triple-nested [V case] | O-B |
| 4.5 | NumIR fuzz/round-trip | corpus of all node kinds; random slot fills; malformed inputs | O-A; libFuzzer-style loop in host harness (S-3) |
| 4.6 | Nested delimiters | 5-deep parens, mixed kinds, bar-in-bracket | O-B + `assert_math_glyphplan` (vector vs assembly rungs) |
| 4.7 | Functions/trig/log | all FuncKinds, logb, abs (P1) | O-C (exact special angles [V asserted in 8C suite]) |
| 4.8 | Large operators | Σ/Π/∫ display vs inline limits; `@display` flag (P2); differential tail | O-B; O-D |
| 4.9 | Matrices/determinants (P2) | 1×1…6×6, bar fence, det-func form, Greek entries | O-A/O-B; O-C det values vs `MatrixEngine` [V engine] |
| 4.10 | Continued fractions (P2) | 3-level tower, ⋱ termination, depth-budget edge (level 8) | O-B; O-D refusal at 9+ |
| 4.11 | Nested radicals (P2) | √(2+√(2+⋯)) chain lengths 2–5 | O-B |
| 4.12 | Greek/special symbols | full α–ω render row; Φ; ∞ in bounds | O-B + `assert_math_glyphplan tofu=0` |
| 4.13 | Long expressions | wide_scroll [V case]; 300+ px rows; scroll-follow cursor | O-D (scroll state accessor), golden of scrolled view |
| 4.14 | Deep-nesting stress | generated depth 15/23/24/25 towers; node bomb 4096/4097 | O-D refusals; O-F stack HWM on device |
| 4.15 | Fallback glyph paths | font-probe forced off (`g_delimiterAssemblyRenderable=false` path [V]); synthetic missing-cp | O-D glyph-plan tallies; failure injection §7 |
| 4.16 | Unsupported notation | ellipsis eval refusal; ∞ misplacement; CAS J.2 refusal | O-C `assert_math_refusal` |
| 4.17 | Edit navigation | LEFT/RIGHT/UP/DOWN traversals through fraction/matrix/scripts; capture semantics of `insertFactorial`; backspace flattening | O-D `assert_cursor_role` sequences |
| 4.18 | Cross-app consistency | same NumIR typed in Calc vs Grapher slot vs Calculus renders identical geometry | O-B via shared accessor |
| 4.19 | **Exemplar A** | whole formula + 6 subconstructs; truncated eval N=0,1,2 (exact rationals!) | O-A/O-B/O-C + gated golden (P1 exit) |
| 4.20 | **Exemplar B** | 8 decomposed subcases (each its own script+candidate) then composed whole | O-A/O-B/O-D; candidates → golden at P3 gate |
| 4.21 | Memoization | steady-frame `assert_math_layout_calls max=0`; cache-off byte-equality run | O-D; O-E self-diff (T5 pattern) |
| 4.22 | History/serialization | store/reload rich expression via NumIR; history nav | O-A/O-C |

Exemplar A truncated-eval oracle detail: with upper bound N, the sum is a rational; at N=0 the
Ramanujan term gives 1103·2√2/9801 — the assert uses `assert_result_contains` on the exact
symbolic form, making this a **pure semantic** test, no pixels.

## 5. Golden strategy

- **Warning-only candidates first** [P] (T3 pattern): new stems enter
  `generate-emulator-candidates.py` immediately; goldens blessed by a human only when the
  notation's geometry has stabilized (post-MR-05 for existing notation, per-feature after).
- Proposed stems (phase-tagged):
  - P0/P1: `mathv2_identifiers`, `mathv2_factorial`, `mathv2_sum_finite`, `mathv2_sum_inf_bound`,
    `mathv2_abs`, `mathv2_exemplarA` (**gated at P1 exit** MR-10), `mathv2_refusal_chip`.
  - P2: `mathv2_matrix_2x2`, `mathv2_det_bars`, `mathv2_binom`, `mathv2_limit`,
    `mathv2_cfrac_tower`, `mathv2_radical_chain`, `mathv2_ellipsis_row`,
    `mathv2_scripts_combined`, `mathv2_displaystyle_sum_in_frac`, `mathv2_assembly_delims`
    (**re-bless wave with MR-11**).
  - P3: `mathv2_exemplarB_upper`, `mathv2_exemplarB_lower` (two viewport pages),
    `mathv2_exemplarB_scrolled` (**gated at P3 exit** MR-18).
- **Must stay byte-identical during freeze-refactors** (MR-05/06): all 20 existing goldens,
  especially `calc_1_plus_2`, `calc_fraction_sum`, `math_showcase_smoke`, six grapher smokes.
  Any diff in the candidate run = ticket failure, not a re-bless.
- Mask policy unchanged [V]: StatusBar clock rect only; **never mask math canvas pixels**
  (GTEST:89 rule).
- Showcase extension: emulator `MathShowcase` app (id 100 [V], `NativeHal.cpp:1090-1222`) gains
  pages for MRV2 cases, selected by id — keeps captures cursor-off deterministic [V pattern].

## 6. Performance guardrails

| Guardrail | Budget | Vehicle |
|---|---|---|
| Layout+draw frame | ≤16 ms (`kFrameBudgetUs=16000` [V]) | device stress diag [V `MathRenderStressDiagnostics.cpp:75-128`], extended to MRV2 cases incl. exemplars |
| Zero heap delta per render | 0 B | same [V], plus emulator allocator hook [P] |
| Steady-state layout calls | 0 | `assert_math_layout_calls max=0` [P] |
| Layout stack depth | HWM ≥8 KB loopTask floor (REL M-5) | `[MEM]` probe after exemplar-B layout on device (MR-18 HIL step) |
| Editor latency | keypress→invalidate ≤2 ms host-measured | host harness micro-bench (informational, not gated) |
| Node budget | 4096 refusal fires | O-D |
| CI wall time | emulator suite stays ≤30 s/invocation [V rule] | script granularity: one scenario per script |

## 7. Failure injection

- **Glyph-missing injection** [P]: NATIVE_SIM hook `debugForceGlyphMissing(cp)` forces the ladder
  to skip rungs; asserts tofu tally >0 and `[LVGL] glyph-missing` log line; proves Y-8 (no
  invisible glyphs — the C5.7 regression trap).
- **OOM injection** [P]: NATIVE_SIM allocator failure-after-N hook around `MathNode::operator new`;
  asserts edit-op revert (failure model, arch spec §5) and no partial tree (tree well-formedness
  oracle after every injected failure).
- **Depth/node bombs** [V-able now]: generated trees at budget±1 (4.14).
- **Determinism self-diff** (T5): run `mathv2_exemplarB_scrolled` twice, SHA-256 equal —
  extends the existing CI double-run [V `emulator-build.yml:228-237`].
- **Negative controls** (T6): a deliberately wrong geometry snapshot must fail O-B; a corrupted
  NumIR string must exit 2; `assert_math_refusal name=None` on an ellipsis tree must FAIL.

## 8. CI wiring plan (append-only, mirrors existing gates)

1. New step after the KeyCode guard: **build + run host math harness** (MR-02) — hard gate.
2. Existing emulator build → new `.numos` suites appended to the Phase 4B/8C block: `mathv2_*`
   semantic/behavioral scripts — hard gate.
3. Candidate generation: stems added; golden compare behaves per T2/T3 (missing warn, present
   gate) [V mechanism, zero workflow logic changes].
4. Cache-off equivalence job (Y-7): emulator run with `NUMOS_MATH_LAYOUT_CACHE_OFF=1` build flag,
   byte-compare `mathv2_exemplarA` capture vs cache-on — P1 gate, weekly-tier if CI time tight.
5. Firmware compile of `esp32s3_n16r8_validate` [P closes REN-5 partially]: compile-only gate so
   `NUMOS_MATH_VISUAL_VERIFY` paths can't bit-rot.

## 9. Determinism statement

All oracles above are deterministic by construction: geometry is integer math over committed font
metrics (Y-6); NumIR is byte-stable (S-2); emulator runs use `--deterministic` synthetic ticks [V];
goldens Linux-only [V rule]; font rasterization in-tree [V] (GTEST:103 — variance is a bug, never
maskable). The only tolerance-banded assert is `assert_math_layout` (min/max), reserved for cases
meant to survive planned metric tuning; contract cases use `_exact`.

---

*End of Document 5.*
