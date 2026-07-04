# NumOS — Emulator NativeHal App Routing and Input Spec (P-05)

> Part of the P-05 emulator app-enablement spec set. Companion documents:
> `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md` (architecture/product),
> `NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX.md` (per-app detail),
> `NUMOS_EMULATOR_APP_QA_GOLDEN_AND_SCRIPT_PLAN.md` (test plan),
> `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md` (tickets).
>
> Audited against the working tree of branch `claude/emulator-app-parity-p05-spec-1lf7bu`
> (based on `main`, 2026-07-04). Every current-state claim cites `file:line`.
> Sections marked **[PROPOSED]** are design, not current behavior.

---

## A. Current NativeHal routing (verified)

### A.1 AppMode enum

The emulator has its own mode enum, disjoint from the firmware `Mode` enum:

```cpp
enum class AppMode : uint8_t {
    SPLASH, MENU, CALCULATION, SETTINGS, MATH_SHOWCASE,
    STATISTICS, PROBABILITY, SEQUENCES, REGRESSION, GRAPHER
};
```
(`src/hal/NativeHal.cpp:195-206`.) Ten states; eight app-bearing states. Firmware `Mode`
has 24 states (`src/SystemApp.h:76-103`) including `STEP_VIEW`, `APP_TABLE` and all 13
firmware-only apps. There is no shared header; the two enums are hand-maintained.

### A.2 App instances and construction

- All emulator app instances are file-scope statics: `g_calcApp`, `g_settingsApp`,
  `g_statsApp`, `g_probApp`, `g_seqApp`, `g_regApp`, `g_grapherApp`
  (`NativeHal.cpp:249-255`), plus the MathShowcase object cluster
  (`g_showcaseScreen/Bar/Canvas/Caption/Root/Index`, `NativeHal.cpp:262-267`).
- Construction happens in `transitionToMenu()` after the splash completes
  (`NativeHal.cpp:877-918`). CalculationApp is eagerly `begin()`-ed
  (`NativeHal.cpp:882-883`); every other app is constructed only, relying on the
  `load() → if (!_screen) begin()` lazy pattern (`NativeHal.cpp:887-906`), mirroring
  the firmware rationale of `SystemApp.cpp:100-104` (eager begin() of many apps
  exhausts the LVGL heap on device).

### A.3 launchApp and the dead-card path

`launchApp(int appId)` (`NativeHal.cpp:924-997`) switches on the **launcher card id**
(same ids as `MainMenu::APPS[]`): 0 Calculation, 1 Grapher, 4 Statistics,
5 Probability, 6 Regression, 7 Sequences, 10 Settings, plus the emulator-only
`APPID_MATH_SHOWCASE = 100` (`NativeHal.cpp:261`, not a launcher card).
Any other id — i.e. cards 2, 3, 8, 9, 11-19, which the launcher **does render**
in the emulator because `ui/MainMenu.cpp` is whitelisted (`platformio.ini:263`)
and `APPS[]` has 20 unconditional entries (`src/ui/MainMenu.cpp:88-113`) — falls to:

```cpp
default:
    std::printf("[APP] App %d no implementada en simulador\n", appId);
```
(`NativeHal.cpp:993-995`.) The menu stays loaded, `g_mode` stays `MENU`, no error
surfaces to a script. This is the "visible-but-dead card" behavior; see §C.2 and
special-question answers in the parity spec §J.

### A.4 open_app / assert_app command plumbing

- `open_app NAME` is parsed at script load; `scriptAppNameToId()` maps name → launcher
  id (`NativeHal.cpp:1424-1438`). Unknown names fail script load with exit 2
  (`NativeHal.cpp:1521-1532`). The command executes by calling the same
  `launchApp(id)` as a menu ENTER (`NativeHal.cpp:1727-1729`).
- `assert_app NAME` compares against `activeAppName()`, a switch over `g_mode`
  (`NativeHal.cpp:1666-1678`); name canonicalization in `canonicalAppName()`
  (`NativeHal.cpp:1402-1418`).
