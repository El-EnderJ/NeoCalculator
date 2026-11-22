# CALC-CORE-INPUT-01 — Calculation input repair

This candidate repairs the **entered structure**, not the CAS answer. A completed
negative exponent no longer retains an invisible pending operand. Scientific
notation builds an explicit base 10 and puts the cursor in its exponent.
Incomplete expressions still fail validation and remain editable.

## Source and platform identities

- Repository: `C:\Users\Juan Ramón\Documents\Calculadora`, `main`.
- Starting/current HEAD: `ffb7082ba3b7db7741a8a603a5ad83f40c6c2682`.
- Starting index and tracked worktree: clean. No staging or commits in this task.
- Baseline runtime fingerprint:
  `d140125170f6cbfe85876724b7e618518202f99402da2dbe852e95328fc4874c`.
- First repair runtime fingerprint (the physically tested image):
  `9ecf528f5f1f1be5884c6a41e80e42800f63bf1ace09768f40e5d7089a4544d5`.
- Ignored `.vscode/settings.json` preserved byte-for-byte, SHA-256
  `00b9475d436d8c7bfa5b998dec3de2a8fe0c1caef7092597883599d60e395fd7`.

`out/calc-core-input-01/baseline.json`, separate binary-capable index/worktree
patches and `baseline-source.zip` preserve the starting snapshot; archive members
were read back and checked. Final path hashes are recorded separately. Existing
worktrees, reports and ignored evidence were retained.

These are distinct versions:

| Surface | Actual identity / evidence |
|---|---|
| Local source | HEAD above plus this unstaged repair |
| Native emulator | Fresh build of corrected runtime; preserved pre-fix binary is separate |
| Local web | Fresh Emscripten 6.0.3 build/package; actual keypad tested in three browsers |
| Public emulator | Inspected manifest `7fc1739f59e5-release-dirty`, revision `7fc1739f59e5`; not the historical `76b238e` reference |
| Initially installed WROOM | Independently digest-verified ordinary 03B image `693b8d759ac3878f0d69e9b956d6ed7c02caf86720a1ea7001954bf45809f160` |

Public WASM SHA-256 was
`781970a62cb35ad88aad03362640665782579101952240bf06ae4309cb043573`.
Actual public button clicks reproduced the defect. This task does not deploy the
local package or change public browser storage. The public site can therefore
still exhibit the old fault. `public-assets.json`, `public-cases.json` and original
screenshots preserve that observation; no claim that a stale cache caused it.

## Demonstrated first incorrect boundaries

| Input route | Baseline defect | Correction |
|---|---|---|
| Generic power, SUB then 3 | Exponent became `[Sub, Number(3), Empty]` | Replace only the pending placeholder adjacent to the insertion boundary |
| Calculation physical `(-)` / logical NEGATE | Key was unhandled: `10^3` remained positive | Handle NEG and NEGATE through the existing context-sensitive sign operator |
| Web `×10ˣ` button | `r9c2` dispatched POW, making a generic empty-base power | Dispatch EXP, as the canonical production keypad does |
| EXP from empty input | Old Mul/1/0/Power sequence inserted an orphan leading multiplication | Shared transactional `insertPowerOfTen()` creates visible `10^□` |
| SHIFT LOG (`10ˣ`) | Semantic `pow10` had no Calculation insertion path | Use the same structural helper (superseded in INTEGRATION-01: SHIFT LOG now inserts `log_□`; `10^□` is entered with EXP) |

For `66.63 [fraction] 2 × 1 0 [power] [SUB] 3`, the baseline tree was:

```text
Fraction
  numerator: Row[Number("66.63")]
  denominator: Row[Number("2"), Mul,
    Power(base=Row[Number("10")], exponent=Row[Sub, Number("3"), Empty])]
```

The displayed digits were present, but the serializer correctly returned
`ParseError`, diagnostic `incomplete expression`, empty serialized input. **Giac
never received this expression.** After repair, the exponent is `[Sub, Number(3)]`
and serialization is `((66.63)/(2*((10)^((-1)*3))))`; ordinary evaluation returns
33315. The serializer's rejection of genuinely incomplete structures is unchanged.

