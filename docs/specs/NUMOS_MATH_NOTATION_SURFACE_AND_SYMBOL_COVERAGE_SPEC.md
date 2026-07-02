# NumOS — Math Notation Surface and Symbol Coverage Specification

> **Normative coverage spec**: defines what mathematical notation NumOS can express, at which
> support level, in which phase. Part of the MathRenderer v2 (MRV2) document set.
> Verified against `main @ 2d6b796` (2026-07-02); source claims cite `file:line`.
> Markers: **[V]** verified today, **[P]** proposed. Phases: **P1/P2/P3/F**(uture) as defined in
> `NUMOS_MATHRENDERER_IMPLEMENTATION_TICKETS.md`.
> Companions: architecture spec (layering, capability lattice §2.3), layout spec (geometry),
> input/serialization spec (NumIR forms cited here), test plan (coverage oracles).

---

## 0. Support-level vocabulary (used in every table)

Per the capability lattice (architecture spec §2.3), each notation family is scored on five
independent axes — **conflating them is the failure mode this spec exists to prevent**:

- **R** render: lays out and draws.
- **E** edit: cursor can enter, build, and delete it.
- **S** serialize: NumIR emit+parse round-trips it.
- **V** evaluate: `MathEvaluator` produces an `ExactVal` (or a typed refusal by design).
- **C** CAS: `ASTFlattener`→`cas::SymExpr` and/or the Giac text serializer (CGIAC §C.2) map it.

Notation: `R[V]` = renders today (verified); `R[P1]` = renders after Phase 1; `V[—]` = evaluation
permanently refused **by design** (display-only, refusal `DisplayOnly`, never `"Math ERROR"`);
`C[F]` = CAS mapping is future work.

Glyph coverage baseline [V]: all three STIX subsets (18/12/8 px) carry identical ranges
(`stix_math_18.c:4`): `0x21-0x7E, 0xB0-0xFF, 0x391-0x3C9 (Greek), 0x2102/0x2115/0x2119/0x211A/`
`0x211D/0x2124 (blackboard), 0x2000-0x206F, 0x2070-0x209F, 0x2100-0x214F, 0x2190-0x21FF (arrows),`
`0x2200-0x22FF (operators incl. ∀∃∈∑∏∫≤≥≠⋯⋮⋱), 0x2308-0x230B (floor/ceil), 0x25A0-0x25FF,`
`0x27C0-0x27EF, 0x27E8-0x27EB (angle brackets), 0x2900-0x29FF, 0x2A00-0x2AFF, 0x2B00-0x2BFF,`
`0x1D400-0x1D7FF (styled alphabets)` — plus an explicit symbols list (∞ U+221E included).
**Known hole**: extensible-delimiter assembly glyphs U+239B…U+23B3 intentionally absent
(`MathRenderer.cpp:57-61`); mitigated by vector-stroke fallback [V], fixed properly by MR-04/MR-11.

---

## 1. Taxonomy

### A. Numbers and literals

| Item | Display | Node | R | E | S | V | C | Phase | Notes |
|---|---|---|---|---|---|---|---|---|---|
| Integers/decimals `42`, `3.14` | upright digits, monospaced advance | `NodeNumber` | [V] | [V] | [P1] | [V] | [V] | — | `MathAST.h:687-705`; digit editing char-by-char |
| Periodic decimal `0.1̄6̄` | overline on repetend | `NodePeriodicDecimal` | [V] | — (result-only by design, `MathAST.h:1112-1113`) | [P1] | [V] (produced by eval) | [V] (exact fraction, CGIAC C.2) | — | never user-inserted |
| Scientific `4.2×10⁵` | Number×Power row | composition | [V] | [V] | [P1] | [V] | [V] | — | no dedicated node needed |
| Digit-length bound | — | — | — | — | — | — | — | P0 | C5.3: unbounded `appendChar` today; MR-01 caps editable digits at 32 |

### B. Variables and identifiers

