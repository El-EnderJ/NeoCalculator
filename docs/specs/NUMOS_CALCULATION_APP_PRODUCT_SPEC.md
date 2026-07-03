# NumOS — CalculationApp Product Specification

> **The product-level definition of "done" for CalculationApp.** Companion to
> `NUMOS_GRAPHER_APP_PRODUCT_SPEC.md`; evaluation-semantics ground rules inherit from
> `NUMOS_CAS_CORRECTNESS_AND_SEMANTICS_SPEC.md` (CAS §D contracts D.1–D.20, error taxonomy §J) and
> input/serialization rules from `NUMOS_MATH_INPUT_SEMANTICS_AND_SERIALIZATION_SPEC.md` (MINPUT).
> Renderer architecture: `NUMOS_MATHRENDERER_V2_ARCHITECTURE_SPEC.md` + MR-00…MR-20 tickets.
>
> Audit base: branch `claude/numos-core-apps-spec-9at366` @ `6da9012` (2026-07-03); source unchanged
> since the `8662f47`/`2d6b796` audits (docs-only commits since — verified via `git log`).
> Notation: **[V]** verified with `file:line`; **[P]** proposed normative; **PD-Cn** product decision
> recorded here; **OQ-Cn** open decision with options and consequences.

---

## A. Executive summary — what CalculationApp must be

CalculationApp is the calculator's home screen in spirit: the app a student lives in. The product
bar: **natural textbook-style input, exact-first results that are honest about approximation, a
three-mode S⇔D display that never mutates the underlying value, and errors that name their cause
and never eat the user's expression.**

"Polished" means, concretely:

1. **Input** (§C): every key does something predictable or is visibly inert by design. Today two
   keys are silently dead (`RPAREN`, `NEGATE` — §C.9/§C.10) and one documented feature (degree-n
   roots) has no entry path (§C.5) — all three get explicit dispositions.
2. **Evaluation** (§D): exact rational/radical/π-e arithmetic where representable; **every**
   non-exact value visibly marked `≈` (CAS NC-1 — today `fromDouble` results masquerade as exact
   fractions [V `MathEvaluator.cpp:107-149`, flag never set]).
3. **Display** (§E): Symbolic / Periodic / Extended modes cycle without mutating the stored result
   [V `CalculationApp.cpp:785-799`, `_lastResult` retained]; large-denominator auto-decimal stays
   a *display* heuristic (§E.4).
4. **Errors** (§F): a typed taxonomy replaces the single `"Math ERROR"` string; input is preserved;
   recovery is one keystroke.
5. **UX** (§G): after-ENTER behavior, Ans/PreAns rotation, history, AC/DEL are specified exactly —
   including today's surprising-but-kept behaviors (typing after a result *continues the same
   expression*, §G.2).
6. **QA** (§H + QA plan): the 8C semantic suite grows from 17 scripts to the full §H corpus; angle
   mode, radicals, π/e, errors beyond div-0, and display modes gain oracles (today none exist —
   QA dossier: no angle/trig/radical assertion anywhere in CI).

Out of scope this cycle: complex numbers (CAS D-2 decision pending), Giac-backed evaluation in this
app (not reached today [V — §B.3] and stays so), MathRenderer v2 node types (MR tickets own them),
`VariableManager`/`VariableContext` unification (ground-truth P-04).

---

## B. Current architecture and code-path map [V]

### B.1 Lifecycle & ownership

- Owned and pumped by `SystemApp`: constructed at boot (`SystemApp.cpp:105`), keys forwarded
  (`SystemApp.cpp:503-504`), ended on mode exit (`SystemApp.cpp:184`). No per-frame app loop —
  LVGL renders (`SystemApp.cpp:259-262`).
- `KeyCode::MODE` is intercepted by SystemApp before the app sees it (`SystemApp.cpp:500-505`) —
  HOME handling lives outside the app.
- `begin()` guards double-create and builds UI (`CalculationApp.cpp:88-93,141-186`); `end()`
  destroys canvases + StatusBar before the screen and nulls pointers (`CalculationApp.cpp:99-123`);
  `load()` lazy-begins + fades in (`CalculationApp.cpp:129-135`).

### B.2 Widget layout

Two `vpam::MathCanvas` widgets on a white screen: editable input + read-only result, plus a 1-px
separator (`CalculationApp.cpp:25-30,141-186`). Geometry: content area 308×209 below a 25-px
StatusBar band (`CalculationApp.cpp:49-59`). After ENTER the area splits content-proportionally
between input and result with `BAND_MIN_H=8` floors and invariant `inH+resH==200`
(`applyResultLayout`, `CalculationApp.cpp:593-701`).

### B.3 Evaluation stack

`ENTER → _evaluator.evaluate(_rootRow)` — the VPAM exact evaluator, AST tree-walk, **no string
round-trip** (`CalculationApp.cpp:529`; `MathEvaluator.h:109`; confirmed: `MathAST` has no
toString/serialize — input-stack dossier §5.1). Results are `ExactVal`
`(num/den)·outer√inner·π^piMul·e^eMul` with a double `approximate` escape (`ExactVal.h:39-56`).
**No CAS/Giac in the numeric path**: Giac calls are `#ifdef ARDUINO` inside MathEvaluator and never
invoked from this app (`MathEvaluator.h:94-100`, `MathEvaluator.cpp:508-515`); the only `cas::`
usage is the optional edu-steps display (`CalculationApp.cpp:36-38,911-1126`, gated by
`setting_edu_steps`, default false, `Config.h:79`).

### B.4 Stores

Ans/PreAns/A–F/x/y/z live in the `VariableManager` singleton (ExactVal, LittleFS `/vars.dat`,
`VariableManager.h:51-52`, `VariableManager.cpp:73-74,169-190`). The parallel `VariableContext`
(double, NVS) is **not used by this app** (grep-verified — Calculation dossier §5); it is the
Grapher's store. Cross-app variable visibility is therefore zero (UX spec, GC-140/141).

### B.5 Modifier layer

