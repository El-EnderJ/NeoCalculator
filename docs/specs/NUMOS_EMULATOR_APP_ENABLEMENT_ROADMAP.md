# NumOS — Emulator App Enablement Roadmap (P-05)

> Ordered, ticketed implementation roadmap for the P-05 spec set. Read
> `NUMOS_EMULATOR_APP_ENABLEMENT_PARITY_SPEC.md` first (scope, P0-P7 ladder),
> then `..._NATIVEHAL_APP_ROUTING_AND_INPUT_SPEC.md` (ROUTE-*),
> `..._PER_APP_ENABLEMENT_MATRIX.md` (per-app evidence),
> `..._APP_QA_GOLDEN_AND_SCRIPT_PLAN.md` (APPQA-*/LIFE-* test contracts).
>
> Namespaces: **APPPAR-** app parity, **ROUTE-** NativeHal/SystemApp routing,
> **APPQA-** scripts/goldens/asserts, **STORE-** persistence, **LIFE-**
> lifecycle/teardown, **PORT-** host portability. External dependencies use
> their home namespaces (FIX-*, EMUDET-*, CI-*, GS-*, SB-* — see the
> determinism roadmap `NUMOS_EMULATOR_DETERMINISM_IMPLEMENTATION_ROADMAP.md`).
> Owner tiers per the risk-register convention (Human / Fable-Opus / Sonnet /
> Haiku-class).

---

## Wave plan

- **Wave 0 — no new app**: inventory freeze + harness + guards
  (APPPAR-00, ROUTE-01, ROUTE-05, ROUTE-03, APPQA-01, LIFE-01, STORE-01,
  ROUTE-02). External prereqs pulled in: FIX-01, FIX-03, CI-01, EMUDET-03
  (before first rand()-carrying app), CI-04.
- **Wave 1 — low-risk static/UI apps**: APPPAR-04 Matrices, APPPAR-05
  PeriodicTable (+APPQA-02 candidates, APPQA-03 hooks for both).
- **Wave 2 — math-renderer/CAS apps**: APPPAR-02 Calculus, then ROUTE-04 +
  APPPAR-01 Equations (+LIFE-02, APPQA-03 extensions, APPQA-04 first golden
  promotion wave; APPPAR-07/APPPAR-08 ride along as low-cost renderer items).
- **Wave 3 — storage/timer/simulation apps**: requires FIX-02 (sandbox
  default). APPPAR-09a Optics+Bridge (timers, no FS), APPPAR-09b
  Circuit/Fluid2D/ParticleLab (FS + timers), APPPAR-09d Fractal (needs
  ROUTE-04). STORE-02 roundtrips.
- **Wave 4 — hardware/CAS/Giac-sensitive**: APPPAR-09c NeuralLab (seeded-RNG
  hook), APPPAR-06 Python (product decision first). Giac-dependent behavior:
  never (native Giac impossible, `platformio.ini:179-194`) — firmware-only
  harness owns it (P-02).
- **Future / not planned**: Integral, Tutor (dead on firmware — P-07 owns
  deletion; APPPAR-03 resolves against it), NeoLang (after script grammar can
  drive an editor), Math Visual id 20 (emulator keeps MathShowcase instead).

Ticket ordering inside waves is the listing order below.

---

## Wave 0

### APPPAR-00 — App inventory freeze
- **Motivation:** every downstream ticket keys off a fixed app list; the launcher
  table, firmware dispatch and emulator dispatch must be snapshotted so drift is
  detectable.
- **Evidence:** `MainMenu.cpp:88-113` (20 cards), `SystemApp.cpp:885-913`
  (dispatch 0-20), `NativeHal.cpp:924-997` (ids {0,1,4,5,6,7,10,100}),
  dead apps Tutor/Integral (ground truth §F).
- **Files:** `docs/specs/NUMOS_EMULATOR_PER_APP_ENABLEMENT_MATRIX.md` (already
  authored — this ticket ratifies it), plus a machine-readable
  `tests/emulator/app_inventory.json` (id, name, firmware mode, emulator mode
  or null, persistence paths, timer list).
- **Desired behavior:** inventory file committed; adding/removing an app without
  updating it fails ROUTE-05.
- **Non-goals:** no code behavior change.
- **Dependencies:** none. **Tests:** ROUTE-05 consumes it. **Golden impact:**
  none. **Persistence impact:** none. **Firmware impact:** none.
- **Rollback:** delete the JSON. **Risk:** low. **Owner:** Sonnet.