The negative-key failure is separate: the baseline ignored NEGATE and successfully
evaluated the *wrong entered structure*, `((10)^(3))`, to 1000. The web's lone small
5 has a third demonstrated cause: POW with an empty base. A normal 5, without a
template, passed independently on the baseline and public site.

`detecting/same-keys.numos` starts at HOME, opens Calculation, uses exactly the same
events on the preserved pre-fix and corrected binaries, and expects success.
Pre-fix exits 4; corrected exits 0 with its completion marker. It does not clean up
the AST or substitute a source string. Diagnostic baseline builds add observation
only. No introduction commit/bisect is claimed.

## Scientific key and editing contract

- After an operand: `2 [×10ˣ]` creates `2 × 10^□`.
- At an empty row or after an operator: creates `10^□`, without orphan Mul.
- The power's base is the owned Number `10`, never the mantissa or denominator.
- Exactly its exponent remains pending; the cursor enters that row at index zero.
- Repeated templates follow the current cursor slot (an exponent can itself
  contain a power). They do not silently move outside the exponent.
- Generic POW still supports an empty base; it does not become a base-10 shortcut.
- A sign alone retains its placeholder. A completed operand replaces that local
  placeholder; other incomplete slots remain incomplete.
- New nodes and vector capacity are prepared before publication. The scientific
  insertion returns failure without changing the prior tree/cursor on a thrown
  allocation failure. Layout/drawing paths and allocator policy are unchanged.

NEG/NEGATE use the editor's existing contextual unary-minus/binary-subtraction
representation. No precedence, decimal language, parser or Giac changes were made.
The independent tests distinguish `(-2)^2=4` from `-(2^2)=-4`.

Two **different** editor structures deliberately give different answers:

```text
66.63 [fraction] 2 [×10ˣ] [(-)] 3           -> 33315
66.63 [fraction] 2 [RIGHT] [×10ˣ] [(-)] 3   -> 0.033315
```

The second RIGHT leaves the denominator before inserting the product. Both trees
are inspected before EXE and preserved after it. Tests cover standalone `.5`,
`2.`, decimal mantissas, negative mantissas, parentheses, linear division with an
explicit group, numerator/denominator placement, sign deletion, mantissa edits,
history retrieval, FORMAT, Ans, AC, BACK, HOME and re-entry. Decimal expectations
use Python `Fraction`; approximate comparisons allow `max(1e-14, |value|*2e-13)`.
No global comma-to-point conversion was introduced.

## Permanent regression gate

```sh
python scripts/test-calculation-input.py --bin <native-program> --cycles 10
python scripts/test-tutor-composition-host.py --source <snapshot> \
  --build <native-build> --out <test-output> --tests calculation_input_checks
node tests/wasm/keycode-catalog.mjs
NUMOS_BROWSER=chromium node tests/wasm/calculation-input.mjs
```

Use Firefox/WebKit in the last command for the other engines. The browser test
clicks the real visible virtual keys on both shell and reusable component, using
isolated profiles with persistence enabled; it also checks desktop keyboard input
separately. No `window.numos` expression/AST injection substitutes for key clicks.

The native gate runs 31 fixtures through logical keys and canonical production
row/column events, then recovery, editing/history/Ans, four negative detector
controls and lifecycle. It checks tree ownership/cursor validity, completeness,
selected exact serializations and mathematical value, not just absence of an
error label. The negative controls reject a missing base, lost negative sign,
wrong denominator grouping and a stale previous answer.

The host editor suite separately injects persistent failures through MathNode and
C++ vector allocation: 17 failing construction positions across empty/mantissa
inputs. Faults remain active through unwinding; tree/cursor and live C++ allocation
count restore, followed by healthy insertion without resetting Giac. This does not
prove all Giac malloc/ESP heap failures safe. The allocator seam is default-off
and never enabled in a product build.

