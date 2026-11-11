# MATH-TEX-PARITY-02 — NumOS mathematical typesetting

## Architecture Assessment

The candidate composes the existing 29 AST node types with STIX Two Math and the eight TeX style states. Geometry uses immutable font metadata, integer pixel coordinates and cached child positions shared by layout, painting and cursor search. The ESP32-S3 target remains N16R8; the separate Windows publication remains available throughout development.

**Acceptance state: full review 2 APPROVED at 8.585/10; the separate fresh challenge APPROVED at 8.545/10.** Both reviewed the corrected 4987 candidate and found zero open issues. The first independent review REJECTED e019 at 7.940/10. Its three findings—Measurement composite ink, the oracle's cached-ink blind spot and duplicated NodeParen padding—are fixed and independently verified closed. Post-review performance/resource closure passed; the identical approved executable is now published locally and the usual launcher smoke passes. The [critical review history](MATH_TEX_PARITY_02_CRITICAL_REVIEW.md) preserves every score and the original failing evidence.

The received working tree includes the uncommitted SCIENTIFIC-QUANTITIES work. It was preserved as the baseline, not reconstructed from HEAD. [A-received](../out/math-tex-parity-02/A-received/) contains the readable source snapshot, SHA-256 manifest, Git state, source/index patches, request, runtime fingerprints and copies of the published emulator and its DLLs. Font files are identified by hashes rather than redistributed. HEAD at receipt was `ab4017764a62a03e0befd51a50fa82f3c31b4b70`; the index was empty. No stage, commit, push, web deployment or hardware flash is part of this task.

The [reference contract](MATH_TEX_PARITY_02_REFERENCE.md) records the primary TeX/OpenType rules, exact font identity and deliberate product policies. The [received gap audit](../out/math-tex-parity-02/B-standards/current-gap-matrix.md) and [candidate closure](../out/math-tex-parity-02/B-standards/candidate-gap-closure.md) retain separate states for all 94 historical recommendations. The [AST matrix](../out/math-tex-parity-02/B-standards/AST_TYPESETTING_MATRIX.md) declares every supported node's class, style propagation, geometry, cursor and screenshot contract.

## Solution

### Shared geometry and styles

`MathTypography.h` preserves the original enum values and adds cramped counterparts for Display, Text, Script and ScriptScript. Super/subscript transitions follow TeX; fraction numerators/denominators keep the base size at script level 0 (readable-fraction policy, TXP PD-4, restored by MATH-READABILITY-REGRESSION-01B) and step down from script level 1; radicands are cramped and root degrees take one ordinary script step. Nominal em sizes remain 18/12/8px. LVGL line heights are 31/20/14px and describe raster line boxes, not the design-unit scale. The finite 8px floor and its difference from ideal 70%/55% sizes are explicit, including in the reference comparison.

`FontMetrics` distinguishes advance, italic correction, ink extents, baseline, axis and line box. Scalar Latin/Greek identifiers use verified mathematical italic glyphs; multi-letter names, functions, numbers, units and descriptive scientific labels stay upright. Existing constant and Delta policies are retained. No semantic symbol name or serialization is rewritten for presentation.

The TeX eight-class table remains a constant-time lookup. Ordered binary demotion and transparent row traversal preserve behavior beyond child 64. Parenthesized subformulas remain INNER. A factorial owns ORD boundaries instead of inheriting the function OP spacing. Tutor integer-membership nodes preserve their REL presentation class through clone, including the upright role flag; this fixes a real clone-only spacing loss without changing the semantic name.

### Scripts and MathKern

`MathScriptGeometry.h` handles italic correction once, baseline shifts, cramped superscript minima, boxed versus glyph nuclei, and all four MathKern corners. Kern heights are evaluated in each glyph's own em; two source sums are compared and the selected design-unit sum is rounded once. Missing records are zero. The generated const subset contains 142 nonzero glyph records and 159 deduplicated corner tables, using 3,426 bytes; independent source-boundary tests cover 30,387 cases.

`Power(Subscript(...))` shares a nucleus through transparent Row/QuantityReference wrappers and enforces the combined-script clearance. Subscript(Power(...)) stays an explicitly nested boxed composition. No general Scripts node or new parser syntax is invented. Calculated positions are cached; draw and cursor consume them rather than re-deriving script formulas.

### Fractions, radicals and periodic decimals