`vpam::KeyboardManager` singleton FSM: SHIFT/ALPHA/locks/STO (`KeyboardManager.h:47-54`,
`KeyboardManager.cpp:42-146`); CalculationApp drives it directly and SystemApp deliberately skips
its own global SHIFT/ALPHA handling for this app (`SystemApp.cpp:484-493`) — the split-brain
modifier risk is recorded in the UX spec.

---

## C. Input model

The complete key map is the switch at `CalculationApp.cpp:314-509` (Calculation dossier §2 is the
cell-by-cell reference). Contract per family:

### C.1 Digits & decimal point

[V] `NUM_0..9` and `DOT` → `clearResult(); _cursor.insertDigit(c)` (`CalculationApp.cpp:326-339`).
`insertDigit` appends into an adjacent `NodeNumber` or creates one; `'.'` is accepted via an
explicit exemption before the digit range test (`CursorController.cpp:320-345`, guard `:326`).
[P] Multiple-dot rejection stays inside `insertDigit` (Phase 8E guard) — corpus row CC-31 pins it.
Digit handling never uses `code - NUM_0` (non-contiguous enum, `KeyCodes.h:119-141`) — CI-guarded.

### C.2 Operators & unary minus

[V] ADD/SUB/MUL insert `NodeOperator` (`CalculationApp.cpp:342-344`; `CursorController.cpp:350-360`).
There is **no unary-minus key path**: neither `NEG` nor `NEGATE` has a case in the switch (dossier
§2 — default: ignored); users type `SUB`, and the evaluator accepts a leading `Sub`
(`MathEvaluator.cpp:570` errors only on lone leading *non-Sub* operators). `calc_semantic_negative`
documents this (`3-5 = -2` via SUB).
[P → CA-07] `NEGATE` becomes an alias for inserting `Sub` at expression/slot start (exact behavior
in the ticket); until then it stays visibly inert and the UX spec records the divergence.

### C.3 Fractions & division

[V] `DIV` inserts a **stacked fraction**, not `÷` (`CalculationApp.cpp:347`); VPAM capture: the
node left of the cursor moves into the numerator and the cursor lands in the denominator; with
nothing to capture, an empty two-slot fraction with cursor in numerator
(`CursorController.cpp:365-413`). Frozen behavior (goldens `calc_fraction_sum`).

### C.4 Powers

[V] `POW` captures the left operand as base and jumps the cursor into the exponent
(`CalculationApp.cpp:348`; `CursorController.cpp:418-456`). Capturable set [V impl]: Number, Paren,
Fraction, Power, Root, Function, LogBase, Constant, Variable (`CursorController.cpp:85-101`;
note the header comment lists a narrower set — doc drift, fixed by CA-11 docs pass).

### C.5 Roots

[V] `SQRT` → `insertRoot()`: square root only, empty radicand, **no capture**, **no degree slot
entry path** (`CalculationApp.cpp:349`; `CursorController.cpp:461-477`). SHIFT is not consulted for
SQRT (only SIN/COS/TAN have shift branches, `CalculationApp.cpp:353-379`), so the SerialBridge `R`
combo (SHIFT then SQRT, `SerialBridge.cpp:218`) currently inserts a plain square root.
**Gap**: `NodeRoot` supports a degree (evaluator handles degree n via `std::pow`,
`MathEvaluator.cpp:775-793`) but no UI can author one.
[P → CA-08] SHIFT+SQRT inserts a degree-n root (cursor in the degree slot); odd-degree negative
radicand follows CAS D-3 recommendation (a) — `³√−8 = −2` real root — replacing today's
NaN→`Math ERROR` (`MathEvaluator.cpp:787-793` general case).

### C.6 Trig & log functions

[V] SIN/COS/TAN insert `NodeFunction`; with SHIFT active → ArcSin/ArcCos/ArcTan
(`CalculationApp.cpp:353-379`). LN, LOG (base-10), LOG_BASE (subscript base, cursor lands in base)
(`CalculationApp.cpp:382-393`; `CursorController.cpp:961-1001`). Function arguments render inside
automatic parentheses that are not child nodes (`MathRenderer.cpp:1970-2001`; labels incl.
`sin⁻¹` UTF-8 superscript, `MathAST.cpp:701-712`).

### C.7 Variables, Ans, PreAns, STO

[V] Direct keys: VAR_X/VAR_Y, ANS (`'#'`), PREANS (`'$'`), ALPHA_A..F
(`CalculationApp.cpp:406-429`). ALPHA layer: NUM_1..6→A..F, VAR_X/Y→x/y, SOLVE→z
(`alphaKeyToVarName`, `CalculationApp.cpp:875-888`; its header comment mentions ENTER→Ans /
FREE_EQ→PreAns mappings that do **not** exist — stale doc [V `CalculationApp.cpp:869-872`]).
STO flow: STO sets pending state; next var key executes `executeStore` = **copy current Ans into
the variable** + `saveToFlash()`; any other key cancels (`CalculationApp.cpp:267-286,894-901`).
PD-C1 (recorded): STO stores *Ans*, not the on-screen unevaluated expression — Casio-like; kept.

### C.8 Parentheses

[V] `LPAREN` inserts an empty `NodeParen`, cursor inside (`CalculationApp.cpp:350`;
`CursorController.cpp:482-498`); the user leaves it with RIGHT. **`RPAREN` is ignored** (no case —
dossier §2), yet SerialBridge maps `)` to it (`SerialBridge.cpp:207-208`), so typing `)` on the
serial console silently does nothing.
[P → CA-07] `RPAREN` = "exit the innermost Paren/Function container to the right" (structural
close). Options considered: (a) exit-container [recommended: matches structural editing, cheap];
(b) insert a literal unmatched paren [rejected: VPAM has no unmatched-paren node]. Until CA-07,
inertness is documented.

### C.9 Deletion

[V] `DEL` → `clearResult(); _cursor.backspace()` (`CalculationApp.cpp:468`). Backspace semantics
(`CursorController.cpp:716-953`): digit-wise inside numbers; whole-node for
operators/constants/variables; **structure flatten** for Fraction/Power/Root/Paren/Function/LogBase
— only the primary slot's content survives (numerator kept, **denominator discarded**; base kept,
**exponent discarded**). Lossy by design; no undo exists (dossier §3.5).
PD-C2 (recorded): flatten-on-backspace is kept for this cycle (matches structural editors;
NumWorks does the same for fractions). OQ-C1: should DEL on a *structure boundary* first select/
highlight the structure and require a second DEL to destroy it (Casio-style guard)? Options:
(a) keep immediate flatten [zero work, risk of accidental loss]; (b) two-step delete [safer,
touches renderer highlight API `MathRenderer.h:103-108`, new goldens]. Needs human product call;
tickets reference OQ-C1.

