# Keystroke-to-answer oracle harness (CORRECTNESS-E2E-01)

Drives the real NumOS pipeline in the SDL2 emulator — keys → CursorController →
MathAST → serializer → Giac → result formatting → result canvas — and checks
it against an independent oracle built directly from the intended expression.
Not wired into CI.

## Command

```
python tests/e2e/run.py --surface calculation --seed 1 --count 20000 \
    --emulator C:/.piobuild/e2e-01/emulator_pc/program.exe [--jobs 4 --batch 40 --shrink 10]
```

Outputs (git-ignored) go to `out/correctness-e2e-01/<surface>/<run>/`:
`results.jsonl` (one record per case, byte-identical for a given seed/count),
`findings.json`, `summary.json`, `stats.json` (timings, emulator SHA-256),
`findings/<case>-<invariant>-<path>/{repro.numos,finding.json}` (shrunk
reproducers, with `--shrink N`).

Other entry points: `calculation/adv_h01.py` (ADV-H01 pattern table),
`validate_mutants.py` (planted-bug validation), `isolation.py`
(serial vs parallel byte comparison + throughput).

Requirements: Python 3.12 with sympy and mpmath; `C:/mingw64/bin` and
`C:/SDL2/x86_64-w64-mingw32/bin` (prepended to PATH by `common/emulator.py`);
an emulator built from a tree that contains the `dump_*` hooks (commit
`test(emulator): add e2e dump hooks for correctness audit`). The emulator
cannot open non-ASCII paths: keep the worktree and output on ASCII paths.

## Layout

| file | role |
|---|---|
| `common/expr.py` | typed VPAM-shaped expression model, expected structure (`to_struct`) |
| `common/gen.py` | seeded generator (case id `s<seed>-<index>`, RNG = sha256(seed:index)), families, typeability rules |
| `common/keys.py` | keystroke projections: `direct`, `toolbox`, `edited`, `naive` |
| `common/evaluator.py` | oracle semantics (SymPy exact, mpmath 50-digit numerics, candidate sets for policy-ambiguous points) |
| `common/giacparse.py` | independent reader of the Giac-bound text (I2) |
| `common/canon.py` | canonical Giac text from the model (third voice, `giac_eval`) |
| `common/invariants.py` | I1/I2/I5 decisions, display/decimal analysis, findings |
| `common/runner.py` | batching, parallel processes, single-process confirmation, triage, outputs |
| `common/shrink.py` | greedy shrinker + standalone reproducers |
| `common/emulator.py` | process driver, one fresh `--fs-sandbox-dir` per process |
| `calculation/` | Calculation driver (preamble, I3/I4 checks, ADV-H01 probe) |
| `grapher/` | minimal Grapher driver (y=f(x) in slot 0, enough for ADV-H01/M1) |

## Emulator hooks (src/hal/NativeHal.cpp only)

Each prints one line `@E2E {json}`; none mutates state or fails a script.

* `dump_calc TAG [tree]` — `input` (S-expression of the input AST), `giac_in`
  (exactly `CalculationEngine::serializeForGiac(root, …, true)`, the call
  `evaluate()` makes), `status`, `kind`, `exact`, `approx`, `display`
  (S-expression of the AST given to the result canvas) or `display_label`
  (text-fallback label), `format`/`format_name`, `fixed_places`, `eng_shift`,
  `angle`, `history_index`, `format_menu` (+`menu_choices`), `toolbox`, and
  with `tree` the full `dumpTree`.
* `dump_graph TAG SLOT [X…]` — slot AST, slot text, Giac text of the slot RPNs,
  validity/kind/reason, sampled values through `GraphModel` entry points.
* `dump_calculus TAG`, `dump_equations TAG` — input AST(s), Giac-bound text,
  status, exact/approx texts, solution groups (for later surface workers).
* `giac_eval TAG TEXT…` — the third voice: `GiacEngine::evaluateStructured`
  on a canonical string, bypassing keys, AST and the NumOS serializer.

Private app members are read through the explicit-instantiation access rule
(`e2e::Rob`), so no shared/firmware file changed; the whole TU is inside
`#ifdef NATIVE_SIM`.

## Observed editor semantics the projections rely on

(probe logs: `out/correctness-e2e-01/probe/`)

* `/` captures the previous atom as numerator; at row start/after an operator
  it opens an empty fraction. RIGHT/DOWN moves numerator→denominator.
* `^` captures one atom; RIGHT leaves the exponent; EXP inserts `×10^□`.
* In Calculation `)` is `moveRight()`; in Grapher it is `closeParen()`.
* `neg` and `-` are the same SUB key; unary minus serializes as `(-1)*`.
* Toolbox insertion right after an operand adds an explicit `×`
  (`2` + Toolbox |x| → `2 × |x|`); keyboard-shortcut entries (fraction,
  power, sqrt, parentheses, sin/cos/tan) are not discoverable in Toolbox search.
