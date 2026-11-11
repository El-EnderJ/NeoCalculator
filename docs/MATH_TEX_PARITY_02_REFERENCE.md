# MATH-TEX-PARITY-02 reference and product contract

## Architecture Assessment

NumOS typesets its existing 29-node C++ AST using STIX Two Math, immutable OpenType MATH metadata and LVGL bitmap fonts. TeX/OpenType rules determine styles, spacing and vertical placement; the integer raster profile and explicitly listed product choices below determine how those rules become pixels. This document defines the reference used by the parity program; source implementation, independent geometry checks, actual screenshots, product invariance and firmware resources are separate acceptance evidence.

The received audit, including all 53 GAP items, 21 MR tickets, 14 PD policies and six OQ defaults, remains in [current-gap-matrix.md](../out/math-tex-parity-02/B-standards/current-gap-matrix.md). Its received-state labels are not claims that a later fix is missing. [Candidate closure](../out/math-tex-parity-02/B-standards/candidate-gap-closure.md) records the current source separately. [AST matrix](../out/math-tex-parity-02/B-standards/AST_TYPESETTING_MATRIX.md) enumerates every current node and binds it to the oracle corpus.

## Solution

### Primary sources and exact identity

The audit consulted these primary sources on 2026-10-09. Locally preserved bytes, sizes and commits are in [the source manifest](../out/math-tex-parity-02/B-standards/sources/manifest.json).