- Enabling one new app therefore requires touching, today, **five hand-maintained
  switch/map sites** in NativeHal.cpp alone: the `AppMode` enum (195-206), instance
  pointer + construction (249-255, 877-918), `launchApp` (924-997),
  `performAppTeardown` (1007-1046), `dispatchKey` (575-769) — plus
  `canonicalAppName` (1402-1418), `scriptAppNameToId` (1424-1438) and
  `activeAppName` (1666-1678). Eight sites total. This is the drift engine behind
  risk EMU-1 (`NUMOS_RISK_REGISTER.md` §4).

### A.5 Home/MODE handling and deferred teardown

- In every app-mode case of `dispatchKey`, `MODE` on the down-edge calls
  `returnToMenu()` (e.g. `NativeHal.cpp:625-627, 651-654, 737-740, 755-758`).
- `returnToMenu()` (`NativeHal.cpp:1065-1087`): flushes any prior pending teardown,
  records `g_teardownMode = g_mode` + `g_teardownStartTick = lv_tick_get()`, resets
  keyboard modifiers via `vpam::KeyboardManager::instance().reset()`
  (`NativeHal.cpp:1081`), loads the menu (fade-in starts with the app screen still
  alive), and sets `g_mode = MENU`.
- The deferred `end()` runs in the main loop once `lv_tick_elaps(g_teardownStartTick)
  >= TEARDOWN_DELAY_MS` (260 ms > the 200 ms menu fade; `NativeHal.cpp:233,
  2088-2092`), via `performAppTeardown(mode)` (`NativeHal.cpp:1007-1046`).
  CalculationApp is special-cased: `end()` then immediate `begin()` re-create
  (`NativeHal.cpp:1010-1015`); all others rely on lazy re-begin in `load()`.
- `flushPendingTeardown()` resolves a pending teardown synchronously before
  launching another app (`NativeHal.cpp:1050-1055`, called at `launchApp` entry
  `NativeHal.cpp:930`), mirroring firmware `flushPendingTeardownNow`
  (`SystemApp.cpp:220-232`, called at `SystemApp.cpp:735`).
- The delay is measured with `lv_tick`, so it is deterministic under
  `--deterministic` (comment `NativeHal.cpp:218-229`).

### A.6 Key dispatch model

`dispatchKey(KeyCode, KeyAction, bool isDown)` (`NativeHal.cpp:575-769`) is the single
dispatch point for both SDL input and script replay (extracted for exactly that
parity, comment 567-574). Per mode:

- `SPLASH`: keys ignored (579-580).
- `MENU`: arrows go to `MainMenu::moveFocusByDelta()` — the same 2D grid model the
  firmware uses (586-608; firmware equivalent `SystemApp.cpp:687-707`); `GRAPH`
  ('g') launches app id 1 directly (612-615); all other keys are synthesized as
  LVGL PRESS+RELEASE pairs through `LvglKeypad::pushKey` so ENTER fires
  `LV_EVENT_CLICKED` on the focused card (617-619).
- Every app mode: `MODE`+down-edge → `returnToMenu()`; everything else (including
  RELEASE) is forwarded to `<App>::handleKey(KeyEvent)` with `row=col=-1`
  (623-749). MathShowcase is inline-handled (751-767): LEFT/UP previous case,
  RIGHT/DOWN/ENTER next case, no app object.

SDL translation: `mapSdlToKeyCode` for navigation/control/letter shortcuts
(`NativeHal.cpp:318-400`); `SDL_TEXTINPUT` + `mapTextChar` for digits/symbols with
OS keyboard layout applied (`NativeHal.cpp:415-444, 792-803`). Script translation:
`scriptNameToKeyCode` (`NativeHal.cpp:456-565`).

### A.7 Script command parser

`.numos` grammar (parse: `loadScript`, `NativeHal.cpp:1457-1622`; execute:
`scriptStepBegin`, 1697-1850):