* RIGHT does not leave an empty FIRST slot of a two-slot template
  (`/ RIGHT 2` → `2/□`); the generator only leaves last/single slots empty.
* History recall needs UP while a result is shown (after AC, UP moves the cursor).
* Implicit multiplication has the precedence of × and ÷ (docs/MATH_ENGINE.md),
  so `8÷2π` = 4π by the documented policy.

## Invariants and severities

I1 value (format-aware: exact forms equal to 30+ digits; decimals within half
a unit of the last shown digit; FIX/SCI/ENG/periodic/mixed/polar decoded),
I2 WYSIWYC (input AST = intended structure; Giac text denotes the displayed
input), I3 path independence, I4 round trips (S⇔D twice, FORMAT menu, history
UP, Ans, STO F/RCL F — only from a result that matched the oracle), I5 no
silent loss (refusal/NaN where the oracle has a value).
S1 wrong value shown as valid, S2 right value shown misleadingly, S3 refusal
where a value exists, S4 cosmetic/policy. Harness extension: **S0** = the
emulator process crashed (CRASH) or did not finish within 20 s (HANG) on a
single case in a fresh process.

Oracle policy (candidate sets, any member accepted): principal complex
branches; 0^0 ∈ {1, undefined}; (−a)^(p/q), q odd ∈ {principal, real root};
asin/acos on the cut ∈ {principal, conjugate}; even root of a negative ∈
{principal, undefined}; poles/undefined points: ∞, −∞, undefined and an error
status are equivalent. Any undefined sub-expression makes the whole value
undefined (so `x/(40÷0)` is undefined, not 0).

Every flagged case is re-run alone in a fresh process; only reproduced
findings are reported (`confirmed`), batch-only effects are `batch_artifact`
and recovered desyncs `desync_recovered`. Confirmed findings get a third-voice
triage (Giac on a canonical string).

## Adding a surface driver

Create `<surface>/surface.py` exposing `generate`, `oracle_of`, `header`,
`check_header`, `case_lines`, `case_lines_for`, `analyze`, `third_voice`,
`triage`, `PATHS` (see `calculation/surface.py`, `grapher/surface.py`), add a
`dump_*` reader of the app's state if needed (NativeHal.cpp only, same `@E2E`
convention), and run with `--surface <name>`.

## Validation evidence (emulator SHA-256 29c5f10a…db977ba0)

### Planted bugs (oracle must catch them)

Mutants were built from detached worktrees of commit 577a89f, each with its
own `PLATFORMIO_BUILD_DIR=C:/.piobuild/e2e-01-mut-<name>`, one build at a
time (`out/correctness-e2e-01/mutants/{m1..m4}.diff`, `*.build.log`).
"Attributable" = findings present with the mutant and absent on the unmutated
build for the same seeded cases; each first finding was shrunk and the shrunk
reproducer re-checked to NOT fail on the unmutated build.

| mutant | diff | detected by | cases to 1st detection | attributable (cases) | shrunk reproducer |
|---|---|---|---|---|---|
| M1 ADV-H01 reverted (Grapher) | `GrapherApp.cpp`: drop `case KeyCode::RPAREN: cur.closeParen();` | I2 (30), I1 (20) | 1 of 40 | 50 (30) | `y = ( ( x ) 2 )` → slot `(( x 2 ))`; also `y=(x+1)*2` gives 4 at x=2 (base: 6) |
| M2 FIX rounding off by one | `CalculationFormat.h fixedDecimal`: `digits[keep] >= '5'` → `>= '6'` | I1/S2 (FIX format) | 63 of 300 | 1 (1) | `pi` ENTER, FORMAT→FIX 9 shows 3.141592653 (correct 3.141592654) |
| M3 dropped negation in serializer | `CalculationEngine.cpp`: unary `"(-1)*"` → `"(1)*"` | I2 (252), I1 (262) | 2 of 300 | 514 (109) | recall `C` (stored as `neg 4`) shows 4, oracle −4 |
| M4 backspace off by one | `CursorController.cpp backspace`: `length() > 1` → `> 2` | I3 (95) only | 26 of 300 | 95 (51) | edited path `… 5 6 del 0 …` turns 50 into 0 (shrinker stopped at 6 nodes) |

Seeds: Calculation seed 7 × 300 cases, Grapher seed 1 × 40 cases
(`out/correctness-e2e-01/mutants/*-validation-*.json`). M2 is caught only
when a FIX choice meets a 5 in the rounding digit (1/300 here); raise the FIX
weight in `calculation/driver.py:i4_plan` for a dedicated formatting sweep.

### Isolation, determinism, throughput