The emulator workflow runs the quick Calculation gate immediately after building
native, before the affected application suites. The keypad integrity check and
editor/failure suite are also permanent gates. Previous coverage missed this
combination: `calc_semantic_negative.numos` explicitly used subtraction instead of
the known ignored negative key; the old power fixture used a positive exponent;
backend serializer tests constructed ASTs directly. Tutor success did not cover
Calculation's event dispatch or the web scientific button.

## First repair software evidence (before the desktop follow-up)

| Gate | Result |
|---|---|
| New native Calculation gate | 74/74, including ten mixed cycles |
| Previous Calculation app scripts | 25 scripts / 118 assertions PASS |
| Fixed 64 KiB LVGL configuration | 74/74, 50 mixed cycles; identical HOME object/timer/payload/handle boundaries asserted |
| Editor/serializer persistent faults | PASS, 17 construction failure positions |
| Ordinary Giac suite | 179 PASS / 0 FAIL, including context, calculation and retained Grapher boundaries |
| Separate Giac Calculus / cross-app / Neo suites | 56 / 14 / 44 checks PASS |
| Existing tutor corpora and challenges, composition | PASS; exact commands/exit codes in `corpus/commands.json` |
| Nonlinear/transcendental/trig/periodic/composition checks | PASS against matching native objects |
| Notation and MathEnginePhaseRegression | PASS; no font/renderer changes |
| Equations, Calculus/F7, Grapher Templates | PASS; Calculus final lifecycle used 50 cycles |
| Native, WROOM normal/bring-up/demo, CAM normal/validation | PASS with pinned dependencies |
| Local web shell + component | Chromium, Firefox, WebKit PASS by real virtual-key clicks, rebuilt final source |
| Headless WASM-MATH | Release and Debug build/package/regression PASS |
| Whitespace | `git diff --check` PASS |

Failed first harness runs are retained: missing SDL DLL PATH; Calculus ten-cycle
run whose existing summarizer discards its first ten samples (same failure on the
baseline); fixed-pool frame allowance ending before the completion marker; local
web's missing pinned LVGL junction; host diagnostic link's SDL main alias/Windows
Unicode output path; temporary probe's Serial macro expansion. Corrected reruns
are identified, not silently substituted. None required weakening mathematical
expectations, goldens or masks. No new golden comparison is claimed.

The exact 320×240 screenshots were inspected: base 10 is visible, the negative
exponent has the correct sign and both fraction structures produce their distinct
values. `out/calc-core-input-01/visual-review.zip` contains its HTML and referenced
original PNGs; relative resources are checked. The prepared web package is
`web-candidate.zip` (SHA-256
`aad312b1f4dee3812b5bc96cd2bc80eb27c24d2c7cd4870a620b8273795d8449`),
with individual resource hashes in `web-package-manifest.json`. It is **not deployed**.

## Resources

| Ordinary WROOM | Accepted 03B baseline | Corrected | Delta |
|---|---:|---:|---:|
| Linked flash | 5,682,397 B | 5,683,565 B | +1,168 B |
| Firmware image | 5,682,768 B | 5,683,936 B | +1,168 B |
| Static RAM | 119,160 B | 119,160 B | 0 B |
| IRAM text | 60,407 B | 60,407 B | 0 B |

Pinned build: PlatformIO 6.1.19 / espressif32 6.12.0, Xtensa GCC 8.4.0,
Arduino ESP32 2.0.17, LVGL 9.5.0; native MinGW GCC 15.2.0, web Emscripten 6.0.3.
The helper's Xtensa own frame is 112 B (`entry a1,112`); this excludes callees,
allocation and exception unwinding. No new layout/render allocations, global
cache, pool enlargement, clock or font changes.

Scientific insertion retains the ordinary expression nodes: Power, two Rows,
Number(10), Empty and optional Mul, plus vector storage. It is not zero heap cost.
Ownership follows the active editable tree/history. Host sizeof values are not
ESP sizes. Fixed-pool HOME samples were identical across 50 cycles: 67 objects,
3 timers, zero retained Giac handles, monitored payload 57,704 B/free 19,336 B.
Monitored payload is distinct from the reserved 65,536 B and from total process
heap. Physical measurements and manual acceptance are recorded below separately.