| Command | Semantics | Source |
|---|---|---|
| `wait N` | consume N frames | 1483-1491, 1705-1707 |
| `key NAME` | PRESS+RELEASE via dispatchKey | 1492-1502, 1708-1711 |
| `keydown NAME` / `keyup NAME` | PRESS only / RELEASE only | 1492-1502, 1712-1717 |
| `screenshot PATH` | deferred PPM capture after this frame's render | 1503-1509, 1718-1721; capture 1855-1871 |
| `log "msg"` | stdout | 1510-1520 |
| `open_app NAME` | launchApp(id) | 1521-1532, 1727-1729 |
| `assert_app NAME` | active mode name equality | 1533-1542, 1732-1740 |
| `assert_result TEXT` / `assert_result_contains TEXT` | CalculationApp-only, reads `debugLastResult()` | 1543-1557, 1741-1765 |
| `assert_no_error` | CalculationApp result `.ok` | 1558-1562, 1766-1775 |
| `assert_error [TEXT]` | CalculationApp result is error | 1563-1575, 1778-1804 |
| `assert_variable NAME VALUE` | reads `VariableManager` singleton | 1576-1595, 1805-1820 |
| `assert_menu_focus NAME\|ID` | `MainMenu::debugFocusedCardId()` | 1596-1611, 1823-1848 |

Contract properties (CI-relied-upon, append-only per the context header's fragile-file
list): whole-script validation before execution, exit 2 on parse error
(`NativeHal.cpp:1882-1884`); assertion failure exit 4 (`assertFail`, 1682-1688);
screenshot write failure exit 3 (1855-1866); one command per frame, executed before
SDL events and `lv_timer_handler` (1697-1701, 2049).

Notable asymmetries:

- `assert_result*`, `assert_no_error`, `assert_error` are **hardwired to
  CalculationApp** (`g_mode != AppMode::CALCULATION` → assert fail, 1743-1747,
  1779-1783). No other app exposes result state.
- Only CalculationApp (`debugHasResult/debugLastResult`,
  `src/apps/CalculationApp.h:95-107`) and MainMenu (`debugFocusedCardId`,
  `debugResolveCardToken`, `debugCardNameById`, `src/ui/MainMenu.h:58-79`) have
  NATIVE_SIM debug hooks. Grapher, Settings, Statistics, Probability, Sequences,
  Regression have **none** — they are pixel-opaque to the assert harness.

### A.8 Exit cleanup

At process exit, apps are `end()`-ed and deleted (`NativeHal.cpp:2142-2152`).
**Verified divergence:** `g_grapherApp` is missing from this cleanup block — calc,
settings, stats, prob, seq, reg are deleted; Grapher is neither ended nor deleted
(`NativeHal.cpp:2144-2149`). Harmless today (process exit + `lv_deinit()`), but it
breaks the "every app has a symmetric end" invariant and would corrupt any future
LVGL leak accounting. Recorded as ticket ROUTE-03 in the roadmap.

---

## B. Current firmware routing (verified)

### B.1 SystemApp launcher path

- `SystemApp::begin()` news all 20 apps (21 with `NUMOS_MATH_VISUAL_APP_ENABLED`)
  without calling `begin()` (`SystemApp.cpp:105-127`), creates the LVGL MainMenu,
  binds `launchApp` as the card callback (`SystemApp.cpp:135-138`), then mounts
  LittleFS and loads `/vars.dat` (`SystemApp.cpp:144-156`).
- `MainMenu::APPS[]` (`src/ui/MainMenu.cpp:88-113`) is the card source of truth:
  ids 0-19 unconditional + id 20 under `NUMOS_MATH_VISUAL_VERIFY`. ENTER on a card →
  `launchApp(id)` → `switchApp(id)` sets `_mode` (`SystemApp.cpp:885-913`) and
  `<app>->load()` (`SystemApp.cpp:744-855`).
- The separate `_apps` vector (`initApps()`, `SystemApp.cpp:162-180`) feeds only the
  legacy TFT `renderMenu()`; its "11-18 are hidden/experimental" comment
  (`SystemApp.cpp:175`) does **not** apply to the LVGL launcher.

### B.2 App construction/destruction

- Deferred teardown: `returnToMenu()` records `_pendingTeardownMode` + `millis()`
  (`SystemApp.cpp:860-883`); `update()` calls `teardownModeNow()` after 250 ms
  (`SystemApp.cpp:243-250`); `launchApp` flushes any pending teardown first
  (`SystemApp.cpp:735`).
