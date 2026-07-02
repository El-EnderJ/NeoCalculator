# NumOS — Runtime Diagnostics & Telemetry Specification

> Defines every diagnostic surface of NumOS on ESP32-S3: log line grammar, probe points, crash records, the serial diagnostic command set, ring buffer, build-variant policy, emulator equivalents, and privacy rules. Verified against the working tree at `63e7408` (post Phase MEM-A). **VERIFIED** facts cite `file:line`; everything else is **PROPOSED** and maps to tickets RT-xx in `NUMOS_RELIABILITY_AND_FAULT_CONTAINMENT` companion docs.
>
> Prior art that this spec extends (and must not break): the MEM-A `[MEM]` probe line (`src/utils/MemProbe.h:76-99`) is already a grep-stable contract used by the on-device leak-canary procedure (`NUMOS_MEM_A_IMPLEMENTATION_NOTE.md`). Its format is frozen here.

---

## 1. Existing diagnostic surface (verified)

| Surface | Format / behavior | Evidence |
|---|---|---|
| Serial backend | UART0 @115200 (`NUMOS_SERIAL` = `Serial`; USB-CDC variant exists) | `main.cpp:44,85`; `NumosSerialBackend.h:6-29`; `platformio.ini:46-47` |
| `[MEM]` probe | `[MEM] <tag> int=<free>/<largest> psram=<free>/<largest> low=<intLow>/<psramLow> lvgl=<used%>/<max%> stack=<words>`; no heap use (stack `char[160]`); native = no-op | `MemProbe.h:47-111` |
| Probe points | `boot` (`main.cpp:231`), `hb <N>s` heartbeat every 5 s (`main.cpp:256-261`), `enter id=<n>` (`SystemApp.cpp:738-742`), `exit mode=<n>` (`SystemApp.cpp:213-217`), `giac-pre/post/post-ex` (`GiacBridge.cpp:394,439,442,445`) | cited |
| LVGL assert | `[LVGL] ASSERT FAILED at <file>:<line> — most likely lv_malloc OOM …` then `abort()` | `lv_conf.h:153-164` |
| Boot banner | `>>> NumOS: System Ready (<backend label>)`, `=== NumOS Boot ===`, `[PSRAM] %u KB libres` / `[PSRAM] NO DETECTADA!`, `[BOOT] Draw buffer: …` | `main.cpp:99-102,117-122,149-151` |
| Panic decoding | `monitor_filters = esp32_exception_decoder` (needs attached monitor) | `platformio.ini:19` |
| Equations step budget log | `logStepBudget()` prints InternalFree/MaxBlock/PSRAMFree | `EquationsApp.cpp:505-522` |
| Giac console | `:`-prefixed serial line → `solveWithGiac` → result printed raw | `SerialBridge.cpp:149-160` |
| Ad-hoc `Serial.printf` | scattered per-app logs, mixed Spanish/English, no stable grammar | e.g. `GrapherApp.cpp:578` `[GRAPHER] FAIL`, `SystemApp.cpp:1056` |

**Gaps (verified absent):** no reset-reason logging, no boot/session identifier, no crash record, no serial command set beyond keys + `:`giac, no ring buffer, no LVGL-OOM *pre*-warning, no per-operation timing anywhere.

---

## 2. Log line grammar (normative)

### 2.1 General rules

1. One event = one line, `\n`-terminated, ≤ 200 chars, ASCII only.
2. Line shape: `[TAG] <subject> key=value key=value…`. Keys are lowercase, values contain no spaces. Free text only allowed after all key=value pairs.
3. **No heap allocation** on any log path (the MemProbe pattern: fixed stack buffer + `snprintf`, `MemProbe.h:75-100`). Log emission must be safe under memory pressure — that is when it matters.
4. Tags are a closed set: `[BOOT] [MEM] [APP] [LVGL] [CAS] [GIAC] [GRAPH] [FS] [INPUT] [PANIC] [SAFE] [DIAG]`. New tags require a spec update. Existing ad-hoc prefixes (`[GRAPHER]`, `[SYSTEM]`, `[PSRAM]`…) are grandfathered but new code uses the closed set.
5. Grep-stability contract: the *first two tokens* (`[TAG] subject`) and existing key names never change meaning; new keys append.
6. Every line is emitted through `NUMOS_SERIAL` (`main.cpp:44`) and, when the ring buffer exists (§7), mirrored into it.

