# NumOS — MathRenderer V2 Implementation Tickets (MR-00 … MR-20)

> **Execution roadmap** for MathRenderer v2. Ticket format matches GR-xx/RT-xx/MT-xx conventions
> (`NUMOS_GRAPHER_IMPLEMENTATION_TICKETS.md` template): Motivation · Source evidence · Files ·
> Design · Invariants · Acceptance · Visual · Firmware · Budget · Rollback · Risk · Owner.
> Owner legend (fixed repo-wide meaning): **Sonnet** = mechanical/CI-verifiable, **Opus** =
> multi-file judgment, **Fable** = cross-cutting/fragile/adversarial (renderer core is a
> Fable/Opus lane, GT:395), **Human** = HIL / golden blessing / decision ratification.
> Verified against `main @ 2d6b796` (2026-07-02).
>
> **Global rules (all tickets)**: never edit goldens/masks by tool; `.numos` grammar append-only;
> `src/math/**` stays LVGL/Arduino-free; memory per `NUMOS_MEMORY_ALLOCATION_POLICY.md`
> (nodes→PSRAM, no bulk internal fallback, LVGL pool budgets); log tags from the closed DIAG set;
> any geometry-visible change ⇒ regenerate candidates + human review (REN-1).

---

## Phase map

| Phase | Tickets | Exit gate | Goldens |
|---|---|---|---|
| **P0 Observability & test enablement** | MR-00…MR-03 | host harness + math asserts green in CI; NumIR round-trips all existing notation | all 20 existing goldens **byte-identical** |
| **P1 Exemplar A** | MR-04…MR-10 | `mathv2_exemplarA` golden blessed; truncated-eval asserts green; refusal UX shipped | existing byte-identical **except** none; new `mathv2_*` P1 stems blessed |
| **P2 Structure wave** | MR-11…MR-15 | every exemplar-B subconstruct renders in isolation; matrix/binom/limit/ellipsis/scripts editable+serialized | MR-11 is the **planned re-bless wave** (delimiter glyphs); other tickets byte-identical for old stems |
| **P3 Composition & final boss** | MR-16…MR-18 | exemplar B whole-formula golden pair blessed; on-device HIL sign-off | new P3 stems blessed |
| **F Optional/Future** | MR-19, MR-20 | — | — |

Dependency graph (→ = hard prerequisite):

```
MR-00 → MR-01 → MR-02 → MR-03 → {MR-05, MR-07}
MR-04 → MR-11                    MR-05 → MR-06 → MR-11
MR-07 → {MR-08, MR-09, MR-13}    MR-08,MR-09 → MR-10 (needs MR-03..MR-09)
MR-12, MR-13, MR-14 → MR-15 → MR-16 → MR-17 → MR-18
MR-19, MR-20 independent after P2
```

---

### MR-00 — Spec ratification + decision-record sign-off  *(docs-only)*
- **Motivation**: freeze AD-1 (single tree), MRD-1 (NumIR over LaTeX), MRD-2…5, budgets (depth 24
  / nodes 4096 / editor 16), phase gates, before code moves.
- **Files**: the six MRV2 docs (this set); no code.
- **Acceptance**: user ratifies or amends decision records; amendments propagate to all six docs.
- **Rollback**: n/a. **Risk**: Low. **Owner**: Human (decisions) + Fable (doc updates).

### MR-01 — Renderer observability + editor caps  *(safe implementation)*
- **Motivation**: close C5.1/C5.2's *detection* gap before touching layout; enable every later
  oracle. Depth/node counters, NATIVE_SIM accessors, `.numos` math asserts, CalculationApp editor
  cap (Grapher already gated `MAX_NEST=8`, `GrapherApp.cpp:2547-2593`; Calc ungated —
  `CalculationApp.cpp:225-519`), digit-length cap (C5.3).
- **Source evidence**: REL C5.1/C5.2/C5.3; GHOOK accessor pattern (`CalculationApp.h:95-107`).
- **Files**: `src/ui/MathRenderer.h/.cpp` (accessors, counters), `src/math/CursorController.cpp`
  (insert refusal ≥16 depth / ≥512 nodes / ≥32 digits), `src/apps/CalculationApp.cpp` (toast),
  `src/hal/NativeHal.cpp` (append `.numos` commands: `type_numir` stub-off until MR-03,
  `assert_math_depth/nodes/layout/layout_calls/refusal/glyphplan/cursor_role`),
  `tests/emulator/scripts/mathv2_caps_*.numos`.