- `teardownModeNow(Mode)` is the firmware analogue of `performAppTeardown`: one
  `end()` case per app (`SystemApp.cpp:182-218`), plus a memory-probe exit marker
  under `NUMOS_MEM_PROBE_ENABLE` (`SystemApp.cpp:213-217`) — a leak canary the
  emulator has no equivalent of.

### B.3 Keyboard dispatch

- `SystemApp::handleKey` filters to PRESS/REPEAT only (`SystemApp.cpp:461`) — note
  the emulator forwards RELEASE into apps and relies on each app to ignore it
  (`NativeHal.cpp:629-644`), whereas firmware apps never see RELEASE at all.
- Global modifier handling: SHIFT/ALPHA toggle `KeyboardManager` **except** when
  the active mode is APP_CALCULATION or APP_CALCULUS, which own their modifier
  handling (`SystemApp.cpp:484-493`). The emulator has no such global layer —
  every key goes straight to the app.
- SHIFT+AC power-off is intercepted globally and currently disabled
  (`SystemApp.cpp:475-481`, `powerOff()` stub `SystemApp.cpp:1052-1057`).

### B.4 MODE/HOME and app-specific exit behavior

Per-app exit rules in `SystemApp::handleKey` (`SystemApp.cpp:495-671`):

| App | Exit keys | Extra behavior |
|---|---|---|
| Most apps | MODE | plain `returnToMenu()` |
| PeriodicTable | MODE **or AC** (`SystemApp.cpp:580`) | — |
| CircuitCore | MODE; also AC when toolbar focused | `autoSave()` before exit (`SystemApp.cpp:595-606`) |
| Fluid2D | MODE | `autoSave()` before exit (`SystemApp.cpp:608-615`) |
| Fractal | **AC only** (no MODE case) | plus app-initiated exit via `consumeExitRequest()` (`SystemApp.cpp:649-658`) |
| MathVisual | MODE or AC (`SystemApp.cpp:660-666`) | — |
| Grapher | MODE; AC at tab level on Expressions tab also exits (`SystemApp.cpp:1019-1033`) | emulator does **not** replicate the AC-exit path — GrapherApp receives AC and handles internal focus retreat only (`NativeHal.cpp:733-749`) |
| Equations | MODE, but keys forwarded only `if (_equationsApp->isActive())` (`SystemApp.cpp:513-519`) | Equations also gets a cooperative `update()` tick (`SystemApp.cpp:265-269`) |
| Fractal | — | gets `update()` tick (`SystemApp.cpp:290-292`) |

The emulator main loop calls **no app `update()` at all** — there is no per-frame
app tick in `NativeHal.cpp`'s loop (2045-2130). Any app whose behavior depends on
`update()` (EquationsApp staged solve pipeline, FractalApp render state machine)
cannot be enabled without adding an update dispatch (ROUTE-04).

---

## C. Divergences (verified inventory)