### 2.2 `[BOOT]` — one block per boot (RT-02)

```
[BOOT] session=5f3a91c2 build=esp32s3_n16r8 sw=1.0-alpha reset=poweron boots=1 abnormal=0 safe=0
[BOOT] Draw buffer: 32768 B internal DMA OK          ← existing line, kept (main.cpp:149-151)
[PSRAM] 8123 KB libres                               ← existing, kept (main.cpp:118)
[MEM] boot int=... psram=... ...                     ← existing, kept (main.cpp:231)
```

- `session`: 32-bit hex boot-session id, generated once per boot from `esp_random()`; stored in the RTC crash record so post-mortem lines correlate with the session that died.
- `reset`: `esp_reset_reason()` mapped to `poweron|sw|panic|int_wdt|task_wdt|brownout|deepsleep|other(N)`.
- `boots`: RTC lifetime warm-boot counter; `abnormal`: consecutive abnormal-reset counter (drives safe mode, R-2); `safe`: 1 when this boot is safe mode.
- Emitted **before display init** (as early as `main.cpp:99`), so a display-dead device still reports.

### 2.3 `[MEM]` — frozen existing format + two proposed events

Existing (frozen, `MemProbe.h:76-99`):
```
[MEM] enter id=1 int=182344/65536 psram=7712840/7340032 low=170000/7500000 lvgl=31%/44% stack=9812
```
Proposed additions (RT-08), same emitter, new tags-as-subjects:
```
[MEM] LOW domain=int largest=38912 floor=40960
[MEM] CRITICAL domain=int largest=18944 floor=20480 action=return-to-menu
```
Rate limit: one `LOW` line per domain per 30 s; `CRITICAL` always emitted.

### 2.4 `[APP]` — lifecycle events (RT-05)

```
[APP] enter id=1 name=Grapher
[APP] exit id=1 ms=48211 lvglmax=61%
[APP] begin-fail id=14 stage=buf:velocity
[APP] launch-refused id=13 reason=lvgl pool=83%
[APP] state-violation from=TEARING_DOWN ev=launch
```
- `enter`/`exit` complement the existing `[MEM] enter/exit` probes (same call sites, `SystemApp.cpp:738-742,213-217`); `ms` = session duration; `lvglmax` = pool high-water during the session (`lv_mem_monitor().max_used`, reset per LVGL API availability — LVGL 9.5 has no reset hook, so report the monotonic max and let tooling diff).
- `stage` in `begin-fail` names the exact allocation that failed (each guarded alloc site gets a short label).

### 2.5 `[LVGL]`

```
[LVGL] ASSERT FAILED at src/core/lv_obj.c:123 — most likely lv_malloc OOM: ... ← existing (lv_conf.h:157-159)
[LVGL] pool used=52% max=61% objs~=118        ← proposed: emitted with each [APP] exit (debug builds)
[LVGL] layout-depth cap=16 node=Fraction      ← proposed: defensive layout cap hit (RT-11)
```

### 2.6 `[CAS]` — in-tree engine (RT-09)

```
[CAS] solve kind=omni vars=1 ms=482 steps=12 arena=51200 status=ok
[CAS] solve kind=system2x2 ms=5003 steps=7 status=deadline
[CAS] steps-cap shown=90 total=143
[CAS] ac-skip terms=11
```
- Emitted at the single exits of `EquationsApp::solveOmni/solveSystem` (`EquationsApp.cpp:2173-2257`) and `CalculusApp::computeResult` (`CalculusApp.cpp:783-886`) — `kind=diff|integrate` for Calculus.
- `arena` = `SymExprArena` bytes used at completion. `status` ∈ `ok|deadline|oom|error`.

### 2.7 `[GIAC]` — bridge telemetry (extends the existing MEM probes, RT-09)