## Physical acceptance

The user explicitly authorized this task's application-only update. Rediscovered
USB 303A:1001 / COM9 / MAC `44:B1:76:A7:B7:2C`; active OTA slot offset `0x10000`,
capacity `0x640000`, determined from the board's current partition/OTA metadata.
The previous ordinary image was digest-verified and preserved before writing.
Prefix readbacks remain byte-identical. No bootloader/partition/NVS/LittleFS/
security write, erase-all or format command was used.

The isolated temporary probe image is
`a5df1a4833e09fcdf648f473d366180e9c92f5fd50ff1dff33e3cc2da7e21739`.
It is independently read back and booted. Its private CP KEY command injects
canonical production electrical coordinates through SystemApp's existing semantic
resolver into the actual Calculation app; CP SNAP only observes its current
serialized input/result and resources. This is automated semantic evidence, not
a claim that the user's fingers pressed the matrix keys.

The rerun completed all 31 key-built fixtures (input serialization and result),
four incomplete-input recovery paths, sign editing/deletion, history, FORMAT/Ans,
preservation of Ans after an invalid expression, and ten mixed
evaluate/error/correct/BACK/HOME/re-entry cycles: 124 boundary snapshots. No crash,
watchdog or unexpected reset appeared. The first run stopped because one USB
telemetry line was incomplete; its preceding successful `CP-EVAL` recorded 1/8.
The complete rerun retained all logs and added read-only snapshot retry handling;
it required zero retries. No firmware change was needed for that transport event.

Calculation's existing 50-entry history deliberately remains owned by the app
across firmware HOME transitions. Its varying AST sizes explain why replacing
small historical inputs with the larger fraction changes PSRAM usage. A separate
bounded run replaced all 50 entries with the same fraction, without resetting
Giac, and then repeated five identical enter/evaluate/HOME cycles. Results:

| Settled physical HOME boundary | Observed value |
|---|---:|
| Internal free / largest | 143,656 / 102,388 B, identical in all five |
| PSRAM free | 8,170,995–8,171,027 B, reversible 32 B range |
| Largest PSRAM block | 8,126,452 B, identical |
| LVGL monitored total / free / largest | 61,680 / 41,220 / 40,168 B, identical |
| Objects / screens / timers | 71 / 5 / 3, identical |
| Minimum unused calling-task stack | 57,940 B |

The pinned Xtensa port defines `portSTACK_TYPE uint8_t`; the probe reports
`sizeof(StackType_t)=1`. These are bytes, not figures multiplied by four.
The monitored LVGL high-water was 24,780 B. Sampling covers boundaries, not the
total transient allocator peak. Independent minima must not be added together.
The firmware's retained history and native app destruction are different existing
lifecycle contracts; their absolute memory totals are not directly comparable.

Instrumented evaluation segment (entry through ordinary engine return, before
result/history widget work), in milliseconds:

| Serialized mathematical input | n | First observed | Median | Maximum |
|---|---:|---:|---:|---:|
| 5 | 2 | 19.671 | 19.377 | 19.671 |
| 10^(-3) | 5 | 19.907 | 19.907 | 20.802 |
| 2×10^(-3) | 4 | 20.030 | 20.389 | 20.427 |
| 66.63/(2×10^(-3)) | 77 | 20.595 | 20.507 | 20.773 |
| (66.63/2)×10^(-3) | 1 | 20.350 | 20.350 | 20.350 |

Samples come from the declared acceptance/retention runs, with Giac retained
between cases. First observed is not a cold-boot claim. These are not LCD
first-pixel timings or a before/after speed benchmark. Raw samples and scope are
in `board/analysis.json`.