| # | Divergence | Emulator evidence | Firmware evidence | Impact on P-05 |
|---|---|---|---|---|
| D1 | Two hand-written mode enums/dispatchers | `NativeHal.cpp:195-206` | `SystemApp.h:76-103` | every new app touches 8 NativeHal sites (§A.4) |
| D2 | Dead launcher cards: 13 cards render but only print a console line on ENTER | `NativeHal.cpp:993-995`; cards from `MainMenu.cpp:88-113` | all 20 ids launch (`SystemApp.cpp:744-855`) | scripts can't detect a failed launch; see F.3 |
| D3 | RELEASE events forwarded to apps (apps must self-filter) | `NativeHal.cpp:629-644` | PRESS/REPEAT filter at dispatcher (`SystemApp.cpp:461`) | new apps must tolerate RELEASE; add to checklist |
| D4 | No global SHIFT/ALPHA layer in emulator | absent from `dispatchKey` (575-769) | `SystemApp.cpp:484-493` | apps relying on SystemApp toggling modifiers behave differently |
| D5 | Modifier reset on returnToMenu exists in both, but emulator resets **only** there | `NativeHal.cpp:1081` | firmware also resets on SHIFT+AC (`SystemApp.cpp:477-478`) | minor |
| D6 | No app `update()` tick in emulator loop | loop body `NativeHal.cpp:2045-2130` | `SystemApp.cpp:238-304` | blocks Equations (staged pipeline) and Fractal (state machine) until ROUTE-04 |
| D7 | Grapher AC-at-tab-level exit path missing in emulator | `NativeHal.cpp:733-749` | `SystemApp.cpp:1024-1030` | existing parity gap; scripts use `home` instead |
| D8 | CircuitCore/Fluid2D autoSave-on-exit is SystemApp-side, not app-side | n/a in emulator | `SystemApp.cpp:595-615` | if these apps are ever enabled, the emulator must replicate the exit hook or storage behavior diverges |
| D9 | CalculationApp teardown re-begins immediately in emulator | `NativeHal.cpp:1010-1015` | firmware relies on lazy `load()` re-begin | screen pre-exists on relaunch; cosmetic timing difference |
| D10 | `g_grapherApp` missing from exit cleanup | `NativeHal.cpp:2144-2151` | firmware has no process exit | breaks symmetric-teardown invariant (ROUTE-03) |
| D11 | Settings/vars globals defined twice | `NativeHal.cpp:125-127` | `main.cpp:31-33` (per header comment `NativeHal.cpp:119-124`) | values match today; drift possible |
| D12 | Emulator ships a committed persistent store: `emulator_data/vars.dat` is git-tracked and loaded at boot | `NativeHal.cpp:2171-2179`; `git ls-files` shows `emulator_data/vars.dat` | firmware starts from device LittleFS | fixture-hygiene precondition; see persistence sandbox spec |
| D13 | `VariableContext` NVS persistence silently no-ops natively | `VariableContext.h:36-50` (native stubs) | NVS `calcVars` on device | Grapher/legacy vars are session-only in emulator |
| D14 | Firmware MEM probe on app exit; emulator has none | — | `SystemApp.cpp:213-217` | emulator teardown-loop tests can't watch heap trend without a new probe |
| D15 | Heartbeat log collapses all app modes to "CALC" | `NativeHal.cpp:2113-2118` | n/a | cosmetic; misleading logs for new apps |

---

## D. Desired routing pattern for new apps **[PROPOSED]**

### D.1 The per-app enablement checklist (ROUTE-01)

Adding app X to the emulator is exactly this closed list; a PR that skips a line is
incomplete:

1. `platformio.ini` `build_src_filter`: add `+<apps/XApp.cpp>` and every new
   transitive `.cpp` (engines, cas:: files, math files) — individual files only, no
   globs (existing policy, `platformio.ini:232`).
2. `NativeHal.cpp` `AppMode`: append one enumerator (append-only, never reorder).
3. Instance pointer + lazy construction in `transitionToMenu()` (constructor only;
   never eager `begin()` unless the app is the boot-critical CalculationApp).
4. `launchApp()`: one `case <launcher-id>:` calling `x->load(); g_mode = ...;`.
5. `performAppTeardown()`: one case calling `x->end()` (no re-begin unless justified).
6. `dispatchKey()`: one case with the standard contract — MODE+down → returnToMenu,
   else forward KeyEvent with `row=col=-1`. App-specific exit keys (AC, EXE) are
   routed **inside the app**, not in NativeHal, unless firmware SystemApp itself
   special-cases them (D7/D8); if it does, replicate the SystemApp behavior and cite
   it in a comment.
7. `canonicalAppName()` + `scriptAppNameToId()` + `activeAppName()`: add the name
   triple. Names must equal the `MainMenu::APPS[]` card name modulo
   case/spaces, and `assert_app` output must match `open_app` input.
8. Exit cleanup block: `end()` + `delete` (fixes the D10 class by construction).
9. If the app needs a per-frame `update()` (Equations, Fractal): add to the
   ROUTE-04 update dispatch (§D.4).
10. Scripts: at minimum `<app>_smoke.numos` per the QA plan; candidate stem
    registered in `scripts/generate-emulator-candidates.py`.
11. `docs/emulator-sdl2-quickstart.md` command table updated.