- **Invariants**: H-5 (editor cap), append-only grammar, accessors `#ifdef NATIVE_SIM` only.
- **Acceptance**: caps refuse with toast + `[INPUT]` log; asserts run in CI; firmware.bin
  byte-comparable size delta ≤ +2 KB (cap logic only).
- **Visual**: all existing goldens byte-identical (no geometry change).
- **Firmware**: compile `esp32s3_n16r8`; manual: hold DIV 20× → refusal toast, no crash.
- **Budget**: +0 heap; +~1.5 KB flash. **Rollback**: revert single PR; asserts unused remain inert.
- **Risk**: Low-Medium (CursorController edits). **Owner**: Opus.

### MR-02 — Host math-layout harness + geometry snapshots  *(test infra)*
- **Motivation**: pixel-independent geometry oracle (O-B) so refactors can prove bit-identity;
  revive the orphaned regression intent of `MathEnginePhaseRegression.cpp` (2,124 lines,
  referenced by nothing — GT:328) in the CAS-harness style.
- **Source evidence**: `NUMOS_CAS_HOST_HARNESS_SPEC.md` (g++ harness, XFAIL, forked timeouts);
  `MathStressExpressions.cpp:220-238` (17 cases); `MathRenderVisualCases.cpp:157-179` (21 cases).
- **Files**: `tests/host/mathlayout_geometry_test.cpp` [new],
  `scripts/build-mathlayout-host-tests.sh` [new], `tests/host/golden_geometry/*.json` [new],
  `.github/workflows/emulator-build.yml` (append one step after the KeyCode guard).
- **Design**: link `MathAST.cpp`, `MathTypography`, `MathGlyphAssembly`, the three `stix_math_*.c`
  + a minimal LVGL font-shim (emulator already proves these compile on host,
  `platformio.ini:195-267`); dump per-case `LayoutResult` JSON; compare committed snapshots.
- **Acceptance**: harness green; deliberately corrupted snapshot fails (negative control T6);
  spacing/fraction/superscript unit tests pass; runs <10 s in CI.
- **Visual**: n/a. **Firmware**: n/a.
- **Rollback**: remove CI step. **Risk**: Low. **Owner**: Sonnet (harness) + Opus (font shim).

### MR-03 — NumIR serializer + parser  *(parser/semantic, additive)*
- **Motivation**: canonical serialization (C-3); unlock `type_numir`, round-trip oracle, history
  persistence path, Grapher adapter.
- **Source evidence**: no serializer exists (`MathAST.h:1421` debug dump only); Grapher's private
  flattener (`GrapherApp.cpp:1103-1264`); grammar in input spec §3.
- **Files**: `src/math/numir/NumIR.h/.cpp` [new], `src/hal/NativeHal.cpp` (`type_numir`,
  `assert_math_serialized` live), host harness round-trip + fuzz tests,
  `tests/emulator/scripts/mathv2_numir_*.numos`.
- **Invariants**: S-1…S-5; T-1 capability totality (emit switch exhaustive).
- **Acceptance**: round-trip identity on all 38 existing cases + fuzz corpus (10⁵ random trees
  host-side); malformed inputs exit typed errors; zero behavior change elsewhere.
- **Visual**: goldens byte-identical. **Firmware**: compile-only (NumIR is target-agnostic).
- **Budget**: emit strings PSRAM; parser stack-bounded (explicit depth cap 24 shared with Y-3).
- **Rollback**: unit is leaf — delete files + CI lines. **Risk**: Medium (grammar freeze quality).
- **Owner**: Fable (grammar+parser) — correctness of the freeze is cross-cutting.

### MR-04 — Glyph inventory + font regeneration pipeline  *(font/glyph)*
- **Motivation**: add extensible-delimiter assembly glyphs U+239B…U+23B3 (+ brace/bar parts, ∫/√
  size variants) so tall delimiters stop depending on vector strokes; audit every codepoint the
  notation spec needs; automate the StixGlyphGallery drift check.