Fractions apply the font's nominal numerator/denominator shifts and rule-to-ink gap minima independently. The same axis, odd-thickness split, centered child positions and one-mu overhang define layout and the filled rule rectangle. Text fractions demote children to Script; nested fractions reach the physical ScriptScript floor honestly.

Radicals retain the declared vector-sign policy, with source MATH gap, rule thickness, extra ascender, degree kern and percentage raise. The degree stays ScriptScript. The extra ascender is reserved above the rule; it does not introduce a fictitious gap beneath the radicand. Logical child/cursor boxes remain enclosed independently of the visible ink.

Periodic decimals now derive their bar from the actual repeat-run ink and OverbarVerticalGap, OverbarRuleThickness and OverbarExtraAscender. The cached rectangle is painted exactly; no separate draw-time constants remain.

### Delimiters, large operators and functions

The existing genuine STIX parentheses/brackets and their assembly plan are preserved. Braces and bars use the declared compact-font vector fallback. `MathVectorDelimiterGeometry.h` shares the height-dependent brace width and rounded-cap allowance with layout. Segment centerlines are inset inside the reserved ink box. A tall brace previously crossed a numerator because the renderer used a 14px bulge in a 6px reserved slot; the new stroke detector records that baseline defect and checks the correction. Vertical bars always use the bounded vector path because the compact subset's variant table could otherwise leave a short base glyph inside a tall requested slot.

Large operators use authentic source base/display glyphs in small supplemental immutable fonts. `MathOperatorGeometry.h` owns axis centering, limit baselines, independent upper/lower gap constraints and italic correction. Display and cramped Display use over/under limits; all other styles use side scripts. Variant height is not used as a substitute for style: the source's terminal sum variant is shorter than DisplayOperatorMinHeight at 18px yet remains the correct display glyph. Integral differential/variable spacing uses actual advances.

Function, logarithm, call, equation, special-value and piecewise labels use actual glyph metrics. Matrix/piecewise boxes center their content around the math axis. Delimiter groups retain child logical boxes even when delimiter height is based on ink. Every currently supported node is listed in the AST contract rather than implicitly treated as an ordinary text leaf.

### Editing, resource limits and failure recovery

`calculateLayoutChecked` is the external admission point. Its allocation-free iterative preflight checks parent links, required children, cycles, at most 192 edges of depth and 4,096 nodes including the root. Vacant Matrix cells remain legal. It reuses an existing traversal field rather than allocating a stack proportional to input size. Trusted recursive layout runs only after admission.

Checked 32-bit intermediates latch coordinate overflow before narrowing into cached 16-bit fields. The supported coordinate range is ±30,000px and full boxes are checked too. This leaves headroom for viewport/cursor origins. A failed layout clears the root geometry, disables cursor coordinates and pan, and displays the localized size-refusal text. The source AST remains available for editing; a later valid edit is admitted normally. CalculationEngine and all external app/layout callers use the checked entry point. Existing semantic result-size gates remain distinct from renderer admission.

Focused product tests cover all eight styles, 191/192/193-edge depth, 4,095/4,096/4,097 nodes, coordinate edges, long text, mixed nesting, missing children, bad parents/cycles, 90 accepted roots, 2,000-number rows, and 50 refusal/recovery cycles. The guard-bypass negative control produces 139 failures. Reference typography tests include scientific subscripts, clone/memory paths and integer membership; stripping upright roles produces 94 failures.

### Evidence and reproduction

Evidence is retained under [out/math-tex-parity-02](../out/math-tex-parity-02/). Each phase retains its earlier captures and verdict; newer filenames do not retroactively make an older binary a final candidate.