### ROUTE-01 — Generic app enablement checklist
- **Motivation:** enabling an app touches 8 hand-maintained NativeHal sites +
  whitelist + scripts; missed sites are today's silent-drift engine (EMU-1).
- **Evidence:** routing spec §A.4 (sites list with lines); Settings as the
  worked template (`NativeHal.cpp:102,199,979-985,1016-1018,647-663,1409,1434`).
- **Files:** `src/hal/NativeHal.cpp` (doc comment block near AppMode),
  `.github/pull_request_template.md` (checklist section), routing spec §D.1
  (normative text — already written).
- **Desired behavior:** every enablement PR carries the filled checklist; each
  touched site gets an `// APP-ENABLE(<Name>)` marker.
- **Non-goals:** no registry refactor (a table-driven registry is a possible
  later ticket; P-05 keeps append-only switches to protect the CI contract).
- **Dependencies:** APPPAR-00. **Tests:** ROUTE-05 mechanizes. **Golden:** none.
  **Persistence:** none. **Firmware:** none. **Rollback:** doc-only.
- **Risk:** low. **Owner:** Sonnet.

### ROUTE-05 — Route-sync checker
- **Motivation:** mechanize ROUTE-01; a half-wired app (e.g. whitelisted but no
  teardown case) must fail CI, not soak as UB.
- **Evidence:** the eight sites (routing spec §A.4); precedent guard style
  `scripts/check-keycode-digit-patterns.py` + `emulator-build.yml:96-104`.
- **Files:** new `scripts/check-emulator-app-routes.py`; `emulator-build.yml`
  (new early step); reads `tests/emulator/app_inventory.json`.
- **Desired behavior:** for every app marked emulator-enabled in the inventory,
  assert presence of: whitelist entry, AppMode enumerator, launchApp case,
  teardown case, dispatchKey case, name-map triple, exit-cleanup entry; fail
  with a per-site diagnostic.
- **Non-goals:** no semantic verification of the cases' bodies.
- **Dependencies:** APPPAR-00, ROUTE-01. **Tests:** self-test with a synthetic
  bad fixture (negative control, part of ROUTE-02 acceptance). **Golden:** none.
  **Persistence:** none. **Firmware:** none (script only).
- **Rollback:** remove CI step. **Risk:** low. **Owner:** Sonnet.

### ROUTE-03 — Symmetric exit cleanup (fix D10)
- **Motivation:** `g_grapherApp` is never `end()`-ed/deleted at process exit —
  breaks the symmetric-teardown invariant and poisons future leak accounting.
- **Evidence:** `NativeHal.cpp:2144-2151` (grapher absent from cleanup block).
- **Files:** `src/hal/NativeHal.cpp` (one block).
- **Desired behavior:** every constructed app appears in cleanup; ROUTE-05
  checks it thereafter.
- **Non-goals:** no teardown-order redesign.
- **Dependencies:** none. **Tests:** full emulator suite (behavioral no-op at
  runtime); LIFE-02 later relies on it. **Golden:** none. **Persistence:** none.
  **Firmware:** none.
- **Rollback:** revert one hunk. **Risk:** low. **Owner:** Haiku-class.

### APPQA-01 — Per-app smoke script harness
- **Motivation:** every wave needs the same smoke shape; encode it once.
- **Evidence:** existing shape `settings_smoke.numos` /
  `math_showcase_smoke.numos`; CI per-script contract
  (`emulator-build.yml:308-356`); candidate generator registration
  (`generate-emulator-candidates.py:39-143`).
- **Files:** `tests/emulator/scripts/` (new stems per wave),
  `scripts/generate-emulator-candidates.py` (stem entries),
  `.github/workflows/emulator-build.yml` (one new suite step per wave, same
  run_semantic pattern).
- **Desired behavior:** adding a smoke = script + generator entry + suite-list
  line; QA plan §B is the per-app content authority.
- **Non-goals:** no new script grammar in this ticket.
- **Dependencies:** ROUTE-01. **Tests:** the scripts are the tests; two-run
  SHA self-diff per new stem. **Golden impact:** candidates only (pending).
  **Persistence:** none. **Firmware:** none.
- **Rollback:** remove stems/steps. **Risk:** low. **Owner:** Sonnet.

### LIFE-01 — Repeated app enter/exit loop
- **Motivation:** teardown bugs (UAF/hang class — the Phase 9F incident) only
  surface on re-entry; nothing loops today.
- **Evidence:** deferred teardown windows (`NativeHal.cpp:230-233,2088-2092`;
  `SystemApp.cpp:243-250`); idempotent-end contract (`NativeHal.cpp:1005`).