The ordinary image has been written, esptool-verified, independently read back in
full and booted with `[BOOT] OK`:
`0ed7ba1c63f17bccfeb1ad3f2c47e35b6e0c43fc48021a302673d8f27995846f`.
The ordinary binary contains none of the private CP probe markers. Boot reports
the unchanged production SAFE display configuration (40 MHz write / 10 MHz read)
and available PSRAM. The existing missing display-profile record and unavailable
variables-file diagnostics also appeared on probe boot; this task did not change
or repair storage. No temporary probe remains installed.

Human observation: the user subsequently reported, “sí funciona ahora la cuenta
en la PCB”. This confirms the repaired calculation in their use; it is not a
separate recorded walkthrough of both prescribed key sequences or the entire
automated matrix.

## Desktop launch follow-up

The user reported that the Windows program did not open or closed immediately.
The documented Windows launcher reproduced `Emulator executable not found`:
neither `C:/.piobuild/numOS/emulator_pc/program.exe` nor the `.pio/build` fallback
existed. The tested binary was still in the isolated build directory. SDL2 and
the compiler were installed; the dependency checker confirmed the absent normal
build artifact. This was a local delivery-path omission, separate from the input
defects and from the undeployed public web package.

The exact tested native executable was copied into the launcher’s configured
build folder with its four existing runtime DLLs (SDL2, libgcc, libstdc++ and
libwinpthread). Hashes and origins are in `desktop-launch/manifest.json`; no
toolchain upgrade, global PATH edit or source rebuild substitution was made.
The existing PowerShell launcher now passes its startup/exit check. No source,
firmware or editor settings were changed for this local artifact repair.

## Desktop negative-power follow-up (24 September 2026)

The user's Windows log and screenshot demonstrated a missed **editing** path.
The previous simple key-entry tests passed but did not establish that deleting
nested templates worked. The full supplied event sequence now has a permanent
fixture: `tests/emulator/calculation-desktop-recovery.numos`. It replays SDL text,
key press/release and repeat events, including the two literal carets reported by
SDL; it does not inject a finished AST. No keyboard layout remapping, timing
filter or duplicate-character suppression was added.

The immediate source remained the uncommitted first repair above. Its patches,
readable feature archive, hashes and actually launched executable were preserved
under `out/calc-core-input-01/negative-power-followup/`. The index remains empty.
The ignored editor settings retain the same SHA-256. No history was rewritten.

### Reproduced boundaries and repair

1. **Empty-slot deletion:** `2 ^ ^ 2 DEL DEL - 2` left an empty nested power
   beside the new negative exponent. DEL at the start of an empty trailing slot
   merely navigated left; it did not remove the pending template. Repeating DEL
   could strand more structures to the right. The complete user log reproduced
   the supplied screenshot, including Syntax ERROR.
2. **Invisible missing operands:** `drawEmptyBaseline()` drew nothing, on the
   assumption that a cursor sufficed. Only the active slot has a cursor; after
   EXE Calculation disconnected it entirely. Thus a tree with missing bases and
   exponents appeared to contain only the complete number `2^-2`. Serialization
   correctly rejected it as `incomplete expression`; Giac was **not called**.
3. **Signed linear division:** `2 [DIVIDE] - 2` emitted `2/(-1)*2`, yielding -4
   instead of -1. The serializer now groups the signed operand as `2/((-1)*2)`.
   It preserves left-to-right product/division precedence and all original
   operands; this is not string cleanup or an alternate evaluator.
4. **Template flattening order:** removing the parentheses in `(2-3)` reversed
   the retained nodes. A shared template-unwrapping operation now moves them in
   their original order. The detecting sequence is `( 2 - 3 ) DEL`.

DEL in an empty exponent now unwraps its power while preserving the base; an
empty denominator similarly preserves its numerator. Populated sibling slots are
not silently removed by this empty-slot rule. Generic power construction and
unwrapping reserve capacity before mutation, preserving ownership and the cursor
if preparation fails. Other template deletion uses the same ordered operation.