| Item | Display | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|---|
| `x y z A–F Ans PreAns` | italic (x/y/z), upright A–F, word chips | `NodeVariable(char)` [V] `MathAST.h:1080-1098` | [V] | [V] | [P1] | [V] | [V] | — |
| Multi-char identifiers (`s`, `n`, `k` as free symbols beyond the 11-slot map) | italic letters | `NodeIdentifier` [P] | [P1] | [P1] | [P1] | [P1]* | [P1] | 1 |
| Reserved word identifiers (`det`, `lim`, `mod`) | upright roman | `NodeIdentifier(IdentKind::Word)` [P] | [P1] | [P2] | [P1] | via owner node | via owner node | 1–2 |

*Free-symbol evaluation: an unbound `NodeIdentifier` evaluates to refusal `UnboundVariable` in
CalculationApp; in Grapher/CAS contexts it flows through as a symbol (existing `SymVar` behavior,
`SymExpr.h:186+`). Bound-variable positions (Σ index, ∫ differential) are never looked up in
`VariableManager` — they are binders (input spec §5.3).

`NodeIdentifier` design [P1]: `{uint32_t cp; IdentKind kind; uint8_t nameRef;}` where multi-char
names index a static name table (no heap strings per node). `NodeVariable` remains as a deprecated
alias until MR-07 completes migration; `VariableManager`'s 11 slots extend by a Greek page
(α…ω user slots) in P2 — storage cost 24 more `ExactVal` slots ≈ 816 B PSRAM, trivial.

### C. Greek letters

| Item | Display | Semantics | R | E | S | V | C | Phase | Glyphs |
|---|---|---|---|---|---|---|---|---|---|
| Lowercase α–ω | STIX italic Greek | user variable (assignable) | [P1] | [P1] (ALPHA-layer page / palette) | [P1] | [P2] (needs VariableManager page) | [P1] (SymVar ext.) | 1 render, 2 eval | in subset [V] `0x391-0x3C9` |
| Uppercase Α–Ω (incl. Φ, Σ-as-letter, Π-as-letter) | upright | user variable / display label | [P1] | [P1] | [P1] | [P2] | [P1] | 1–2 | in subset [V] |
| π, e, i as *constants* | existing | `NodeConstant` | [V] | [V] | [P1] | [V] (i → error, real mode D-2) | [V] | — | `MathAST.h:196-200` |

Disambiguation rule [P]: π typed from the CONST_PI key is always `NodeConstant::Pi` (semantic
constant); Greek-page π is refused to avoid a lookalike variable (single exception in the Greek
page). Σ/Π letters are allowed as identifiers but the palette labels them "letter, not operator".

### D. Latin styled letters (blackboard ℝ ℂ ℕ ℚ ℤ, script/bold from 0x1D400+)

Display-only labels for pedagogy (sets, domains). `NodeIdentifier(IdentKind::Styled)`; R[P2],
E[P2] (palette only), S[P2], V[—] display-only, C[—]. Glyphs already in subset [V]. Phase 2, low
priority; excluded from P1 to keep the identifier table small.

### E. Operators

| Item | Display | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|---|
| `+ − × ±` | binary, TeX BINARY spacing | `NodeOperator` [V] `MathAST.h:95-107` | [V] | [V] | [P1] | [V] (± only in result trees) | [V] | — |
| `÷` inline division sign | BINARY | `OpKind::Div` [P] | [P2] | [P2] | [P2] | [P2] | [P2] | 2 — result/display convenience; fraction remains the canonical division |
| Unary minus | prefix − | leading `Sub` in row [V] (`MathEvaluator.cpp:565-574`) | [V] | [V] | [P1] | [V] | [V] | — |
| Factorial `!` (postfix) | base immediately followed by `!`, ORD spacing after | `NodeFactorial{base}` [P] | [P1] | [P1] (captures left operand like `insertPower`) | [P1] | [P1] (integer n≤170; exact ≤20 via int64, beyond → approximate flag) | [P1] (Giac `factorial()`, SymExpr Func ext.) | **1** — exemplar A |
| `→` (arrow, limits) | REL spacing | `OpKind::To` [P] | [P2] | [P2] (inside lim template) | [P2] | binder syntax only | binder syntax only | 2 |
| mod, permutation nPr/combination nCr | word ops | future | — | — | — | — | — | F |

