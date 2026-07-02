# NumOS — Math Input, Semantics, and Serialization Specification

> **Normative spec** unifying input, editor model, canonical serialization (**NumIR**), parsing,
> semantic meaning, and the evaluator/CAS bridge for MathRenderer v2.
> Verified against `main @ 2d6b796` (2026-07-02); source claims cite `file:line`.
> Markers: **[V]** verified today, **[P]** proposed, **MRD-n** = MRV2 decision record (ADR style,
> matching `NUMOS_CAS_SEMANTICS_DECISION_RECORDS.md` conventions).
> Companions: architecture spec (capability lattice, failure model), notation spec (per-family
> coverage), layout spec, test plan (round-trip oracles), tickets (MR-03/07/08/09/15/16).

---

## 1. Input model

### 1.1 What exists [V]

- Physical 8×6 matrix + virtual SHIFT-combos (`KeyCodes.h:34-117`); Casio-style modifier state
  machine (`KeyboardManager.cpp:42-146`).
- **Structural insertion**, never text: keys call `CursorController::insertDigit/insertOperator/
  insertFraction/insertPower/insertRoot/insertParen/insertFunction/insertLogBase/insertConstant/
  insertVariable` (`CursorController.h:110-174`), with VPAM contextual capture (fraction captures
  the left operand into the numerator; power captures the base) and structural backspace
  (`CursorController.h:200-209`).
- Template insertion exists only in GrapherApp (six-template modal, `GrapherApp.cpp:1462+`,
  opened by RIGHT-on-empty `:2534-2539`) and is built by private code (`buildTemplateAST`).

### 1.2 v2 input surface [P]

1. **New CursorController inserters** (MR-07/08/12/13/14):
   `insertIdentifier(cp)`, `insertFactorial()` (captures left operand, same capture class as
   `insertPower` — capturable set extended: `isCapturable` today = Number/Paren/Fraction/Power/Root,
   `CursorController.h:244`; v2 adds Identifier/Function/Matrix/Scripts/Factorial),
   `insertSummation()/insertProduct()/insertDefIntegral()` (cursor lands in lower bound),
   `insertLimit()`, `insertMatrix(rows, cols, fence)`, `insertBinom()`, `insertEllipsis(kind)`,
   `insertAbs()/insertFloor()/insertCeil()` (DelimKind wrappers), `insertScripts(sub|sup)`.
2. **Template palette** (MR-15): a paged LVGL modal in CalculationApp (budgeted per MP §3 /
   G-4 like `canAllocStep()`, `EquationsApp.cpp:486-503`), sections = notation-spec families;
   each entry shows a rendered mini-preview (MathCanvas per visible cell, recycled) and a
   **capability chip**: `=` evaluable, `⌁` CAS-route, `◻` display-only. SHIFT+LPAREN opens it.
3. **Greek page**: ALPHA-lock + F3 cycles Latin→Greek page; keys map positionally to α β γ δ …
   (full map in MR-15 ticket; render-only in P1, assignable in P2 per notation spec §C).
4. **Editor caps** [P, closes REL C5.2 for CalculationApp]: insertion refused past structural
   depth 16 (H-5) and 512 nodes per editor tree; toast + `[INPUT]` log; Grapher keeps its
   `MAX_NEST=8` [V] (`GrapherApp.cpp:2547-2593`) until MR-16 unifies on the shared cap.

### 1.3 Serial/emulator input [P]

`.numos` gains `insert_template NAME` and `type_numir "…"` (append-only grammar per GHOOK §11)
so rich notation is scriptable without 40-keypress sequences — required by the test plan.

---

## 2. MRD-1 — Canonical representation: calculator-native structural, not LaTeX

**Status: recommended. Context**: NumOS needs one canonical serialization for storage, history,
tests, and cross-engine alignment. Candidates: (A) LaTeX subset, (B) calculator-native structural
text ("NumIR"), (C) both as peers.

**Decision: (B) NumIR** — a deterministic, prefix/functional, ASCII text form purpose-built to be
isomorphic to the VPAM tree. LaTeX is rejected as the *internal* language because:

| Criterion | LaTeX subset | NumIR |
|---|---|---|
| Parse complexity on-device | macro-ish, context-dependent (`^` scripts, `{}` grouping ambiguity, optional args `\sqrt[n]`) — a real grammar project | LL(1), one token of lookahead, ~600 lines |
| Isomorphism to VPAM | lossy both ways (spacing cmds, `\left..\right` vs semantic parens) | 1:1 by construction (S-1) |
| Alignment with existing repo formats | none | matches the proposed AST→Giac serializer style (`integrate(f,x,a,b)`, `sum(f,k,a,b)`, `surd(x,n)` — CGIAC §C.2) and the CAS corpus JSONL `input` fields |
| Round-trip testability | needs canonicalizer | `parse(emit(t)) ≡ t` directly |
| Human authorability in tests | good | good (that's the point of functional form) |

LaTeX **export** (one-way, for documentation/sharing) is cheap and deferred to optional MR-20
(arch spec Q8). LaTeX **import** is rejected for MRV2 (no user story on a keypad device).

---

## 3. NumIR — grammar and rules

### 3.1 Grammar (EBNF, LL(1)) [P]

```
expr        := row
row         := item+                          # juxtaposition = implicit row
item        := number | ident | const | op | rel | structure
number      := DIGITS ['.' DIGITS]
const       := 'pi' | 'euler' | 'imag' | 'inf'
ident       := "id(" IDENTTOKEN ")"           # id(x), id(alpha), id(Phi), id(s)
op          := '+' | '-' | '*' | '+-' | '->' | '!'-postfix-via-structure
rel         := '=' | '<' | '>' | '<=' | '>=' | '!='
structure   := NAME '(' args ')'
NAME        := frac | pow | sub | scripts | root | paren | bracket | brace | bar
             | floor | ceil | func | logb | fact | sum | prod | bigop | integral
             | lim | matrix | binom | ellipsis | pdec
args        := row (';' row)*                 # ';' separates slots; ',' stays user-visible text
```

Slot signatures (fixed arity; `_` = empty slot = `NodeEmpty`):

```
frac(num; den)                    pow(base; exp)              sub(base; sub)
scripts(base; sub; sup)           root(radicand)              root(radicand; degree)
paren(row) bracket(row) brace(row) bar(row) floor(row) ceil(row)
func(sin; row)  …  func(det; row)             logb(base; arg)
fact(base)                        sum(lo; hi; body)           prod(lo; hi; body)
bigop(union|inter|and|or|coprod; lo; hi; body)
integral(lo; hi; body; var)       lim(under)                  binom(top; bottom)
matrix(fence; rows; cols; cell11; cell12; …)                  ellipsis(c|l|v|d)
pdec(int; nonrep; rep; neg)
```

Attributes ride as a leading `@` group: `@display sum(...)` (forceDisplayStyle),
`@fence=bar matrix(...)` is expressed by the fence arg instead. Whitespace insignificant;
encoding ASCII (identifiers use names: `alpha`, `Phi` — the codepoint table is the single map).

Examples:

```
1/2+1/3   ⇒  frac(1;2) + frac(1;3)
(x+1)²    ⇒  pow(paren(id(x)+1); 2)
exemplar A ⇒ frac(1;pi) = frac(2*root(2);9801) *
             @display sum(id(k)=0; inf;
               frac( fact(paren(4*id(k))) * paren(1103+26390*id(k));
                     pow(fact(paren(id(k)));4) * pow(396; 4*id(k)) ))
```

### 3.2 Emitter/parser contracts

```cpp
namespace numir {
  bool emit(const MathNode& root, std::string& out);   // PSRAM string; total for CAP_SERIALIZE
  struct ParseResult { bool ok; uint16_t errPos; ParseErr code; };
  ParseResult parse(const char* text, NodePtr& out);   // total; never partial trees on error
}
```

### 3.3 Where NumIR is used

History persistence (today history stores live cloned trees only, RAM-bound,
`CalculationApp.h:131-138` [V]); `.numos` test oracles (`assert_math_serialized`,
`type_numir`); cross-engine golden alignment (same textual shape as Giac serializer);
future LittleFS expression storage (`/exprs/…`, F).

### 3.4 Serialization invariants

- **S-1 [P]** Round-trip identity: for every tree whose nodes all have `CAP_SERIALIZE`,
  `parse(emit(t))` is structurally identical to `t` (node kinds, slots, values, attributes).
- **S-2 [P]** Determinism: `emit` output depends only on the tree (no pointers, no locale, no
  float formatting variance — numbers emit their stored digit strings verbatim,
  `NodeNumber::_value` [V] is already textual).
- **S-3 [P]** Totality of parse: any byte string yields `ok` or a typed error with position;
  fuzz target (test plan §4.5).
- **S-4 [P]** Version header: files/persisted strings carry `numir1:` prefix; unknown structure
  NAME under `numir1` is a parse error (no silent skip). Grammar evolution is append-only, like
  the `.numos` command contract (GHOOK §11).
- **S-5 [P]** Emit never allocates in the draw path (emit happens at commit points only).

---

## 4. Display ↔ semantic mapping

### 4.1 Direction 1: display form → semantic form (at commit points)

The tree **is** the display form. Semantic derivation at ENTER/solve/plot:

```
evalAdmissible(root):                       # pre-flight, O(n)
    if any node lacks CAP_EVAL:      return DisplayOnly(worstNode)
    if unbounded binder (inf bound): return UnboundedDomain
    if depth > 24 or nodes > 4096:   return DepthExceeded
    else                             return None → MathEvaluator.evaluate(root)
```

CAS route: `ASTFlattener` (extended, MR-16) with the same admissibility pre-flight against
`CAP_CAS`; Giac route: NumIR-shaped text serializer per CGIAC §C.2 (refusal J.2 on unlisted
nodes — spec-aligned [V-spec]).

### 4.2 Direction 2: semantic form → display form

CAS/evaluator results come back as VPAM trees via `resultToAST/resultToPeriodicAST/
resultToExtendedAST` [V] (`MathEvaluator.h:116-147`) and `SymExprToAST/SymToAST/CasToVpam` [V].
v2 requirements: result trees pass through the same LayoutBudget (Y-4 caps `SymExprToAST` output —
closes C5.8) and must themselves satisfy S-1 (results are serializable — they contain no
display-only nodes except `NodePeriodicDecimal`, which serializes as `pdec`).

### 4.3 Semantics of specific constructs (explicit list per prompt)

| Construct | Semantic meaning | Binder rules |
|---|---|---|
| `sum(lo;hi;body)` / `prod` | Σ/Π over integer index; `lo` must be row `id(k) = start` (relation splits binder/start [V-analog: NodeSummation embeds "n=1" in lower, `MathAST.h:1207-1210`) | index shadows outer variable during body eval; never read from VariableManager |
| `integral(lo;hi;body;var)` | definite Riemann integral; `var` is the binder; numeric via Simpson [P1] | same shadowing |
| `lim(under)` + following group | binder `id(n) -> inf` in under-row; **prefix operator**: applies to the immediately following item in the row (layout spec §8.4); eval refused until F | binder-only |
| `matrix(...)` | matrix-of-ExactVal value; arithmetic via `MatrixEngine` ≤5×5 [P2] | — |
| `func(det; matrix)` | determinant scalar | arg must be square matrix or identifier bound to one |
| `binom(n;k)` | C(n,k) exact | integer args required for eval; else refusal Domain |
| `fact(x)` | Γ-free integer factorial; non-integer/negative → Domain error; >170 → overflow error; 21…170 → approximate flag [P1] | — |
| `bar(row)` | absolute value (semantic), not just fence [P1] | — |
| `floor/ceil` | integer part functions [P2] | — |
| implicit multiplication | juxtaposition in a row = multiplication [V] (`evalRow`, `MathEvaluator.cpp:589-597`); NumIR keeps rows verbatim, `*` explicit only where user typed × | — |
| `id(alpha)` etc. | variable lookup (extended VariableManager) or unbound-refusal | — |
| `inf` | admissible only as a large-op bound or lim target; anywhere else → refusal `UnboundedDomain` at pre-flight | — |
| `ellipsis(*)` | none — display-only (T-2) | — |
| prime/derivative notation | P2 display sugar; semantic form `func(diff; body; var)` produced by template only (notation spec §R) | — |
| `pow(base; exp)` vs `root` | distinct operators per D-3 [V-spec]: root glyph = real odd root, power form = principal; serializer keeps them distinct (`surd` vs `^` at the Giac boundary, CGIAC §C.2) | — |

### 4.4 Result-mode interaction [V→P]

The 3-state S⇔D result system (`ResultMode{Symbolic,Periodic,Extended}`,
`MathEvaluator.h:81-85`; rotation on FREE_EQ, `CalculationApp.cpp:785-799`) is unchanged; v2 adds
a 4th, non-rotating verdict chip when evaluation was refused (§6).

---

## 5. Special-question decisions owned by this doc

### 5.1 MRD-2 — Notation the evaluator cannot compute (Q10)

**Supported, explicitly.** The capability lattice admits Renderable∧¬Evaluable nodes. Pre-flight
`evalAdmissible` produces the typed refusal *before* any partial computation, so the user never
sees half-evaluated garbage. Refusals are not errors: distinct styling, no history-error entry —
the expression **is** stored in history (it round-trips via NumIR), only its "result" field holds
the verdict.

### 5.2 MRD-3 — Per-app behavior (Q11)

| App | Behavior on rich notation |
|---|---|
| CalculationApp | full editor; ENTER → pre-flight → evaluate / refusal chip ("display only" / "needs finite bounds" / "unbound variable α — STO to define") |
| GrapherApp | expression slots accept only plot-admissible subsets (function of x/y); pre-flight adds `PlotAdmissible` check; templates palette filtered accordingly; serializer to legacy pipeline replaced by NumIR→legacy adapter (MR-03), same 63-char legacy cap until GraphModel v2 (out of MRV2 scope) |
| EquationsApp | requires a top-level relation; capability pre-flight against the CAS route (`CAP_CAS`), refusal J.2 verbatim from flattener |
| CalculusApp / IntegralApp | already structure-first [V]; gains factorial/identifier support transparently (shared inserters) |
| MatricesApp | keeps `lv_table` editor [V] through P2; MR-19 (optional) adds "view as math" read-only MathCanvas rendering via NodeMatrix; full editor migration is F |

### 5.3 MRD-4 — Renderable-only / evaluable / CAS-transformable / decorative distinction (Q "should there be")

Yes — it **is** the capability lattice (CAP_RENDER/CAP_EVAL/CAP_CAS), plus `IdentKind::Styled`
covering "non-semantic decorative" (blackboard ℝ etc., notation spec §D: render+serialize only).
No fifth category is needed; decorative = Renderable∧Serializable∧¬Evaluable∧¬CAS.

### 5.4 MRD-5 — det/matrix duality (Q3)

`fence=bar` matrix display and `func(det; matrix(...))` both **canonicalize to
`func(det; matrix(fence=…, ...))` at the semantic boundary**, but NumIR preserves the authored
display (`matrix(bar; ...)` keeps its bars on round-trip; S-1 is display-faithful). Rationale:
semantics must be single (one det), display must be faithful (user's notation is not rewritten
under them).

### 5.5 Error-state surfacing (Q "how should error states be surfaced")

Three visually distinct classes in the result band [P]:

1. **Math error** (domain/div0/overflow): red-accent AST message (existing `errorToAST` path [V],
   `MathEvaluator.h:141`).
2. **Refusal** (DisplayOnly/UnboundedDomain/UnboundVariable/DepthExceeded): neutral gray chip with
   verbatim short reason; expression preserved; FREE_EQ disabled.
3. **Resource** (OOM/timeout, J.6/J.7): orange chip; `[MEM]`/`[CAS]` log correlation.

### 5.6 Preventing "pretty but meaningless" (Q13)

Mechanism stack (all four required): total capability function (T-1) → pre-flight refusal (T-2)
→ serializer refusal at CAS boundary (T-3, J.2) → round-trip S-1 in CI so no construct can exist
that renders but has no defined storage/semantics classification. A construct is *added* to the
system by adding its capability row first — the compiler forces the classification.

---

## 6. The master table (input → display → NumIR → semantics → support)

Legend: V=verified today, P1/P2/P3/F=phase, ⊘=refused by design.

| Input example (how typed) | Display | Canonical NumIR | Semantic meaning | Evaluator | CAS (SymExpr/Giac) | Notes |
|---|---|---|---|---|---|---|
| `1 ÷ 2 +` fraction key flow | ½+⅓ stacked | `frac(1;2)+frac(1;3)` | exact rational add | V (`5/6` [V] CI-asserted) | V | golden `calc_fraction_sum` [V] |
| SQRT key, 12 | √12 | `root(12)` | principal root, simplified | V (`2√3` [V] `MathEvaluator.cpp:460-501`) | V (`surd(12,2)`) | |
| SHIFT-root degree 3, −8 | ³√−8 | `root(-8;3)` | real odd root | V (=−2, D-3 [V]) | V `surd(-8,3)` | distinct from `pow(-8;frac(1;3))` |
| POW flow | x² | `pow(id(x);2)` | power | V | V | |
| Palette: abs | \|x\| | `bar(id(x))` | absolute value | P1 | V (SymExpr Abs [V] `SymExpr.h:80`) | renders today, evaluates P1 |
| Palette: factorial after `(4k)` | (4k)! | `fact(paren(4*id(k)))` | integer factorial | P1 (≤170) | P1 (Giac `factorial`) | **new node** |
| Palette: Σ | Σ_{k=0}^{10} body | `sum(id(k)=0;10;body)` | finite series | P1 (≤10⁴ iter, 5 s deadline) | P1 (`sum(f,k,0,10)`) | renders today [V], evaluates P1 |
| Σ with ∞ upper | Σ_{k=0}^{∞} | `sum(id(k)=0;inf;body)` | formal series | ⊘ UnboundedDomain | F (Giac) | renders P1 |
| Palette: ∫ with bounds | ∫₀¹ x² dx | `integral(0;1;pow(id(x);2);id(x))` | definite integral | P1 numeric (Simpson n=100 [V] cap) | V-path (SymIntegrate/Giac `integrate`) | renders today [V] |
| Palette: lim | lim_{n→∞} (…) | `lim(id(n)->inf) paren(…)` | limit binder | ⊘ (F: Giac `limit`) | F | **new node** P2 |
| Palette: matrix 2×2 paren | (α β / γ δ) | `matrix(paren;2;2;id(alpha);id(beta);id(gamma);id(delta))` | matrix value | P2 (numeric entries ≤5×5) | P2 (`[[..],[..]]`) | symbolic-entry det: CAS-only F |
| Palette: det of it | det(…) or \|…\| | `func(det; matrix(...))` | determinant | P2 | P2 | MRD-5 duality |
| Palette: binom | (n over k) | `binom(id(n);id(k))` | C(n,k) | P2 | P2 (`comb`) | **new node** |
| Greek page: α | α | `id(alpha)` | variable (assignable P2) | P2 / ⊘ UnboundVariable in P1 | P1 (SymVar) | renders P1 |
| CONST_PI key | π | `pi` | constant π | V | V | unchanged |
| Palette: ⋯ | ⋯ | `ellipsis(c)` | none (display) | ⊘ DisplayOnly | ⊘ J.2 | **new node** P2 |
| `2` `x` juxtaposition | 2x | `2*id(x)`? **No**: `2 id(x)` row | implicit multiplication | V [V] | V (explicit `*` at Giac boundary per CGIAC C.2) | NumIR preserves juxtaposition; serializers to CAS insert `*` |
| Palette: floor | ⌊x⌋ | `floor(id(x))` | floor | P2 | P2 | glyphs in subset [V] |
| **Exemplar A** (whole) | fig. 1 | §3.1 example | equation (relation) | ⊘ as relation; RHS with finite N evaluable P1 | Giac route F | render P1 gate MR-10 |
| **Exemplar B**: `[...]^{det(matrix)}` | superscripted bracket | `pow(bracket(…); func(det; matrix(paren;2;2;…)))` | power | P2 pieces | P2 pieces | composition free [V] |
| **Exemplar B**: √(1+cfrac tower ⋱) | nested | `root(1 + frac(pow(id(x);2); 1 + frac(…; 1 + ellipsis(d))))` | ⊘ DisplayOnly (ellipsis) | ⊘ | ⊘ J.2 | render P2 |
| **Exemplar B**: √(2+√(2+⋯)) | nested radicals | `root(2+root(2+root(2+ellipsis(c))))` | ⊘ DisplayOnly | ⊘ | ⊘ | render P2 |
| **Exemplar B** (whole Φ=…) | fig. 2 | full NumIR (test corpus file) | relation, display-only | ⊘ DisplayOnly | ⊘ | render P3 gate MR-18 |

---

## 7. CalculationApp UX contract for rich objects (Q11 detail)

- Palette entries show capability chips (§1.2); display-only entries are insertable but the chip
  pre-warns.
- ENTER on a refusal-class expression: result band shows the chip verdict; UP/DOWN history still
  navigates; the expression re-loads editable (existing history mechanics [V],
  `CalculationApp.cpp:807-864`).
- STO onto an identifier binds it in VariableManager (P2 Greek page).
- Nothing is ever auto-rewritten: the user's tree is immutable except by their edits (MRD-5
  display-faithfulness generalized).

---

*End of Document 4.*