| Layer | Evidence / purpose |
|---|---|
| Source standards | `B-standards/`: primary source identities, complete internal-spec audit, all 56 MATH constants and font coverage |
| Received geometry | `A-baseline/geometry-final-contract/`: historical 131-recipe/four-style replay; 4,992 nodes and 2,954 findings under the earlier v3 detector. Expanded received images are in `geometry-ink-corpus`; the cached-ink blind spot is explicitly superseded. |
| Candidate geometry | `C-oracle/ink-after/`: 145 recipes in all eight styles, actual LVGL submissions, per-node boxes, rules, glyphs, clipped delimiter pieces, strokes, placeholder borders and cursor observations |
| Same-font reference | `C-oracle/tex-reference-ink/`: LuaHBTeX with the exact STIX input at 18/12/8; 290 Display/Text raster references, independent of productive placement helpers |
| Generated stress | Two deterministic seeds, each with 2,000 valid trees actually rendered, preserving per-style distribution, node counts and all signed deviations |
| Negative controls | Wrong em, superscript shift, cursor origin, fraction rule, delimiter ink, removed MathKern, incorrect operator glyph, clone class, periodic bar and guard bypass |
| Product | `I-product-stress/`: native scripts, A–N corpus, generated editor sessions, references, refusal/recovery, web matrices and immutable baseline comparisons |
| Resources | `resources/`: same-compiler baseline/candidate ELF, type layouts, own-frame stack model, allocation traps, fixed-pool lifetime and allocation-failure tests |
| Original review | `Z-final/*original-review*.json`: individual 320×240 PNG inspection, including byte-identical style aliases and the subsequently corrected vector findings |
| Gallery | `gallery/index.html` and `gallery/MATH-TEX-PARITY-02-review.zip`: mathematical originals/reference plus 65 baseline/candidate product pairs and eight corrected Measurement pairs; no font binary |

The Measurement-corrected `4987f4ea…3b7f3058` native checkpoint is 18,338,483B. Its stronger oracle pair records 314,262 and 312,399 checks, respectively, over 4,000 actually rendered random trees plus the 145-recipe fixed corpus. The combined original-review manifest hash-verifies all 1,160 images; all 1,048 earlier originals are unchanged and all 112 new originals were inspected. The earlier e019 all-pass counts are preserved as superseded evidence, not treated as proof after the critic exposed their premise. [Final exact changed-file list](../out/math-tex-parity-02/Z-final/accepted-final-2/changed-files.md) and [snapshot provenance](../out/math-tex-parity-02/Z-final/SNAPSHOT_HISTORY.md) distinguish this task from inherited scientific changes and retain the audit script's superseded classification attempts.

The independent geometric oracle uses source constants and observed glyph boxes; it does not import the productive geometry helpers. Following CR1-002 it additionally unions actual submitted descendant ink and proves compound-cache enclosure before accepting cache-based placement. Fraction/radical clearances are separately checked against observed primitives and raw MATH gap minima. The frozen e019 source fails 2,751 strengthened checks; deliberately removing only the final Measurement cache update from a private build produces 476 deterministic failures. Nine data mutations include both corrupted composite caches and displaced descendant ink. Signed deviations remain visible. The generic heuristic for arbitrary glyph-to-glyph overlap is not an acceptance oracle; dedicated constraints and original-image review complement it.

TeX raster equality is not claimed. Bitmap advances accumulate rounding on long rows, and vector radicals/braces differ from the source outline. The 70-term long-row horizontal difference reached 17.89px in a recorded normalized comparison; it remains visible rather than being masked. Small 8px glyphs and the ScriptScript floor limit readability under deep nesting. These are declared raster/product differences, not formula-specific offset patches.

Build candidates with `python scripts/build-math-tex-parity.py --profile native` (or `pool`, `wroom`, `cam`). This helper clears inherited `PLATFORMIO_BUILD_DIR`, checks isolation and builds only under `C:/.piobuild/numOS-parity-02/`. It does not publish or flash. The harness/oracle/reference/gallery entry points are `scripts/math-tex-parity-{harness,oracle,tex,compare-reference,gallery}.py`; their `--help` output describes source/build/output inputs. Tests retain the exact commands, binary/source identities and subprocess logs in their output directories.

### Product and browser checkpoints

The Measurement-corrected `4987…` native checkpoint passed 132 product scripts, 89 A–N corpus cases, 96 generated edit sessions with 1,248 states, 437 focused reference checks, 424 bounded-layout checks and all seventeen product suites, including Toolbox 208 checks, Tutor composition in English/Spanish and 900 Tutor traces. Product assertions compare mathematical output and provenance with the complete received scientific baseline. Current evidence is under `I-product-stress/final-v4`; historical passes are not relabeled. The product reviewer directly inspected 65 integration originals and eight corrected nested-Measurement originals. Four actual-key Measurement repros also attach 256 compound-ink checks and 72 observed-gap checks across all eight styles, with a one-pixel minimum observed gap; [current validation](../out/math-tex-parity-02/I-product-stress/CURRENT_PRODUCT_VALIDATION_4987.md) records the binding.

One inherited viewport expectation assumed twelve nested fractions must scroll vertically. Correct script demotion makes that result 134px high inside a 162px canvas, so no pan is correct. A parity-only adapter retains this fitting case and adds eighteen levels with a 194px result in a 172px canvas to check overflow, saturation and exact return. The original test and original failure are preserved.

