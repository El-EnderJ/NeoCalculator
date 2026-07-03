# NumOS — Core Apps UX & Input Consistency Specification

> Defines what must be **the same** between CalculationApp and GrapherApp (and, where relevant, the
> launcher/SystemApp layer), what is **deliberately different**, and what is **accidentally
> different and must converge**. Companion to the two product specs
> (`NUMOS_CALCULATION_APP_PRODUCT_SPEC.md`, `NUMOS_GRAPHER_APP_PRODUCT_SPEC.md`).
>
> Audit base: `claude/numos-core-apps-spec-9at366` @ `6da9012` (2026-07-03).
> Notation: **[V]** verified `file:line`; **[P]** proposed; **PD-Un** decision recorded here;
> **OQ-Un** open question. Tickets UX-01… defined in §J and expanded in the roadmap doc.

---

## A. Principles

1. **Same key, same meaning** wherever both apps accept the key; divergence is allowed only when
   the *context* differs in kind (e.g. AC as "back" in a tabbed app vs "clear" in a single-screen
   app) and must be recorded as a PD with rationale.
2. **One modifier truth**: SHIFT/ALPHA/STO state has exactly one owner (`vpam::KeyboardManager`)
   and one indicator (StatusBar).
3. **One angle-mode truth**: `vpam::g_angleMode`, settable in Settings, displayed by StatusBar,
   synced into every legacy `Evaluator` instance at evaluation entry (CAS D.19, CLASS §B).
4. **Errors speak one language** (English, short, typed classes) across apps.
5. **Nothing is silently inert**: a key either acts, or its inertness is a documented design fact
   per app context.

---

## B. Current keymap table [V]

Contexts: **Calc** = CalculationApp main editor (`CalculationApp.cpp:314-509`); **Gr-edit** =
Grapher expression editing (`GrapherApp.cpp:2513-2633`); **Gr-graph** = Graph tab
NAVIGATE/TRACE (`GrapherApp.cpp:2700-2866`); **Gr-other** = tab bar/toolbar/list/table/modals.
"—" = ignored (falls to default).

| Key | Calc | Gr-edit | Gr-graph | Gr-other |
|---|---|---|---|---|
| NUM_0..9 | insertDigit | insertDigit | — | — |
| DOT | insertDigit('.') | insertDigit('.') | — | — |
| ADD | Op Add | Op Add | zoom in (NAV `GrapherApp.cpp:2709`) | — |
| SUB | Op Sub | Op Sub | zoom out (`:2719`) | — |
| MUL | Op Mul | Op Mul | — | — |
| DIV | **fraction** (`CalculationApp.cpp:347`) | **fraction** (`GrapherApp.cpp:2580`) | — | — |
| POW | power+capture | power (depth-capped) | — | — |
| SQRT | root (square only) | root (`:2582`) | — | — |
| LPAREN | paren | paren (`:2583`) | — | — |
| RPAREN | **inert** | **inert** (no case) | — | — |
| SIN/COS/TAN | func; **SHIFT→arc** (`CalculationApp.cpp:353-379`) | func; **no SHIFT layer** (`GrapherApp.cpp:2586-2588`) | — | — |
| LN / LOG | func (`:382-389`) | func (`:2590-2591`) | — | — |
| LOG_BASE | insertLogBase (`:390-393`) | insertLogBase (`:2593`) | — | — |
| CONST_PI / CONST_E | constant (`:396-403`) | constant (`:2611-2612`) | — | — |
| VAR_X / VAR_Y | variable (`:406-413`) | variable (`:2596-2597`) | — | — |
| ANS / PREANS | variable `#`/`$` (`:414-421`) | **inert** (no case) | — | — |
| ALPHA_A..F | variable A–F (`:424-429`) | **inert** | — | — |
| SHIFT / ALPHA | KeyboardManager FSM (`:257-266`) | **inert** (no modifier consult) | — | — |
| STO | store-pending → var (`:267-286`) | **inert** | — | — |
| FREE_EQ | S⇔D cycle (`:485-490`) | inserts `=` (`GrapherApp.cpp:2601`) | — | — |
| LESS / GREATER | **inert** | inserts `<`/`>` (`:2607-2608`) | — | — |
| NEG / NEGATE | **inert** (type SUB) | Op Sub (`:2576-2577`) | — | — |
| LEFT/RIGHT | cursor; Extended-scroll overload (`:432-447`) | cursor; RIGHT-on-empty opens Templates (`:2534-2542`) | pan / trace-step | tab/tool/step/visibility (per context) |
| UP/DOWN | cursor; history overload (`:448-465`) | cursor | pan / trace fn-switch | selection / table scroll |
| DEL | backspace (`:468`) | backspace (`:2621`) | — | delete function row (list, `:2497`) |
| AC | full clear (`:471-476`) | clear expression (`:2523-2531`) | trace→NAV→toolbar retreat (`:2748-2753,2838-2851`) | focus retreat chain (`:2440-2447,2897-2900`) |
| ENTER | evaluate (`:479-482`) | commit (stopEditing, `:2519`) | NAV→trace; trace→calc menu | activate |
| F2 | steps viewer toggle (`:316-323`) | — | — | — |
| FACT (SHIFT+TABLE) | factorize result (`:306-310,493-504`) | — | — | — |
| GRAPH / TABLE | — | — (global) | jump to tab (`GrapherApp.cpp:2381-2390`) | idem |
| ZOOM | — | — | zoom in (`:2709`) | — |
| MODE | intercepted by SystemApp → menu (`SystemApp.cpp:500-505` & per-mode cases) | idem | idem | idem |