### F. Relations

`= < > ≤ ≥ ≠` all exist [V] (`NodeRelation`, `MathAST.h:729-735`, REL spacing class). S[P1],
V: `=` splits equation context (EquationsApp [V]); inequalities evaluable as booleans [P3] /
Grapher regions [V via legacy pipeline]. `≈` is emitted by result formatting (CAS-10) — display
token only, never parseable. `∈ ⊂ ⊆` glyphs present, F (set notation).

### G. Delimiters

| Item | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|
| `( ) [ ] { }` stretchy | `NodeParen(DelimKind)` [V] `MathAST.h:164-191` | [V] (vector fallback for tall) | [V] | [P1] | [V] (grouping) | [V] | — |
| `\| \|` absolute value | `DelimKind::Bar` [V] | [V] | [V] | [P1] | [P1] (`abs` semantics — today Bar renders but MathEvaluator has no Abs; SymExpr has `Abs` [V] `SymExpr.h:80`) | [V] | 1 |
| `⌊ ⌋ ⌈ ⌉` floor/ceil | `DelimKind::Floor/Ceil` [P] | [P2] | [P2] | [P2] | [P2] | [P2] (Giac floor/ceil) | 2 — glyphs in subset [V] 0x2308-230B |
| `⟨ ⟩` angle | future | — | — | — | — | — | F — glyphs present |
| `‖ ‖` norm | future | — | — | — | — | — | F |

### H. Fractions / stacked constructs

| Item | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|
| Stacked fraction | `NodeFraction` [V] | [V] | [V] | [P1] | [V] | [V] | — |
| Nested fractions ≥4 deep | composition [V] (`four_level_fraction` stress case, `MathStressExpressions.cpp:141`) | [V] | [V] | [P1] | [V] | [V] | — |
| Continued fraction (finite) | `NodeFraction` nesting — **no new node** (AD in arch spec §8 Q5) | [V] (readable policy keeps glyph size, `MathAST.h:314-326`) | [V] | [P1] | [V] | [V] | — |
| Continued fraction (infinite, with ⋱) | nesting + `NodeEllipsis(ddots)` | [P2] | [P2] (template) | [P2] | [—] display-only | [—] | 2 |
| Binomial coefficient | `NodeBinom` [P] — stacked pair in stretchy parens, **no bar** | [P2] | [P2] (template) | [P2] | [P2] (`C(n,k)` exact via factorials, n≤170) | [P2] (Giac `comb(n,k)`) | **2** — exemplar B |

### I. Powers / subscripts / combined scripts

| Item | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|
| Superscript power | `NodePower` [V] (MATH-constant shifts, italic correction, `MathAST.h:817-842`) | [V] | [V] | [P1] | [V] | [V] | — |
| Subscript (indexing/labels) | `NodeSubscript` [V] | [V] | [V] (tutor-produced today) | [P1] | [—] label-only in P1 (refusal `DisplayOnly` when evaluated standalone) | [P] SymSubscript exists [V] `SymExpr.h:104` | — |
| Combined `x_i^2` | `NodeScripts{base, sub, sup}` [P] — placement per SubSuperscriptGapMin/SuperscriptBottomMaxWithSubscript constants (already extracted [V] `MathTypography.h:355-360`) | [P2] | [P2] | [P2] | as sub+power composition | [P2] | 2 |
| Power of delimited group (superscripting a whole bracket) | `NodePower(base=NodeParen)` — **works today** [V] | [V] | [V] | [P1] | [V] | [V] | — (exemplar B superscript-of-bracket is structurally free) |
| Tensor/prescripts | — | — | — | — | — | — | F |

### J. Roots