Three inherited GIAC-B01 checks concern stale/scalar serializer expectations and remain separately classified. An inherited `sqrt(2+sqrt(3+sqrt(x)))` evaluation can exceed 120 seconds; the baseline exhibits the same CAS timeout while its supported rendering/editing path works. This is an unresolved baseline CAS issue, not an intentional renderer size limit or a repaired semantic feature. The inherited conversion of NTHROOT results into Call nodes is also documented as a result-converter limitation.

The current rebuilt UI package passed the complete Chromium, Firefox and WebKit matrix on both shell and component surfaces, with device scale/canvas geometry and genuine input-event synchronization. All eight top-level suites passed, including scientific quantities, temperatures, brackets, spacing, input and the legacy quantities/FORMAT/viewport/four-locale group. The separately recorded accumulated-WebKit run also passed. All 92 mathematical spacing frames and diagnostics match across the three browsers; only the status bar is excluded from that comparison. Package hashes are identical before and after the matrix. See the [current browser record](../out/math-tex-parity-02/I-product-stress/final-v4/web-current-summary.json). No browser-specific CSS changes mathematical placement. Baseline and final browser timing each comprise 324 samples across eighteen workloads, three repetitions and six surfaces; the final run occurred after both independent approvals. Historical WebKit incidents are kept separate.

### Final integration and exact files

The final isolated native build succeeded on 2026-10-09 and retained exactly SHA-256 `4987f4ea928aaa138c90ca3aecdb92bd1a6c5d6edbf28046a17633d03b7f3058`. The official `scripts/publish-emulator-windows.ps1` then atomically published it to `C:/.piobuild/numOS/emulator_pc/program.exe`; `previous-program.exe` holds the received `57a0203257…e6570` version and all four DLLs are unchanged. No user session was terminated. The ordinary `scripts/run-emulator-windows.ps1` completed the final headless `2+3=5` script with exit 0. [Publication](../out/math-tex-parity-02/Z-final/publication.json) and [launcher smoke](../out/math-tex-parity-02/Z-final/launcher-smoke.json) preserve commands and identities. The first automation attempt could not open an absolute Unicode script argument; its failed log is retained, and the successful invocation uses the repository-relative ASCII script path.

The [final acceptance record](../out/math-tex-parity-02/Z-final/final-acceptance.json), [contract checklist](../out/math-tex-parity-02/Z-final/ACCEPTANCE_CHECKLIST.md) and [immutable final source manifest](../out/math-tex-parity-02/Z-final/accepted-final-2/manifest.json) bind the complete result. The changed-file list compares with the entire received scientific working tree, not HEAD alone. Copied files are read back; font data is hash-only. The original HEAD/index and both protected files remain unchanged. No stage, commit, push, web deployment or hardware flash was performed.

## Memory & Typographic Profile

The final actual-product performance matrix also completed after both approvals. Baseline and candidate each passed 126 native observations (42 groups × three repetitions) and 160 history evaluations. Root-layout, node-visit and draw counts match for every corresponding action; changed glyph consultations are shown explicitly. Long-row navigation layout is 1.310→1.786ms, periodic-result pan layout 0.540→1.158ms, Tutor activation layout 0.142→0.246ms and Tutor page navigation 0.337→0.564ms. The 767 checked calls include 1.447ms aggregate admission overhead across all current observations. The [complete native table](../out/math-tex-parity-02/I-product-stress/PERFORMANCE_NATIVE_4987.md) separates its O0 instrumented root-layout, drawing and guard scopes; it is not interchangeable with the O1 microbenchmark.

All 324 final browser events passed, with exactly three samples for each of 108 matched workload/surface groups and an unchanged package. The [complete browser comparison](../out/math-tex-parity-02/I-product-stress/PERFORMANCE_BROWSER_4987.md) preserves absolute increases and decreases. The largest observed wall-time increase is Firefox component short entry, 161.249→198.489ms (+37.241ms, 1.231×). Browser intervals include automation, processing/publication barriers and two RAFs; the baseline was measured earlier, so these are observational comparisons with scheduler/compositor confounders, not causal renderer latency or an invented statistical threshold. The [performance closure](../out/math-tex-parity-02/I-product-stress/performance-final-closure-4987.json) binds every report, raw record, current source and package hash.