Serial console layer (emulator + firmware serial): character map in `SerialBridge.cpp:108-241`;
emulator scripts use `scriptNameToKeyCode` (`NativeHal.cpp:456-565`); SDL interactive input rides
the same ASCII path (input dossier §2). LVGL indev (`LvglKeypad`) forwards **only**
nav/ENTER/ESC/backspace/F1/F2 to widget groups (`LvglKeypad.cpp:101-121`) and is used only by the
launcher (`SystemApp.cpp:679-719`) — apps receive keys by direct dispatch.

---

## C. Desired keymap [P] — deltas only

The table above is the baseline; the desired state changes exactly these cells:

| Key | Context | Desired | Ticket |
|---|---|---|---|
| RPAREN | Calc + Gr-edit | exit innermost container right (structural close) | CA-07 / UX-04 |
| NEGATE | Calc | leading-Sub alias | CA-07 |
| SIN/COS/TAN + SHIFT | Gr-edit | **stay unsupported** — PD-U1 below | — |
| ANS | Gr-edit | **stay inert** until variable stores unify (P-04); pressing it shows hint "n/a in Grapher" instead of silence | UX-05 |
| SHIFT/ALPHA/STO | Gr-edit | consume-and-ignore with indicator update (so the StatusBar never shows a stuck `S`) | UX-02 |
| SQRT + SHIFT | Calc | degree-n root | CA-08 |
| MAX_NEST overflow | Gr-edit | visible "max depth" feedback | GR-20 |
| depth-16 overflow | Calc | visible refusal | CA-03 |

PD-U1 — **no inverse trig in Grapher this cycle (recorded)**: the numeric Evaluator has no
arcsin/arccos/arctan (`Evaluator.cpp:125-161`), so a SHIFT layer in Gr-edit would insert functions
that always fail. Adding the functions to the legacy engine is real work (tokenizer + evaluator +
tests, PROJECT_BIBLE §9 recipe) tracked as UX-06 (stretch); until then the *absence of the key
path* is the correct honest state. The UX cost (same physical key does arc in one app, nothing in
the other) is accepted and documented in the manual.

---

## D. Consistency findings & rulings

### D.1 Function insertion & rendering

[V] Both apps insert `NodeFunction` with auto-parentheses rendered by the shared MathRenderer
(`MathRenderer.cpp:1970-2001`) — consistent. Both route LOG_BASE through `insertLogBase`
(subscript UI) — consistent. Divergence: evaluation — Calc's VPAM evaluator handles
Ln/Log/LogBase/arc natively (`MathEvaluator.cpp:845-973`); Grapher serializes LogBase to
`(log(x)/log(b))` (`GrapherApp.cpp:1232-1255`) and has no arc at all. Ruling: rendering layer is
the consistency surface (same glyphs, same parens) [already true]; evaluation gaps are per-app
facts recorded in the product specs.