A single greppable marker comment (`// APP-ENABLE(<Name>)`) at each touched site is
required so `scripts/check-emulator-deps.py`-style tooling can later verify the
eight sites stay in sync (ROUTE-05 proposes automating this check).

### D.2 open_app / assert_app for new apps **[PROPOSED]**

- `open_app` keeps calling `launchApp(id)` — same code path as a menu ENTER; no
  bypass constructors.
- `open_app` on a compiled-out app must become a **script-load error** (exit 2),
  which it already is (unknown name); the failure mode to fix is the *menu* path
  (§F.3).
- `assert_app` gains no new semantics; each new app adds its canonical name.

### D.3 Home/MODE for new apps **[PROPOSED]**

Uniform: MODE on down-edge → `returnToMenu()` with deferred teardown, per §A.5.
Apps with firmware-side AC-exit or autosave-on-exit semantics (PeriodicTable,
CircuitCore, Fluid2D, Fractal, Grapher-AC, MathVisual) must replicate the exact
SystemApp conditions cited in §B.4, with the SystemApp line cited in a comment.
Divergence here is a firmware-parity checklist failure (parity spec §I).

### D.4 App update ticks (ROUTE-04) **[PROPOSED]**

Add to the NativeHal main loop, after `lv_timer_handler()`:

```cpp
// Firmware parity: SystemApp::update() gives some apps a cooperative tick.
if (g_mode == AppMode::EQUATIONS && g_equationsApp && g_equationsApp->isActive())
    g_equationsApp->update();
if (g_mode == AppMode::FRACTAL && g_fractalApp)
    g_fractalApp->update();
```

Scope: only apps that firmware ticks (`SystemApp.cpp:265-269, 290-292`). Do not
invent ticks for apps firmware doesn't tick.

### D.5 App state reset between scripts **[PROPOSED]**

Each CI script invocation is a fresh process (`generate-emulator-candidates.py`
runs the binary per stem), so in-RAM state resets by construction. The two
carriers of cross-run state are:

1. `emulator_data/` (LittleFS root, `src/hal/FileSystem.cpp:38`) — governed by the
   persistence sandbox spec; P-05 apps that write files must run under the
   sandboxed fixture root before their scripts are gated (STORE-01).
2. The committed `emulator_data/vars.dat` loaded at boot
   (`NativeHal.cpp:2171-2179`) — scripts must treat it as a **fixture** with
   defined content, or the sandbox spec's per-run scratch copy must land first.

Within one script, state reset across app switches is *deliberately* not provided:
firmware keeps app state alive across menu round-trips too (objects persist,
`end()` destroys screens, not engines). Scripts asserting "fresh app" semantics
must encode the app's own clear action (e.g. AC) rather than assuming reset.

---

## E. Input parity

### E.1 Current key alias surface (verified)

- SDL live-input map: `NativeHal.cpp:318-400` (navigation, control, modifiers,
  letter shortcuts) + `SDL_TEXTINPUT` chars `NativeHal.cpp:415-444`.
- Script name map: `scriptNameToKeyCode`, `NativeHal.cpp:456-565`. Covered tokens:
  symbols `+ - * / ^ . ( ) = < >`, digits, `enter/left/right/up/down`,
  `backspace|del|delete`, `ac|esc|escape`, `home|mode`, `graph`, `x|varx`,
  `y|vary`, `pow`, `frac|fraction|div`, `dot/add/sub/mul/lparen/rparen`,
  `sqrt/sin/cos/tan/ln/log/pi/e`, `negate|neg`, `ans`, `preans`, `shift`,
  `alpha`, `sto`, `freeeq|sd`, `exe`, `table`, `f1..f5`, `logbase|log_n`,
  `zoom`, `lt|less`, `gt|greater`.
- `input_alias_surface.numos` exists as an alias-coverage script
  (`tests/emulator/scripts/`).

### E.2 KeyCodes with **no** script token today (verified against KeyCodes.h)

`SETUP` (KeyCodes.h:41), `ON` (48), `TRACE` (63), `SHOW_STEPS` (64), `SOLVE` (65),
`NEG` physical (77 — distinct from virtual `NEGATE`), `ALPHA_A..ALPHA_F` (97-102),
`FACT` (108), `F3`/`F4` have tokens (`NativeHal.cpp:545-546`) but `F3/F4` are
consumed by no current emulator app.