### C.10 Clear

[V] `AC` → `clearResult(); resetExpression(); _historyIndex=-1; KeyboardManager.reset()`
(`CalculationApp.cpp:471-476`) — clears expression, result, error state, and modifier locks;
history *entries* are retained (only the index resets). CI-gated (`calc_semantic_ac_clear`).

### C.11 Cursor movement

[V] LEFT/RIGHT enter/exit containers with slot hopping (numerator→denominator, base→exponent,
logbase base→argument) and wrap at row ends (`CursorController.cpp:510-674`). UP/DOWN move
vertically **only** in Fraction (num↔den) and Power (base↔exp) (`verticalSibling`,
`CursorController.cpp:209-259`) — and in CalculationApp UP/DOWN are overloaded: once a result is
shown they navigate history instead (`CalculationApp.cpp:448-465`); LEFT/RIGHT are overloaded to
scroll the result canvas in Extended mode (`CalculationApp.cpp:432-447`).
[P] No change; the overloads are pinned by corpus rows (CC-88, CC-92).

### C.12 Templates / menus

[V] None exist in CalculationApp — no template modal, no function catalog (dossier: no such UI).
[P] The rich-notation template palette is MR-15 (MINPUT §1.2) and stays in the MathRenderer v2
roadmap, not this cycle. PD-C3: CalculationApp ships this cycle **without** a template palette;
the Grapher remains the only templated app.

### C.13 Editor caps

[V] **None**: no depth cap, no node cap, no length cap in the editor or evaluator (dossier §9 —
verified absence); the only bound is the renderer's `MAX_RENDER_DEPTH=12` visual clip (red "…"
rect, `MathRenderer.h:239`, `MathRenderer.cpp:1507-1514`), while layout recursion itself is
unguarded (`MathRenderer.cpp:737`; layout has no depth guard — input dossier §4.3).
[P → CA-03] Adopt MINPUT §1.2 caps: insertion refused past structural depth 16 and 512 nodes per
tree, with visible feedback; evaluator gets a defensive depth guard returning the new
`DepthExceeded` refusal (aligns RT-10). Until CA-03, deep-nesting behavior is undefined-but-
must-not-crash (stress corpus CC-101).

---

## D. Evaluation semantics

Normative value semantics are CAS §D (D.1 integers … D.20 display selection). This section pins
the CalculationApp-visible layer.

### D.1 Exact arithmetic [V]

Integer/rational add/sub/mul/div exact within int64 (`ExactVal.h:39-56`); gcd-reduced, den>0
(CAS D.2). Radicals: single `outer√inner` term, perfect-square extraction, `√2·√3=√6`
(`MathEvaluator.cpp:333-334,460-501`). π/e monomials via int8 exponents (`ExactVal.h:45-46`).

### D.2 Decimal honesty — the ≈ contract [P, flagship]

Sources of silent approximation today [V]: unlike-radical sums, division by radicals, non-integer
or |exp|>30 powers, sqrt of radical-carrying values — all route through `fromDouble` which builds a
≤10-digit fraction with `approximate=false` (`MathEvaluator.cpp:107-149,309-311,375-400,431-437,
500-502`). Consequence: `sin(1)` displays as `8414709848/10000000000`-derived decimals with no
marker (CAS D.4 calls this the single largest contract violation).
**[P → CA-01 / CAS-10]**: every such path sets `approximate=true`; display prefixes `≈` in all
three modes; Periodic-mode period detection is disabled for approximate values (a truncated
fraction fabricates a false period — CAS D.20 unacceptable list). Acceptance: corpus rows CC-40…46.

### D.3 Symbolic results [V]

Display forms: integer / stacked fraction / `k√m` / π-e monomial with sign normalization
(`resultToAST`, `MathEvaluator.cpp:1052-1191`). Sums of *unlike* exact species (e.g. `π+1`,
`√2+√3`) are **not representable** in one ExactVal and degrade to §D.2 approximations — with CA-01
they become honest `≈` decimals. OQ-C2: should NumOS keep exact *sums* (a small linear-combination
type or cas:: escalation)? Options: (a) accept ≈ degrade [recommended this cycle — honest and
cheap]; (b) extend ExactVal [large, collides with cas:: work]; (c) route to cas::SymExpr on degrade
[couples app to CAS perf]. Deferred; corpus pins (a).

### D.4 Trig/log [V + P]

Evaluated as doubles then re-exactified via fromDouble (§D.2 applies). Special-angle exactness
(`sin(30°)=1/2`) is a CAS D.11/ANGLE-* aspiration requiring degree-grid recognition — **not** in
this cycle's acceptance; the honest-≈ contract is. Domain errors per §F table.

### D.5 Angle mode [V bug → P]

`vpam::g_angleMode` is global, initialized RAD, **never assigned anywhere** (grep-verified;
`MathEvaluator.cpp:53`), while StatusBar displays it (`StatusBar.cpp:243-248`) and SystemApp keeps
a *separate* `_angleMode=DEG` pushed only into the legacy evaluators (`SystemApp.cpp:79,96-97`) —
three unsynced states (input dossier §7). CalculationApp trig is therefore RAD-locked under a
"RAD" badge (accidentally consistent) but there is **no UI to change mode at all** (SettingsApp has
no angle setting — grep-verified).
**[P → UX-01/CA-06]**: one authoritative mode = `vpam::g_angleMode`; SettingsApp gains the toggle;
StatusBar already reads it; Grapher syncs per CLASS §B angle rule (GR-02); the DEG→rad input /
rad→DEG output conversion machinery already works when the global is DEG
(`MathEvaluator.cpp:849-896`). Acceptance: corpus CC-56…58 + emulator `set_angle_mode` (GHOOK §D.4).

### D.6 Errors