The source and generated tables remain flash-resident; layout and cursor contain no allocator or transient string construction. Each final native denial run checks C allocation, C++ allocation and AST allocation separately over 48,000 layouts, 960 draw frames and 1,920 cursor calls. Across five runs this is 240,000 layouts, 4,800 frames and 9,600 cursor calls, with 244,800 admissions/root layouts and zero own geometry/cursor allocation or cursor relayout. LVGL's legitimate draw-task allocations are counted separately rather than falsely attributed to geometry.

| Metric | Received baseline | Current evidence interpretation |
|---|---:|---|
| WROOM application image | 6,297,920B; 255,680B partition free | Measurement-corrected 6,313,760B (+15,840); 239,840B free, above 131,072B gate |
| CAM application image | 6,209,824B; 343,776B partition free | Measurement-corrected 6,225,888B (+16,064); 327,712B free, above the same gate |
| WROOM linked flash | 6,297,553B | 6,313,389B (+15,836) |
| Windows native executable | 18,287,972B | 18,338,483B (+50,511) |
| WROOM static RAM / IRAM | 119,672B / 60,407B | Unchanged in corrected ELF |
| CAM static RAM / IRAM | 118,264B / 59,039B | Unchanged in corrected ELF |
| Host node / LayoutResult | 56B / 16B | Unchanged |
| Host FontMetrics | 64B | 72B with per-invocation overflow latch pointer; target pointer delta is 4B |
| Host Subscript / Power / Symbol | 72B / 80B / 96B | 80B / 80B / 96B; combined-script cache increases Subscript only |
| Supplemental operator bitmap data | — | 5,710B total for source base/display operators at all three sizes |
| Fixed LVGL pool | Configured 65,536B | Native monitor usable 57,696B after allocator metadata |

Both corrected firmware builds retain a measured own-frame recursive maximum of 176B. The conservative 192-edge model totals 33,904B of own frames, leaving 31,632B of a 64KiB modeled task stack for unmodeled callees; a realistic ninety-Root/Row chain models 23,152B. These are compiler `.su` calculations, **not physical stack high-water measurements**. Hardware remains unexecuted. No assertion of measured physical PSRAM cache behavior or ESP32 heap peak is made from host tests.

After both independent approvals, five alternating-order baseline/current pairs were run in an exclusive workstream window. The median of per-case layout ratios is **1.881×**; draw/navigation is **1.312×**. Text-style examples are short scalar 2.310→2.942µs, long-row layout 236.957→630.005µs, nested scripts 5.328→19.731µs and unchanged-panning layout 239.035→806.909µs. The associated panning draw/navigation phase is 829.380→1,976.395µs. All 96 phase records and ten raw run logs are preserved in [the final paired comparison](../out/math-tex-parity-02/resources/final-performance/paired/comparison.md), including larger ratios and unchanged deterministic counters. Italic/kern/ink work and checked admission cost real CPU time. Cursor navigation performs no layout. These are O1 instrumented host observations, not ESP32 latency promises. Historical e019 timings (median ratios 1.445×/1.128×) remain in their [original checkpoint](../out/math-tex-parity-02/resources/hotpaths-paired/comparison.md); differences in host conditions prevent treating those earlier absolutes as the current result. The subsequent actual-product/browser matrix above is complete.

The refresh/navigation interval includes pan/cursor movement, invalidation, the one checked layout in `onDraw`, drawing, LVGL tasks and refresh. It is not a pure paint measurement. Each measured frame has exactly one admission/root layout; `computeCursorPosition` itself performs none and has separate time/call counters.

The fixed-pool baseline and corrected candidate both completed 100 measured cycles after four warmups, with minimum sampled free 6,128B and largest free block 5,520B. Menu settled at 67 LVGL objects, three timers, 19,352B free and zero source handles. A separate 100-cycle private C++/scientific ownership run after 54 warmups found constant sampled bytes, payloads, contributions and unique-source counts. Persistent allocation-failure injection passed 27 real scientific journeys plus four no-allocation Help controls, preserving prior expression/Ans and subsequent `2+2` recovery without resetting Giac. The [resource report](../out/math-tex-parity-02/resources/RESOURCE_REPORT.md) binds these records to their native, firmware and private probe identities. The third reviewer independently rehashed the retained fault/ownership executables and source pairs, resolving the prior location-only verification gap.

