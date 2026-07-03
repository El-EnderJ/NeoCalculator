# NumOS — Core Apps QA Oracle & Stress Plan

> How Calculation and Grapher get tested deeply: inventory, gaps, hooks, goldens, stress, loops,
> human QA, CI gating, artifacts — plus the break-it corpus (50 Calculation + 75 Grapher cases).
> Companion to the two product specs (corpora CC-01…CC-112, GC-01…GC-153) and to
> `NUMOS_GRAPHER_TEST_ORACLE_AND_VISUAL_COVERAGE_PLAN.md` (TEST tiers T1–T7, which this plan reuses).
>
> Audit base `6da9012` (2026-07-03). Harness facts verified against `src/hal/NativeHal.cpp`,
> `.github/workflows/emulator-build.yml`, `scripts/*`, `tests/emulator/**`, `tests/host/**`.
> Tickets QA-01… (§K; full fields in the roadmap doc).

---

## A. Existing inventory [V]

### A.1 Harness

- `.numos` grammar (parse `NativeHal.cpp:1457-1622`, execute `:1697-1850`): `wait`, `key/keydown/
  keyup`, `screenshot`, `log`, `open_app`, `assert_app`, `assert_result[_contains]`,
  `assert_no_error`, `assert_error [substr]`, `assert_variable`, `assert_menu_focus`. Exit codes:
  0 pass, 1 SDL, 2 parse, 3 screenshot write, 4 assert fail (`:1382,1686,1863,1883,1902,2164`).
  One command per deterministic frame (`:1697-1703`).
- **Semantic hooks are Calculation-only + Menu-only**: `assert_result*`/`assert_*error` require
  `g_mode==CALCULATION` and read `debugLastResult()` (`:1743-1804`); `assert_variable` reads the
  VariableManager singleton (`:1805-1820`); `assert_menu_focus` reads the launcher (`:1826-1848`).
  **No `assert_graph_*` of any kind exists yet** (enum ends at AssertMenuFocus, `:1345-1362`) —
  GR-14/GHOOK specifies them; nothing is implemented.
- Result formatter asserts exact **integers/fractions only**; radicals/π/e are explicitly
  out of assertion scope (`formatExactVal`, `:1628-1663`).
- Determinism: `--deterministic` synthetic tick; CI double-runs `calc_1_plus_2` and SHA-256
  compares (`emulator-build.yml:228-236`).

### A.2 Fixtures

71 scripts / 19 goldens (+README) / 18 masks (all masks = the single StatusBar-clock rect
`4,6,37,13`; `launcher_smoke` is the only maskless golden). Per-app: Calculation 19 scripts
(2 goldens), Grapher 36+1 scripts (6 goldens), Stats/Prob/Seq/Reg 2 each (2 goldens each),
Settings 1 (1), MathShowcase 2 (1), launcher/menu 4 (1), input parity 1. Host tests: exactly one —
`tests/host/keycode_digit_test.cpp` (keycode digit helper).

### A.3 CI gates (emulator-build.yml, in order)

keycode guard (`:96-104`) → build → 3A/3B boot+determinism smokes (`:132-177`) → 4A replay +
SHA-256 (`:201-237`) → 4B-C semantic (1+2=3, 5/6) (`:260-286`) → 8C Calculation suite
(`calc_semantic_*` + div-zero + store, `:308-356`) → 9A input parity (`:405-407`) → 9B menu parity
(`:459-460`) → 9F Grapher no-hang/function/template gates (`:507-531`) → candidate generation
(25 stems, `generate-emulator-candidates.py:39-143`) → golden compare (mismatch fails, missing
warns, `:575-607`). Firmware workflow runs zero tests (`compile-and-release.yml`).

## B. Gaps [V — each verified as absence]

1. **Grapher semantics are pixel-or-nothing**: no graph/table/trace/classification hook; the 6
   goldens + no-hang timeouts are the entire oracle. (GR-14 closes.)