- **Source evidence**: subset hole documented (`MathRenderer.cpp:57-61`, `MathGlyphAssembly.h:29-34`);
  generator (`scripts/generate_stix_math_font.sh:32-137`); REN-2; flash headroom 1.35 MB (MA:173-177).
- **Files**: `scripts/generate_stix_math_font.sh`, `scripts/extract_stix_math.py`,
  regenerated `src/fonts/stix_math_{8,12,18}.c`, `src/math/font/stix_math_variants.h`
  (assembly parts become available), `src/ui/StixGlyphGallery.cpp` (probe list extension).
- **Design**: extend ranges by `0x239B-0x23B3, 0x2320-0x2321`; measure flash delta (budget ≤60 KB
  across the trio; abort ticket if generator output exceeds 100 KB).
- **Invariants**: font regen is generator-only (hand edits overwritten, GT:69); **renderer must
  not switch to the new glyphs in this ticket** — availability probe still gates (assembly stays
  cold until MR-11).
- **Acceptance**: firmware+emulator build; glyph-probe diagnostic lists new cps present; goldens
  **byte-identical** (fonts add glyphs; existing glyph ids/bitmaps for old cps must be verified
  identical — if the generator reorders cmap, capture proves rendering unchanged, which is the
  real contract).
- **Visual**: candidates diff empty. **Firmware**: flash % recorded in PR (MA-style numbers).
- **Rollback**: restore old `.c` files (generated artifacts, git-clean).
- **Risk**: Medium (generator churn). **Owner**: Opus + Human (flash sign-off).

### MR-05 — Layout-core hardening: memoization, budgets, policy extraction  *(risky renderer-core)*
- **Motivation**: fix per-frame full relayout (`MathRenderer.cpp:737`); cap layout recursion
  (C5.1, RT-10 alignment: cap **24**, ⚠ box, `[LVGL] layout-depth` log); raise draw cap 12→24 in
  lockstep; extract geometry policy out of `MathAST.h` (B-1) so the AST header stops being
  golden-fragile; add saturating arithmetic.
- **Source evidence**: arch spec §2.6; `MathAST.h:314-588` (policy block); `MathRenderer.h:239`.
- **Files**: `src/math/layout/LayoutPolicy.h/.cpp` [new], `src/math/MathAST.h/.cpp` (memo fields,
  budget plumbing; policy functions become thin forwards then move), `src/ui/MathRenderer.cpp`
  (onDraw memo path, depth 24, ⚠ box), host harness (cache-off A/B, snapshot diff must be empty).
- **Invariants**: Y-3/Y-4/Y-5/Y-7; **bit-identical geometry** (freeze-refactor — snapshot diff
  empty, candidates diff empty); M-5 stack floor measured.
- **Acceptance**: `assert_math_layout_calls max=0` on steady frame; depth-25 tree renders ⚠ not
  crash; cache-off run byte-identical captures; host snapshots unchanged.
- **Visual**: all goldens byte-identical — **hard gate; any diff = bug in this ticket**.
- **Firmware**: stress diag suite [V] re-run on device; `[MEM]` stack HWM line in PR.
- **Budget**: +4 B/node (memo key+flag); frame-time improvement recorded.
- **Rollback**: memoization behind `NUMOS_MATH_LAYOUT_CACHE_OFF` flag (also the test hook);
  revert = flag default flip. **Risk**: **High** (touches every layout path).
- **Owner**: **Fable**; Human re-verifies goldens in PR.

### MR-06 — GlyphProvider unification + tofu  *(risky renderer-core, geometry-frozen)*
- **Motivation**: one ladder (exact→variant→assembly→vector→tofu) replacing three fallback paths
  that can draw nothing (C5.7) or silently advance (`MathRenderer.cpp:2480-2483`); per-piece
  availability probing generalized from the single U+239C probe (`MathRenderer.cpp:431-435`).
- **Files**: `src/math/font/GlyphProvider.h/.cpp` [new, wraps `MathGlyphAssembly`],
  `src/ui/MathRenderer.cpp` (draw paths call `planGlyph`), emulator failure-injection hook.