| Item | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|
| Square root | `NodeRoot` [V] (vector radical + MATH gaps) | [V] | [V] | [P1] | [V] (radical simplification √12→2√3 [V] `MathEvaluator.cpp:460-501`) | [V] (`surd` mapping, D-3) | — |
| nth root ⁿ√ | `NodeRoot` with degree [V] (`MathAST.cpp:582-616`) | [V] | [V] | [P1] | [V] (odd-root real semantics D-3 [V] `MathEvaluator.cpp:788-796`) | [V] `surd(x,n)` | — |
| Nested radicals (√ inside √, ≥3) | composition [V] (`nested_radicals` stress case) | [V] | [V] | [P1] | [V] | [V] | — |
| Infinite nested radical (with ⋯) | nesting + `NodeEllipsis(cdots)` | [P2] | [P2] | [P2] | [—] display-only | [—] | 2 — exemplar B tail |

### K. Large operators

| Item | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|
| Σ sum with bounds | `NodeSummation` [V] display/inline limits per DisplayOperatorMinHeight (`MathAST.h:410-415`) | [V] | [V-partial: no keypad key; Grapher templates exist] → [P1 palette] | [P1] `sum(body,k,lo,hi)` | [P1] **finite bounds**: iterate ExactVal, caps: ≤10 000 iterations, 5 s deadline (REL H-4); ∞ bound → refusal `UnboundedDomain` | [P1] Giac `sum(f,k,a,b)` (CGIAC C.2 [V-spec]) | **1** — exemplar A |
| Π product (and ∐ ⋂ ⋃ ⋀ ⋁) | `NodeBigOp` [V] `MathAST.h:131-156` | [V] | [P1 palette] | [P1] `prod(...)` etc. | [P1] finite Π only; set/logic ops [—] display-only | [P1] Π; others [—] | 1 |
| ∫ definite integral | `NodeDefIntegral` [V] (4 slots: lo/hi/body/var) | [V] | [V] (IntegralApp/CalculusApp) | [P1] `integrate(f,x,a,b)` | [P1] numeric Simpson via `MathAnalysis` (n=100 cap [V] `MathAnalysis.h:105`); exact [C] | [V-path] SymIntegrate / Giac | — |
| Indefinite ∫ | `NodeDefIntegral` with empty bounds [P2] | [P2] | [P2] | [P2] | [—] numeric refusal | [P2] SymIntegrate [V exists] | 2 |
| lim | `NodeLimit{under: Row}` [P] — word operator "lim" + under-script; OP class; inline style puts under-row as right subscript | [P2] | [P2] (template) | [P2] `lim(body, n, inf)` | [—] refusal in P2/P3 | [F] Giac `limit()` — device-only, needs H-2 guards | **2** render, F eval — exemplar B |
| Double/triple/contour ∫∫ ∮ | — (glyphs U+222C-2233 in subset [V]) | — | — | — | — | — | F |
| min/max under-scripted | — | — | — | — | — | — | F |

### L. Combinatorial constructs

Factorial → §E. Binomial → §H. Explicit `nPr/nCr` word operators: F (semantic duplicates of
factorial expressions; palette-level insert that expands to factorial form is acceptable in P2 —
decision deferred to MR-15).

### M. Matrices / determinants / piecewise

| Item | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|
| Matrix (pmatrix/bmatrix/vmatrix) | `NodeMatrix{rows≤6, cols≤6, DelimKind fence, vector<NodePtr> cells}` [P] | [P2] | [P2] (cell-wise 2D cursor; template asks dims) | [P2] `matrix(r,c; a,b; c,d)` | [P2] element-wise eval → matrix-of-ExactVal via `MatrixEngine` (exists [V], `MatrixEngine::MAX_DIM=5`; NodeMatrix caps 6×6 but eval bridges only ≤5×5 in P2) | [P2] Giac `[[a,b],[c,d]]`; today matrices are J.2-refused at the Giac boundary [V-spec CGIAC C.3] | **2** |
| Determinant, bars display | `NodeMatrix(fence=Bar)` | [P2] | [P2] | [P2] emits `det(matrix(...))` | [P2] via MatrixEngine det [V exists] | [P2] | 2 |
| Determinant, `det(...)` form | `det` word + matrix arg (`NodeFunction` ext. or Word identifier + paren) | [P2] | [P2] | same canonical `det(matrix(...))` | [P2] | [P2] | 2 |
| Piecewise/cases `{` | — (today faked with brace + fractions, `MathStressExpressions.cpp:200-207`) | — | — | — | — | — | F |
| Vectors (column matrix) | `NodeMatrix` n×1 | [P2] | [P2] | [P2] | [P2] | [P2] | 2 |