2. **~18 Phase-10 scripts are T4 orphans** — implicit/inequality/aspect/mixed/trace-domain/stress
   scripts exist but run in **no** CI step and have **no** goldens. A regression blanking the
   implicit renderer passes CI today. (GR-00 closes.)
3. **Calculation oracle ceiling**: no assertion for radicals/π/e/decimal-marked values; no
   angle-mode script (no `angle|deg|rad` match anywhere in scripts — verified); no display-mode
   (S⇔D/Periodic/Extended) script; only one error class (div-0) covered; no history script.
4. **No negative controls**: nothing proves the comparator or assert plumbing can fail (TEST T6).
5. **No self-diff tier**: per-script byte determinism claimed in comments, automated nowhere
   except the single 4A double-run (TEST T5).
6. **No stress gating**: `grapher_breakit_stress`/`grapher_curve_stress` exist, non-gating;
   Calculation has no stress scripts at all.
7. **No teardown-loop coverage beyond single home-return walks**; no repeated relaunch loop.
8. **Emulator input path cannot produce REPEAT** (SerialBridge PRESS-only, `SerialBridge.cpp:70-72`;
   scripts have `key`/`keydown`/`keyup` only) — autorepeat UX untestable in CI; goes to human QA (§H).
9. **CAS suites, Giac, firmware-only apps**: out of this plan's scope (owned by CAS-01/02,
   CAS-15/P-02, P-05) but listed so nobody re-discovers them.

## C. Semantic assert hooks needed

Append-only additions to the `.numos` grammar (fragile-file rule: never modify existing commands).

| Hook | Reads | Consumer rows | Ticket |
|---|---|---|---|
| `assert_graph_relation_count/slot_kind/slot_valid/slot_invalid_reason/table_value/trace_state/no_nan_ui/relation_op/expr_text` + `set_angle_mode`/`assert_graph_angle_mode` | GHOOK §B–§D accessors | GC corpus, classifier contract | **GR-14** (spec done) |
| `assert_result_exact <canon>` | new canonical ExactVal formatter (`k√m·π^p·e^q` + `≈` flag) — extends `formatExactVal` scope deliberately | CC-32…72 (A† rows) | QA-03 |
| `assert_result_mode <symbolic\|periodic\|extended>` + `assert_result_approx <0\|1>` | `CalculationApp::_resultMode`, `_lastResult.approximate` debug accessors | CC-22, CC-93…97, CA-01 flips | QA-03 |
| `assert_history_count <n>` | `_history.size()` | CC-98…100 | QA-03 |
| `assert_status_indicator <text>` | `KeyboardManager::indicatorText()` | modifier break-it rows | QA-03 |
| `assert_exit_clean` (implicit via exit 0) — no new command needed | — | teardown loops | — |

All NATIVE_SIM-only accessors, following the `debugLastResult` precedent
(`CalculationApp.h:95-107`). Negative control per new command (must exit 4 when wrong — TEST §D).

## D. Visual goldens needed

Bless via the human-only pipeline; mask = clock rect only; Linux-generated (TEST §E.7 rule).

Grapher (from TEST §D, unchanged list + product additions): implicit_circle, implicit_sideways
(post-GR-02), ineq_disk, ineq_exterior, one halfplane/parabola, aspect_circle, mixed graph+table,
intersect_markers, calcmenu_explicit, calcmenu_implicit_disabled [post-GR-08], trace_implicit
[post-GR-07], ineq dashed pair [post-GR-11], slot_error [post-GR-10], multifn graph/table,
y-table [post-GR-12], Δx chip [post-GR-18].

Calculation (new list): `calc_radical_result` (√12 → 2√3), `calc_periodic_result` (1/3 overline),
`calc_extended_result` (2/3, 200 digits), `calc_error_band` (Domain error styling, post-CA-04),
`calc_history_view` (entry reloaded), `calc_steps_viewer` (edu steps on), `calc_deep_nesting`
(depth-cap refusal state, post-CA-03), `calc_approx_marker` (≈ display, post-CA-01),
`calc_logbase` (promote the existing warning-only script).