`python tests/e2e/isolation.py --seed 11 --count 400 --jobs 4 --batch 40`:
serial, serial again and 4 parallel processes produced byte-identical
`results.jsonl` (sha256 b8f80f39…) and `findings.json` (b390476e…). Each
process uses a fresh `--fs-sandbox-dir`; Giac state is per process.
Throughput (all four paths + I4 on every 2nd case, confirmation included):
serial 1.19–1.23 cases/s, 4 processes 2.64 cases/s (emulator wall 4.29
cases/s). 20 000 cases ≈ 2.1 h with 4 jobs on this machine; the analysis
(SymPy) and Giac hangs (20 s budget each) dominate, not the emulator.

### First real batch (no product fixes; discovery only)

`python tests/e2e/run.py --seed 3 --count 3000 --jobs 4 --batch 40` then
`--reshrink <run dir> --shrink 16`: 520 s, 5.77 cases/s; 106 of 3000 cases
have confirmed findings (single fresh process each); 289 batch desyncs were
recovered by the single-process re-run; 3 cases could not be judged (2 SymPy
recursion errors in the analysis, 1 FORMAT-menu prediction left a modal open).

| class (cases) | minimal reproducer (keys) | NumOS | oracle | triage |
|---|---|---|---|---|
| CRASH 0xC0000005 (8 crashes total, both codes) | `sqrt neg 2 . 4 2 2 right ^ / 1 right 4 right right` = (√−2.422)^(1/4); also log(⁴√(3/(5727÷−27))) | process dies at EXE | finite complex | Giac `simplify` SIGSEGV (gdb: `giac::operator_equal` ← `compute_lv_lvnum_lvden` ← `_simplify`) |
| CRASH 0xC00000FD stack overflow | Toolbox nthroot `2 right 6 alpha 4 divide 6 ^ 9 right right` = ²√(6D÷6⁹) | process dies at EXE | 2^(1/4)/1296 | not triaged further |
| HANG (9) | `findings/s3-1126-HANG--/repro.numos` (not shrunk: each probe costs the 20 s budget) | no result after 20 s / 60 s | finite | Giac evaluation does not return |
| S1 result display drops grouping (15) | Toolbox nthroot `4 right neg 1 6 right` = ⁴√(−16) | shows `1 + i√2` (exact text `(1+i)*sqrt(2)`) | √2+√2 i | pipeline: Giac text correct, result AST loses the parentheses |
| S1 (policy) division by zero absorbed (24) | `6 divide / alpha 5 right 0 right` = 6 ÷ (E/0) | 0 | undefined | Giac projective arithmetic (x/0=∞, y/∞=0) |
| S1 DEG ignored on complex path | DEG `shift cos 3 cos 2 right right` = acos(3·cos 2°) | 180 − 39.595 i | 100.96 i (degrees) | Giac: inner cos taken in radians when the acos argument leaves [−1,1]; same for atan returning radians |
| S1 wrong branch simplification | `logbase 3 right / 1 right e ) tan <22 digits> ) ^ 8 right` (DEG) | (8 ln(tan N) − 1)/ln 3 (complex) | real | Giac rewrites ln(t^8) as 8 ln t with t < 0 |
| S1 numeric garbage, huge arguments | DEG `shift sin sqrt 2 ) - 7 1 ^ 1 2 right` = asin(√2 − 71¹²) | −88.856 − 1016.1 i | −90 + 2970.5 i | Giac double evaluation |
| S2 decimal precision (25) | `2 * neg <22 digits> divide 3`, FIX 3 | −1364408679240000000000.000 | −1364408679235699957246.667 | FIX pads a 12-digit approximation; also last-digit errors such as sin(22448/5) |
| S3 NaN as an OK result | `tan shift cos 1 . 8 3 3 ) sqrt 5 6 6 . 7 9 3 8 ) 3 . 0 9 3 4 exp 4 right` | `nan.0 + nan.0 i`, status ok | finite | Giac numeric |

Reproducers: `out/correctness-e2e-01/calculation/first-batch-seed3-n3000/findings/`,
`out/correctness-e2e-01/evidence/` (keys-only scripts, gdb backtrace) and
`out/correctness-e2e-01/hang/repro-s1-24.numos`, `repro-s1-26.numos`
(Giac does not return within 60 s).

### ADV-H01 pattern on Calculation

`python tests/e2e/calculation/adv_h01.py` (table in
`out/correctness-e2e-01/calculation/adv-h01/adv_h01.md`): in Calculation `)`
leaves only the innermost slot, so `2 ^ ( 1 + 1 ) * 3` builds 2^((1+1)×3) = 64,
`/ 1 right ( 2 + 3 ) * 4` builds 1/((2+3)×4), `sin π/6 ) ^ 2` builds
sin((π/6)²). In every case the input display shows exactly the computed
structure and the value equals the oracle of the displayed tree: no wrong
answer, but the behaviour differs from Grapher's `closeParen()`. With RIGHT
(canonical path) all 24 patterns build the intended tree.