Pending slots in an editable expression have an outline inside their existing
measured bounding box. Failed Calculation input retains this feedback after EXE
and from history. Valid results, STIX glyph metrics and delimiter geometry are
unchanged. The serializer still rejects incomplete slots. For example, typing
`2 ^^ -2` without correcting the missing base remains incomplete and now shows
that base explicitly; it is not arbitrarily interpreted as `2^-2`.

After replaying **the entire supplied log**, the tree is exactly:

```text
Row[Power(base=Row[Number(2)], exponent=Row[Sub, Number(2)])]
serialized: ((2)^((-1)*2))
ordinary Giac result: 1/4
```

The simple clean path `2 ^ - 2` already worked on both preserved binaries. The
actual deletion reproducer, signed division, and unwrapping-order detector each
fail on the preserved pre-follow-up executable and pass after repair.

### Expanded permanent gate and evidence

The quick gate now contains 49 key-built fixtures through logical and production
routes, plus 120 generated signed-power evaluations: bases -3, -2, 2, 3, 10;
negative exponents 1, 2, 3, 4, 6, 12; both sign keys. These use independent Python
Fraction oracles and exact rational comparisons, with numerical checks additional.
There are also fractional/decimal exponents, negative bases, nested powers,
scientific fractions, deletion/re-entry, history/Ans and incomplete recovery.
The linear DIVIDE alias has no separate physical key and remains a logical-route
control; the actual fraction key is tested separately.

The SDL fixture and a framebuffer rectangle detector cover the previously missed
keyboard/editing and invisible-placeholder failures. Pixel evidence supplements
the structural/serialization checks; it does not prove mathematical equivalence.
Persistent allocator tests cover 33 failing preparation positions, including
ordinary/scientific power creation and unwrapping capacity, followed by healthy
recovery. This is C++ allocation evidence, not a total Giac/ESP heap-safety proof.

`negative-power-review.zip` includes the original 320x240 before/after images,
pending-slot and fraction images, and an HTML index with checked relative links.
These images were inspected directly. Complete formulas keep their previous
geometry; only incomplete editable slots gain visible ink inside existing bounds.

First failed runs are retained: a detector initially moved RIGHT once too often
and therefore failed to exercise flattening; the corrected sequence above detects
the baseline defect. Browser scalar-output checks initially rejected valid exact
forms `1/-8` and `(sqrt(4))^-1`. Their test-only reader now admits signed rational
denominators and independently checked perfect-square constant roots. Product
answers and expected mathematical values were not changed to satisfy these tests.

Final follow-up gates (raw evidence is under `negative-power-followup/`):

| Gate | Result and scope |
| --- | --- |
| Calculation native | 114/114 script checks, including 49 input fixtures, the supplied SDL replay and 120 exact signed-power evaluations |
| Normal Windows launcher | Same supplied SDL replay in a real SDL window: complete single power, exact serialization, result 1/4 |
| Fixed 64 KiB pool | 114/114 checks; 50 mixed cycles; stable HOME counters |
| Allocation recovery | 33 persistent failing preparation positions; subsequent healthy recovery |
| Browser virtual keypad | Chromium, Firefox and WebKit; standalone shell and reusable component; desktop keyboard recovery also passes |
| Existing Calculation/Calculus scripts | 49 scripts, 247 assertions, no failures |
| Shared input consumers | Equations rebuild, Calculus (50 cycles), Grapher Templates (50 cycles), STIX/stretchy delimiters and MathEnginePhaseRegression pass |
| Ordinary math | Giac (179), Calculus (56), cross-app (14), Neo (44) assertions pass; WASM-MATH Release/Debug pass |
| Tutor preservation | All 900 old/composition traces unchanged mathematically; only timing fields and executable-path metadata differ; checker/mutation suites pass |
| Firmware builds | WROOM normal/bring-up/demo and CAM normal/validation pass with pinned dependencies; no image from this follow-up flashed |