```
[GIAC] q=17 len=42 ms=1832 dpsram=-131072 stackhw=1892 status=ok
[GIAC] q=18 len=42 ms=10002 dpsram=-905216 stackhw=1211 status=timeout
[GIAC] refused reason=mem largest=311296 floor=524288
[GIAC] refused reason=len len=3970 max=2048
```
- `q` = per-boot query counter; `dpsram` = free-PSRAM delta pre→post (leak/pressure trend); `stackhw` = loopTask high-water after the call (`uxTaskGetStackHighWaterMark`) — the *minimum ever*, so watch its trend across `q` values.
- Emitted from `solveWithGiac` around the existing `giac-pre/post` probes (`GiacBridge.cpp:394,439-445`). The `[MEM] giac-pre/post` lines remain (frozen contract).
- Result *content* is never logged (§10); only `len`.

### 2.8 `[GRAPH]` (RT-12)

```
[GRAPH] replot funcs=3 implicit=1 ms=142 evals=5136 aborted=0
[GRAPH] replot funcs=1 implicit=1 ms=201 evals=18240 aborted=1
[GRAPH] calc op=root ms=88 status=ok
[GRAPH] buf-fail bytes=91520
```
Emitted at the single exit of `replot()` (`GrapherApp.cpp:1865-1912`) and `executeCalcOption` (`GrapherApp.cpp:3083-3155`). Debug builds only for per-replot lines (§8); `aborted=1` and `buf-fail` always.

### 2.9 `[FS]` (RT-13)

```
[FS] mount=ok
[FS] mount=fail action=disabled
[FS] vars=loaded n=11
[FS] vars=corrupt action=quarantine file=/vars.bad
[FS] vars=short action=quarantine
[FS] save.pt=invalid action=ignored
[FS] write-fail file=/vars.dat stage=body
```

### 2.10 `[INPUT]` (RT-04/RT-14)

```
[INPUT] drop q=lvgl n=7
[INPUT] depth-cap                      ← debug only
[INPUT] mod-reset from=ALPHA_LOCK      ← L-7 reset on menu return, debug only
```
`drop` lines aggregate: counter per queue, flushed at most once per 5 s (piggybacks the heartbeat).

### 2.11 `[PANIC]` — next-boot replay of the RTC crash record (RT-02)

```
[PANIC] n=2 reason=task_wdt app=13 uptime=417s session=5f3a91c2
[PANIC] mem int=18944/12288 psram=311296/262144 lvgl=88% stack=402
```
- Replayed by `setup()` immediately after the `[BOOT]` line when `reset` is abnormal. `n` = consecutive abnormal count; `app` = the `Mode` value mirrored into RTC at each `launchApp`/`returnToMenu`; `mem` = the last heartbeat snapshot before death.
- The record itself is **not** written from panic context — it is continuously refreshed from the heartbeat (fault-containment spec §E.2).

### 2.12 `[SAFE]` (RT-03)

```
[SAFE] enter cause=bootloop count=3
[SAFE] enter cause=user
[SAFE] exit action=reboot
[SAFE] reset action=vars confirm=yes
```

---

## 3. Boot session ID

- 32-bit, hex-printed, from `esp_random()` (true RNG post-RF-init; if generated before WiFi/BT init entropy is weaker — acceptable for correlation, not security).
- Printed once in `[BOOT]`; stored in the RTC crash record; echoed in `[PANIC]` replay so a log capture spanning a crash can stitch sessions.
- Emulator: derived from the deterministic tick seed when `--deterministic` (stable across replays ⇒ goldenable logs), `SDL_GetTicks`-seeded otherwise (RT-16).

## 4. App enter/exit events

Covered by §2.4 + existing `[MEM] enter/exit`. Invariant: **every** `enter` has a matching `exit` (deferred teardown guarantees `teardownModeNow` runs, `SystemApp.cpp:238-248`; the exit probe sits at its end `:213-217`). Tooling may treat a missing `exit` as a crash-in-app marker — this is exactly what the RTC `app` field records.

## 5. Memory probes