- **Invariants**: Y-8 (no invisible glyphs); geometry frozen for all cps present in fonts
  (tofu only ever appears where *nothing* appeared before — by definition non-golden-covered).
- **Acceptance**: forced-missing injection shows tofu + `[LVGL] glyph-missing`; glyph-plan tally
  asserts; goldens byte-identical.
- **Rollback**: provider is a facade — revert call sites. **Risk**: Medium-High. **Owner**: Fable.

### MR-07 — `NodeIdentifier` (Greek, multi-char, ∞)  *(AST + renderer + input, additive)*
- **Motivation**: exemplars need Φ α β γ δ s n k ∞; `NodeVariable(char)` can't hold them
  (`MathAST.h:1096`).
- **Files**: `src/math/MathAST.h/.cpp` (node, factory, capability row), `src/math/numir/`
  (id() emit/parse), `src/ui/MathRenderer.cpp` (SymbolBox draw — reuses text path),
  `src/math/CursorController.cpp` (`insertIdentifier`), MathEvaluator (UnboundVariable refusal),
  host+emulator tests, `mathv2_identifiers` candidate.
- **Invariants**: T-1 (new capability row), S-1; `NodeVariable` untouched (alias period).
- **Acceptance**: Greek row renders tofu-free (glyphs already in subset [V]); NumIR round-trip;
  unbound eval refuses typed; existing goldens byte-identical.
- **Risk**: Low-Medium. **Owner**: Opus.

### MR-08 — `NodeFactorial` + evaluation  *(AST + eval)*
- **Motivation**: exemplar A; zero factorial support today (grep-verified).
- **Files**: MathAST (node: postfix box, capture-class CLOSE-right per layout spec §4),
  CursorController (`insertFactorial` capture), MathEvaluator (exact ≤20 via int64, 21…170
  approximate-flagged double, else overflow error; non-integer → domain), NumIR `fact()`,
  ASTFlattener/Giac serializer mapping, tests + `mathv2_factorial` candidate.
- **Acceptance**: `5!`→120 exact; `0.5!`→domain; `200!`→overflow; `(4k)!` renders with correct
  spacing (O-B snapshot); goldens byte-identical.
- **Risk**: Low-Medium. **Owner**: Opus.

### MR-09 — Finite Σ/Π numeric evaluation + admissibility pre-flight  *(eval/semantic)*
- **Motivation**: sums render but error opaquely (`MathEvaluator.cpp:533`); exemplar A truncated
  eval; typed-refusal UX (T-2).
- **Files**: `src/math/MathEvaluator.h/.cpp` (`evalAdmissible`, Summation/BigOp-Product eval:
  binder parse from lower row `k=0` shape [V shape `MathAST.h:1207-1210`], iteration ≤10⁴,
  deadline 5 s per REL H-4, index shadowing), `src/apps/CalculationApp.cpp` (refusal chip UI),
  `.numos` `assert_math_refusal`, corpus tests.
- **Invariants**: T-2 (refusal never "Math ERROR"); H-4 deadline; existing eval semantics
  untouched (regression: full 8C calc suite byte-stable).
- **Acceptance**: `sum(k=1;10;k)`→55 exact; ∞ bound → `UnboundedDomain` chip; ⋂/⋃ → `DisplayOnly`;
  deadline fires on 10⁴-iteration pathological body without WDT reset (firmware HIL).
- **Risk**: Medium (evaluator core). **Owner**: Fable.

### MR-10 — **Exemplar A acceptance gate**  *(validation)*
- **Motivation**: P1 exit criterion made executable.
- **Files**: showcase case + template (`MathRenderVisualCases.cpp` additive,
  `NativeHal.cpp` showcase id list append), `mathv2_exemplarA.numos` (build via `type_numir`,
  assert serialization, layout bands, glyphplan tofu=0, truncated-eval N=0/1 exact asserts),
  candidate stem; **golden blessed here** (Human).
- **Acceptance**: everything in test plan §4.19 green in CI; on-device photo review (HIL) of the
  showcase page; `[MEM]`/frame-budget lines recorded.