### D.2 Decimal & negative behavior

[V] Decimal: identical (`insertDigit('.')` with the ASCII-46 guard, `CursorController.cpp:320-345`).
Negative: **inconsistent** — Calc ignores NEG/NEGATE (SUB only); Gr-edit maps both NEG and NEGATE
to Sub (`GrapherApp.cpp:2576-2577`). Downstream, Grapher's parser needs the leading-unary string
rescue (`GraphModel.cpp:38-52`) and does **not** rescue interior unary (`2*-x` invalid — GC-09),
while Calc's VPAM evaluator accepts leading Sub structurally. Ruling [P → CA-07/UX-04]: NEGATE =
Sub-insert in both apps; interior-unary remains a Grapher grammar fact until the legacy parser
grows a unary rule (explicitly out of scope — GEOM kept the parser frozen).

### D.3 Variable behavior — the two-store split

[V] Calc reads/writes `VariableManager` (ExactVal, LittleFS); Grapher's evaluator reads
`VariableContext` (double, NVS; zero-init, case-insensitive, `VariableContext.cpp:23-32`,
`Evaluator.cpp:47-77`). STO in Calculation is invisible to `y=A` in Grapher (Grapher corpus
GC-140/141). Ruling: unification is P-04 (out of scope); until then **UX-05** ships the honest
version — Grapher's ANS/ALPHA keys give a hint instead of silence, and the manual documents
"Grapher letters default to 0". Corpus pins current behavior so unification later is a visible,
tested flip.

### D.4 Angle mode

[V] Three states, none user-settable: `vpam::g_angleMode` RAD-locked (`MathEvaluator.cpp:53`,
never assigned — grep-verified), `SystemApp::_angleMode` DEG pushed once into SystemApp's own
legacy evaluator + EquationSolver (`SystemApp.cpp:79,96-97`), per-instance `Evaluator::_angleMode`
RAD default (`Evaluator.cpp:24-25`) — the Grapher's instance is never set. StatusBar badge reads
the vpam global (`StatusBar.cpp:243-248`); SystemApp's legacy header string reads its own member
(`SystemApp.cpp:372`) — the two can *display differently*. SettingsApp has no angle row
(grep-verified). SETUP key exists (`KeyCodes.h:41`) with no handler.
**Ruling [P → UX-01]**: single source = `vpam::g_angleMode`; SettingsApp toggle writes it;
`GraphModel::syncAngleModeFromSystem()` maps **by name** onto the legacy enum (value orders
differ! `Evaluator.h:32-35` vs `MathEvaluator.h:70-73` — CLASS §B warning) at `preCacheRPN` +
`replot()`; SystemApp's `_angleMode` and its startup pushes are retired; EquationSolver syncs the
same way at solve entry. Emulator: `set_angle_mode` command (GHOOK §D.4). Acceptance: CC-56/57,
GC-137/138, and a "badge equals engine" assert.

### D.5 Error language

[V] Three vocabularies: VPAM `"Math ERROR"`/`"FACT ERROR"` (English), legacy Spanish
(`"Error: Dominio"`, `"Error: Div por 0"`, `"Variable no definida"`, `Evaluator.cpp:125-207`),
Grapher user-facing English info-bar strings. Users see Spanish nowhere today (Grapher converts
errors to NAN silently [V dossier]), but GR-10's visible slot reasons and CA-04's taxonomy both
surface error text — they must share one vocabulary.
**Ruling [P → UX-03]**: the CLASS §A.2 canonical strings (`syntax`, `one-sided relation`,
`multiple relations`, `unknown: <id>`, `expression too long`, `constant relation`,
`unsupported node`) are the Grapher slot vocabulary; the CA-04 classes (`Syntax ERROR`,
`Divide by 0`, `Domain ERROR`, `Overflow`, `Not supported`) are the Calculation result vocabulary;
both live in per-app string tables (GR-17 pattern); legacy Spanish strings stay internal-only
(matched by the CLASS §A.3 dry-run classifier — renaming them would break it; explicitly frozen).

### D.6 Unsupported notation