Frozen as implemented (MemProbe, §1). Additions:
- **Low-water alarms** (§2.3) from a `memGuard()` check running inside the existing 5 s heartbeat (`main.cpp:256-261`) — no new timers.
- **LVGL OOM events**: the assert handler already fires on exhaustion (G-1). *Pre*-exhaustion visibility comes from the `lvgl=` field (present in every probe) plus the §2.4 `launch-refused` gate at 80 %.
- **CAS/Giac probes**: `giac-pre/post/post-ex` exist (`GiacBridge.cpp:394-445`); RT-09 adds `[CAS]`/`[GIAC]` timing lines; no new MemProbe tags needed (per-solve arena deltas ride in `[CAS] arena=`).
- **Graph replot probes**: `[GRAPH] replot` (§2.8) carries `ms`/`evals`; memory impact is visible via heartbeat (replot allocates only the transient implicit grid, `GrapherApp.cpp:1728`).

## 6. Serial diagnostic command set (RT-04)

### 6.1 Grammar

- Prefix `!`, full line, case-insensitive command word, optional args. Verified free: `SerialBridge::processChar` maps no `'!'` (single-char map `SerialBridge.cpp:174-233`; keyword list `:162-171`; unknown lines are echoed+dropped `:235-239`). `:` remains Giac (`:149-160`); bare chars remain key injection. Grammar is append-only once shipped (same policy as the `.numos` vocabulary).
- Every command answers at least one line; unknown → `ERR unknown cmd=<word>`. All answers are `[DIAG]`-tagged so scripts can grep.

### 6.2 Commands (v1)

| Command | Action / output |
|---|---|
| `!help` | list commands, one line each |
| `!ver` | `[DIAG] ver sw=1.0-alpha build=esp32s3_n16r8 lvgl=9.5.0 giac=1.9.0 session=<id>` |
| `!mem` | emit one `[MEM] diag …` probe line immediately |
| `!uptime` | `[DIAG] uptime s=<n> hb=<n>` |
| `!app` | `[DIAG] app mode=<n> pending=<n> state=<lifecycle-state>` |
| `!panic` | replay stored `[PANIC]` record(s); `[DIAG] panic none` if clean |
| `!panic clear` | zero the abnormal counter + record; answers `[DIAG] panic cleared` |
| `!fs` | `[DIAG] fs mount=ok used=<kb> total=<kb>` + one line per known file (`/vars.dat`, `/neolang.nl`, `/save.pt`, `/py/*`, `/circuits/*`) with size |
| `!vars` | `[DIAG] vars n=<count>` + per-slot line: name + *type only* (no values — §10); `!vars full` prints values (debug builds only) |
| `!vars reset` | requires `!vars reset CONFIRM`; quarantines `/vars.dat` → `vars.bad`, resets in-RAM store; `[SAFE] reset action=vars` |
| `!log` | dump the ring buffer (§7) |
| `!safemode` | set the RTC safe-mode-request flag + `esp_restart()`; next boot enters safe mode with `cause=user` |
| `!reboot` | `esp_restart()` after `[DIAG] reboot` |
| `!selftest` | run the non-destructive subset of the diagnostic screen checks (mem floors, fs mount, LVGL pool, clock) and print PASS/FAIL per check |

Safety rules: destructive commands (`reset`, `format` — the latter exists only in safe mode, see safe-mode spec §6) require the literal `CONFIRM` argument; nothing destructive is reachable from a single line without it.

### 6.3 Placement

Implemented inside `SerialBridge::processChar` line handling (`SerialBridge.cpp:127-171`), branching on `!` before the keyword table, delegating to a new `src/utils/DiagConsole.{h,cpp}`. The Giac `:` path and key mappings are untouched (fragile-file rule: SerialBridge grammar is append-only).

## 7. Ring buffer (optional, RT-04 phase 2)

- 8 KB PSRAM circular text buffer of the last emitted lines (mirror hook in the single emit function). PSRAM because internal RAM is the scarce class (memory audit §1) and the buffer is diagnostic-only.
- Dumped by `!log`; **never** written to flash automatically; **not** preserved across resets (RTC is too small; flash-on-panic rejected — fault spec §H.6). Its value: post-hoc inspection of the seconds *before* a hang that did not panic (WDT resets lose RAM, so `!log` is for live/frozen-but-serial-alive states and pre-reboot capture).
- Mirror writes are memcpy-only (no allocation), guarded by a single `bool` so early-boot lines (before PSRAM check, `main.cpp:117`) skip mirroring.

## 8. Release vs debug policy