**One semantics, two displays** for determinants (arch spec Q3): both forms serialize to
`det(matrix(...))`; the fence kind is a display attribute preserved in NumIR as an annotation
(input spec §5.4) so round-trip keeps the user's chosen notation.

### N. Continued fractions — see §H rows 3–4. No dedicated node; policy + ellipsis.

### O. Repeating nested radicals — see §J row 4. Display-only as a whole; finite prefixes evaluable.

### P. Dots

| Item | Codepoint | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|---|
| ⋯ cdots (centered) | U+22EF [V in subset] | `NodeEllipsis(Cdots)` [P] | [P2] | [P2] | [P2] | [—] | [—] | 2 |
| … ldots (baseline) | U+2026 [V] | `NodeEllipsis(Ldots)` | [P2] | [P2] | [P2] | [—] | [—] | 2 |
| ⋮ vdots | U+22EE [V] | `NodeEllipsis(Vdots)` | [P2] | [P2] | [P2] | [—] | [—] | 2 |
| ⋱ ddots | U+22F1 [V] | `NodeEllipsis(Ddots)` | [P2] | [P2] | [P2] | [—] | [—] | 2 — exemplar B cfrac |

`NodeEllipsis` is the archetypal display-only node: `CAP_RENDER|CAP_EDIT|CAP_SERIALIZE` only.
Any tree containing one refuses evaluation with `DisplayOnly` (T-2) and refuses CAS serialization
(J.2). This is the mechanism that keeps "pretty but meaningless" honest (arch spec Q13).

### Q. Functions

| Item | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|
| sin cos tan, arcsin/arccos/arctan (as sin⁻¹) | `NodeFunction` [V] 8 kinds `MathAST.h:117-126` | [V] | [V] | [P1] | [V] (exact special angles [V] `MathEvaluator.cpp:846-879`) | [V] | — |
| ln, log, log_b | `NodeFunction`/`NodeLogBase` [V] | [V] | [V] | [P1] | [V] | [V] | — |
| abs / floor / ceil / sign | Bar delim (§G) + FuncKind ext. [P] | [P1-2] | [P1-2] | [P1-2] | [P1] abs; [P2] floor/ceil/sign | [V] SymExpr Abs; Giac all | 1–2 |
| sinh/cosh/tanh + inverses | FuncKind ext. | [P2] | [P2] | [P2] | [P2] (double-backed, approximate flag) | [P2] | 2 |
| exp | FuncKind ext. (SymExpr has `Exp` [V] `SymExpr.h:79`) | [P1] | [P1] | [P1] | [P1] (`e^x` equivalence) | [V] | 1 |
| det | §M | | | | | | 2 |
| gcd/lcm/min/max | word functions | — | — | — | — | — | F |

### R. Calculus notation

| Item | Node | R | E | S | V | C | Phase |
|---|---|---|---|---|---|---|---|
| ∫ with bounds + differential | `NodeDefIntegral` [V] | [V] | [V] | [P1] | [P1] numeric | [V-path] | — |
| Leibniz d/dx as display | `NodeFraction(Row[d],Row[d,x])` composition — display-only sugar; palette template [P2]; canonical semantic form is `diff(f,x)` word form | [P2] | [P2] | [P2] annotated | [—] as raw fraction; [P2] when produced by template (carries semantic tag) | [P2] SymDiff [V exists] | 2 |
| Prime notation f′(x) | `NodeIdentifier` + U+2032 prime [V glyph] as display; semantic `diff` | [P2] | [P2] | [P2] | [—]/[P3] | [P2] | 2–3 |
| Partial ∂ | glyph [V in subset]; nodes/semantics | — | — | — | — | — | F |