§F. Error results set `ok=false` + message; `updateAns` is skipped for errors
(`CalculationApp.cpp:552-554`) but the history push is unconditional — **errors enter history but
not Ans** [V `CalculationApp.cpp:556-568`]. PD-C4: keep both (history is a log of attempts; Ans is
a value register); documented, corpus CC-73.

### D.7 CAS/Giac boundary

[V] Not reached from this app (§B.3). [P] unchanged this cycle. Edu-steps remain display-only
sugar over `cas::SymSimplify` passes (≤20, `CalculationApp.cpp:931-967`) with an FPU guard
(`:955-961`); F2 opens the viewer only when steps exist (`CalculationApp.cpp:316-323,979-1126`).

### D.8 Unsupported notation

[V] The AST can host node types the evaluator has no case for (`PeriodicDecimal` as input,
`DefIntegral`, `Summation`, `Subscript`, `BigOp`, relation OpKinds) — they hit the default →
`"Math ERROR"` (`MathEvaluator.cpp:517-535`; `MathAST.h:72-107`). None are insertable from this
app's keys today, so unreachable [V].
[P] When MR-07+ inserters land, the MINPUT pre-flight (`evalAdmissible` — typed refusals
DisplayOnly / UnboundedDomain / DepthExceeded, MINPUT §4.1) becomes the gate; refusals are **not**
errors (gray chip, expression preserved, no Ans write, MINPUT §5.1/5.5). This spec adopts that
contract verbatim for forward compatibility.

---

## E. Display / rendering contract

### E.1 Input rendering [V]

MathCanvas structural rendering, STIX Two Math 18/12/8 by script level
(`MathRenderer.h:159-164`, `MathRenderer.cpp:1500-1506`), baseline-oriented, TEXT style (compact
inline fractions, `CalculationApp.cpp:161`). Cursor = 3-px black bar, 500-ms blink; Empty slot =
8-px grey square (`MathRenderer.h:233-238`). Auto-scroll keeps the cursor visible; scroll never
goes positive (`MathRenderer.cpp:762-780`).

### E.2 Result rendering [V]

Same widget, read-only, mode-dependent AST (§E.3). Content-proportional band split with 8-px
floors (§B.2). Result colors and edu-steps styling per `CalculationApp.cpp:1044-1126`.

### E.3 Modes [V]

FREE_EQ cycles Symbolic → Periodic → Extended → Symbolic (`CalculationApp.cpp:785-799`):
- **Symbolic**: canonical exact form (`resultToAST`); approximate → scientific `m×10^e`
  (`MathEvaluator.cpp:1058-1093`).
- **Periodic**: repeating decimal with overline via 500-step long division
  (`resultToPeriodicAST`, `MathEvaluator.cpp:1198-1287`); irrationals 12 sig digits.
- **Extended**: 200 digits; π/e digits from 1000-digit PROGMEM tables
  (`resultToExtendedAST`, `MathEvaluator.cpp:1293-1359`; `Constants.h:52-101,149-159`);
  horizontally scrollable via LEFT/RIGHT (`CalculationApp.cpp:432-447`).
Mode toggles never mutate `_lastResult` [V, CAS D.20.3]. [P] Extended mode displays its digit
count (`(200 digits)` suffix chip) — CA-10; today the count is implicit.

### E.4 Large-denominator policy [V, pinned]

On ENTER, `ok && isRational && den ≥ 100000` defaults the *display* to Periodic
(`kDecimalDenThreshold`, `CalculationApp.cpp:532-549`; rationale comment `:534-540`). S⇔D back to
Symbolic recovers the exact fraction. PD-C5: threshold 10^5 is pinned as product behavior; CA-01
interacts (once ≈-marked, approximate values skip period detection and show plain decimals).

### E.5 MathRenderer failure states

[V] Depth>12 → red translucent rect + red "…" (`MathRenderer.cpp:1507-1514`) — visible, non-fatal.
Missing large delimiters → vector-stroked fallback (`MathRenderer.cpp:59-84,430`). No node/width
caps; layout recursion unguarded (CA-03 adds the editor-side cap so the renderer bound is never
the first line of defense).
[P] Tofu policy: any newly reachable glyph must be added to the font subset + regenerated
(risk REN-2); no screen may ship a placeholder box.

### E.6 Clipping/scrolling policy

[V] Input: cursor-following horizontal scroll (§E.1); no vertical viewport (tall expressions
overflow the band — bounded in practice by nothing until CA-03; MR-17 adds vertical scroll later).
Result: `scrollBy` only wired in Extended mode [V `CalculationApp.cpp:432-447`].
[P → CA-10] LEFT/RIGHT scroll the result canvas in **all** modes when the result is wider than the
canvas (a long Symbolic fraction can clip today); scroll resets on mode change
(`CalculationApp.cpp:729,854` already reset).

### E.7 Observed rendering reference