- **What remains unsupported after P1** (explicit): ∞-bound evaluation (refused, by design);
  matrix/binom/limit/ellipsis (P2); vertical scroll (P3-adjacent MR-17).
- **Owner**: Sonnet (scripts) + Human (bless/HIL).

### MR-11 — Assembly-glyph delimiters go live  *(font/renderer, **planned golden re-bless wave**)*
- **Motivation**: replace vector strokes with real STIX assemblies for tall `()[]{}|` and radical
  variants (MR-04 made glyphs available; probe now passes).
- **Files**: `GlyphProvider.cpp` (rung order flips per availability), possibly
  `stix_math_variants.h` regen; candidates for every delimiter-bearing stem.
- **Invariants**: vector tier remains as fallback (layout spec §12); geometry changes are
  **expected** — the one sanctioned re-bless wave of MRV2: PR carries before/after candidate
  gallery; Human re-blesses affected goldens (expect: `calc_fraction_sum` unaffected,
  delimiter-bearing stems affected).
- **Acceptance**: side-by-side gallery approved; new goldens blessed; `assert_math_glyphplan`
  shows Assembly rung on tall delimiters.
- **Risk**: High (visual churn). **Owner**: Fable + Human.

### MR-12 — `NodeScripts`, `NodeLimit`, `@display` flag  *(renderer-core)*
- **Motivation**: combined sub+sup (layout spec §5), lim with under-script (§8.4), displaystyle
  pinning (§2) — exemplar B numerator Σ.
- **Files**: MathAST (+3 node kinds/flag), MathRenderer (draw), CursorController (slot graphs),
  NumIR, host snapshots, `mathv2_limit`/`mathv2_scripts_combined`/
  `mathv2_displaystyle_sum_in_frac` candidates.
- **Invariants**: SubSuperscriptGapMin/BottomMaxWithSubscript constants only (Y-1); old
  Power/Subscript untouched (goldens byte-identical).
- **Risk**: Medium-High. **Owner**: Fable.

### MR-13 — `NodeMatrix` + determinant  *(renderer-core + semantic, big)*
- **Motivation**: exemplar B; matrix layout spec §9; det duality MRD-5.
- **Files**: MathAST (NodeMatrix ≤6×6), MathRenderer (grid draw, fence via DelimitedBox path),
  CursorController (cell navigation, column-aware UP/DOWN via `enterRowAtX`), NumIR `matrix()/`
  `func(det;…)`, MathEvaluator bridge to `MatrixEngine` (≤5×5), ASTFlattener refusal (CAS matrix
  F), template dims dialog (minimal, palette in MR-15), tests: 4.9 suite +
  `mathv2_matrix_2x2`/`mathv2_det_bars` candidates.
- **Invariants**: Y-4 node budget covers 6×6; MatricesApp untouched.
- **Acceptance**: 2×2 Greek matrix renders (exemplar-B fragment); det of numeric 3×3 matches
  MatrixEngine golden values; cursor tour script covers all cells.
- **Risk**: High (largest new surface). **Owner**: Fable.

### MR-14 — `NodeBinom` + `NodeEllipsis` + floor/ceil delims  *(renderer, medium)*
- **Files**: MathAST (+2 nodes, +2 DelimKinds), MathRenderer, NumIR, CursorController, snapshots,
  `mathv2_binom`/`mathv2_ellipsis_row`/`mathv2_cfrac_tower`/`mathv2_radical_chain` candidates.
- **Invariants**: ellipsis = archetypal display-only (T-2 test: eval refusal); StackBox constants
  (Y-1).
- **Risk**: Medium. **Owner**: Opus.

### MR-15 — Template palette + Greek input page  *(input)*
- **Motivation**: rich notation must be typable, not just constructible by tests (input spec §1.2).
- **Files**: `src/apps/CalculationApp.cpp` (+palette modal), `src/input/KeyboardManager` (Greek
  page state), `LvglKeypad` untouched; widget budget per MP §3 with `canAllocStep()`-style gate
  (`EquationsApp.cpp:486-503` precedent); `insert_template` `.numos` command; parity scripts.