| Source | Version / immutable identity | SHA-256 of consulted bytes |
|---|---|---|
| [TeX `tex.web`](https://raw.githubusercontent.com/TeX-Live/texlive-source/47e9c9cf1a2342e8e60d80bbcfdd1c21fe79cbcb/texk/web2c/tex.web) | TeX3.141592653; last file commit `47e9c9cf1a2342e8e60d80bbcfdd1c21fe79cbcb` | `c62ab513ef167e93f71a23bd34f311e243210afd7c7a0f9b779614b71e398324` |
| [OpenType MATH](https://learn.microsoft.com/en-us/typography/opentype/spec/math) | OpenType1.9.1 | `646eeab656f4a6297733b157c168cfae6fcfe5e563fe6aae96963e4154cf5877` |
| [MathML Core](https://w3c.github.io/mathml-core/) | Editor draft dated2026-10-01; local bytes are the authority for this audit | `57cd75da4bedaf4f7fd7860b49b8502ab04935f5a151316164a83724ab77bfcc` |
| [HarfBuzz MATH reader](https://github.com/harfbuzz/harfbuzz/blob/a37794ab3f8e5f390941ac9006eb928e93f774e9/src/hb-ot-math-table.hh) | Last file commit `a37794ab3f8e5f390941ac9006eb928e93f774e9` | `b9e805404cae798f85d48c7997b3ec8db9a53db8ea8753fc56824d745fb291f4` |

TeX establishes ordered atom demotion, the eight-class spacing table, cramped styles, fractions and script attachment. Relevant saved-file locations are style macros13855–13859, `make_fraction`14583, `make_op`14679, `make_scripts`14884 and spacing15062. OpenType defines the actual font constants, italic correction, corner kern staircases, variants and assemblies. MathML Core provides an independent account of geometric constraints, especially combined-script gap distribution. HarfBuzz confirms data-reading behavior; it is not treated as a complete layout engine. No primary-source implementation block was copied into production.

The production font input is `assets/fonts/STIXTwoMath-Regular.ttf`, STIX Two Math2.12 b168a, UPM1000, SHA-256 `562551b15b836e6e01d1b7350909baf3c8c8d83260c1190fbf4544333e6936de`. Its existing OFL1.1 license remains unchanged. [Font-source audit](../out/math-tex-parity-02/B-standards/font-source-audit.json) records all56 constants and raster coverage; no font binary is redistributed in the evidence package.

### Physical font profile and eight styles

Nominal em sizes are18/12/8 pixels. Their actual LVGL line heights are31/20/14 pixels because the font includes tall mathematics. MATH DU scaling uses the nominal em; line heights and baselines are used only for raster line-box conversion. Replacing nominal em with line height would distort every MATH constant. The current task's explicit product profile supersedes the generic instruction to equate those quantities.

The source's70%/55% script percentages are recorded, but the available physical profile is12/18≈66.7% and8/18≈44.4%. Both the native reference and normalized TeX comparison declare that discretization. ScriptScript remains at8px at deeper nesting; no fictitious6px font is measured while8px is painted.

| Child slot | Base transition | Cramped state |
|---|---|---|
| Ordinary child, delimiter content, function argument | Inherit | Inherit |
| Superscript / upper operator limit | D/T→S, S/SS→SS | Preserve |
| Subscript / logarithm base / lower operator limit | D/T→S, S/SS→SS | Force cramped |
| Fraction numerator | D→T, T→S, S/SS→SS | Preserve |
| Fraction denominator | D→T, T→S, S/SS→SS | Force cramped |
| Radicand | Inherit size | Force cramped |
| Root degree | Ordinary ScriptScript | Explicitly uncramped, following TeX's root macro |
| Measurement nominal and uncertainty | Inherit | Inherit |

Cramped is a rule state, not a fourth font size. The older full-size nested-fraction readability override is superseded by the standard transitions above.

### Geometry contracts

Let `R(x)` mean signed nearest rounding of `x*em/1000`, and let rule thickness have a minimum of one pixel. Screen y increases downward. Positive `u` raises a superscript/numerator baseline; positive `v` lowers a subscript/denominator baseline. Clearance constraints use painted ink; the final logical box also encloses child line boxes and editing placeholders.

| Construct | Rule and source constants |
|---|---|
| Superscript | `u >= R(cramped?252:360)` and `u >= d_sup+R(120)`; boxed nuclei also enforce baseline drop from their laid-out top by230DU. |
| Subscript | `v >= R(210)` and `v >= h_sub-R(368)`; boxed nuclei add the160DU baseline-drop constraint. |
| Joint scripts | Require `u+v-d_sup-h_sub >= R(150)`. For a deficit, first raise sup by at most `max(0,R(380)-(u-d_sup))`, then lower sub by the remainder.380DU caps this extra rise; it is not an unconditional minimum. |
| Fraction | Axis258DU; rule68DU. Numerator/denominator baseline shifts640DU in display and585DU otherwise. Independently enforce rule gaps150DU in display and68DU otherwise. Split odd pixel thickness consistently above/below the axis. |
| Radical | Radicand-to-rule gap170DU display /85DU otherwise; rule68DU; reserved extra ascender78DU. Degree bottom raised55% of actual radical ink height; subtract degree ink descent to get its baseline. |
| Display operator limits | Upper gap135DU, upper baseline rise300DU; lower gap135DU, lower baseline drop670DU, all measured from selected operator ink. Upper/lower horizontal centers receive opposite halves of the selected glyph italic correction. |
| Inline operator limits | Base glyph with baseline-based script minima and joint clearance, selected-glyph italic split and40DU SpaceAfterScript. |
| Periodic decimal | Repeat-run ink determines bar top; gap175DU, rule68DU and reserved extra ascender68DU. Prefix/repeat advances match separate painted runs; the cached rule is drawn as the same filled rectangle. |
| Matrix / Piecewise | Center the bounded content about the math axis; `ascent=axis+ceil(H/2)`, `descent=H-ascent`, then union child logical/ink bounds. |

The full fraction and corner-kern equations, with sign conventions, are recorded in [standards-report.md](../out/math-tex-parity-02/B-standards/standards-report.md). The oracle computes its expectations independently of productive geometry helpers.

For italic scalar atoms, nucleus advance `A`, atom advance `A+IC` and ink width are distinct. An ordinary italic atom reserves `IC`. A superscript attaches at `A+IC+kern`; a subscript attaches at `A+kern`. The script helper subtracts an already-reserved correction before applying the proper attachment, preventing double addition. Multi-letter names, numbers, function words, units, existing constants and descriptive scientific subscripts stay upright. The broad Delta product mapping remains unchanged. Complete Latin/Greek coverage was checked at all three sizes, including italic h atU+210E.

MathKern evaluates two source-height pairs and chooses the smaller sum. Base and script kern values are weighted by their own em before one final signed rounding. Equality at a source correction height selects the next upper interval; absent records/corners are zero. `Power(Subscript(...))`, including transparent Row/QuantityReference wrappers, has the declared shared-nucleus presentation used by electron-mass powers. An explicitly parenthesized base is boxed. `Subscript(Power(...))` remains a boxed composition; the AST does not introduce a general Scripts node or silently redefine semantic grouping.

### Declared product deviations and limits

- The18/12/8 integer bitmap profile, signed rounding and one-pixel rule floors are explicit. Pixel equality with a continuous-size TeX rasterizer is not expected; geometry constraints and justified normalized deltas are checked instead.
- Fractions retain one mu of rule overhang on each side, with a one-pixel floor. This is an accepted raster margin.
- Radicals retain a vector sign: hook3mu, slope6mu, right pad2mu, scaled by the current physical em. Vertical MATH rules and the degree baseline remain source-derived. The source's negative degree kern is clamped to leave one mu of clearance from this vector hook because it has no source glyph left bearing. Authentic larger radical glyph variants are not claimed to be compiled.
- Genuine STIX parenthesis/bracket variants and assemblies remain the accepted path, with source100DU minimum connector overlap. Brace/bar fallback is explicit and bounded. Its shared vector plan reserves stroke-cap radius on all sides, limits brace horizontal span by height and em, and keeps rounded endpoints inside the reported ink slot. Vertical bars always use the full-height vector plan, including short bars, so a base glyph cannot paint beyond that slot. Piecewise uses the same cap-aware width. The legacy generic assembly helper can still reduce connector overlap to zero; the active brace/bar tables have no assembly parts. This is a scoped limitation, not a claim of universal OpenType assembly conformance.
- Display style places limits above/below all supported large operators, including integrals, as the explicit calculator product contract. Text/script styles place them to the right. This integral policy differs from TeX macro conventions that often use `\nolimits` for integrals.
- The largest authentic summation variant is1326DU, below the source1800DU advisory minimum. NumOS uses the largest real available variant and does not suppress display limits because that threshold is unreachable. All eight supported operators have authentic base/display pairs; none of those16 source glyphs has MathKern data.
- The existing29 NodeTypes are the scope. Factorial is a Function kind, relations an Operator/Symbol role, and uncertainty formats are presentations of scientific values. Binomial, general accent, forced-display and general Limit nodes remain unsupported rather than being invented for an old proposal.
- Layout admission permits at most192 raw parent-child edges,4096 nodes and30000px representable extent. Refusal is visible, preserves the source AST and admits a later healthy edit. These are geometry safety limits, distinct from existing serialization/evaluation budgets and from measured hardware stack high-water.

### Reference and evidence policy

The independent source-font oracle, pinned LuaHBTeX/unicode-math run, actual NumOS captures, overlays and per-node screenshot bindings live in [C-oracle](../out/math-tex-parity-02/C-oracle/README.md). TeX uses the same repository font, not an implicit system font. Original reference versions and received screenshots are preserved. A changed normalization or expected presentation must retain its before/after and rationale; references are not silently re-blessed to match the candidate.

Native product tests cover canonical serialization, Giac results, measurement SourceId, unit/reference identities, editing, Tutor pages and viewport/cursor behavior. Metadata extraction, geometry assertions, screenshot inspection and product journeys answer different questions. A passed source-table test alone cannot establish visual parity or scientific invariance.

The first independent critical review rejected the candidate after finding that a Measurement's cached ink omitted its final enclosing parentheses. Fractions and radicals could therefore satisfy cache-based clearance checks while colliding with a physical fence. The accepted correction assigns the final delimiter ascent/descent to both the logical box and cached ink. The oracle must independently reconstruct the submitted glyph/stroke bounds and compare composite caches against those bounds; treating the production cache as the expected ink is insufficient. Earlier passing reports and images remain historical checkpoints, and this correction requires fresh review rather than retroactive approval. The same review removed duplicated NodeParen padding arithmetic: layout and painting now use its shared `innerPad()` accessor without adding storage.

The strengthened detector reports zero issues across [314,262 checks](../out/math-tex-parity-02/C-oracle/ink-after-verdict/summary.json) and [312,399 checks with a second seed](../out/math-tex-parity-02/C-oracle/ink-after-seed2-verdict/summary.json). Its [frozen before-state replay](../out/math-tex-parity-02/C-oracle/ink-before-complete-verdict/summary.json) detects 2,751 findings, and the [compiled stale-cache mutant](../out/math-tex-parity-02/C-oracle/ink-cache-mutant-verdict/summary.json) detects 476. These causal results validate the detector's sensitivity; the fresh full review and separate challenge remain independent decisions.

## Memory & Typographic Profile

MathKern contributes3426 bytes of immutable metadata:142 nonzero compiled-codepoint records,159 deduplicated corner tables and543 signed16-bit values. The independent C++ boundary test checks30387 source-derived queries. The operator subset contributes5710 bytes of bitmap payload across18/12/8px, plus descriptors and576 bytes of ink metadata. The full final linked image delta is the cost authority; no runtime font parsing, font-table allocation or global glyph cache is added.

Reproduce font evidence with `scripts/generate-stix-math-kern.py --check`, `scripts/test-stix-math-kern.py` and `scripts/generate-stix-operators.py`. Generated operator output was byte-identical across repeated runs, with temporary remapped input fonts removed. Font hashes, generation metadata and bitmap/raster records remain in B-standards.

Own Xtensa frames, actual DWARF object sizes, app margins, allocation denial, fixed-pool lifecycle and native timings are in [resources](../out/math-tex-parity-02/resources/MEMORY_LIFETIME_AUDIT.md). The loop stack is64KiB. Recursive layout cost is evaluated along simultaneous call paths; sequential row phases and iterative paint/cursor frames are not multiplied and summed as an invented peak. Device stack high-water, PSRAM latency and live device fragmentation remain unmeasured.

The [final post-review resource report](../out/math-tex-parity-02/resources/final-performance/RESOURCE_REPORT.md) contains five alternating baseline/current pairs executed after full review 2 and separate challenge 3 approved the corrected candidate. It preserves absolute timing regressions, deterministic counters, fresh ELF/DWARF profiles and source/binary identities. Refresh/navigation includes the checked layout inside each timed frame; it is not exclusive painting time. Native timings do not establish ESP32 latency. Earlier reports and long lifecycle runs retain their original scope and execution identity.

## Risks & Edge Cases

- Script kernels must keep mixed-font height units, source step-boundary equality and negative rounding consistent.
- Small-font coverage does not prove readability; original8px captures must remain reviewable.
- Logical line boxes can exceed painted ink. Parent enclosures must retain both without using line padding as a fabricated clearance constraint. Composite ink must include its own delimiters/rules, as demonstrated by the first critic's Measurement finding.
- Overflow refusal must occur before narrowing can wrap to a positive value; the guard also must cover direct app layout entry points.
- The inactive generic assembly limitation, vector radical and integral limit policy remain explicit departures. They must not be hidden behind a blanket claim of complete TeX/OpenType equivalence.

## Alternatives Considered

A new semantic tree, parser, font shaping runtime or whole-font rebuild is unnecessary for this supported-subset parity program. Small immutable source-derived subsets preserve existing glyph identities and flash margin. Shared placement helpers and per-node caches keep ownership local and make layout, drawing and cursor geometry auditable without a mutable global scratch arena.