The repo's reference screenshot `info/Calculation/Calculation Example of use case Screen.png`
(85×6×(9 → 4590) shows a detached oversized open-paren against inline factors — treat as the
canonical "function/paren spacing needs an audit" exhibit for CA-02 (image may predate current
renderer; CA-02 step 1 is reproducing it on current source before changing anything).

---

## F. Error model

### F.1 Current [V]

One string dominates: `"Math ERROR"` via `ExactVal::makeError` (`ExactVal.h:79`,
`MathEvaluator.cpp:175-180`), rendered as a text-bearing NodeNumber row (`errorToAST`,
`MathEvaluator.cpp:1412-1422`). Distinct: `"FACT ERROR"` (`MathEvaluator.cpp:1431-1435`).
Trigger inventory (Calculation dossier §8): empty/dangling rows (`MathEvaluator.cpp:543,570,576,
605`), div-by-zero (`:355-357`), sqrt/even-root of negative (`:458-468,797`), tan pole (`:874`),
arcsin/arccos domain (`:882,889`), ln/log/logBase domains (`:910,927,950-951`), `0^0`/`0^neg`
(`:406,413`), NaN/Inf guards (`:109-111,396-398,419-421,433-435,800-802,970-971`), bad variable
name (`:999-1001`).

### F.2 Target taxonomy [P → CA-04]

Adopt the CAS §J classes with CalculationApp-visible strings (English, short, stable for asserts):

| Class | Display string | Triggers (from F.1) | Ans | History |
|---|---|---|---|---|
| Syntax | `Syntax ERROR` | empty row, dangling operator, empty number, empty function arg | no | yes |
| Divide by zero | `Divide by 0` | exactDiv zero, fraction den 0, `0^neg` | no | yes |
| Domain | `Domain ERROR` | sqrt/root neg, tan pole, arc domain, ln/log ≤0, logBase base ≤0/=1 | no | yes |
| Overflow | `Overflow` | NaN/Inf guards, int64 blowout beyond approximate escape | no | yes |
| Unsupported | `Not supported` | evaluator default case (unhandled node), `0^0` | no | yes |
| Refusal (future) | gray chip per MINPUT §5.5 | pre-flight verdicts | no | yes (expression only) |
| Factor | `FACT ERROR` (kept) | factorize non-integer/<2 | no | yes |

Rules: (1) the error band styles distinctly from results (red accent — MINPUT §5.5 class 1);
(2) the input expression is always preserved and editable (already true [V] — §G.4); (3) exact
strings live in one table (mirrors GR-17) and are semantic-assertable via `assert_error <substr>`
[V grammar exists, `NativeHal.cpp:1563-1575`].

### F.3 Depth/node cap, OOM, timeout, internal bug [P]

- **Depth/node cap**: CA-03 editor caps (16/512) + evaluator guard → `Depth exceeded` refusal
  chip; never a crash. Today: unbounded recursion [V dossier §9] — stack overflow is a real
  hazard on the 64-KB loop task for adversarial trees (reliability RT-10 owns the firmware side).
- **OOM**: MathAST nodes allocate PSRAM-first with heap fallback (`MathAST.cpp:50-60`); allocation
  failure today throws/aborts unhandled. [P] RT-11/MR-01 observability applies; CalculationApp's
  contract: OOM during layout/eval → orange resource chip (MINPUT §5.5 class 3), expression
  preserved, `[MEM]` log.
- **Timeout**: no evaluation deadline exists [V]. [P] Not added this cycle for the numeric
  evaluator (bounded by CA-03 caps); the CAS/Giac deadlines are RT-09/CAS-05 scope.
- **Internal bug**: any unexpected evaluator state must resolve to `Not supported` rather than a
  wrong number; differential tests vs cas::/Giac (CAS-11/CAS-04) are the detection net.

---

## G. UX contract

### G.1 After ENTER [V, pinned]

Evaluate → default display mode heuristic (§E.4) → Ans update (ok only) → unconditional history
push (cap 50) → cursor hidden (canvas rebound without cursor) → optional edu-steps → band split +
result shown (`evaluateExpression`, `CalculationApp.cpp:525-584`).

### G.2 Typing after a result [V, pinned — the load-bearing surprise]

Any input key first calls `clearResult()`, which hides the result band and **rebinds the same
`_rootRow` for editing** — it does *not* reset the expression (`CalculationApp.cpp:750-779`,
early-return `:751`, no `_rootNode` reset). So after `2+2 ⏎`, pressing `+1` yields `2+2+1`, NOT
`Ans+1`. To chain on the result the user types `Ans` explicitly; to start fresh, `AC`.
PD-C6 (recorded, deliberate): this "continue editing the same expression" model is **kept** —
it is CI-documented (`calc_semantic_ans_chain` header) and structurally honest for a tree editor.
The alternative (Casio-style implicit `Ans` continuation when the first key is a binary operator)
is recorded as OQ-C3 with consequences: (a) keep [zero risk, mild surprise for Casio migrants];
(b) implicit-Ans on leading operator [muscle-memory match; requires distinguishing "fresh typing"
state; changes two gated scripts + goldens]. Human call; default (a).

### G.3 Ans/PreAns [V]

`updateAns` rotates Ans→PreAns, writes new Ans (`VariableManager.cpp:103-106`); full ExactVal
precision (`MathEvaluator.cpp:995-1004`). **Not persisted** on update — only STO calls
`saveToFlash` [V dossier §4]; OQ-C4: persist Ans on every ENTER? Options: (a) no [current; flash
wear-friendly]; (b) yes [session continuity across reboots; LittleFS wear acceptable at human
ENTER rates]. Recommendation (b) *deferred* to CA-05 with wear math; corpus pins current (a).

### G.4 Error recovery [V]

Error shows in the result band; expression retained; any edit key resumes editing (§G.2);
AC full-resets; DEL clears result + backspaces. Errors never write Ans (§D.6).

### G.5 AC/DEL

§C.9/§C.10; cross-app consistency table in the UX spec (AC in Grapher is focus-retreat, not
clear — deliberate divergence recorded there).

### G.6 History [V]

`UP` from a result (or while navigating) walks older entries, `DOWN` newer
(`CalculationApp.cpp:448-465,807-864`); entries reload **editable** clones with their result;
`_historyIndex=-1` = live expression. Cap 50 FIFO (`CalculationApp.h:136-138`). RAM-only, lost on
app exit (`end()` clears — `CalculationApp.cpp:120-122`); NumIR-based persistence is MR-03+
territory, not this cycle.

---

## H. Edge-case corpus (Calculation)

Row ids CC-nn are stable for scripts. Columns: **Expected** = the [P] contract (equals current
behavior unless flagged); **Display** = Symbolic-mode display unless noted; **Oracle**:
A = `assert_result[_contains]` / `assert_error` / `assert_variable` (all exist today,
`NativeHal.cpp:1543-1595`), G = golden, M = manual/HIL, H = host-unit (future MR-02 harness);
**Status**: `gated` (in CI now — QA dossier §3), `pass-V` (verified in source, no script),
`bug` (known contract violation), `untested`, `future` (needs ticket).
Assertion caveat [V]: `formatExactVal` in the emulator asserts exact **integers/fractions only**;
radical/π/e results are best-effort and excluded from exact assertion (`NativeHal.cpp:1628-1663`)
— rows needing them are marked `A†` (requires the QA plan's `assert_result_exact` extension, QA-03).

### H.1 Integer arithmetic (10)

| # | Input | Expected | Display | Oracle | Status |
|---|---|---|---|---|---|
| CC-01 | `1+2` | 3 | `3` | A | gated (`calc_1_plus_2` + golden) |
| CC-02 | `2+3*4` | 14 | `14` | A | gated |
| CC-03 | `(2+3)*4` | 20 | `20` | A | gated |
| CC-04 | `(2+3)*(4-1)` | 15 | `15` | A | gated |
| CC-05 | `3-5` | −2 | `-2` | A | gated |
| CC-06 | `2^10` | 1024 | `1024` | A | gated |
| CC-07 | `85*6*9` | 4590 | `4590` | A | untested (reference image case) |
| CC-08 | `12345*6789` | 83810205 exact | integer | A | untested |
| CC-09 | `7*0` | 0 | `0` | A | untested |
| CC-10 | `2^30` | 1073741824 exact (≤30 exact-power cap, `MathEvaluator.cpp:431-437`) | integer | A | untested |

### H.2 Fractions (12)

| # | Input | Expected | Display | Oracle | Status |
|---|---|---|---|---|---|
| CC-11 | `1/2+1/3` | 5/6 | stacked fraction | A+G | gated |
| CC-12 | `6/4` | 3/2 | fraction | A | gated |
| CC-13 | `1+1/2` | 3/2 | fraction | A | gated |
| CC-14 | `1/2+1/2` | 1 | integer | A | gated |
| CC-15 | `1/2` STO flow `2+2⏎` STO x | x=4 | — | A | gated (`calc_store_variable`) |
| CC-16 | `1/3*3` | 1 | integer | A | untested |
| CC-17 | `(1/2)/(3/4)` | 2/3 | fraction | A | untested |
| CC-18 | `-1/2+1/4` (SUB first) | −1/4 | sign in numerator (CAS D.2) | A | untested |
| CC-19 | `22/7` | 22/7 | fraction (den<10^5 stays Symbolic) | A | untested |
| CC-20 | `1/2-1/2` | 0 | `0` | A | untested |
| CC-21 | `(1+1/2)/(1-1/4)` | 2 | integer | A | untested |
| CC-22 | `1/99999` vs `1/100000` | den threshold: first Symbolic, second auto-Periodic (`CalculationApp.cpp:541-549`) | fraction vs repeating decimal | A+G | untested — pins PD-C5 |

### H.3 Decimals (8)

| # | Input | Expected | Oracle | Status |
|---|---|---|---|---|
| CC-23 | `0.5` | 1/2 exact (CAS D.3) | A | gated (`calc_semantic_decimal_fraction`) |
| CC-24 | `0.1+0.2` | 3/10 exactly (never 0.30000000000000004) | A | untested — flagship exactness row |
| CC-25 | `0.125*8` | 1 | A | untested |
| CC-26 | `3.14*2` | 157/25 | A | untested |
| CC-27 | `1.5^2` | 9/4 | A | untested |
| CC-28 | `2.5+2.5` | 5 | A | untested |
| CC-29 | 11-digit decimal input | beyond 10-digit fromDouble cap → must be `≈`-marked [P CA-01] (today unmarked, `MathEvaluator.cpp:139-144`) | A | bug |
| CC-30 | `0.333333333333` (12 digits) | ≈ 0.333… decimal, marked | A | bug |
| CC-31 | `1.2.3` entry | second `.` rejected by insertDigit guard; expression stays `1.23`-free | A | untested |

### H.4 Powers, roots, radicals (15)

| # | Input | Expected | Display | Oracle | Status |
|---|---|---|---|---|---|
| CC-32 | `√12` | 2√3 | radical glyph | A† | pass-V (`MathEvaluator.cpp:460-501`), no script |
| CC-33 | `√8` | 2√2 | radical | A† | untested |
| CC-34 | `√(4/9)` | 2/3 | fraction | A | untested |
| CC-35 | `√0` | 0 | `0` | A | untested |
| CC-36 | `√2*√3` | √6 | radical | A† | pass-V (`:333-334`) |
| CC-37 | `√2*√2` | 2 | integer | A | untested |
| CC-38 | `2^-2` | 1/4 | fraction | A | untested |
| CC-39 | `5^0` | 1 | integer | A | untested |
| CC-40 | `2^0.5` | ≈1.414213562 **marked** [P CA-01] | `≈` decimal | A | bug (unmarked today) |
| CC-41 | `√2+√3` | ≈3.146264370 marked | `≈` decimal | A | bug (silent fromDouble, `:309-311`) |
| CC-42 | `1/√2` | ≈0.7071… marked (division by radical degrades, `:375-377`) — OQ: rationalize to √2/2 instead? recorded in CA-01 | A | bug |
| CC-43 | `2^31` | 2147483648 (double-exact by luck; must be ≈-marked once CA-01 lands since it exits the exact-power path `:431-437`) | A | bug (contract) |
| CC-44 | `2^62+2^62` | ≈9.223372037e18 scientific, approximate escape (`ExactVal.h:51-56`, `MathEvaluator.cpp:116-123`) | scientific | A | pass-V |
| CC-45 | `0^0` | error (Unsupported class [P]; `Math ERROR` today, `:406`) | error band | A | pass-V |
| CC-46 | `³√−8` | [V] `Math ERROR` (`:787-793`) → [P CA-08/D-3] −2 | — | A | future |

### H.5 Trig (10) — all RAD until CA-06; angle rows marked

| # | Input | Expected | Oracle | Status |
|---|---|---|---|---|
| CC-47 | `sin(0)` | 0 | A | untested |
| CC-48 | `sin(30)` | RAD: ≈−0.988031624 (the documented huge-den case, `CalculationApp.cpp:534-540`) — auto-Periodic display | A | untested |
| CC-49 | `cos(0)` | 1 | A | untested |
| CC-50 | `tan(0)` | 0 | A | untested |
| CC-51 | `arcsin(1)` (SHIFT+SIN) | RAD: ≈1.570796327 marked [P]; rad→DEG output conversion when DEG (`MathEvaluator.cpp:884-896`) | A | untested |
| CC-52 | `arcsin(2)` | Domain error | A (`assert_error`) | untested |
| CC-53 | `sin(π)` | [P] 0 exact is the D.11 aspiration; current: double sin of exact-π monomial → tiny fraction — pin current, tag `expect:approx` | A | untested — needs value pin at implementation |
| CC-54 | `tan(π/2)` | Domain error iff |cos|<1e-15 triggers (`:874`) — pin actual at implementation | A | untested |
| CC-55 | `sin(sin(1))` | ≈ nested, marked [P] | A | bug (unmarked) |
| CC-56 | DEG mode `sin(30)` | 1/2-adjacent decimal (conversion works when mode set, `:849-861`) — **blocked on CA-06 UI + `set_angle_mode` hook** | A | future |
| CC-57 | DEG mode `arcsin(1)` | 90 | A | future |
| CC-58 | mode change does not alter stored Ans | Ans unchanged by display/mode | A | future |

### H.6 Logs (8)

| # | Input | Expected | Oracle | Status |
|---|---|---|---|---|
| CC-59 | `ln(1)` | 0 | A | untested |
| CC-60 | `log(100)` | 2 | A | untested |
| CC-61 | `log_2(8)` | 3 | A | untested (render-only script exists — `calc_logbase_smoke`, warning-only) |
| CC-62 | `log_2(1024)` | 10 | A | untested |
| CC-63 | `ln(0)` | Domain error (`:910`) | A | untested |
| CC-64 | `log(-5)` | Domain error (`:927`) | A | untested |
| CC-65 | `log_1(5)` | Domain error (base=1, `:950-951`) | A | untested |
| CC-66 | `ln(e)` | [P] 1 exact (eMul path) — pin actual at implementation; if approx today, `expect:approx` until CAS D.12 work | A | untested |

### H.7 Constants (6)

| # | Input | Expected | Display | Oracle | Status |
|---|---|---|---|---|---|
| CC-67 | `π` | π exact monomial | π glyph | A† | pass-V |
| CC-68 | `2*π` | 2π | `2π` | A† | untested |
| CC-69 | `π+π` | 2π | `2π` | A† | untested |
| CC-70 | `π+1` | ≈4.141592654 marked [P] (unlike-species sum degrades) | `≈` decimal | A | bug (unmarked) |
| CC-71 | `e` | e monomial | `e` | A† | pass-V |
| CC-72 | `π` Extended | 200 digits from PROGMEM (`Constants.h:52-101`) | scrollable digits | G | untested |

### H.8 Ans / PreAns / STO / variables (10)

| # | Scenario | Expected | Oracle | Status |
|---|---|---|---|---|
| CC-73 | `2+2⏎` then `Ans+1⏎` | 5 | A | gated (`calc_semantic_ans_chain`) |
| CC-74 | two results then `PreAns⏎` | penultimate value | A | gated (`calc_semantic_preans`) |
| CC-75 | `1/0⏎` then `Ans⏎` | Ans = previous ok value (errors skip Ans, `CalculationApp.cpp:552-554`) | A | untested — pins PD-C4 |
| CC-76 | STO→A stores Ans; `A+1` | Ans+1 | A | untested (x covered by gated script) |
| CC-77 | ALPHA,NUM_1 inserts A | variable renders italic `A` | A+G | untested |
| CC-78 | unset variable `B⏎` | 0 (zero-init store) | A | untested — pin |
| CC-79 | STO then non-var key | STO cancels, key falls through (`CalculationApp.cpp:283-285`) | A | untested |
| CC-80 | STO persists across app exit/re-enter | `/vars.dat` reload (`VariableManager.cpp:169-190`) | A | untested (emulator FS) |
| CC-81 | Ans NOT persisted without STO after reboot-equivalent | current [V]; flips if OQ-C4(b) | A | untested — pins OQ-C4 |
| CC-82 | fraction stored exact: `1/3⏎` STO C, `C*3⏎` | 1 exactly | A | untested |

### H.9 Errors & recovery (10)

| # | Input | Expected | Oracle | Status |
|---|---|---|---|---|
| CC-83 | `1/0` | Divide-by-0 class [P]; `Math ERROR` today | A | gated (`calc_error_div_by_zero`) |
| CC-84 | `√(-4)` | Domain | A | untested |
| CC-85 | `2+⏎` | Syntax (dangling operator, `:605`) | A | untested |
| CC-86 | `⏎` on empty | Syntax (empty row, `:543`) | A | untested |
| CC-87 | error then digit | result clears, same expression continues editing (§G.2) | A | untested |
| CC-88 | error then UP | history navigation includes the error entry [V unconditional push] | A | untested — pins PD-C4 |
| CC-89 | error then AC | full reset, no error remnant | A | gated (ac_clear covers the pattern) |
| CC-90 | `FACT` on 3/2 result | `FACT ERROR` (distinct string, `:1431-1435`) | A | untested |
| CC-91 | `FACT` on 12 | 2²×3 AST (factorizeToAST) | A†+G | untested |
| CC-92 | LEFT/RIGHT on error result | no scroll crash; cursor ops resume edit | A | untested |

### H.10 Display modes & history (12)

| # | Scenario | Expected | Oracle | Status |
|---|---|---|---|---|
| CC-93 | `1/3` FREE_EQ | Periodic `0.3̄` (overline node) | G+A† | untested |
| CC-94 | `1/7` Periodic | 6-digit repeating block via longDivision(500) | G | untested |
| CC-95 | FREE_EQ ×3 returns to Symbolic | 3-cycle, value unmutated | A | untested |
| CC-96 | `2/3` Extended | 200 digits, LEFT/RIGHT scrolls (`CalculationApp.cpp:432-447`) | G | untested |
| CC-97 | Extended scroll then FREE_EQ | scroll resets (`:729,854`) | G | untested |
| CC-98 | 51 evaluations | oldest history entry dropped (cap 50, `CalculationApp.h:138`) | A | untested (stress script) |
| CC-99 | UP UP DOWN | correct entry order, editable reload (`:807-864`) | A | untested |
| CC-100 | history entry re-ENTER | re-evaluates; new history entry; Ans rotates | A | untested |
| CC-101 | 20-deep nested fractions | [V] undefined (no cap; renderer clips at 12 with red "…") → [P CA-03] insert refused at 16 with feedback | G+M | future — stress |
| CC-102 | very wide expression (40 digits) | input auto-scroll keeps cursor visible (`MathRenderer.cpp:762-780`) | G | untested |
| CC-103 | result wider than canvas in Symbolic | [V] clips → [P CA-10] scrollable | G | bug (minor) |
| CC-104 | rapid typing 20 keys/s (script `wait 0`) | no dropped/reordered input; final expression exact | A | untested — stress |

### H.11 Editing structural semantics (8)

| # | Scenario | Expected | Oracle | Status |
|---|---|---|---|---|
| CC-105 | `1 DIV` | 1 captured into numerator, cursor in denominator (`CursorController.cpp:365-413`) | A (result after completing) | gated-adjacent (fraction_sum exercises) |
| CC-106 | `(2+3) POW 2` | paren captured as base → 25 | A | untested |
| CC-107 | DEL on fraction bar from right | flatten: numerator survives, denominator discarded (`CursorController.cpp:774-800`) | A | untested — pins PD-C2 |
| CC-108 | DEL on power from right | base survives, exponent discarded (`:802-826`) | A | untested |
| CC-109 | UP/DOWN inside root radicand | no-op (verticalSibling wires only Fraction/Power, `:209-259`) | A | untested — documented limitation |
| CC-110 | RIGHT at expression end | cursor wraps to start (`:510-585`) | A | untested — pin (surprising; UX spec reviews) |
| CC-111 | `RPAREN` key | [V] inert → [P CA-07] exits container right | A | future |
| CC-112 | `NEGATE` key | [V] inert → [P CA-07] leading Sub alias | A | future |

**Corpus count: 112 rows** (CC-01…CC-112). `bug` rows (CC-29/30/40/41/42/43/55/70/103) are tracked
expected-fails until CA-01/CA-10 land; `future` rows name their ticket.

---

## I. Acceptance definition — when Calculation is "polished"

All must hold, in CI where an oracle exists:

1. **Corpus green**: every CC row `gated`/`pass-V`/`untested` is scripted and passing; every `bug`
   row flipped by its ticket; every `future` row either landed or explicitly re-scheduled.
2. **≈ honesty**: no value produced through any `fromDouble` degrade path displays without `≈`
   (CA-01); Periodic mode never fabricates a period for approximate values.
3. **Error taxonomy**: five typed classes render distinctly, are substring-asserted, and never
   destroy input (CA-04).
4. **No dead keys**: RPAREN/NEGATE resolved (CA-07); degree-root entry exists (CA-08) or is
   explicitly deferred with the SQRT+SHIFT combo documented as square-root-only.
5. **Angle mode**: one global state, settable in Settings, badge truthful, DEG trig correct
   (CA-06/UX-01); emulator `set_angle_mode` hook gated.
6. **Caps**: depth-16/512-node editor caps with visible feedback; no input can crash or hang the
   app (CA-03); 20-deep stress row passes.
7. **Goldens**: input edit, fraction result, radical result, Periodic overline, Extended digits,
   error band, steps viewer — each has one blessed golden (QA plan §D list).
8. **Determinism**: all Calculation scripts byte-reproducible under `--deterministic` (existing
   SHA-256 double-run stays green, `emulator-build.yml:228-236`).

---

## J. Implementation tickets (CA-*)

Full fields (files, rollback, owner tier) in `NUMOS_CORE_APPS_IMPLEMENTATION_ROADMAP.md`; summary:

- **CA-01 — Exact/approx display hardening (≈ contract)** — implements §D.2; equals CAS-10 scope
  for this app. Files: `MathEvaluator.cpp` (set `approximate` on every degrade path),
  `resultTo*AST` (≈ prefix; skip period detection for approx), corpus flips CC-29/30/40/41/42/43/
  55/70. Risk: medium (display churn → golden wave).
- **CA-02 — Function-call & delimiter rendering polish** — reproduce §E.7 exhibit; audit function
  auto-paren metrics (`MathRenderer.cpp:1970-2001`) and stroked-delimiter fallback weights
  (`:59-84`); coordinate with MR-06/MR-11 (do not fork glyph work). Risk: medium (goldens).
- **CA-03 — Editor depth/node caps + evaluator guard** — §C.13/§F.3; MINPUT §1.2 caps (16/512),
  visible refusal, evaluator defensive depth check; aligns RT-10, replaces "no cap" [V].
- **CA-04 — Error taxonomy** — §F.2 table; one string table; `assert_error` substrings; history/Ans
  rules unchanged.
- **CA-05 — Variable/Ans/PreAns persistence policy** — §G.3/OQ-C4; decide Ans persistence, add
  `updateAns` flash policy + wear note; document error-Ans asymmetry (PD-C4); serialize
  `approximate` flag (today dropped on save — `VariableManager.cpp:143-163`, a silent
  exactness-laundering bug once CA-01 lands).
- **CA-06 — Angle mode UI + single source of truth** — §D.5 with UX-01; SettingsApp toggle writes
  `vpam::g_angleMode`; retire `SystemApp::_angleMode` divergence (`SystemApp.cpp:79,96-97`).
- **CA-07 — Dead-key dispositions (RPAREN, NEGATE)** — §C.8/§C.2.
- **CA-08 — Degree-n root entry + odd-root semantics (D-3)** — §C.5.
- **CA-09 — History polish** — cap/pinning scripts (CC-98…100); optional NumIR persistence stays
  MR-03.
- **CA-10 — Result scrolling in all modes + Extended digit chip** — §E.3/§E.6.
- **CA-11 — Calculation corpus execution (CC-01…CC-112)** — scripts + candidates + goldens per QA
  plan batching; includes the doc-drift fixes flagged by the dossiers (stale comments at
  `CalculationApp.cpp:869-872`, `CursorController.h:243`).
- **CA-12 — Auto-Periodic threshold pinning** — PD-C5 script pair CC-22; docs.

Order: CA-11 batch 1 (pin current behavior) → CA-04 → CA-01 → CA-03 → CA-06 → CA-07/08 →
CA-10/12/05/09 → CA-02 (golden wave last). Cross-app sequencing in the roadmap doc.

---

*End of Calculation product spec.*