## E. Warning-only candidates needed

Everything in §D enters `CANDIDATES` **before** blessing (warn-on-missing is the safe default,
`emulator-build.yml:602-604`). Additional permanently-warning-only candidates (frames too
content-volatile to gate but worth artifact eyes): `grapher_breakit_stress` final frame,
`calc_stress_typing` final frame, launcher after 10 relaunch cycles.

## F. Stress tests

Each is a `.numos` script (T1 assert-only unless noted); "gate" = CI-gated no-hang + asserts.

| Stress | Script sketch | Oracle | Gate? |
|---|---|---|---|
| Rapid typing | 200 keys with `wait 0`… `wait 1` alternating, then assert final result | A | gate |
| Deep fractions | Calc: 20×DIV then digits; Grapher: 9×DIV | no crash; cap behavior (CA-03/GR-20) | gate |
| Long expressions | 40-digit number; 63/64-char Grapher boundary | A + slot-kind | gate |
| Invalid-input storm | every §I break-it invalid row in sequence, AC between | A per row | gate (batched) |
| Repeated HOME/app launch | 10× (open_app → interact → MODE) per app | exit 0, `assert_app Menu` each cycle | gate |
| Trace movement | 150 trace steps incl. domain holes + fn switches | trace_state + no_nan_ui | gate post-GR-14 |
| Pan/zoom stress | 40 mixed pan/zoom incl. extremes | no hang; post-GR-21 clamp assert | gate |
| Template insertion | open/insert/delete ×6 templates ×3 rounds | expr_text + plot | gate |
| Multi-function graph/table | 6 slots, all tabs, toggles | goldens + table_value | gate |
| Implicit/inequality stress | `sin(x*y)>0` zoom-out ladder; all-NaN relation `sqrt(-1-x^2-y^2)=0` | no-hang; [post-GR-13] truncation banner assert | gate |
| Discontinuities | tan/1/x/ln trace+integral attempts | off-domain pill; `∫ n/a` [post-GR-08] | gate |
| Angle mode | DEG/RAD flips mid-session, both apps | table_value + result asserts | gate post-UX-01 |

Caveat: serial/script input can't generate REPEAT (§B.8) — held-key autorepeat goes to §H.

## G. Automated loop strategy

The iteration loop for any agent/human working these specs:

1. **Capture**: `pio run -e emulator_pc` once; run the target script headless+deterministic with
   `--screenshot`; artifacts to `out/`.
2. **Inspect**: view the PPM (any viewer; CI uploads artifacts on `if: always()`).
3. **Compare**: `scripts/compare-ppm.py out/X.ppm tests/emulator/golden/X.ppm --mask-file …`
   (exit 0 = match) for visual rows; for semantic rows the script's own asserts are the comparison.
4. **Assert**: extend the script with `assert_*` lines *before* fixing code — a failing assert
   (exit 4) is the red test; a passing run is green.
5. **Iterate**: fix → rerun → when stable, add stem to `CANDIDATES` (warn) → human blesses via
   `scripts/promote-emulator-golden.py` after eyeballing the diff → mismatch now gates.
Rules: never promote from a truncated/nondeterministic frame; every new assert gets a negative
control; every corpus row cites its script name back in the corpus table (traceability sweep in
QA-06).

## H. Human QA checklist (per release / after renderer waves)

Hardware+emulator manual passes; each item ~1 min:

1. Boot → launcher; open Calculation; type `1/2+1/3` watching capture animation; ENTER; S⇔D ×3.
2. Type after result (continues expression); Ans chain; STO→A; verify `A` in a new expression.
3. Produce each error class; verify wording + recovery; AC.
4. Deep-nest fractions until refusal/clip; confirm no crash, message readable.
5. Grapher: type each relation family (line, parabola, sideways, circle, disk inequality, invalid
   `x=`), check list states; Graph tab; equal-aspect circle roundness by eye.