Re-run the fast application gate with
`python scripts/test-calculation-input.py --bin C:/.piobuild/numOS/emulator_pc/program.exe --out out/calculation-recheck --cycles 10`.
The existing native CI runs this gate before the large tutor suites. The test
emits the complete, launchable SDL wrapper alongside its log. Run the browser
keypad gate using `npm run calculation-input` in `tests/wasm`, with the existing
`NUMOS_BROWSER` and `NUMOS_SOURCE` options for each pinned local web build.

The standalone SDL replay file is a fixture fragment that begins **inside
Calculation**. One manual launcher attempt incorrectly ran that fragment from
HOME and failed its assertions; its log is retained. Running the generated
wrapper that enters Calculation first passes in the actual window. An earlier
four-second window run only established startup, not completion of the fixture.
Neither incomplete run is counted as mathematical acceptance.

The final web package is `negative-power-followup/web-candidate.zip`,
2,389,612 B, SHA-256
`95a56fb5e0ea1d7ea292775105e3c60e34bf4b34a64ba65f051252df7273f727`.
The normal launcher executable SHA-256 is
`94fcccfc5bbe88f4cb5d185559c430cec9d47cdf9d363713d9c5068bfd227fba`.
These are local artifacts; neither is evidence of a public deployment or a new
board installation.

### Follow-up resource and physical scope

Ordinary WROOM: linked flash **5,682,929 B**, image **5,683,296 B**, static RAM
**119,160 B**, IRAM text **60,407 B**. Relative to the first repair, linked flash
and image decrease by 636/640 B; static RAM and IRAM are unchanged. This does not
mean zero dynamic cost. Unwrapping removes its former temporary vector; power
construction still owns nodes/vector capacity. Placeholder drawing uses a stack
LVGL rectangle descriptor and the existing LVGL draw queue, with no new widget,
page cache or typesetting-node allocation per frame.

Xtensa own frames: generic power 48 B, unwrap 48 B, serializer row 64 B,
placeholder drawing 208 B. These exclude callees, LVGL draw-task allocation and
exception handling; they are not measured task-stack peaks. Fifty fixed-pool
native cycles return to 67 objects, 3 timers, zero retained Giac handles and
57,704 B monitored payload / 19,336 B free. Reserved LVGL pool remains 65,536 B.
The repeated boundary samples do not establish total allocator peak usage.

Final source/runtime fingerprint:
`5f499072f14891e0df0b40f3532ad6ad68cea723de70094d057b8e6814637647`.
Prepared ordinary image SHA-256:
`8f5a46ed1d82620d8f4ce347f5e41df982ec988569c7b0b8a6edec16d7483e65`.

**This follow-up has not been flashed.** The PCB still has the earlier ordinary
image identified in Physical acceptance; the user's confirmation applies to that
image. No new physical timing, visual approval or heap evidence is claimed here.
The Windows launcher is updated with the tested executable and DLLs. Local web
artifacts are prepared separately; the public emulator is not deployed by this
repair. Existing expression-size/domain limits remain in force: the finite
regression matrix is not a proof for arbitrary inputs or a promise that every
incomplete expression should evaluate.

## Exact changed-file boundary

```text
.github/workflows/emulator-build.yml
scripts/test-calculation-input.py
scripts/test-tutor-composition-host.py
src/apps/CalculationApp.cpp
src/apps/CalculationApp.h
src/apps/EquationsApp.cpp
src/hal/NativeHal.cpp
src/math/CalculationEngine.cpp
src/math/CursorController.cpp
src/math/CursorController.h
src/math/MathAST.cpp
src/math/MathAST.h
src/ui/MathRenderer.cpp
tests/emulator/calculation-desktop-recovery.numos
tests/emulator/calculation-input.json
tests/host/calculation_input_checks.cpp
tests/wasm/calculation-input.mjs
tests/wasm/keycode-catalog.mjs
tests/wasm/package.json
wasm/numos-keypad.js
docs/CALC_CORE_INPUT_01.md
```

All probe overlays, binary backups, generated screenshots, packages and logs stay
under ignored output/cache paths. No new visible prose was added; the existing
EN/ES catalog and mathematical language are unchanged. No tutor family, checker,
budget or ordinary-answer authority was changed.