Calc: evaluator-default → `Not supported` class (CA-04), future MINPUT refusal chips (§D.8 of the
Calc spec). Grapher: serializer-unsupported nodes → `unsupported node` invalid reason (CLASS R14).
Consistent principle: **render ≠ evaluate; refusals are visible and typed** (MINPUT §5.6 stack).

### D.7 Home/Mode behavior

[V] Consistent by construction: SystemApp intercepts MODE per-mode and calls `returnToMenu()`
(`SystemApp.cpp:500-661` cases); deferred ~250 ms teardown (`SystemApp.cpp:229-239`); emulator
mirrors (Phase 9F). CI-gated for Grapher (`grapher_home_return_smoke`); Calculation's own
home-return walk is a QA-plan addition (no equivalent calc script exists today — QA dossier).
Ruling: keep; add the Calc walk (QA-05).

### D.8 AC/DEL behavior

[V] Deliberately different, now **recorded as design** (PD-U2):
- Calc: AC = clear-everything (single-screen calculator idiom); DEL = structural backspace.
- Grapher: AC = retreat one focus level (content→toolbar→tab bar→Expressions→exit;
  `GrapherApp.cpp:2440-2447,2748-2753,2838-2851,2897-2900`); AC in edit = clear the slot
  (`:2523-2531`); DEL in list = delete the function row (`:2497`), in edit = backspace.
Rationale: Grapher is a multi-pane app; AC-as-back matches NumWorks. The one convergence rule:
**AC never destroys committed data without a same-screen recovery** — Grapher AC-in-edit clears an
expression the user can retype but was committed before editing began; OQ-U1: should AC-in-edit
restore the pre-edit AST instead of empty (i.e., cancel-edit semantics)? Options: (a) keep clear
[current]; (b) cancel-edit restores prior expression [safer, needs pre-edit clone]. Leaning (b);
human call; ticket UX-07 implements whichever is chosen.

### D.9 Result/input transition

Calc: §G.2 of the Calc spec (continue-same-expression, PD-C6). Grapher: ENTER commits a slot and
returns to the list — no result band exists. No conflict; both pinned by corpus.

### D.10 Emulator vs hardware vs serial input

[V] One dispatch funnel (`SystemApp::handleKey` / NativeHal `dispatchKey`), three producers:
- **Physical matrix**: compiled off (`Keyboard.h:82` `CONNECTED_COLS=0`); when enabled, produces
  PRESS/RELEASE/REPEAT with row/col set.
- **SerialBridge**: PRESS-only events, `row=col=-1` (`SerialBridge.cpp:70-72`) — **REPEAT never
  arrives via serial**, so autorepeat-dependent UX (trace held-key speed) differs between input
  paths. Recorded as a test-fidelity caveat (QA plan §F stress uses repeated discrete presses).
- **Emulator scripts**: `key` = PRESS+RELEASE, `keydown/keyup` available
  (`NativeHal.cpp:1492-1502`).
Doc-vs-code drift to fix in UX-02 docs pass [V input dossier §2.1]: banner claims `=` is EQUALS-not-EXE
but code maps `=`→FREE_EQ (`SerialBridge.cpp:211`); header claims `z`→ENTER (no such case);
`f` documented as SHIFT+DIV but emits bare DIV (`:212`); KeyboardManager indicator doc says
"SL"/"AL" vs actual "S-LOCK"/"A-LOCK" (`KeyboardManager.h:143` vs `.cpp:141-142`).

### D.11 Modifier state split-brain

[V] SystemApp keeps `_shiftActive/_alphaActive` booleans in parallel with KeyboardManager and
bypasses them for Calculation/Calculus only (`SystemApp.cpp:478-493`) — a second modifier truth
that other apps consult implicitly. **Ruling [P → UX-02]**: KeyboardManager is the only modifier
state; SystemApp's booleans become reads of it; StatusBar indicator already reads it
(`StatusBar.cpp:231-237`). Also: on `returnToMenu`, modifiers reset (aligns RT-14).

### D.12 Cursor-wrap divergence