6. Trace across a tan pole (off-domain pill); UP/DOWN slot switch; calc menu root/min/intersection;
   integral shading; tangent.
7. Table: step halve/double; implicit column `--`; empty state.
8. Templates: insert all six; delete rows; visibility toggles.
9. Hold-key autorepeat on trace and digit entry (hardware only — REPEAT path).
10. HOME from every screen incl. modals; relaunch ×3; watch for teardown hangs.
11. Firmware-only: PSRAM-fail label (if reproducible), Giac serial `:` command smoke, watchdog
    silence during inequality zoom-out (TEST §E.6 list).
12. StatusBar: clock ticks, modifier indicator during SHIFT/ALPHA/STO, angle badge truthfulness
    (post-UX-01), battery icon presence.

## I. CI gating plan (staged)

| Stage | Adds | Precondition |
|---|---|---|
| 1 (now) | GR-00: gate the 18 T4 Grapher scripts in the 9F-style loop + add to CANDIDATES | none — scripts exist |
| 2 | negative controls (T6): wrong-golden compare must fail; wrong assert must exit 4 (inverted-exit CI steps) | none |
| 3 | T5 self-diff: run each graph-body + calc-result script twice, `cmp` PPMs | none |
| 4 | QA-01 batch: new CC/GC scripts using **existing** grammar (assert_result/error/variable, assert_app) — ~40 scripts | none |
| 5 | GR-14 hooks + QA-03 calc hooks + their negative controls; QA-02 batch: hook-dependent scripts (~60) | GR-14/QA-03 merged |
| 6 | stress gates (§F) incl. teardown loops | batches 4-5 stable |
| 7 | golden waves per feature ticket (§D lists) — human promotion only | each ticket |
| 8 | `expect-fail` ledger: `bug` corpus rows run inverted (fail = still-broken is green; pass = flip the row + close ticket) — keeps known bugs visible without red CI | QA-04 tooling |

Wall-clock budget: stages 1–5 ≈ +6 min CI (5 s/script × ~110 + build unchanged) — acceptable
against the current single-job layout; if it grows past 10 min, shard scripts across a matrix.

## J. Failure artifact plan

On any gated failure, CI must retain enough to debug without a local repro:

- [V] already uploaded `if: always()`: build binary, all logs, candidates, smoke PPMs.
- [P → QA-05]: (a) on assert-fail (exit 4), the runner already prints `[ASSERT] … FAIL` with
  expected/actual — add an automatic `screenshot` of the failing frame (runner change: capture on
  assertFail before quit, `NativeHal.cpp:1686-1687` area, append-only behavior);
  (b) golden mismatches upload a visual diff PPM (extend `compare-ppm.py` with `--diff-out`,
  red-marking differing pixels);
  (c) every failed step names its script + exit code in the step summary (`::error::` pattern
  already used, `emulator-build.yml:598-600`);
  (d) artifact retention 14 days, name-spaced per stem.

---

## K. Break-it corpus

Format: **ID | scenario | expected behavior | oracle | CI-gated or manual**. "Expected" is the
post-ticket contract; rows marked ⚠bug are expected-fail today (stage-8 ledger). Oracles:
A = semantic assert, G = golden, NH = no-hang (timeout+exit 0), M = manual, X2/X4 = expected
exit-code control.

### K.1 Calculation break-it (50)