| Surface | Release (esp32s3_n16r8) | Debug/validate (`_validate*`, emulator) |
|---|---|---|
| `[BOOT]`, `[PANIC]`, `[SAFE]`, `[FS]` actions, `[MEM]` probes + LOW/CRITICAL, `[APP]` enter/exit/begin-fail/refused, `[GIAC]` status lines, `[GRAPH] aborted=1`, `[INPUT] drop` | **on** (they are the product's black box; ≤ a few lines/min steady-state) | on |
| Per-op timing: `[GRAPH] replot` every pass, `[CAS] solve` every solve, `[LVGL] pool` per exit | off (flag `NUMOS_DIAG_VERBOSE=0`) | on (`-DNUMOS_DIAG_VERBOSE=1`) |
| Content-bearing output: `!vars full`, expression text, `[INPUT] depth-cap` | compiled out (`NUMOS_DIAG_CONTENT=0`) | on |
| Ring buffer | on (8 KB PSRAM is negligible) | on |
| `LV_USE_ASSERT_NULL/MALLOC` | **on** (already shipped, `lv_conf.h:126-127`) | on |
| Probe kill-switch | `-DNUMOS_MEM_PROBE_ENABLE=0` remains available (`MemProbe.h:47-53`) | n/a |

## 9. Emulator equivalents & hardware-only measurements

Emulator (`emulator_pc`, `NATIVE_SIM`):
- `[MEM]` probes are no-ops by design (`MemProbe.h:51,109`) — heap numbers would be libc lies. **Keep no-op.**
- All *logic* lines are emitted identically: `[APP]`, `[CAS]` (whitelisted cas:: subset), `[GRAPH]`, `[FS]` (via `hal/FileSystem`), `[INPUT]`, `[BOOT]` (session id per §3), `[SAFE]` (simulated trigger, RT-16), `[DIAG]` console (SerialBridge is not compiled into the emulator — `platformio.ini:195-266` whitelist — so the emulator reaches DiagConsole through a new `.numos` command `diag <cmd>`; append-only vocabulary extension, RT-16).
- CI consumption: scripts assert on stdout lines exactly as Phase 4B/8C already grep `PASS -` (`.github/workflows/emulator-build.yml`); the fault-injection tests (fault spec §I-E) key off `[APP] begin-fail`, `[FS] …quarantine`, `[GRAPH] aborted=1`.

Hardware-only (cannot be measured in the emulator; procedures in fault spec §I-H):
- true internal/PSRAM/largest-block/low-water numbers; LVGL 64 KB pool behavior (emulator uses CLIB, `platformio.ini:176`); loopTask stack high-water; TWDT/brownout/panic behavior and reset reasons; Giac timing/memory/timeout behavior (Giac never builds natively, `platformio.ini:192-194`); flash wear and power-loss write truncation; boot-loop counting across warm resets.

## 10. Privacy / data-content rules

NumOS is a single-user offline device, but logs leave the device (bug reports, CI artifacts, shared captures). Rules:
1. **No user math content in release logs**: expressions, variable *values*, Giac queries/results, filenames of user scripts are never emitted by release builds — only lengths, counts, node counts, hashes if correlation is needed. (Giac console output printed as the *interactive response* at `SerialBridge.cpp:156-157` is the user's own foreground session, not telemetry — allowed.)
2. Debug builds may log content behind `NUMOS_DIAG_CONTENT=1`, which the four `_validate*` envs may set; the production env never does.
3. No identifiers beyond the random per-boot session id: no MAC, no chip id, no serial number in any log line.
4. Crash records contain mode ids and memory numbers only — never buffer contents.
5. There is no network path in the firmware (no WiFi/BT usage in `src/` today); this spec introduces none.

---

*Implementation decomposition: RT-01 (WDT), RT-02 (crash record/[BOOT]/[PANIC]), RT-04 (DiagConsole + ring buffer + [INPUT]), RT-08 ([MEM] LOW/CRITICAL), RT-09 ([CAS]/[GIAC]), RT-12 ([GRAPH]), RT-13 ([FS]), RT-16 (emulator diag plumbing) — see `NUMOS_RELIABILITY_IMPLEMENTATION_TICKETS.md`.*