[V] Calc's cursor wraps around at row ends (`CursorController.cpp:510-585` wrap-to-start) — shared
CursorController, so Gr-edit inherits the same wrap. Consistent; pinned by CC-110; OQ-U2: is wrap
the desired feel vs stop-at-end? Low priority; default keep.

---

## E. Conflicts summary (accidental divergence register)

| # | Conflict | Evidence | Resolution |
|---|---|---|---|
| K-1 | Angle mode: 3 states, 2 badges, 0 setters | §D.4 citations | UX-01 |
| K-2 | SHIFT/ALPHA dual bookkeeping | `SystemApp.cpp:478-493` | UX-02 |
| K-3 | Error vocabularies (EN typed vs ES internal vs per-app strings) | §D.5 | UX-03 / CA-04 / GR-17 |
| K-4 | RPAREN inert both apps; NEGATE inert in Calc only | §B table | CA-07 / UX-04 |
| K-5 | ANS/ALPHA silent in Grapher; two variable stores | §D.3 | UX-05 (+P-04 later) |
| K-6 | SHIFT-arc exists only in Calc | PD-U1 | documented; UX-06 stretch |
| K-7 | Editor caps differ: none (Calc) vs MAX_NEST=8 (Grapher) | `CalculationApp` no cap [V]; `GrapherApp.cpp:2553` | CA-03 + GR-20; long-term shared cap (MR-16) |
| K-8 | Serial doc drift (=, f, z, SL/AL) | §D.10 | UX-02 docs |
| K-9 | Calc DIV=fraction vs serial doc "SHIFT+DIV" | `SerialBridge.cpp:212` | UX-02 docs |
| K-10 | REPEAT absent on serial path | `SerialBridge.cpp:70-72` | QA caveat; no code change |

---

## F. Open questions

| ID | Question | Options (⭢ = leaning) | Consequence |
|---|---|---|---|
| OQ-U1 | Grapher AC-in-edit: clear vs cancel-edit | (a) clear [V] · ⭢(b) restore pre-edit AST | (b) needs pre-edit clone + teardown care; changes no goldens (empty-slot flow unchanged) |
| OQ-U2 | Cursor wrap at row ends | ⭢(a) keep wrap [V] · (b) stop | (b) would change CC-110 and both editors |
| OQ-C3 | Implicit-Ans continuation after result (Calc spec §G.2) | ⭢(a) keep continue-editing · (b) Casio implicit Ans | (b) rewrites two gated scripts + goldens |
| OQ-C1 | Two-step structure delete | ⭢(a) immediate flatten [V] · (b) select-then-delete | (b) renderer highlight + goldens |
| OQ-U3 | SETUP key purpose | (a) leave unbound · ⭢(b) open Settings from anywhere | (b) is a SystemApp-level dispatch addition; cheap; pairs with UX-01 angle toggle |

---

## J. Tickets (UX-*)

- **UX-01 — Angle-mode single source of truth + Settings toggle** (§D.4; pairs CA-06, feeds GR-02
  sync). Files: `SettingsApp.*`, `SystemApp.cpp:79,96-97` retirement, `GraphModel` sync fn,
  `EquationSolver` sync, emulator `set_angle_mode` (append-only grammar).
- **UX-02 — Modifier unification + input-docs truth pass** (§D.11, K-2/K-8/K-9; KeyboardManager as
  sole state; SerialBridge banner/header corrections; indicator string doc fix).
- **UX-03 — Cross-app error-string tables** (§D.5; two per-app tables, one style guide; Spanish
  internals frozen for dry-run compatibility).
- **UX-04 — RPAREN structural close in both editors** (with CA-07).
- **UX-05 — Grapher honest-inert keys** (ANS/ALPHA/STO/SHIFT hints in Gr-edit; no store coupling).
- **UX-06 — (stretch) inverse trig in the legacy engine** (tokenizer+evaluator+corpus; unblocks
  Grapher SHIFT layer; explicitly optional this cycle — PD-U1 stands without it).
- **UX-07 — Grapher AC-in-edit cancel semantics** (executes OQ-U1 once decided).

Full fields in the roadmap. Acceptance for this spec as a whole: the §B table regenerated from
source after the tickets matches the §C desired deltas, and every K-row is closed or re-recorded
as a PD.

---

*End of UX & input consistency spec.*