- **Invariants**: G-2/G-4 (pool gating); palette teardown deferred (L-1).
- **Acceptance**: every P1/P2 node insertable via palette; LVGL pool HWM recorded ≤32 KB app
  budget; input-parity script green.
- **Risk**: Medium (LVGL lifecycle). **Owner**: Opus.

### MR-16 — Semantic-boundary v2: capability enforcement at CAS/Giac bridges  *(CAS interop)*
- **Motivation**: T-3/J.2 refusals at `ASTFlattener`/Giac serializer; cap `SymExprToAST` output
  (C5.8, coordinated with RT-11); align Giac serializer notation (`sum/integrate/surd/factorial/`
  `comb/matrix`) with CGIAC §C.2; unify Grapher on NumIR→legacy adapter.
- **Files**: `src/math/cas/ASTFlattener.*`, `SymExprToAST.*` (node cap 4096), `src/apps/GrapherApp.cpp`
  (serializer swap), CAS corpus rows for new constructs, host CAS harness suites.
- **Invariants**: NC/DV registers updated; Grapher goldens byte-identical (adapter must emit the
  exact legacy strings — differential test old-vs-new serializer over the template corpus).
- **Risk**: Medium-High (two engines + an app). **Owner**: Fable.

### MR-17 — Vertical viewport + scroll  *(renderer)*
- **Motivation**: exemplar B is ~250 px tall vs 209 px content area (`CalculationApp.cpp:44-59`);
  auto-height grows unbounded and never shrinks (`MathRenderer.cpp:593-604`).
- **Files**: `src/ui/MathRenderer.h/.cpp` (`setViewport`, `_scrollY`, cursor-follow-Y, ⋮ chips,
  height shrink fix), `CalculationApp.cpp` (viewport wiring), scripts incl. scrolled capture.
- **Invariants**: existing goldens byte-identical (their content fits; shrink fix must not alter
  fitting cases — snapshot proof); scroll deterministic.
- **Risk**: Medium-High (placement math). **Owner**: Fable.

### MR-18 — **Exemplar B acceptance gate (final boss)**  *(validation)*
- **Motivation**: P3 exit. Compose the full Φ formula from shipped pieces; verify budgets.
- **Files**: showcase pages (upper/lower), `mathv2_exemplarB_*.numos` (NumIR build, serialization
  round-trip, depth assert =17, nodes assert, refusal `DisplayOnly`, glyphplan tofu=0, layout
  bands, scroll tour, T5 self-diff), candidates → **Human-blessed goldens**; firmware HIL:
  showcase on device, `[MEM]` stack HWM + frame-time lines, photo review.
- **What remains unsupported after P3** (explicit): evaluating B as a whole (permanent, by
  ellipsis rule); lim/exact-∫/symbolic-det evaluation (F, CAS-backed); LaTeX export (MR-20).
- **Owner**: Sonnet (scripts) + Human (bless/HIL).

### MR-19 — MatricesApp math-view integration  *(optional, app)*
Read-only NodeMatrix rendering of MatA/B/C and results next to the `lv_table` editor
(`MatricesApp.cpp:294-482` untouched as editor). **Owner**: Opus. Risk Low-Medium.

### MR-20 — LaTeX export (one-way)  *(optional, leaf)*
`numir→latex` string emitter for docs/sharing (arch spec Q8); no import. Host-tested only.
**Owner**: Sonnet. Risk Low.

---

## Cross-ticket honesty ledger

- **Byte-identical always**: MR-01, 02, 03, 04, 05, 06, 07, 08, 09, 12*, 13*, 14*, 15, 16, 17*
  (*for pre-existing stems; their own new candidates are warning-only until blessed).
- **Sanctioned re-bless**: MR-11 only (plus the two exemplar gates blessing *new* goldens).
- **Firmware-affecting**: all except MR-02/MR-20; each PR records flash/RAM deltas (MA format).
- **HIL-required**: MR-04 (flash sign-off), MR-09 (deadline/WDT), MR-10, MR-18 (photo + `[MEM]`).
- **Fable-grade tickets**: MR-03, 05, 06, 09, 11, 12, 13, 16, 17 — the renderer-core and
  cross-engine set; do not delegate to cheaper tiers (GT:395 rule).

*End of Document 6.*