| ID | Scenario | Expected | Oracle | Gate |
|---|---|---|---|---|
| BC-01 | 200-key rapid-type storm then `⏎` | exact final result; no dropped keys | A | CI |
| BC-02 | 20 nested fractions (DIV×20) | [P CA-03] refusal at depth 16; no crash (today: undefined ⚠) | A+G | CI |
| BC-03 | 12-deep function nesting `sin(sin(…))` | renders (clip ok at depth 12 red-…); evaluates or typed error | NH+G | CI |
| BC-04 | 40-digit number literal | int64 overflow → approximate scientific; `≈` post-CA-01 ⚠ | A | CI |
| BC-05 | `⏎` ×60 on empty | 60 Syntax errors; history cap 50 holds; no leak/hang | A | CI |
| BC-06 | DEL ×100 on empty expression | inert, no crash (`ensureNotEmpty`) | NH | CI |
| BC-07 | AC ×50 rapid | stable reset each time | NH | CI |
| BC-08 | SHIFT SHIFT ALPHA SHIFT STO AC chain | indicator follows FSM (`KeyboardManager.cpp:42-130`); AC resets | A (`assert_status_indicator`, QA-03) | CI |
| BC-09 | STO then MODE (leave app with pending STO) | modifier reset on return-to-menu (RT-14 rule) | A | CI |
| BC-10 | STO then digit (non-var) | STO cancels; digit falls through and inserts | A | CI |
| BC-11 | ALPHA-lock then 20 digits | 6 vars A–F inserted for 1–6, others consume/pass per map | A | CI |
| BC-12 | `1.2.3.4` dot storm | single decimal point survives | A | CI |
| BC-13 | `.` as first key then `5⏎` | 0.5 → 1/2 (or defined literal rule — pin at implementation) | A | CI |
| BC-14 | 50 unclosed LPAREN | evaluates innermost-empty → Syntax; editor stable | A | CI |
| BC-15 | RPAREN ×20 | inert today; post-CA-07 exits containers; never crashes | A | CI |
| BC-16 | `2^99999` via POW digits | double overflow → Overflow class (post-CA-04); no hang | A | CI |
| BC-17 | `9223372036854775807+1` | approximate escape, scientific, ≈ ⚠ | A | CI |
| BC-18 | FACT on `2^30⏎` result | factorization completes (2^30 trial division fast) or bounded; no hang | NH+A | CI |
| BC-19 | FACT on large prime result (e.g. 2147483647) | completes < timeout; correct or typed refusal — pin at implementation | NH | CI |
| BC-20 | FACT with no result yet | ignored (guard `_hasResult`, `CalculationApp.cpp:493-504`) | A | CI |
| BC-21 | F2 with no steps | ignored (guard `_hasEduSteps`) | A | CI |
| BC-22 | F2 viewer open → all keys | only UP/DOWN/AC/DEL/F2 act (hijack, `CalculationApp.cpp:233-252`) | A | CI |
| BC-23 | FREE_EQ with no result | ignored (`:485-490`) | A | CI |
| BC-24 | FREE_EQ ×30 | mode cycles, value never mutates | A | CI |
| BC-25 | Extended scroll past both ends ×20 | clamped; no artifacts | G | CI |
| BC-26 | UP ×60 (history walk past oldest) | clamps at oldest; DOWN returns; live expr at index −1 | A | CI |
| BC-27 | history entry edit + ⏎ | new entry appended; original unchanged | A | CI |
| BC-28 | error → UP → DOWN → type | error entry navigable; editing resumes cleanly | A | CI |
| BC-29 | `1/0⏎` ×10 then `Ans⏎` | Ans still last-ok value (never poisoned) | A | CI |
| BC-30 | divide by zero inside fraction node `1 DIV 0` | Divide-by-0; input preserved | A | CI (gated today as `calc_error_div_by_zero`) |
| BC-31 | `√(√(√(2)))` | exact-or-≈ chain; no crash | A | CI |
| BC-32 | `π^30` via repeated `*π` | piMul int8 bounds — must not wrap silently; pin behavior (overflow → approx/error) | A | CI |
| BC-33 | `e^…` analog of BC-32 | idem | A | CI |
| BC-34 | mixed `π*e*√2*3/7` | single-monomial exact display | A† (QA-03) | CI |
| BC-35 | type 30 keys after result (continue-edit path) | §G.2 semantics hold at scale | A | CI |
| BC-36 | MODE during ENTER-heavy loop | clean exit; deferred teardown; relaunch ok | NH | CI |
| BC-37 | 10× relaunch loop with STO writes | `/vars.dat` consistent; no FS corruption | A | CI |
| BC-38 | evaluate `x` with unset x | 0 (store default) — pinned | A | CI |
| BC-39 | STO into z via ALPHA+SOLVE | z stored (map `CalculationApp.cpp:875-888`) | A | CI |
| BC-40 | expression using PreAns before 2 results exist | default 0 — pin | A | CI |
| BC-41 | 2000-frame idle | clock ticks only; zero unexpected redraw diffs outside mask | G (self-diff) | CI |
| BC-42 | keydown without keyup (stuck key) | PRESS handled once; no repeat storm in emulator | NH | CI |
| BC-43 | unknown script key token | parse error exit 2 | X2 | CI (control) |
| BC-44 | `assert_result 4` after `1+2⏎` | exit 4 | X4 | CI (control) |
| BC-45 | screenshot to unwritable path | exit 3 | X-code | CI (control) |
| BC-46 | golden compare vs wrong golden | comparator exits non-zero | inverted step | CI (control) |
| BC-47 | held-key autorepeat digit entry | rate per driver config; no double-insert | M | manual (REPEAT unreachable via serial) |
| BC-48 | PSRAM exhaustion during history growth | orange resource state, no crash | M | manual/firmware |
| BC-49 | evaluate during low LVGL pool (many steps rendered) | steps viewer caps; app survives | M | manual |
| BC-50 | power-loss (reset) after STO | vars intact on reboot | M | manual/firmware |