### S. Set / logic notation — display-only glyph coverage exists (∀∃∈∪∩ etc. [V in subset]); ⋂⋃⋀⋁ big-ops render [V]. Semantics F. No P1/P2 work.

### T. Vector / linear algebra — column vectors via NodeMatrix (P2); arrows/hats over identifiers (accents) F (MATH accent constants already extracted [V] `MathTypography.h:466-476`, unused).

### U. Explicitly out of scope for MRV2 (F, revisit post-P3)

Multi-line equation arrays / alignment; piecewise-cases editor; accents (hat/vec/bar/dot);
under/over-braces; contour/multiple integrals; tensor indices; units rendering; chemistry
notation; LaTeX *import*; arbitrary font scaling; RTL math.

---

## 2. Exemplar B requirement audit (explicit, per prompt §D.2)

| Requirement | Verdict | Support strategy |
|---|---|---|
| Matrices | **New node required** — no matrix exists in any tree family (VPAM 17 kinds `MathAST.h:72-90`; SymExpr kinds `SymExpr.h:93-105`; matrices J.2-refused at Giac boundary CGIAC §C.3) | `NodeMatrix` P2 (§M); layout in layout spec §9; MatricesApp keeps `lv_table` editor until MR-19 |
| Determinant as function/operator | Two display forms, one canonical semantic `det(matrix(...))` | §M; FuncKind/Word `det` + fence-Bar display; eval via `MatrixEngine` [V exists] |
| Infinite continued fractions | Finite nesting renders today [V]; infinity expressed by terminating `NodeEllipsis(ddots)` — whole expression display-only | §H/§P; depth budget Y-3 covers the tower |
| ⋱ / ellipsis | Glyphs already in font [V] (U+22EF/22EE/22F1/2026); node missing | `NodeEllipsis` P2 |
| Greek α β γ δ (matrix entries), Φ (lhs) | Glyphs [V] (0x391-0x3C9); no identifier node | `NodeIdentifier` P1 |
| Displaystyle Σ with bounds inside a fraction numerator | `NodeSummation` renders [V], but inside a fraction the style lattice demotes to TEXT (limits move to the side, `shouldUseDisplayLimits` `MathAST.h:410-415`) — exemplar B *shows display limits inside the numerator* | add per-node `forceDisplayStyle` flag (TeX `\displaystyle` equivalent) — layout spec §8.4, P2 |
| ∫ with lower 0 / upper π/2 | Fully supported [V] (`NodeDefIntegral`; π/2 bound is a fraction in the upper slot) | none needed |
| Superscripting an entire bracketed expression by a det-expression | `NodePower` accepts any base incl. `NodeParen` [V]; exponent Row can hold `det`+`NodeMatrix` once those exist | P2 composition; italic-correction path already handles delimited bases |

---

## 3. Phase gates (what the notation surface guarantees per phase)

- **End P1**: everything in exemplar A renders, edits, serializes; factorial + finite Σ/Π evaluate;
  Greek/∞ identifiers render+serialize; abs evaluates; capability refusals live.
- **End P2**: matrix/det/binom/limit/ellipsis/floor-ceil/combined-scripts render+edit+serialize;
  matrix arithmetic ≤5×5 evaluates; displaystyle flag; every exemplar-B subconstruct renders in
  isolation.
- **End P3**: exemplar B composes whole (viewport + depth budget), navigable, serializable,
  display-only evaluation verdict surfaced correctly; exemplar A truncated-eval verified on device.
- **F**: CAS-backed lim/∫-exact/partials; set/logic semantics; piecewise; accents; LaTeX export.

---

*End of Document 2.*