DWARF target ABI deltas are explicit: FontMetrics 48→52B; Power 56→60B; Root 72→76B; PeriodicDecimal 120→128B; Subscript 52→60B; MathCanvas 196→212B. MathNode, LayoutResult, Measurement and QuantityReference are unchanged. Per-node retained storage is a real AST construction cost even when already-constructed layout is allocation-free. All enumerated linked geometry/cursor functions reside in `.flash.text`; unchanged IRAM totals do not mean these functions execute from IRAM.

The final product history profile runs the same 160-evaluation script on each version. From cycle 50 through 160, both retain 50 history entries, 578 live MathNode objects and 46,752 B of node payload. That payload count excludes allocator headers, child vectors, strings and Giac. Windows `PrivateUsage` is separately measured whole-process private committed memory ([Microsoft definition](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex)); its candidate offset is +1,949,696 B at cycle 1 and +1,888,256 B at cycle 50, then stays flat through cycle 160. The script and allocation-probe overlay are identical. Startup/runtime/allocator composition was not isolated, so the offset is neither labeled a renderer leak nor assigned to profiler storage or PSRAM without evidence.

## Risks & Edge Cases

- Host timings have increased in some workloads; absolute costs and measurement limitations are recorded above. Device latency still requires the later physical checklist.
- Physical LCD readability, task stack high-water, PSRAM/cache contention and board interaction require later hardware acceptance. No firmware is flashed here.
- The inherited nested-radical CAS timeout and NTHROOT result-converter limitation are separately documented, not concealed as typographic successes.
- Integer raster advances, vector radical/brace/bar outlines and the fixed 8px floor prevent an honest claim of complete TeX raster equality.
- Checked admission protects geometry before recursive layout; it does not redefine all unrelated semantic traversals or promise arbitrary-depth AST destruction.
- A native executable in use can block candidate linking or publication. The live published file is never removed to force replacement.

## Alternatives Considered

Importing a second layout engine would duplicate the AST/editor geometry contract and increase embedded resource risk. Measuring text via temporary strings or dynamically assembled glyph vectors would allocate inside hot paths. Per-formula offsets would hide rather than solve the source-rule errors. The implemented shared inline helpers and bounded metadata preserve existing application semantics and make the key typographic invariants directly testable.

### Later physical acceptance checklist

Perform this checklist separately on WROOM and CAM only in a later authorized hardware task. Record the firmware SHA, board revision, locale and photographs at native LCD scale; preserve the Windows fallback throughout.

- [ ] Negative exponent and nested powers: readable signs, separated scripts and matching cursor slots.
- [ ] Nested fractions and indexed/nested roots: visible clear rows around rules and no index/fence collisions.
- [ ] Mixed superscripts/subscripts: clear vertical separation and no loss of the 8px glyphs.
- [ ] Tall parentheses, brackets, braces and bars: full content enclosure and no neighboring ink overlap.
- [ ] Display and side operator limits: correct style-dependent placement and legible integrals/sums.
- [ ] Upright units, scientific labels and absolute/difference temperatures: unchanged mathematical roles.
- [ ] Measurement uncertainty inside roots, fractions and scripts: complete/incomplete slots remain visible and clear of rules.
- [ ] A long Tutor page: all formulas readable, page navigation correct, no clipping that prevents access.
- [ ] Horizontal/vertical pan to both ends and exact return: no inaccessible content or persistent blank overscroll.
- [ ] Cursor navigation, insertion and deletion through every placeholder type: no stale position or lost AST.
- [ ] All four locales: LCD readability, labels and mathematical output remain consistent.
- [ ] Record internal/PSRAM heap minima, largest free block, task stack high-water, frame/input latency and PSRAM contention during repeated application cycles; compare against the documented build budgets without substituting host timing.

```text
MATH_TEX_PARITY_IMPLEMENTADO: SÍ
CRITICAL_REVIEW_SCORE: 8.5 / 10
SEMANTICS: 9.0
HORIZONTAL: 8.6
SCRIPTS: 8.5
FRACTIONS_RADICALS: 8.6
DELIMITERS_OPERATORS: 8.1
CURSOR_EDITING: 8.6
PRODUCT_STRESS: 8.5
RESOURCES_MAINTAINABILITY: 8.2
CRITICAL_OPEN: 0
HIGH_OPEN: 0
MEDIUM_OPEN: 0
LOW_OPEN: 0
NORMAL_SCHOOL_STRESS: PASS
BACHILLERATO_STRESS: PASS
UNIVERSITY_STRESS: PASS
INCOMPLETE_EDITING_STRESS: PASS
SCIENTIFIC_UI_STRESS: PASS
HARDWARE: NO EJECUTADO
```