### K.2 Grapher break-it (75)

| ID | Scenario | Expected | Oracle | Gate |
|---|---|---|---|---|
| BG-01 | `x=` commit | invalid `one-sided relation`; **no x=0 line** ⚠ | A+G | CI (post-GR-02; inverted until) |
| BG-02 | `=x` | idem ⚠ | A | CI |
| BG-03 | `y<` | invalid; **no y<0 shading** ⚠ | A+G | CI |
| BG-04 | `<x` | invalid ⚠ | A | CI |
| BG-05 | `xy=1` | `unknown: xy`; axes-only ⚠ | A+G | CI |
| BG-06 | `x2` | `unknown: x2` ⚠ | A | CI |
| BG-07 | `abc=1` | `unknown: abc` ⚠ | A | CI |
| BG-08 | `y=x=2` | `multiple relations` ⚠ | A | CI |
| BG-09 | `0<x<1` | `multiple relations` ⚠ | A | CI |
| BG-10 | `1=2` | `constant relation` ⚠ | A | CI |
| BG-11 | `pi<e` | `constant relation` ⚠ | A | CI |
| BG-12 | `sin()` | `syntax` (dry-run) ⚠ | A | CI |
| BG-13 | `x(x+1)` | `syntax` (function-token trap) ⚠ | A | CI |
| BG-14 | 64-char expression | `expression too long`; **no truncated plot** ⚠ | A+G | CI |
| BG-15 | exactly-63-char expression | valid; plots correctly (boundary) | A | CI |
| BG-16 | `((((((((x` (unbalanced ×8) | invalid `syntax`; editor stable | A | CI |
| BG-17 | 9th nested fraction in editor | MAX_NEST drop + [P GR-20] feedback | A/G | CI |
| BG-18 | all 6 slots invalid | axes-only; table `--`; trace refused; no hang | A | CI |
| BG-19 | commit → immediately re-edit → AC → re-commit ×10 | slot state consistent; no stale RPN | A | CI |
| BG-20 | DEL row while 6 slots full then re-add | numbering/colors stable; Add row reappears | A+G | CI |
| BG-21 | attempt 7th slot | Add hidden; addFunction no-op (`GrapherApp.cpp:942-946,994`) | A | CI |
| BG-22 | visibility toggle ×20 rapid with replot each | no hang; dot opacity tracks | NH+G | CI |
| BG-23 | templates open/close ×20 | timer cleanup correct; no leak/hang | NH | CI |
| BG-24 | templates open → MODE | clean app exit (teardown incl. tpl timer, `GrapherApp.cpp:167-228`) | NH | CI (partially covered by home_return) |
| BG-25 | calc menu open → MODE | clean exit | NH | CI |
| BG-26 | ENTER on calc menu with function deleted mid-flight | guard `_traceFn` invalid → no-op (`GrapherApp.cpp:3084`) | A | CI |
| BG-27 | trace 150 steps LEFT at left edge | clamps; camera stable; no drift | A (trace_state) | CI post-GR-14 |
| BG-28 | trace across ln(x) boundary ×20 crossings | off-domain pill toggles correctly; no nan text | A (`no_nan_ui`) | CI |
| BG-29 | UP/DOWN fn-switch with only 1 traceable among 6 | stays on it; no index corruption | A | CI |
| BG-30 | trace on slot whose expression is edited to implicit | traceable set recomputed; trace exits gracefully | A | CI |
| BG-31 | zoom in ×50 | [P GR-21] clamp + message; today: undefined-no-hang ⚠ | NH→A | CI |
| BG-32 | zoom out ×30 with inequality | budget banner [post-GR-13]; no watchdog/hang | NH | CI |
| BG-33 | pan ×100 alternating directions | viewport arithmetic stable (no NaN); labels sane | A+G | CI |
| BG-34 | Auto with all slots hidden | default y∈[−7,7] (`GrapherApp.cpp:2043-2045`) | A | CI |
| BG-35 | Auto on constant `y=5` | fits with margin; no zero-height viewport | A | CI |
| BG-36 | Auto on `y=1/x` | |y|>1e6 samples ignored (`:2029`) | A | CI |
| BG-37 | `y=tan(x)` zoomed out ×10 | branch count grows; no smear; bounded time | G+NH | CI |
| BG-38 | `y=sin(1/x)` at default + zoomed to 0 | bounded sampling (depth 3); no hang | NH | CI (exists in curve_stress family) |
| BG-39 | `sqrt(-1-x^2-y^2)=0` (all-NaN G) | nothing plotted; no hang; valid-but-empty (per-point domain rule) | NH+A | CI |
| BG-40 | `sin(x*y)>0` zoom-out ladder | worst-case shading completes/truncates per GR-13 | NH | CI |
| BG-41 | `x^2+y^2=0` point relation | ≤ few px or nothing; no special-case crash | G | CI |
| BG-42 | `x^2+y^2=-1` empty set | axes-only; slot valid | G | CI |
| BG-43 | implicit + trace ENTER attempts ×10 | refusal message every time; no state leak | A | CI (T4 exists: implicit_tabletrace_safe) |
| BG-44 | table step LEFT ×10 (floor 0.1) | clamps; cells re-render | A | CI |
| BG-45 | table step RIGHT ×10 (cap 10) | clamps | A | CI |
| BG-46 | table UP ×50 at top | window walks; x column monotonic | A | CI |
| BG-47 | table with 6 valid slots | 7 columns capped; widths equal | G | CI |
| BG-48 | table after deleting the selected fn | `_tblFuncIdx` re-picked; no dangling index | A | CI |
| BG-49 | `y=99999999*x` table | `%.6g` formatting; no overflow text | A | CI |
| BG-50 | GRAPH key from every focus state | jumps to Graph, focus CONTENT (`GrapherApp.cpp:2381-2390`) | A | CI |
| BG-51 | TABLE key during edit | [V pin at implementation: edit continues or commits — assert actual] no crash | A | CI |
| BG-52 | AC chain from deep trace to app exit | TRACE→NAV→toolbar→tab→Expressions→exit ordering | A | CI |
| BG-53 | rapid tab cycling ×20 | lazy graph init once; no leak | NH | CI |
| BG-54 | 10× relaunch loop | teardown clean every cycle | NH | CI |
| BG-55 | POI flood `y=sin(50x)` + `y=0` | ≤20 POIs; async timer completes; [P] cap notice | A | CI |
| BG-56 | trace + async POI racing (key spam during 10-ms ticks) | deterministic under `--deterministic`; no crash | NH+self-diff | CI |
| BG-57 | intersection of identical curves `y=x` twice | today arbitrary/none; post-GR-06 zero-POI overlap rule | A | CI future |
| BG-58 | integral across `1/x` pole | today NaN-poisoned result; post-GR-08 `∫ n/a (domain gap)` ⚠ | A | CI future |
| BG-59 | tangent at domain edge `sqrt(x)` @ 0 | today silent no-draw; post-GR-09 `tangent n/a (domain)` ⚠ | A | CI future |
| BG-60 | root search on `y=x^2+1` | "Not found in range"; cursor unmoved | A | CI |
| BG-61 | intersection with implicit-only partner | today false "Not found" ⚠; post-GR-06/08 real answer or labeled refusal | A | CI |
| BG-62 | menu ENTER spam ×10 on Draw Tangent | overlay redrawn, not duplicated | G | CI |
| BG-63 | `y=x` then edit to `y>x` then to `y=x` | class transitions explicit↔ineq clean; overlays cleared | A | CI |
| BG-64 | inequality boundary pixel test `y>x^2` vs `y>=x^2` | identical today; differ only in dash post-GR-11 | G pair | CI future |
| BG-65 | serial `lt`+`=` non-strict entry | relation_op `le` (only pre-GR-11 oracle) | A | CI post-GR-14 |
| BG-66 | template insert into slot being edited | defined per `handleTemplates` (`GrapherApp.cpp:1582-1598`); AST replaced atomically | A | CI |
| BG-67 | template modal UP at top / DOWN at bottom | selection clamps | G | CI |
| BG-68 | `y=A` (unset var) | y=0 line; pinned two-store fact | A | CI |
| BG-69 | `y=ans` | 0 default from VariableContext; pinned | A | CI |
| BG-70 | DEG set + all six templates | curves follow mode post-GR-02 | A | CI future |
| BG-71 | `grapher_breakit_stress` full walk | exit 0 | NH | CI (GR-00) |
| BG-72 | `grapher_curve_stress` | frame budget respected | NH | CI (GR-00) |
| BG-73 | wrong-golden negative control | comparator fails (inverted step) | X | CI (control) |
| BG-74 | `assert_app Calculation` inside Grapher | exit 4 | X4 | CI (control) |
| BG-75 | PSRAM buffer alloc failure at graph panel | red ERR label; app navigable (`GrapherApp.cpp:578-587`) | M | manual/firmware |

---

## L. Tickets (QA-*) — summary

- **QA-01 — Corpus batch 1 (existing grammar)**: ~40 CC/BC + GC/BG scripts needing no new hooks;
  gate + candidates. (Stage 4.)
- **QA-02 — Corpus batch 2 (hook-dependent)**: remaining rows after GR-14/QA-03. (Stage 5.)
- **QA-03 — Calculation assert hooks**: `assert_result_exact/mode/approx`, `assert_history_count`,
  `assert_status_indicator` + debug accessors + negative controls. Append-only grammar.
- **QA-04 — Expect-fail ledger tooling**: inverted-run list for ⚠bug rows; flip protocol. (Stage 8.)
- **QA-05 — Failure artifacts**: assert-fail auto-screenshot; `compare-ppm.py --diff-out`;
  Calc home-return walk script.
- **QA-06 — Traceability sweep**: corpus↔script cross-reference table committed and CI-checked
  (a row without a script name fails the sweep).
- **QA-07 — Self-diff + negative-control CI steps** (T5/T6 wiring; pairs GR-00 stage 2-3).
- **QA-08 — Stress suite** (§F scripts + teardown loops; stage 6).

---

*End of QA oracle & stress plan.*