**[PROPOSED]** Alias policy for P-05:

- Add tokens only when an enabled app's handler consumes the KeyCode (the Phase
  8B/9A precedent, comments `NativeHal.cpp:537-558`): tokens never invent
  KeyCodes, and unused aliases are allowed only as documented forward stubs.
- P-05 wave needs, per the app matrix: `solve` (EquationsApp/MatricesApp solve
  actions if their handlers consume SOLVE), `alpha_a..alpha_f` (Equations variable
  entry if ALPHA_x codes are consumed directly), `setup` only if an enabled app
  consumes SETUP. Each addition cites the consuming `handleKey` line in a comment.
- SHIFT/ALPHA in scripts drive the same `KeyboardManager` FSM as firmware
  (`src/input/KeyboardManager.cpp:42-92`); scripts should prefer modifier
  sequences over direct ALPHA_x tokens when testing firmware-realistic flows, and
  may use direct tokens for terseness where the app consumes them identically.

### E.3 Physical-key mapping caveats

- Physical scanning is disabled on hardware (`src/drivers/Keyboard.h:82`,
  `CONNECTED_COLS = 0`); firmware input arrives via SerialBridge. Emulator parity
  target is therefore the **KeyCode stream**, not a physical matrix.
- OS autorepeat produces REPEAT actions in the emulator (`NativeHal.cpp:826-834`);
  scripts have no `repeat` primitive — `keydown`/`keyup` pairs cannot synthesize
  REPEAT actions. Apps whose UX depends on autorepeat (scroll-heavy apps) can only
  be REPEAT-tested live, not by script. Accepted limitation for P-05; a
  `keyrepeat NAME` token is a candidate future command (kept out of scope).

### E.4 Text input

No enabled or P-05-candidate app uses LVGL textarea free-text input; app text
entry (Python IDE, NeoLang editor) is one reason those apps are deferred (see
matrix). The `SDL_TEXTINPUT` path exists solely to resolve keyboard-layout symbols
(`NativeHal.cpp:783-803`).

---

## F. Assertions and script APIs **[PROPOSED unless cited]**

### F.1 Existing generic commands (verified)

`assert_app`, `assert_menu_focus`, `assert_result[_contains]`, `assert_no_error`,
`assert_error`, `assert_variable` — see §A.7 table.

### F.2 Proposed generic commands for P-05

| Command | Semantics | Feasibility |
|---|---|---|
| `assert_app_ready` | active app's `debugIsReady()` hook returns true (screen built, first layout done) | needs a per-app NATIVE_SIM hook; cheap — most apps can report `_screen != nullptr` |
| `assert_no_crash` | not a runtime command: the *absence* of exit≠0 plus a final `assert_app` already encodes it; document as an idiom, do not implement | idiom only |
| `assert_statusbar_title TEXT` | reads the active `ui::StatusBar` title string via a NATIVE_SIM accessor | requires StatusBar debug hook + a registry of "current bar"; medium |
| `assert_lvgl_object_count N` / `assert_lvgl_object_count_between A B` | walks `lv_obj` tree of active screen | feasible via LVGL child traversal; brittle to UI evolution — use for leak deltas (LIFE-02), not absolute values, and keep warning-only |
| `assert_app_dirty_state ...` | per-app dirty flag | only meaningful per app; defer to app-specific hooks |
| `assert_storage_clean` | fixture root matches its pre-run snapshot | belongs to the sandbox spec's dirty-tree guard, exposed to scripts once STORE-01/sandbox lands; until then implemented as a CI-side directory diff, not a script command |

Per-app hooks follow the CalculationApp precedent exactly: `#ifdef NATIVE_SIM`
read-only const accessors in the app header (`CalculationApp.h:95-107` pattern),
consumed by new `case` arms in `scriptStepBegin`. Hook proposals per app live in
the QA plan (Document 4 §D).

### F.3 Dead-app selection reporting (answers special question 11)

Current: menu-ENTER on an unrouted card prints to stdout and silently stays in
MENU (`NativeHal.cpp:993-995`). Proposed:

- Keep the menu behavior (visible-but-refusing, see parity spec §J for the
  decision rationale), but make it **script-detectable**: after `launchApp` on an
  unsupported id, a following `assert_app Menu` already passes today — that is the
  wrong polarity (silent failure looks like success). Add a
  `[APP] launch-refused id=<n> name=<name>` structured log line and a negative
  control script (`route_dead_card_control.numos`) that asserts refusal via
  `assert_app Menu` **by design** and is documented as such (ROUTE-02).
- `open_app` for unrouted apps stays a load-time error (exit 2) — scripts cannot
  even express launching them, which is the stronger guarantee.

---

## G. Teardown and lifecycle tests **[PROPOSED]**

Verified baseline: teardown correctness is protected today only by the
grapher home-return scripts (e.g. `grapher_home_return_smoke.numos`,
`grapher_tan_exit_smoke.numos`) and Phase 9F design (§A.5). There is no repeated
enter/exit loop test, no timer-active-exit test, no leak detection.

Planned tests (details and gates in the QA plan; tickets LIFE-01/LIFE-02):

1. **Repeated enter/exit loop** (`life_enter_exit_loop.numos` per app): open app →
   minimal interaction → `home` → `assert_app Menu` → repeat ×5 within one process.
   Catches non-idempotent `begin()`, dangling StatusBar pointers, double-delete.
   The 260 ms teardown delay must be respected between iterations
   (`wait` ≥ 17 frames at 16 ms/frame).
2. **HOME while timers active**: enter Grapher, trigger `_poiAsyncTimer`
   (`GrapherApp.cpp:2994`) or template load timer (`GrapherApp.cpp:1508`), `home`
   immediately, then relaunch. Extends to every timer-owning app as it is enabled
   (Bridge 16 ms, Circuit 16 ms, Optics/Particle/Neural/Fluid 33 ms timers —
   `BridgeDesignerApp.cpp:836`, `CircuitCoreApp.cpp:781`, `OpticsLabApp.cpp:253`,
   `ParticleLabApp.cpp:203`, `NeuralLabApp.cpp:230`, `Fluid2DApp.cpp:257`). The
   app's `end()` must delete its timers; the test's oracle is process exit 0 +
   `assert_app` after relaunch.
3. **App switch while modal open**: for apps with modal overlays (Equations step
   view, Matrices editors), open modal → `home` → relaunch → assert base state.
4. **AC/MODE contract**: per-app script asserting the §B.4 exit table behavior.
5. **Destructor safety**: process-exit path already runs `end()`+`delete`+
   `lv_deinit()` (`NativeHal.cpp:2142-2156`); every enabled app must be in that
   block (fixes D10).
6. **LVGL object leak detection**: only after `assert_lvgl_object_count` (F.2)
   exists; run enter/exit loop and compare object count at menu between iteration
   1 and N — delta > 0 is a leak. Warning-only until proven stable.

---

## H. Implementation tickets

Routing tickets defined here; full ticket bodies (motivation/evidence/files/tests/
rollback) live in `NUMOS_EMULATOR_APP_ENABLEMENT_ROADMAP.md`:

- **ROUTE-01** — Generic app enablement checklist (§D.1) landed as a doc block in
  `NativeHal.cpp` + PR template item.
- **ROUTE-02** — App-route negative controls: dead-card refusal script + broken
  route control (§F.3).
- **ROUTE-03** — Add `g_grapherApp` (and every future app) to the exit cleanup
  block (§A.8 / D10).
- **ROUTE-04** — App `update()` dispatch in the emulator main loop for Equations/
  Fractal parity (§D.4 / D6).
- **ROUTE-05** — Route-sync checker: script that greps the eight NativeHal sites +
  whitelist and fails on a partial enablement (mechanizes §D.1).
- **ROUTE-06** — `assert_app_ready` + per-app ready hooks (§F.2).
- **ROUTE-07** — `assert_statusbar_title` (§F.2), needed for showcase-class apps
  whose only cheap semantic signal is the bar title.
- **ROUTE-08** — Script-token additions for newly consumed KeyCodes (§E.2),
  strictly consumer-cited.
