# NumOS — Safe Mode & Recovery Specification

> Defines boot-loop detection, safe-mode triggers, safe-mode UI and app policy, storage preservation/reset semantics, on-screen diagnostics, input-degraded operation, and the emulator simulation of all of it. Verified against the working tree at `63e7408`. **VERIFIED** facts cite `file:line`; the safe-mode subsystem itself is entirely **PROPOSED** (verified absent: no safe-mode, recovery, reset-reason, or boot-counter code exists anywhere in `src/` — grep audit; the only "panic-mode recovery" string is NeoParser's parser-level error recovery, `NeoParser.h:27,92`, unrelated).
>
> Companion: `NUMOS_FIRMWARE_RELIABILITY_AND_FAULT_CONTAINMENT_SPEC.md` (invariants R-1…R-5), `NUMOS_RUNTIME_DIAGNOSTICS_AND_TELEMETRY_SPEC.md` (`[BOOT]`/`[PANIC]`/`[SAFE]`/`[DIAG]` formats), tickets RT-02/RT-03/RT-04/RT-13/RT-15/RT-16.

---

## 1. Why safe mode must exist (current-state evidence)

1. A panic today reboots into an identical boot: `SystemApp::begin()` unconditionally re-loads `/vars.dat` (`SystemApp.cpp:144-156`), NeoLanguage auto-loads `/neolang.nl` on app open (`NeoLanguageApp.cpp:116`), CircuitCore auto-loads its autosave (`CircuitCoreApp.cpp:1712-1725`). **Any persistent-state-triggered crash is a permanent boot/app-open loop** with no escape short of reflashing.
2. Boot-blocking failures spin forever: draw-buffer alloc failure (`main.cpp:145-148`) and display-init failure (`main.cpp:156-159`) are `while(1) delay(1000)` with one serial line.
3. The only mount-failure policy is silent reformat (`LittleFS.begin(true)`, `SystemApp.cpp:144` + five app-level sites) — recovery today *is* data destruction.
4. There is no way to distinguish "user turned it off" from "it crashed": no `esp_reset_reason()` call, no RTC-retained state, no counters (verified absent).
5. The physical keyboard is inert (`Keyboard.h:82` `CONNECTED_COLS = 0`; scan loop degenerates at `Keyboard.cpp:135`) — **serial is currently the only input**, so any recovery path must be fully serial-operable from day one.

## 2. Boot-loop detection (RT-02 + RT-03)

### 2.1 Retained state

```c++
// PROPOSED — src/utils/CrashRecord.h
struct CrashRecord {                    // lives in RTC_NOINIT_ATTR memory
    uint32_t magic;                     // 0x4E4D4F53 'NMOS'; invalid after power-on
    uint32_t sessionId;                 // diagnostics spec §3
    uint16_t bootCount;                 // lifetime warm boots (wraps)
    uint8_t  abnormalCount;             // CONSECUTIVE abnormal resets with short uptime
    uint8_t  flags;                     // bit0: user requested safe mode (!safemode)
                                        // bit1: this session reached STABLE (60 s)
    uint8_t  lastMode;                  // SystemApp::Mode at last transition
    uint8_t  lastResetReason;           // esp_reset_reason() of the PREVIOUS boot
    uint16_t uptimeAtDeathS;            // coarse, refreshed by heartbeat
    uint32_t memSnapshot[5];            // intFree,intLargest,psFree,psLargest,lvglPct
    uint32_t crc;                       // over all prior fields
};
```

- **Update sites:** `launchApp`/`returnToMenu` mirror `lastMode` (`SystemApp.cpp:727,860`); the 5 s heartbeat (`main.cpp:256-261`) refreshes `uptimeAtDeathS` + `memSnapshot`. **Never written from panic context** (fault spec §H.6): RTC RAM survives the panic as-is.
- **Boot classification** (in `setup()`, before display init):
  - `magic` invalid (power-on / first flash) → initialize, `abnormalCount = 0`.
  - `esp_reset_reason()` ∈ {panic, int_wdt, task_wdt, brownout} **or** (reason==sw/unknown **and** previous session never set STABLE) → `abnormalCount++`.
  - reason == deepsleep or previous session STABLE → `abnormalCount = 0`.
- **STABLE latch:** SystemApp sets `flags.STABLE` once `millis() ≥ 60 000` with at least one successful `lv_timer_handler` pass (checked from the heartbeat). A crash before 60 s counts toward the loop; a crash after a stable hour does not put the *next* boot closer to safe mode.

### 2.2 Trigger threshold

**`abnormalCount ≥ 3` ⇒ this boot enters safe mode** (invariant R-2). Rationale: 1 tolerates a stray panic; 2 tolerates a panic plus its own aftershock (e.g. torn write discovered on the next boot); 3 in a row with sub-60 s uptimes is a loop. The counter and threshold print in every `[BOOT]` line, so field tuning is data-driven.

### 2.3 Other triggers (evaluated in this order, first hit wins)

| # | Trigger | Cause code |
|---|---|---|
| 1 | `flags.bit0` set by `!safemode` command (diag spec §6.2) | `user` |
| 2 | `abnormalCount ≥ 3` | `bootloop` |
| 3 | LittleFS mount failure with format-on-fail **removed** (RT-13 changes `SystemApp.cpp:144` to `begin(false)`) | `fs` |
| 4 | `/vars.dat` quarantined this boot **and** previous reset was abnormal (corrupt persistence implicated in the crash) | `vars` |
| 5 | (future, once keyboard exists) recovery chord held at boot — reserved, not specced until HW-1 lands | `key` |

Not triggers: draw-buffer/display-init failure (`main.cpp:145-159`) cannot reach *any* UI — those paths get the serial-console-then-reboot treatment (R-4, fault spec §G last row) and their reset increments `abnormalCount`, which after 3 loops parks the device in safe mode *if* the display recovers, or keeps a serial console alive if it never does.

## 3. Safe-mode boot path (what is skipped)

Safe mode is a **flag consulted by the normal boot**, not a second firmware. Deltas from the normal sequence (`main.cpp:84-232`, `SystemApp.cpp:93-159`):

| Step | Normal | Safe mode |
|---|---|---|
| Splash | full animation (`main.cpp:173-186`) | skipped entirely (splash never created; saves pool + 2 s) |
| `_vars.begin()` NVS load (`SystemApp.cpp:95`) | yes | skipped |
| App construction (`SystemApp.cpp:104-123`) | all 20 `new`ed | all constructed (they are cheap post-MT-04 lazy arenas) — **but launcher policy hides them (§5)**; constructors must stay allocation-free per allocation policy §4.1 |
| `LittleFS.begin(true)` + vars load (`SystemApp.cpp:144-156`) | mount+format-on-fail, load vars | `begin(false)`, **no** vars load, no proactive file creation (`:145-148` skipped) |
| Giac | linked, initialized lazily on first `:` query (`GiacBridge.cpp:390-395` `initGiac()`) | `:` console answers `Error: disabled in safe mode` without touching `initGiac()` |
| MainMenu | 21 cards (`MainMenu.cpp:88-115`) | safe whitelist only (§5) + SAFE banner |
| SerialBridge | begins at `main.cpp:218` | identical (serial is the lifeline) |
| Heartbeat/probes | on | on (unchanged) |

## 4. Safe-mode UI

- **Screen:** the normal MainMenu machinery reused with a reduced `APPS[]` slice — no new screen framework. Additions: a full-width red banner label "SAFE MODE — <cause>" above the grid, and StatusBar title set to `SAFE` (StatusBar text APIs exist, `StatusBar.cpp:46-146`). Pool cost ≈ 3 cards + 1 label ≪ the normal 21-card grid, so safe mode has *more* LVGL headroom than normal boot, by construction.
- **Visible content:** banner; three cards (§5); footer label with one-line stats: `resets=3 last=task_wdt uptime=41s` (from the crash record).
- **Interaction:** identical key model (LvglKeypad group focus, `MainMenu.cpp:159-197`); every action also reachable via serial `!` commands (§7) since serial is today's only input (§1.5).

## 5. App policy in safe mode

| Card | Included | Why |
|---|---|---|
| Calculation (id 0) | **yes** | the product's core; exercises VPAM/renderer, no persistence needed when vars load is skipped; store commands (`→VAR`) are disabled (no `/vars.dat` writes in safe mode — `executeStore` save at `CalculationApp.cpp:894-901` gated off) |
| Settings (id 10) | **yes** | RAM-only globals (`SettingsApp.cpp:244-305`), zero risk |
| Diagnostics (new, §6) | **yes** | the point of safe mode |
| Grapher, Equations, Calculus, Matrices, Statistics, Probability, Sequences, Regression | no | not needed for recovery; each adds pool/PSRAM surface |
| Python, PeriodicTable, Bridge, Circuit, Fluid2D, ParticleLab, NeuralLab, OpticsLab, NeoLang, Fractal | no | the highest-risk apps (fault spec §C3) and/or auto-load persistent files (`NeoLanguageApp.cpp:116`, `CircuitCoreApp.cpp:1712-1725`) — exactly what a vars/fs-triggered loop must avoid |

Launcher ids remain stable; safe mode filters the card list, it does not renumber (KeyCode/launcher-id append-only discipline).

## 6. Storage semantics in safe mode (and the new global rules)

Global rule changes (RT-13, active in *all* modes, invariant R-3):
1. Every `LittleFS.begin(true)` becomes `begin(false)`: `SystemApp.cpp:144`, `NeoLanguageApp.cpp:903,926`, `CircuitCoreApp.cpp:1364,1483,1720`, `Fluid2DApp.cpp:1454,1485,1517`. Mount failure ⇒ persistence disabled + `[FS] mount=fail action=disabled` — never silent reformat.
2. Corrupt/short `/vars.dat` (magic mismatch, short read, **or new content validation failure: `den==0`, absurd magnitudes** — closing `VariableManager.cpp:153-163,219-225`) ⇒ rename to `/vars.bad` (one generation kept), start empty, `[FS] vars=corrupt action=quarantine`.
3. New writes to `/vars.dat` use write-temp-then-rename (`/vars.tmp` → `rename`) with write-return checking (closing `VariableManager.cpp:169-190`); format v2 adds a CRC32 + version byte.

Safe mode additionally offers (Diagnostics app actions + serial commands, all requiring confirm):

| Action | Effect | Command |
|---|---|---|
| Reset variables | quarantine `/vars.dat` → `vars.bad`, clear in-RAM store | `!vars reset CONFIRM` |
| Delete quarantined files | remove `*.bad` | `!fs clean CONFIRM` |
| Delete app saves | remove `/save.pt`, `/neolang.nl`, `/circuits/*`, `/py/*` selectively (listed first) | `!fs rm <path> CONFIRM` |
| Format filesystem | `LittleFS.format()` — the **only** place format exists post-RT-13 | `!fs format CONFIRM` (safe mode only) |
| Clear NVS `calcVars` | `Preferences::clear()` on the namespace (`VariableContext.cpp:38-70` store; currently write-dead but may hold stale data) | `!nvs clear CONFIRM` (safe mode only) |
| Clear crash record | zero `abnormalCount` + record | `!panic clear` |

Settings are **not** persisted anywhere today (globals re-initialized each boot, `main.cpp:31-33`; SettingsApp has no save path — verified), so "reset settings" is vacuously a reboot; when settings persistence is eventually added it must join table rows above.

## 7. Exiting safe mode

1. **Normal exit:** menu action "Exit safe mode" (or `!reboot`): clears `flags.bit0`, sets `abnormalCount = 0`, `esp_restart()`. `[SAFE] exit action=reboot`.
2. **Auto-exit:** safe mode never persists by itself — it is computed per boot from the trigger table (§2.3). If the underlying cause is gone (counter cleared, fs mounts, no user flag), the next boot is normal.
3. **Re-entry protection:** if safe mode *itself* crashes 3× (counter keeps counting; `[BOOT] safe=1` sessions that die < 60 s still increment), the boot degrades to **serial-only mode**: no LVGL init at all after the `[BOOT]` block, DiagConsole servicing `!` commands in a minimal loop. This is the floor of the recovery ladder: full UI → safe UI → serial console. (The draw-buffer-failure path from `main.cpp:145-148` converges on this same serial loop, R-4.)

## 8. On-screen diagnostics (the Diagnostics app, RT-15)

One new app (`src/apps/DiagnosticsApp.{h,cpp}`), launcher-visible in **both** modes (safe mode: always; normal mode: behind Settings → "Diagnostics" or a launcher card — owner's call at implementation, default: visible card id 21 appended).

Pages (UP/DOWN to switch; one `lv_label` + monospace text per page — deliberately primitive, ≤ 10 LVGL objects total):
1. **Boot/crash:** `[BOOT]` fields, full `[PANIC]` record, abnormalCount, STABLE state.
2. **Memory:** live `[MEM]`-probe values (same sources as `MemProbe.h:65-100`), refreshed 1 Hz by `update()` (pull-based, no timer — StatusBar pattern).
3. **Storage:** mount state, file list + sizes (the `!fs` data), quarantine files present.
4. **Input echo:** last 8 KeyEvents (code/action/source) — the hardware bring-up page: when the matrix is finally enabled (`Keyboard.h:82`), pressing keys must show scancodes here; also displays modifier FSM state (`KeyboardManager`) live.
5. **Display test:** full-screen RGB bars + border rectangle (bring-up: catches BGR/rotation/clipping regressions, cf. rotation/BGR flags `platformio.ini:55-71`).
6. **Actions (safe mode only):** the §6 table, ENTER-to-confirm dialogs.

All pages render from fixed buffers (`snprintf` into `char[512]`), zero heap, zero content from user files (privacy §10 of diagnostics spec).

## 9. Operating without a keyboard

Today the matrix is disabled (`Keyboard.h:82`) and *everything* rides SerialBridge (`main.cpp:248-251`) — so the degraded-input story is the *current* story, made explicit:

1. Every safe-mode/Diagnostics action has a `!` command equivalent (§6); the UI is a convenience layer over DiagConsole, not the other way around.
2. Key injection over serial (`SerialBridge.cpp:108-241` single-char map) keeps working in safe mode — the menu is navigable with `w/a/s/d` + Enter exactly as in normal mode.
3. Serial-only floor mode (§7.3) requires no display and no keypad.
4. When the physical matrix is brought up (HW-1), a stuck-key storm (hardware fault) must not lock recovery out: DiagConsole remains reachable because SerialBridge polls independently of the key queues (`main.cpp:248-251`), and queue-overflow drops are logged (`[INPUT] drop`), not blocking.

## 10. Emulator simulation (RT-16)

The emulator has no RTC, no resets, and no panics — it simulates the *decision logic*, not the physics:

1. **CLI:** `--boot-count N` (sets simulated `abnormalCount`), `--safe-mode` (equivalent to `flags.bit0`), `--reset-reason <name>` — parsed in NativeHal's arg block (`NativeHal.cpp:1231-1258` region; append-only). Deterministic: flags, not files.
2. **Persistent analogue (optional):** `emulator_data/crash_record.bin` read/written by the NativeHal boot path so multi-run scripts can exercise the counter across emulator invocations; deleted by `!panic clear`. (The emulator FS shim is already the LittleFS analogue, `FileSystem.cpp:38-85`.)
3. **`.numos` vocabulary:** new commands `diag <cmd>` (feed DiagConsole, since SerialBridge isn't compiled natively — `platformio.ini:195-266`) and `assert_safe_mode` (reads a NativeHal-side `g_safeMode` debug flag). Append-only additions per the script-grammar contract (`NativeHal.cpp:1447-1612`).
4. **Whitelist additions:** `DiagnosticsApp` + `CrashRecord`/`DiagConsole` sources join the emulator `build_src_filter`; MainMenu safe-filtering compiles natively already (pure logic).
5. **What the emulator proves:** trigger arithmetic, card filtering, Diagnostics rendering, quarantine/reset flows against `emulator_data/`, `[SAFE]`/`[DIAG]` line formats. **What it cannot prove:** RTC survival across real resets, reset-reason mapping, panic→record integrity, WDT interplay — hardware tests I-H2/I-H3 (fault spec §I) own those.

## 11. Acceptance criteria (summary; full matrix in fault spec §I)

- I-E8 (emulator): `--boot-count 3` boots to safe mode; whitelist enforced; exit action returns to normal; deterministic replays byte-stable.
- I-E6/I-E7 (emulator): corrupt/truncated vars quarantine flows.
- I-H3 (hardware): three real crash-boots enter safe mode; stable session clears the counter.
- I-H2 (hardware): `[PANIC]` replay matches the killed session (app id, uptime ±5 s, session id).
- Negative: 2 crashes then a 60 s stable session then 2 more crashes must **not** trigger safe mode (counter reset by STABLE latch).
- Negative: safe mode itself must never write to `/vars.dat`, `/neolang.nl`, `/circuits/*`, `/save.pt` (verify by fs snapshot diff across a full safe-mode session).