- **Files:** `tests/emulator/scripts/life_<app>_loop.numos` (one per enabled
  app, starting with the 7 already enabled), `emulator-build.yml` (LIFE suite).
- **Desired behavior:** open→key→home→wait≥17 frames→assert Menu, ×5, exit 0.
- **Non-goals:** leak quantification (LIFE-02).
- **Dependencies:** APPQA-01. **Tests:** self. **Golden:** none.
  **Persistence:** none (loop apps in wave 0 don't write). **Firmware:** none.
- **Rollback:** drop suite. **Risk:** low (may *reveal* real bugs — that is the
  point; triage as app bugs, not harness bugs). **Owner:** Sonnet.

### STORE-01 — Per-app persistence inventory guard
- **Motivation:** each enabled app must declare its storage surface before it
  can pollute the fixture root; the sandbox spec's §A.4 parity register is the
  ledger, this ticket wires enforcement.
- **Evidence:** app write sites and their **native behavior class** (matrix doc
  §cross-app table): ARDUINO-gated → native no-op today:
  `PythonApp.cpp:657-698` (`/py`), `CircuitCoreApp.cpp:1363,1707-1708`
  (`/circuits`), `Fluid2DApp.cpp:1453,1484,1516` (`/fluid`),
  `ParticleLabApp.cpp:409,427` (`/save.pt`). **Writes natively already**:
  `NeuralLabApp.cpp:950-967` (`/neural.nn`), `NeoLanguageApp.cpp:897-944`
  (`/neolang.nl`), plus NeoIO's raw `std::fopen` cwd bypass
  (`NeoIO.cpp:62-119`). Live store: `VariableManager` `/vars.dat`
  (`VariableManager.cpp:169-190`); FS root `hal/FileSystem.cpp:38`. The
  inventory must record which class each surface is in, because "gated no-op"
  apps need an explicit un-gate-or-register-divergence decision at enablement.
- **Files:** `tests/emulator/app_inventory.json` (persistence field),
  `scripts/check-emulator-app-routes.py` (cross-check: an enabled app whose
  sources match `LittleFS.open(...,"w")` must have inventory rows), sandbox
  spec §A.4 table updates per wave.
- **Desired behavior:** enabling an FS-writing app without inventory rows and
  without FIX-01 sandbox flags available fails CI.
- **Non-goals:** implementing the sandbox itself (FIX-01/02/03 own that).
- **Dependencies:** APPPAR-00; FIX-01 for the runtime half. **Tests:** negative
  control = synthetic app with undeclared write. **Golden:** none.
  **Persistence impact:** ledger only. **Firmware:** none.
- **Rollback:** disable the cross-check. **Risk:** low. **Owner:** Sonnet.

### ROUTE-02 — App route negative controls
- **Motivation:** prove the harness can fail: dead-card refusal, broken hooks,
  half-wired routes.
- **Evidence:** dead-card path `NativeHal.cpp:993-995`; QA plan §H.
- **Files:** `tests/emulator/scripts/route_dead_card_control.numos` (+ retired/
  repointed as cards come alive), CI step running ROUTE-05 against a bad
  fixture, hook negative controls per APPQA-03.
- **Desired behavior:** structured `[APP] launch-refused id=<n>` log line added
  to the default branch of `launchApp`; control script asserts Menu-after-ENTER
  on a dead card; inverted-gate wiring in CI (expect exit 4 / expect fail).
- **Non-goals:** changing dead-card UX (SB-06 owns dimming).
- **Dependencies:** ROUTE-05, APPQA-01. **Tests:** self. **Golden:** none.
  **Persistence:** none. **Firmware:** none (NativeHal-only log line).
- **Rollback:** drop step. **Risk:** low. **Owner:** Sonnet.

---

## Wave 1 — low-risk static/UI apps

### APPPAR-04 — Enable Matrices
- **Motivation:** cleanest candidate: pure LVGL + `<cmath>`, no CAS/Arduino/FS/
  rand/timers; same profile as already-enabled Stats/Prob; establishes the
  wave-1 template.
- **Evidence:** includes only lvgl/StatusBar/KeyCodes/KeyboardManager/
  MatrixEngine (`MatricesApp.h:31-35`); static `double data[5][5]`
  (`MatrixEngine.h:41`); `end()` order StatusBar→screen
  (`MatricesApp.cpp:118-141`); no persistence/timers/rand (matrix doc §
  Matrices).
- **Files:** `platformio.ini` (+`apps/MatricesApp.cpp`, `apps/MatrixEngine.cpp`),
  `src/hal/NativeHal.cpp` (8 sites, card id 9, AppMode `MATRICES`),
  scripts `matrices_smoke/edit/ops`, generator entries, CI suite line,
  `docs/emulator-sdl2-quickstart.md`.
- **Desired behavior:** P3 at merge (smoke + `assert_matrices_cell` +
  `assert_matrices_result`), P5 in APPQA-04 wave.
- **Non-goals:** matrix math extensions; SHIFT/ALPHA additions (app doesn't use
  the modifier FSM today).
- **Dependencies:** Wave 0 complete. **Tests:** QA plan §B rows + LIFE-01 loop
  + hook negative control. **Golden impact:** 2 new pending stems.
  **Persistence:** none. **Firmware impact:** none (app source untouched except
  the NATIVE_SIM hook block).
- **Rollback:** remove whitelist lines + NativeHal cases (scripts become
  load-errors — delete with them).
- **Risk:** low. **Owner:** Sonnet.

### APPPAR-05 — Enable Periodic Table
- **Motivation:** second zero-engine app; first emulator coverage of the
  `LV_EVENT_DRAW_MAIN` custom-draw path; exact-rational ChemCAS balancer gets
  its first automated oracle.
- **Evidence:** constexpr DBs (`ChemDatabase.h:51,172,186`; `ChemExtraData.h:46,
  167`; no PROGMEM reads); draw callback (`PeriodicTableApp.cpp:204,515-599`);
  extra `resetPointers()` teardown nuance (`PeriodicTableApp.cpp:106-111`);
  firmware exits on MODE **or AC** (`SystemApp.cpp:579-585`) — emulator must
  replicate; SOLVE key consumed (`PeriodicTableApp.cpp:870`).
- **Files:** `platformio.ini` (+`apps/PeriodicTableApp.cpp`, `apps/ChemCAS.cpp`),
  NativeHal 8 sites (card id 11, name "Chemistry"), `solve` script token
  (ROUTE-08, consumer-cited), scripts per QA plan §B, generator/CI entries.
- **Desired behavior:** P3 at merge (selection + balancer hooks); grid + modal
  goldens pending.
- **Non-goals:** ChemCAS correctness expansion (its corpus is future QA).
- **Dependencies:** Wave 0. **Tests:** QA plan rows; AC-exit parity script.
  **Golden impact:** 2 pending stems. **Persistence:** none.
  **Firmware impact:** none beyond hook block.
- **Rollback:** as APPPAR-04. **Risk:** low. **Owner:** Sonnet.

### APPQA-02 — Golden candidate generation for wave-1/2 apps
- **Motivation:** register all new stems; keep the pending ledger honest.
- **Evidence:** generator list structure (`generate-emulator-candidates.py:
  39-143`); warning-only compare (`emulator-build.yml:575-607`); CI-05 pending
  classification.
- **Files:** `scripts/generate-emulator-candidates.py`,
  `tests/emulator/golden/PENDING.md` (new, per CI-05).
- **Desired behavior:** every new stem generates in CI, classified
  gated/pending/assert-only; screenshot framing policy (no spinner/rand-caption
  frames) enforced by review.
- **Dependencies:** APPPAR-04/05 (and later -02/-01). **Tests:** generator run
  + two-run self-diff. **Golden impact:** candidates only. **Rollback:** remove
  entries. **Risk:** low. **Owner:** Sonnet.

### APPQA-03 — Semantic assert hooks (wave-1 + generic)
- **Motivation:** hook-before-golden rule; only Calculation/MainMenu have hooks
  today (`CalculationApp.h:95-107`, `MainMenu.h:58-79`).
- **Evidence:** hook table QA plan §D; script parser append points
  (`NativeHal.cpp:1457-1622`).
- **Files:** app headers (`#ifdef NATIVE_SIM` blocks), `NativeHal.cpp` (new
  ScriptCmdType arms), negative-control scripts.
- **Desired behavior:** `assert_matrices_*`, `assert_periodic_*`, generic
  `assert_app_ready` (ROUTE-06); each with a negative control; grammar stays
  append-only, parse-validated.
- **Dependencies:** APPPAR-04/05. **Tests:** controls exit 4. **Golden:** none.
  **Persistence:** none. **Firmware impact:** none (NATIVE_SIM-stripped).
- **Rollback:** hooks are additive; remove arms+blocks. **Risk:** low-medium
  (parser is a CI contract — append-only discipline). **Owner:** Sonnet, parser
  arms reviewed by Opus-tier.

---

## Wave 2 — math-renderer/CAS apps

### APPPAR-02 — Enable Calculus
- **Motivation:** first CAS UI in the emulator; unblocks CAS corpus rows tagged
  "once Calculus is emulator-enabled per P-05"
  (`NUMOS_CAS_IMPLEMENTATION_TICKETS.md:94`).
- **Evidence:** Arduino-clean (no Arduino/ESP includes — full-source audit,
  matrix doc); needs `SymDiff.cpp`, `SymIntegrate.cpp`, `SymToAST.cpp` +
  link-verified transitive closure (`CalculusApp.h:52-60`, `CalculusApp.cpp:31`
  vs `platformio.ini:252-258`); `rand()` captions (`CalculusApp.cpp:449-453`);
  inline `lv_timer_handler()` in `showComputing()` (`CalculusApp.cpp:464`) —
  legal in the emulator loop but must not re-enter (verify once running);
  `_stepRenderers` not cleared in `end()` (`CalculusApp.cpp:129-169` — cleared
  per-compute at `:786,1031`) — watch in LIFE-01 loop.
- **Files:** `platformio.ini` (app + closure; closure fixed by `g++ -M`/link
  iteration per ground truth §Q), NativeHal 8 sites (card id 3), scripts per
  QA plan §B, hooks per §D, EMUDET-03 seed fence must be green first.
- **Desired behavior:** P3 at merge (result hooks); input-screen golden pending;
  spinner frames never captured.
- **Non-goals:** SymDiff/SymIntegrate correctness fixes (CAS tickets own);
  Giac anything.
- **Dependencies:** Wave 0, EMUDET-03, APPQA-03 pattern. **Tests:** diff/int
  semantic scripts + negative controls + LIFE-01 + long-expression stress.
  **Golden impact:** 1 pending stem initially. **Persistence:** none.
  **Firmware impact:** none beyond hook block.
- **Rollback:** remove whitelist+cases; closure files stay harmless if unused
  (prefer full revert to avoid dead weight).
- **Risk:** medium (closure size unknown until linked; cas:: arena lifetime
  MEM-2 now exercisable under host sanitizers — an upside). **Owner:** Sonnet
  build/routing, Opus-tier review on cas:: closure and hooks.

### ROUTE-04 — App update() dispatch in emulator loop
- **Motivation:** firmware ticks Equations (staged solve pipeline) and Fractal;
  without a tick, Equations' solve never advances in the emulator.
- **Evidence:** `SystemApp.cpp:265-269, 290-292`; emulator loop has no app tick
  (`NativeHal.cpp:2045-2130`); Equations pipeline `EquationsApp.cpp:1059-1631`.
- **Files:** `src/hal/NativeHal.cpp` (main loop, one guarded block per ticked
  app).
- **Desired behavior:** tick exactly the apps firmware ticks, same guard
  conditions (`isActive()`), after `lv_timer_handler()`.
- **Non-goals:** inventing ticks for un-ticked apps.
- **Dependencies:** none hard; lands with APPPAR-01. **Tests:**
  `equations_linear_smoke` is the proof. **Golden:** none. **Persistence:**
  none. **Firmware:** none. **Rollback:** remove block. **Risk:** low-medium
  (loop ordering vs deferred teardown — tick must not run for a mode pending
  teardown). **Owner:** Sonnet, Opus review.

### APPPAR-01 — Enable Equations
- **Motivation:** highest-value enablement: 3350-line untested CAS UI
  (EMU-5/PRD-3); MEM-6 leak canary becomes automatable.
- **Evidence:** unconditional `Arduino.h`/`esp_heap_caps.h`/FreeRTOS includes +
  `heap_caps_*` step budget (`EquationsApp.cpp:34,44-46,487-518`) — none
  shimmed (`ArduinoCompat.h` audit); largest cas:: closure (SingleSolver/
  SystemSolver/OmniSolver/SymToAST/RuleEngine/AlgebraicRules/CasToVpam/
  CasMemory/SystemHeuristics/SystemTutor/PersistentAST/TutorTemplates/
  PedagogicalLogger + transitive; `EquationsApp.h:37-52`); `rand()` caption
  (`EquationsApp.cpp:106`); `end()` clears steps + arena
  (`EquationsApp.cpp:400-465`); firmware forwards keys only when `isActive()`
  (`SystemApp.cpp:513-519`).
- **Files:** `src/apps/EquationsApp.cpp` (**guard blocks only**: `#ifdef
  ARDUINO` around the three includes and `canAllocStep()/logStepBudget()` with
  a fixed synthetic native budget — open decision 2 in the parity spec),
  `platformio.ini` (largest closure), NativeHal 8 sites + ROUTE-04 tick
  (card id 2), scripts/hooks per QA plan (incl. `assert_equations_*`,
  `debugPipelineIdle`, steps-leak canary).
- **Desired behavior:** P3 at merge; equation-list golden pending; solve flows
  assert-only; enable complex-settings honesty flip (SET §D.2 "P-05 makes it
  real") in coordination with ST-03.
- **Non-goals:** solver correctness (CAS corpus owns); tutor UX changes.
- **Dependencies:** ROUTE-04, EMUDET-03, FIX-01 (recommended), APPPAR-02 landed
  (closure overlap reduces risk). **Tests:** QA plan §B/§D/§E rows.
  **Golden impact:** 1 pending stem. **Persistence:** none. **Firmware
  impact:** the `#ifdef ARDUINO` guards touch firmware-compiled code — firmware
  build must be compile-verified in the same PR (P-09 compile check helps).
- **Rollback:** revert guards + whitelist + cases; guards are semantically
  no-op for ARDUINO so firmware rollback risk is minimal.
- **Risk:** medium-high (guard correctness on firmware; closure link size;
  pipeline-vs-teardown interaction). **Owner:** Opus-tier implementation,
  Human sign-off on the native budget semantics.

### APPPAR-03 — Integral: resolve as do-not-enable
- **Motivation:** the ticket list requires disposition; evidence says dead code.
- **Evidence:** no SystemApp member/dispatch/no card (`SystemApp.h:138-160`,
  `SystemApp.cpp:885-913`, `MainMenu.cpp:88-113`); ground truth §F "dead
  (merged into CalculusApp)".
- **Desired behavior:** no emulator enablement (would create emulator-only
  reachability, violating parity philosophy §I). Disposition: P-07 dead-code
  sweep deletes or firmware re-wires; only if firmware ever re-wires does an
  enablement ticket reopen.
- **Files:** matrix doc row + P-07 cross-reference; no code.
- **Dependencies:** P-07. **Risk:** none. **Owner:** Human decision recorded;
  Haiku-class doc update.

### APPPAR-07 — Math Visual / Showcase disposition
- **Motivation:** required disposition; avoid a duplicate route.
- **Evidence:** emulator MathShowcase (id 100) already reuses accepted
  `MathRenderVisualCases` (`NativeHal.cpp:1090-1222`); firmware Math Visual is
  validate-env-only (`SystemApp.h:62-65`, `platformio.ini:116-121`).
- **Desired behavior:** keep both as-is; extend showcase case list only via
  `MathRenderVisualCases.cpp` (already whitelisted, `platformio.ini:250`);
  add `math_showcase_delims_smoke` candidate registration (script exists,
  no golden) if delimiter coverage is wanted.
- **Files:** generator entry only. **Dependencies:** none. **Risk:** low.
  **Owner:** Haiku-class.

### APPPAR-08 — Enable StixGlyphGallery (diagnostic route)
- **Motivation:** REN-2 (missing-glyph tofu) has no automated oracle; the
  gallery renders the font inventory.
- **Evidence:** diag-only behind `NUMOS_STIX_DIAGNOSTICS`
  (`main.cpp:49-52,187-194` per ground truth §F); `ui/StixGlyphGallery.*` not
  whitelisted; not a launcher card; **not an app class** — two free functions
  (`StixGlyphGallery.h:24,27`). Blockers: unconditional `#include <Arduino.h>`
  (`StixGlyphGallery.cpp:18`, no native Arduino.h exists) and the gallery's
  blocking `millis()` busy-loop (`:165-169`).
- **Files:** `src/ui/StixGlyphGallery.cpp` (guard the include to
  ArduinoCompat), `platformio.ini` (+`ui/StixGlyphGallery.cpp`), NativeHal:
  showcase-style wrapper mode that builds the panel without the busy-wait
  (`open_app StixGallery`, no launcher card — mirrors id-100 pattern with a
  new non-card id, e.g. 101), `stixgallery_smoke` script + candidate.
- **Desired behavior:** P2 + pending golden per gallery page; additionally
  wire `runStixGlyphAlignmentDiagnostics()` (`StixGlyphGallery.cpp:57-112`)
  as a text-assert CI check (pass/fail line). Explicitly an emulator
  diagnostic (documented emulator-only surface, allowed because it is harness,
  not product UI — parity spec §I).
- **Non-goals:** launcher card on either target.
- **Dependencies:** Wave 0. **Tests:** smoke + self-diff. **Golden impact:**
  pending stems. **Risk:** low. **Owner:** Sonnet.

### APPQA-04 — Golden promotion wave 1
- **Motivation:** convert accumulated pending stems (Matrices ×2, Periodic ×2,
  Calculus ×1, Equations ×1, gallery) to blessed goldens in one reviewed batch.
- **Evidence:** promotion mechanics `golden/README.md:31-113`,
  `promote-emulator-golden.py` behavior (refuse-without-force, never commits).
- **Files:** `tests/emulator/golden/*.ppm`, `masks/*.mask` (clock rect only),
  `PENDING.md` rows closed.
- **Desired behavior:** human reviews each candidate against semantic
  correctness; masks limited to `4,6,37,13`; coordinated with any SB-06
  launcher re-bless.
- **Dependencies:** APPQA-02/03, apps landed. **Golden impact:** the wave
  itself. **Risk:** medium (EMU-2 — bad bless normalizes regressions; mitigated
  by review checklist). **Owner:** Human (mandatory), prepared by Sonnet.

### LIFE-02 — Timer-active teardown stress + object-leak probe
- **Motivation:** teardown while `lv_timer`s live is the highest-severity
  lifecycle class; and enter/exit loops need a leak signal.
- **Evidence:** timer sites (`GrapherApp.cpp:1508,2994`; wave-3 apps
  `BridgeDesignerApp.cpp:836`, `CircuitCoreApp.cpp:781`, `OpticsLabApp.cpp:253`,
  `ParticleLabApp.cpp:203`, `NeuralLabApp.cpp:230`, `Fluid2DApp.cpp:257`);
  firmware MEM probe precedent (`SystemApp.cpp:213-217`).
- **Files:** `NativeHal.cpp` (`assert_lvgl_object_count[_between]` command,
  warning-only mode first), `tests/emulator/scripts/life_*_timer_exit.numos`,
  CI suite.
- **Desired behavior:** per timer-owning app: arm timer → immediate home →
  relaunch → assert; loop object-count delta reported.
- **Dependencies:** LIFE-01, ROUTE-03; extends per wave-3 app as enabled.
  **Golden:** none. **Risk:** medium (count brittleness — keep warning-only
  until stable). **Owner:** Sonnet.

---

## Wave 3 — storage/timer/simulation apps (gated on FIX-02)

### APPPAR-09 — Sim/lab apps: split enablement
Disposition ticket splitting into sub-tickets; each reuses the ROUTE-01
checklist and QA plan patterns. Order and gates:

- **APPPAR-09a — Optics + Bridge** (timers only, no FS — grep-clean audit):
  Optics first — its 33 ms timer only re-renders a static scene
  (`OpticsLabApp.cpp:587`), making it the best sim golden candidate; Bridge's
  16 ms Verlet stepper is fixed-DT deterministic (`BridgeDesignerApp.h:98`).
  Smoke at fixed deterministic frame counts; goldens only if two-run self-diff
  proves byte-stability of the chosen frame; otherwise assert-only.
  Risk medium. Owner Sonnet.
- **APPPAR-09b — Circuit, Fluid2D, ParticleLab** (timers + LittleFS
  autosaves): their FS code is **ARDUINO-gated (native no-op today)**
  (`CircuitCoreApp.cpp:1707-1708`, `Fluid2DApp.cpp:1453`,
  `ParticleLabApp.cpp:409`) — each enablement must decide: un-gate (needs
  `File::printf` shim for Circuit's text format + FIX-02 sandbox) or register
  a no-persistence divergence in the sandbox §A.4 table. Emulator must
  replicate the SystemApp-side autoSave-on-exit hooks
  (`SystemApp.cpp:595-615`) — divergence class D8 becomes real here. Circuit
  also carries the LuaVM-stub product question (ground truth §F); Fluid2D's
  timer is always-on from createUI (`Fluid2DApp.cpp:257`). ParticleLab
  additionally needs a NATIVE_SIM `resetRng()` hook — its engine xorshift is
  fixed-seed but never re-seeded across relaunch
  (`ParticleEngine.cpp:84-95,147`). Risk medium-high. Owner Sonnet + Opus
  review on exit hooks.
- **APPPAR-09c — NeuralLab** (rand()-core + **native FS write**): its
  `/neural.nn` save/load is NOT gated and writes `emulator_data/` natively
  today (`NeuralLabApp.cpp:950-967`) — hard FIX-01 dependency before the app
  is even compiled in. Pixel goldens blocked on a seeded-RNG debug hook
  (weights `NeuralEngine.cpp:140-141`, UI jitter `NeuralLabApp.cpp:372-380`);
  assert-only until seeding lands. Risk high for pixels, medium for asserts.
  Owner Sonnet.
- **APPPAR-09d — Fractal**: needs ROUTE-04 tick (`SystemApp.cpp:290-292`) and
  the AC-exit + `consumeExitRequest()` contract (`SystemApp.cpp:649-658`)
  replicated. Risk medium. Owner Sonnet.
- **NeoLang**: explicitly out of APPPAR-09; future wave. Three gates: it
  saves/loads `/neolang.nl` natively already (`NeoLanguageApp.cpp:897-944`);
  its NeoIO `file()` builtin uses raw cwd-relative `std::fopen` on native,
  escaping the LittleFS root entirely (`NeoIO.cpp:62-119`) — a sandbox bypass
  that must be routed through `hal/FileSystem` first; and its text-editor
  input surface makes scripts unwieldy.

Common fields: **Tests** QA plan §B/§E sim rows; **Golden impact** self-diff-
gated; **Persistence impact** sandbox §A.4 rows per app (STORE-01 enforced);
**Firmware impact** none (plus exit-hook parity code in NativeHal);
**Rollback** per-app revert.

### STORE-02 — Storage roundtrip suites for wave-3 apps
- **Motivation:** persistence parity is behavior, not just paths.
- **Evidence:** two-process protocol (sandbox spec §C); app formats
  (`CircuitCoreApp.cpp:1360-1500`, `Fluid2DApp.cpp:1446-1518`,
  `ParticleLabApp.cpp:406-431`, `NeuralLabApp.cpp:950-967`).
- **Files:** runner scripts + CI steps; fixtures under
  `tests/emulator/fixtures/<name>/` (committed, human-blessed).
- **Desired behavior:** write→read roundtrip per app in sandbox; corruption rows
  align with FIX-05 matrix.
- **Dependencies:** FIX-02/FIX-04, APPPAR-09b. **Risk:** medium.
  **Owner:** Sonnet.

---

## Wave 4 — deferred/decision-gated

### APPPAR-06 — Python: defer with explicit decision gate
- **Motivation:** required disposition. Two blockers: (1) every boot seeds
  `/py/hello.py` and the app lists/creates/deletes scripts
  (`PythonApp.cpp:340,470,657-698`) — needs sandbox + directory listing which
  `hal/FileSystem` doesn't implement (no `openNextFile`/`mkdir`,
  `FileSystem.cpp:52-88` audit) — a real HAL work item; (2) the engine is a
  custom mini-interpreter, not MicroPython (ground truth §F) and its product
  fate is undecided.
- **Desired behavior:** DEFER until Human decides the interpreter question and
  FIX-02 + FileSystem directory-API extension land. Then enable at P3 with
  interpreter-output hooks (`assert_python_output`), goldens for the IDE chrome
  only.
- **Files (when opened):** `hal/FileSystem.*` (dir ops), whitelist, NativeHal,
  scripts. **Risk:** medium. **Owner:** Human decision → Sonnet.

### PORT-06 — Cross-OS drift re-measure after CAS-app goldens
- **Motivation:** CAS UIs render double-math-derived layouts; PORT-05's drift
  measurement must be re-run once wave-2 goldens exist, before any non-Linux
  golden claims.
- **Evidence:** Linux-reference-only policy (CI/golden policy spec §I);
  PORT-05 definition (host portability spec §G).
- **Files:** portability workflow matrix. **Dependencies:** APPQA-04, PORT-05.
  **Risk:** low. **Owner:** Sonnet.

---

## Dependency graph (summary)

```
FIX-01/FIX-03/CI-01/EMUDET-03/CI-04 (external, Wave-0 pull-ins)
        │
APPPAR-00 → ROUTE-01 → ROUTE-05 → ROUTE-02
        │       │
        │   APPQA-01 → LIFE-01
        │       │
        │   STORE-01
        ▼
APPPAR-04 ─┬→ APPQA-02 → APPQA-03 → APPQA-04 (Human)
APPPAR-05 ─┘
        ▼
APPPAR-02 → ROUTE-04 → APPPAR-01 → LIFE-02
        │                        (APPPAR-03/-07/-08 parallel, low-cost)
        ▼
FIX-02 → APPPAR-09a → APPPAR-09b → STORE-02 → APPPAR-09c/09d
        ▼
APPPAR-06 (Human gate) · PORT-06
```

ROUTE-03 lands any time (independent, trivial).
