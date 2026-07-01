# NumOS — ESP32-S3 Memory & PSRAM Audit

> Ground-truth map of every allocation mechanism, allocation site, memory domain, and lifetime contract in NumOS, verified against source at `main` @ `8662f47` (audit branch `claude/esp32s3-memory-audit-zkjqrb`, 2026-07-01).
>
> Verification method: static audit of `src/`, `lib/`, `platformio.ini`, plus **live builds performed during this audit**: `pio run -e emulator_pc` (SUCCESS, 20.9 s, LVGL resolved to 9.5.0) and `pio run -e esp32s3_n16r8` with the real Xtensa toolchain (GCC 8.4.0, IDF v4.4.7, arduino-esp32 core 2.x) — see §9 for measured figures. LVGL-internal claims are cited against the *resolved* LVGL 9.5.0 source in `.pio/libdeps/emulator_pc/lvgl/` (identical library version for both targets: `lvgl/lvgl@^9.2.0` resolves to 9.5.0, `platformio.ini:107,178`).
>
> Companion docs: `NUMOS_MEMORY_ALLOCATION_POLICY.md` (normative rules), `NUMOS_MEMORY_RISK_REGISTER.md` (risks), `NUMOS_MEMORY_IMPLEMENTATION_TICKETS.md` (mitigations). Supersedes/corrects several memory claims in `NUMOS_ARCHITECTURE_GROUND_TRUTH.md` — see §10.

---

## 1. Executive summary — the five facts that change everything

1. **The LVGL heap on firmware is a fixed 64 KB internal-RAM pool, not PSRAM.** `src/lv_conf.h:59-88` configures memory with the **LVGL-8** macro `LV_MEM_CUSTOM`, which LVGL 9 does not read. Since the firmware env never defines `LV_USE_STDLIB_MALLOC` (`platformio.ini:39-98`; only `emulator_pc` sets it, `platformio.ini:176`), LVGL 9.5 falls back to `LV_STDLIB_BUILTIN` (`lv_conf_internal.h:118-122` in the resolved lvgl 9.5.0) with `LV_MEM_SIZE = 64*1024` (`lv_conf_internal.h:199-205`) allocated as a **static `.bss` array in internal DRAM** (`src/stdlib/builtin/lv_mem_core_builtin.c:82`: `static MEM_UNIT work_mem_int[LV_MEM_SIZE/sizeof(MEM_UNIT)]`), with `LV_MEM_POOL_EXPAND_SIZE 0` (no growth). Every LVGL object, style, label text, `lv_table` cell string, and `lv_chart` point array on firmware competes for these 64 KB. The celebrated "512 B internal/PSRAM cutoff" (`lv_conf.h:74-82`) is **dead configuration** on both targets. This retroactively explains the documented boot crash when all apps `begin()` eagerly (`SystemApp.cpp:100-103`) and the emulator hang that forced `-DLV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB` (`platformio.ini:170-176`).

2. **`GiacAlloc.cpp` overrides `operator new`/`delete` for the entire firmware, not just Giac.** `src/math/giac/GiacAlloc.cpp:24-66` (active when `NUMOS_USE_GIAC && ARDUINO && BOARD_HAS_PSRAM`, i.e. every firmware env) replaces the **global** C++ allocation operators: every `new`, every `std::vector`/`std::string`/`std::map` default allocation, every app object, goes `heap_caps_malloc(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)` first with `MALLOC_CAP_8BIT` (any heap) fallback (`GiacAlloc.cpp:27-31`). Consequences: (a) the de-facto policy is "all C++ heap → PSRAM"; (b) the per-class overrides in `MathAST.cpp:50-69` and the STL `PSRAMAllocator` (`src/math/cas/PSRAMAllocator.h:66-96`) are redundant *on firmware* (they still matter on native builds and if Giac is ever compiled out); (c) internal RAM is protected from C++ churn, at the price of PSRAM latency on every small node allocation; (d) the emulator never compiles this file, so allocator behavior diverges between targets by design.

3. **Plain C `malloc`/`calloc` follows a different, sdkconfig-defined policy.** The qio_opi Arduino SDK sets `CONFIG_SPIRAM_USE_MALLOC 1`, `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL 4096`, `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL 0` (`framework-arduinoespressif32/tools/sdk/esp32s3/qio_opi/include/sdkconfig.h:316-319`). So `malloc` requests **< 4 KB prefer internal RAM**, ≥ 4 KB prefer PSRAM — this governs Arduino `String` (`WString` uses `malloc`/`realloc`), libtommath (`MP_MALLOC→malloc`, `lib/libtommath/tommath_private.h:95-98` — never overridden), mbedtls bignum limb arrays (used by `CASInt` promotion), and LittleFS internals. **`RESERVE_INTERNAL 0` means nothing protects the internal heap floor** for future DMA-capable allocations once small-malloc churn accumulates.

4. **Internal DMA RAM carries a dead 32 KB buffer.** The LVGL draw buffer is a single 32 KB `MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA` block (`main.cpp:138-150`) — correct and mandatory. But `DisplayDriver::initLvgl` allocates a **second** 32 KB+31 B internal-DMA "staging" buffer (`DisplayDriver.cpp:167-180`) that is unreachable code: `_dmaEnabled` is force-set `false` (`DisplayDriver.cpp:127,189`), so every flush takes the blocking `pushColors` path (`DisplayDriver.cpp:337`) and the staging path (`:263`) never executes. ≈ 32 KB of the scarcest memory class in the system is allocated and never used.

5. **PSRAM budget is comfortable at boot but not unbounded in use.** Eager, boot-time PSRAM: ~288 KB of CAS arenas constructed inside app constructors (all 20 apps are `new`ed at boot, `SystemApp.cpp:104-123`: CalculationApp `_eduArena` 64 KB, EquationsApp `_stepsTutorArena`+`_arena` 128 KB, CalculusApp `_arena` 64 KB, NeoLanguageApp `_symArena` 32 KB — each `SymExprArena` ctor allocates its first block immediately, `SymExprArena.h:65-71`) plus the app objects themselves. Peak per-app usage on top: PythonEngine **1 MB** heap, ParticleLab **230 KB**, Fractal **~171 KB + 138 KB transient**, NeuralLab **172 KB**, Fluid2D **159 KB**, OpticsLab **138 KB**, NeoLanguage up to **~1.16 MB** (script-dependent), Grapher **91.5 KB**, EquationsApp `_casPool` **256 KB** (lazy). Worst realistic single-app peak ≈ 1.3 MB out of 8 MB — headroom is real, but Giac is **uncapped** (§5.6) and deferred teardown means two apps' buffers can overlap for ~250 ms (§7.2).

## 2. Allocation mechanisms — who decides where a byte lands

Six independent mechanisms coexist. For any allocation in NumOS, the domain is decided by the *first matching row*:

| # | Mechanism | Trigger | Domain on firmware | Domain on emulator | Evidence |
|---|---|---|---|---|---|
| 1 | Explicit `heap_caps_malloc(caps)` | Direct call | As requested (`INTERNAL\|DMA`, `SPIRAM`, `8BIT`) | n/a (sites are `#ifdef ARDUINO`; native branches use `malloc`/`new[]`) | e.g. `main.cpp:141`, `Fluid2DApp.cpp:128-140`, `MemoryUtils.h:70` |
| 2 | Class `operator new` override (`vpam::MathNode`) | `new NodeX` / `make_unique<NodeX>` | PSRAM first, any-heap fallback | libc `malloc` | `MathAST.h:598-599`, `MathAST.cpp:50-69` |
| 3 | STL `PSRAMAllocator<T>` / `utils::PSRAMBuffer<T>` | Container/buffer declared with it | PSRAM first (allocator: any-heap fallback; PSRAMBuffer: **no fallback**, fails) | libc `malloc` | `PSRAMAllocator.h:66-86`, `MemoryUtils.h:64-80` |
| 4 | **Global `operator new` override** | Every other C++ `new`, incl. `std::string`/`std::vector` default allocator, all Giac allocations | PSRAM first, any-heap fallback | **not compiled** → libc `new` | `GiacAlloc.cpp:24-31,42-66` |
| 5 | C `malloc`/`calloc`/`realloc` | Arduino `String`, libtommath, mbedtls limbs, LittleFS, C libs | < 4096 B → internal-preferred; ≥ 4096 B → PSRAM-preferred; no internal reserve | libc | sdkconfig.h:316-319 (qio_opi); `tommath_private.h:95-98` |
| 6 | `lv_malloc` (all LVGL internals) | Every LVGL object/style/text/cell/anim | **Fixed 64 KB static TLSF pool in internal `.bss`**, no expansion | system heap (`LV_STDLIB_CLIB`, `platformio.ini:176`) | lvgl 9.5.0 `lv_conf_internal.h:118-122,199-205`, `lv_mem_core_builtin.c:77-86` |

Static data (`.data`/`.bss`, e.g. the LVGL pool, `Keyboard` state arrays, `PythonEngine` `s_vars[32]`) is internal DRAM (no `CONFIG_SPIRAM_ALLOW_BSS_SEG` in the SDK config). `const`/PROGMEM data (fonts ≈ 320–340 KB, `Constants.h` π/e digit tables 2×1001 B at `Constants.h:54-101`, `ChemDatabase.h:51` ≈ 7 KB) lives in flash/DROM. Stacks: `loopTask` 64 KB (`-DARDUINO_LOOP_STACK_SIZE=65536`, `platformio.ini:44`; SDK default would be 8 KB, sdkconfig.h:88) — **all of NumOS including Giac runs on this one stack** — plus FractalApp's transient 32 KB render task (`FractalApp.cpp:490-498`, core 1, deleted in `stopRenderTask`, `FractalApp.cpp:513-540`).

### 2.1 Precedence pitfalls

- Mechanism 2/3 sites are belt-and-braces on firmware (mechanism 4 would catch them anyway) but are **the only PSRAM routing that exists if `NUMOS_USE_GIAC` is ever unset** — do not remove them as "redundant".
- `GiacAlloc.cpp` frees with plain `free()` (`GiacAlloc.cpp:48-51`) while allocating via `heap_caps_malloc`. This is correct on ESP-IDF (`free` dispatches to the owning capability heap) but is a portability trap.
- `utils::PSRAMBuffer::allocate` has **no internal-RAM fallback** (`MemoryUtils.h:70`) — on PSRAM exhaustion it fails cleanly (returns false). `PSRAMAllocator` and `SymExprArena` *do* fall back to any heap (`PSRAMAllocator.h:78`, `SymExprArena.h:206-208`) — meaning CAS pressure can silently spill into internal RAM precisely when memory is tightest.
- LVGL's pool cannot spill anywhere. Exhaustion ⇒ `lv_malloc` returns NULL ⇒ `LV_ASSERT_MALLOC` (`lv_conf.h:127`) ⇒ default handler `while(1)` ⇒ **silent hang**, the exact failure mode documented for the emulator in `platformio.ini:170-176`.

## 3. Memory domain inventory

| Domain | Size | What lives there (NumOS) | Exhaustion behavior |
|---|---|---|---|
| Internal SRAM (DRAM heap) | 512 KB die SRAM; ≈ 240–280 KB heap after static + framework (measure: §8) | Draw buffer 32 KB + dead staging 32 KB (DMA class); FreeRTOS/task stacks; Arduino `String`s; libtommath/mbedtls limbs; `std::vector<Token>` RPN caches (via mech. 4 → actually PSRAM, see note*) ; malloc'd < 4 KB blocks | `malloc` NULL; DMA-capable allocs fail first (no reserve, sdkconfig `RESERVE_INTERNAL 0`) |
| Internal `.bss`/`.data` | counted in the 512 KB | **LVGL 64 KB pool**; `Keyboard` state (~0.5 KB, `Keyboard.h:115-116`); `LvglKeypad` 8-slot queue (`LvglKeypad.h:77-86`); `SerialBridge` 32-slot buffer (`SerialBridge.h:63-64`); `PythonEngine` `s_vars` 1.5 KB (`PythonEngine.cpp:36-38`) | link-time; visible in `pio run` RAM % |
| DMA-capable internal | subset of DRAM | LVGL draw buffer (`main.cpp:141`), dead staging buffer (`DisplayDriver.cpp:169`) | boot halt if draw buffer fails (`main.cpp:144-147`) |
| PSRAM (8 MB OPI) | ~8 MB minus cache/reserved | All C++ `new` (mech. 4); MathAST nodes; CAS arenas & pools; all sim/render buffers; Python 1 MB heap; Giac working set; `String`/`malloc` ≥ 4 KB | `heap_caps_malloc(SPIRAM)` NULL → per-site fallback or failure; Giac throws `bad_alloc` → caught at `GiacBridge.cpp:435-439` |
| Flash / DROM | 16 MB | Code; STIX fonts ≈ 304 KB bitmap data (+descriptors → ~320–340 KB: `stix_math_18/12/8.c`, `lv_font_montserrat_math_*.c`); `Icons.h` legacy bitmaps up to 96 KB if referenced (referenced via `SystemApp.cpp:163-177` legacy table); π/e digits (`Constants.h:54-101`); element DB (`ChemDatabase.h:51`) | link-time |
| Stack | 64 KB loopTask (+32 KB Fractal task, transient) | Everything: LVGL render recursion, `MathRenderer` ≤ 12-deep draw (`MathRenderer.h:239`), CAS recursion, **all Giac evaluation** | overflow → corruption/panic; Giac recursion guarded only by `MAX_RECURSION_LEVEL=100` (`lib/giac/src/kglobal.cc:1679`) |
| LVGL pool | 64 KB fixed | every `lv_obj`, style, label text, table cell, chart array, anim, timer | `while(1)` hang (§2.1) |
| Filesystem | LittleFS partition (`default_16MB.csv`) | `/vars.dat` 379 B (`VariableManager.h:148-155`: 5 + 11×34 B), `/circuit.bin`, `/save.pt`, `/neolang.nl`, `/scripts/*.py` | write fail; app-level handling |
| NVS | nvs partition | `VariableContext` `PackedVars` ≈ 246 B blob (`VariableContext.h:61-70`) | Preferences API errors |

\* Note: `std::vector<Token>`/`Token::text` (Arduino `String`, `Tokenizer.h:59`) split across mechanisms: the vector's node array via `operator new` → PSRAM; each `String`'s character buffer via `malloc` → internal (< 4 KB). One logical structure, two domains.

## 4. Subsystem memory map

Sizes are computed from source constants; arithmetic shown where non-obvious. "Boot" = allocated before the menu is usable; "Session" = while app is open; "Transient" = scoped inside one operation.

### 4.1 Boot / display / LVGL core

| Item | Domain | Size | Lifetime | Evidence |
|---|---|---|---|---|
| LVGL draw buffer | internal DMA | 32,768 B | boot→forever | `main.cpp:138-150` (single buffer; PSRAM & double-buffering forbidden, comments `:131-137`) |
| DMA staging buffer (dead) | internal DMA | 32,768+31 B | boot→forever | `DisplayDriver.cpp:167-180`; unreachable (`_dmaEnabled=false`, `:127,189`) |
| LVGL builtin pool | internal `.bss` | 65,536 B | static | lvgl 9.5 `lv_mem_core_builtin.c:82`; `LV_MEM_POOL_EXPAND_SIZE 0` |
| SplashScreen (leak) | LVGL pool | 1 screen + 2 labels, ~1 KB pool | boot→forever (never deleted) | `SplashScreen.cpp:51-83`; no `lv_obj_delete` anywhere in the file |
| MainMenu launcher | LVGL pool | 21 cards × 3 objs + grid + screen ≈ 65 objects ≈ 8–12 KB pool | boot→forever (menu persists) | `MainMenu.cpp:88-111` (APPS[]), `:409-459` (3 objects/card), icons drawn via `LV_EVENT_DRAW_MAIN_END` callback — zero pixel buffers (`MainMenu.cpp:508-559`) |
| StatusBar (per app instance) | LVGL pool | 8 objects | app session | `StatusBar.cpp:50-114`; `destroy()` only NULLS pointers — deletion is delegated to the parent screen delete (`StatusBar.cpp:152-165`, contract in `StatusBar.h:61-66`) |
| App objects (20 × `new`) | PSRAM (mech. 4) | ~tens of KB total incl. in-object arrays | boot→forever | `SystemApp.cpp:104-123` |
| Boot-eager CAS arenas | PSRAM | 64+64+64+64+32 = 288 KB | boot→forever (reset ≠ free of block 0) | `SymExprArena.h:65-71` (eager first block); owners: `CalculationApp.h:142`, `EquationsApp.h:203,210`, `CalculusApp.h:151`, `NeoLanguageApp.cpp:94` (`_symArena(32*1024)`) |

### 4.2 Calculation

- History: `std::vector<HistoryEntry> _history`, cap `MAX_HISTORY = 50` (`CalculationApp.h:136-138`). Each entry stores a **deep AST clone** + `ExactVal` (`CalculationApp.h:132-135`) — cost proportional to expression size (MathNode sizes ~24–100 B/node, §5.2); order 1–5 KB per complex entry, so history worst case ~O(100–250 KB) PSRAM — unbounded per-entry, bounded per-count.
- Edu steps: `cas::SymExprArena _eduArena` (64 KB boot-eager) + `StepVec` (PSRAM via `PSRAMAllocator`, `CASStepLogger.h:108`) + `std::vector<std::unique_ptr<StepRenderData>> _stepRenderers` (one AST + one MathCanvas widget per step, `CalculationApp.h:149-153`). Cleared at `CalculationApp.cpp:120-121, 575-576, 758-759, 914-915`.

### 4.3 Grapher

- **Kandinsky buffer**: `utils::PSRAMBuffer<uint16_t> _graphBuf` (`GrapherApp.h:167`), 320 × 143 × 2 = **91,520 B PSRAM** (`GRAPH_CANVAS_H = 240−25−28−24−20 = 143`, `GrapherApp.h:159-160` with constants `:66-70`; allocated `GrapherApp.cpp:564-567`; wrapped by `lv_image` — no copy, `:590-597`). Freed by RAII/`reset()` (`GrapherApp.cpp:184-186`).
- Implicit plots: transient `std::vector<float> g(nx*ny)` = 107×48×4 = **20,544 B** (`GrapherApp.cpp:1723-1728`) — via global `new` ⇒ PSRAM on firmware.
- 6 function slots: `FuncSlot _funcs[6]` (`GrapherApp.h:221,64`), each `CartesianFunction` holds `char text[64]` + 2 × `std::vector<Token>` RPN caches (`GraphModel.h:54-86`); `Token::text` is Arduino `String` (`Tokenizer.h:59`) ⇒ many small internal-heap blocks churned on every expression edit (`GraphModel` re-tokenizes per edit).
- 12 embedded `MathCanvas` (6 expr + 6 template, `GrapherApp.h:134,148`) — draw-callback widgets, no pixel buffers (`MathRenderer.h:154-172`).
- ~60–70 LVGL objects when fully built (tab bar, 6 rows, table 7×21 headers, toolbar, trace) — the single biggest LVGL-pool consumer among covered apps; `lv_table` cell strings also come from the 64 KB pool.
- POIs `POI _pois[20]` ≈ 640 B in-object (`GrapherApp.h:209,77`).

### 4.4 CAS (in-tree) / Giac

- `SymExprArena`: 64 KB blocks × ≤16 = 1 MB ceiling per instance (`SymExprArena.h:59-60`); PSRAM w/ any-heap fallback (`:204-207`); `reset()` keeps block 0 (`:128-142`); `create<T>` returns nullptr on exhaustion (`:101-106`) — **callers must null-check**. **Defect**: `allocRaw` doesn't reject `size > _blockSize` — a single > 64 KB request bump-writes past the freshly allocated block (`SymExprArena.h:86-97`); reachable only via pathological single-node sizes (terms arrays), low likelihood, memory-corruption impact.
- **BigInt leak**: `_bigIntRegistry`/`registerBigInt` (`SymExprArena.h:108-116,196`) has **zero callers** repo-wide; `CASInt` self-manages via its destructor (`CASInt.h:432-440`) — but the bump arena never runs destructors, so an arena-resident `SymNum` whose `CASInt` promoted to `mbedtls_mpi` (struct in PSRAM via `heap_caps_calloc`, `CASInt.h:449-458`; limbs in internal via mbedtls `calloc`) **leaks both on `reset()`**. Arduino-only (native has `CAS_HAS_BIGINT=0`, `CASInt.h:53-59`).
- `CasMemoryPool`: 256 KB PSRAM slab per instance (`CasMemory.h:263,269-273`), lazy `unique_ptr` in EquationsApp/TutorApp (`EquationsApp.h:218`, created `EquationsApp.cpp:1228,2957,3180`). Firmware GCC 8.4 has no `<memory_resource>` (verified against the toolchain) ⇒ `CAS_HAS_PMR=0` ⇒ the in-repo shim is used (`CasMemory.h:90-95,116-247`). Overflow beyond 256 KB falls through to `::operator new` (`CasMemory.h:184-187`) ⇒ PSRAM; **those overflow blocks are never freed by `reset()`** (shim `release()` comment `CasMemory.h:168-170`) — permanent leak per overflowing session. Safety contract: all `shared_ptr<AstNode>` must die before `reset()` (`CasMemory.h:300-304`). Constructing with a failed slab (`_mbr(_buffer=nullptr, size)`) is guarded in the shim (`CasMemory.h:178`: `_buffer &&` check) — falls back to upstream per-allocation.
- `CASStepLogger::StepVec` = `std::vector<CASStep, PSRAMAllocator<CASStep>>` (`CASStepLogger.h:108`), ~80–100 B/step + strings; dedup by hash (`:182-187`); cleared in app lifecycles (§4.2, `CalculusApp.cpp:167,785`, `EquationsApp.cpp:462-463`).
- **Giac**: single global context (`GiacBridge.cpp:44`), lives forever; all allocation via global `new` ⇒ PSRAM (§2 mech. 4); libtommath (bignums under `USE_GMP_REPLACEMENTS`) allocates via **libc malloc ⇒ internal for < 4 KB limbs** (`tommath_private.h:95-98`); **no memory cap of any kind** — the only guards are `MAX_RECURSION_LEVEL=100` (`kglobal.cc:1679`) and `bad_alloc`/`...` catch in `solveWithGiac` (`GiacBridge.cpp:390,435-439`). The static-pool `memmgr.h` in `lib/giac/src` (128 KB, `memmgr.h:69`) is dead: no `memmgr.c` exists in the tree. Giac evaluation runs on the 64 KB loopTask stack (call path `MathEvaluator.cpp` / `SerialBridge.cpp` → `solveWithGiac`) — deep recursion is a stack risk before it is a heap risk.

### 4.5 Renderer / MathAST

- `vpam::MathNode::operator new` ⇒ PSRAM w/ fallback (`MathAST.cpp:50-69`); ownership `unique_ptr` children + raw parent pointers (`MathAST.h:67,644`). Node sizes ≈ 24 B (`NodeEmpty`) to ~100 B (`NodePeriodicDecimal`, 3 × `std::string`); typical ~28–48 B. `std::string` members inside nodes allocate via mech. 4 ⇒ PSRAM.
- `MathCanvas` renderer: zero pixel buffers, pure draw callbacks; recursion bounded by `MAX_RENDER_DEPTH = 12` (`MathRenderer.h:239`), ≲ 1.2 KB stack; one `lv_timer` per active canvas (cursor blink).
- Fonts: flash only (§3). LVGL 9.5 glyph caching uses its defaults (repo `lv_conf.h` has no LVGL-9 cache keys); cache memory comes from the 64 KB pool.

### 4.6 Statistics / Probability / Sequences / Regression

Bounded and tiny: fixed C arrays ≤ 640 B/app (`StatisticsApp.h:56,93-94`: 2×20 doubles; `SequencesApp.h:83-88`; `RegressionApp.h:94-95`), engines pure `<cmath>`/static (`ProbEngine.h:30-45`, `RegressionEngine.h`), `StatsEngine` `std::vector<DataPoint>` ≤ 320 B (`StatsEngine.h:53`). LVGL `lv_chart` series 2 × 80 × 4 B (Probability, `ProbabilityApp.h:52,75-76`) and `lv_table` cells (Statistics) come from the 64 KB pool.

### 4.7 Firmware-only apps (non-sim)

| App | Dynamic peak | Key sites | end() frees? |
|---|---|---|---|
| Equations | step-driven (StepVec + `_stepRenderers` + arenas 128 KB + `_casPool` 256 KB lazy) | `EquationsApp.h:154,203,210,218`; heap probes (read-only diagnostics) `EquationsApp.cpp:487-518` | yes: arena resets + `steps.clear()` + renderers cleared (`EquationsApp.cpp:381-464`) |
| Calculus | step-driven + `_arena` 64 KB | `CalculusApp.h:142,151` | yes (`CalculusApp.cpp:167-168,784-786`) |
| Matrices | **0 B heap**; `Matrix` = 5×5 doubles = 208 B by-value (stack) | `MatrixEngine.h:19,39` | n/a |
| Python | **1,048,576 B PSRAM** (fallback 262,144 B) | `PythonEngine.h:60-61`, alloc `PythonEngine.cpp:92-98`, freed in `deinit()` `:110-112`; init/deinit in `PythonApp.cpp:119,129` | yes |
| PeriodicTable | 0 B heap (flash tables ~7.5 KB) | `ChemDatabase.h:51,186` | n/a |
| NeoLanguage | plot 124,800 B (320×195×2, `NeoLanguageApp.cpp:726-730`, freed `:794-796`) + NeoArena ≥ 32 KB lazy (`NeoAST.h:64-69`) + `_symArena` 32 KB eager, ceiling 1 MB; soft-reset at > 70 % (`NeoLanguageApp.h:86`, `NeoLanguageApp.cpp:620-631`) | `NeoLanguageApp.h:130-131` | yes (`NeoLanguageApp.cpp:133,646,660`) |

### 4.8 Simulations

All render targets are `lv_image` over app-owned PSRAM buffers — **no `lv_canvas` exists anywhere in the tree** (`LV_USE_CANVAS 0`, `lv_conf.h:190`; apps self-document "no lv_canvas", e.g. `Fluid2DApp.h:33`, `BridgeDesignerApp.h:29`).

| App | Peak dynamic (PSRAM) | Breakdown | Freed in `end()`? | Caveat |
|---|---|---|---|---|
| Fluid2D | **158,740 B** | 11 × float[3300] = 145,200 (`Fluid2DApp.cpp:128-139`; `SIZE=(64+2)(48+2)=3300`, `Fluid2DApp.h:73-75`) + obstacle 3,300 + 256 × 40 B particles = 10,240 (`:140`, `Fluid2DApp.h:131-137`) | yes (`:200-226`) | frees gated on `if(_screen)` but allocs precede `_screen` — OOM mid-`begin()` leaks the partial set (`:158-169,187`) |
| ParticleLab | **230,400 B** (+76,800 transient save/load) | render 320×240×2 = 153,600 (`ParticleLabApp.cpp:106-108`) + grid 19,200×4 = 76,800 (`ParticleEngine.cpp:118`; `PG_SIZE`, `ParticleEngine.h:31-33`); save/load temp `:411-443` | yes (`:160-163`) | clean OOM path |
| NeuralLab | **171,904 B** (+4,096 transient) | render 153,600 + decision-boundary 80×60×2 = 9,600 (`NeuralLabApp.cpp:131-132`) + MLP layers ≤ 8,704 (`NeuralEngine.cpp:113-116`; 16×16 max, `NeuralEngine.h:41-42`) | yes (`:179-182`) | `_dbBuffer` orphaned if `_renderBuf` alloc fails (`:133-136`, `end()` gated on `_screen` `:162`) |
| OpticsLab | **138,240 B** | render 320×216×2 (`OpticsLabApp.cpp:92`; `CANVAS_H=216`, `OpticsLabApp.h:62`) | yes (`:149`) | clean |
| Fractal | **171,008 B** in-module (+138,240 transient `scaleBuffer`) | canvas 320×216×2 = 138,240 (`FractalApp.cpp:468`, `PSRAMBuffer`) + `ReferenceOrbit` 2048×16 = 32,768 via `new(nothrow)` (`FractalApp.cpp:96`, `FractalEngine.h:60`); scratch `:818-819` | yes (`:111-127`; buffer also freed on module-leave `:585`) | 32 KB in-object preview atlas `_atlasPreviewPixels[4][64×64]` (`FractalApp.h:94`) rides in the app object (PSRAM); 32 KB FreeRTOS task stack while rendering (`:490-498`) |
| BridgeDesigner | **4,352 B** | 64 × 20 B nodes + 128 × 24 B beams (`BridgeDesignerApp.cpp:98-102`, `BridgeDesignerApp.h:95-96`) | yes (`:150-154`) | same `if(_screen)` OOM caveat as Fluid2D (`:105-134`) |
| CircuitCore | **~22,900 B** heap | MNA 63² floats = 15,876 + 2×252 (`MnaMatrix.cpp:58-64`, dims `MnaMatrix.h:38-41`) + 96-ptr array 384 (`CircuitCoreApp.cpp:126-129`) + ≤ 96 components ≈ 6 KB (`ComponentFactory.cpp:39-76`) | yes, unconditional (`:205-225`) | 33,824 B undo ring lives in-object (`CircuitCoreApp.h:202-212`: 8 × (4+96×44) B) — PSRAM via app-object placement |

## 5. Fragmentation & lifetime risk analysis

1. **Deferred teardown overlap.** `returnToMenu()` → `end()` runs ≥ 250 ms later (`SystemApp.cpp:229-240`). Launching app B within that window is dispatched through `launchApp` → the previous app's buffers still live; `flushPendingTeardownNow` (`SystemApp.cpp:210-222`) exists to force teardown, but for the deferred path the worst case is Python (1 MB) + next sim (~230 KB) coexisting — safe against 8 MB, but any policy math must budget for **two** app peaks.
2. **`end()` gated on `_screen` while allocations precede `_screen`** (Fluid2D, BridgeDesigner, NeuralLab; §4.8): an OOM mid-`begin()` strands the earlier allocations permanently (the destructors call `end()`, which early-returns on `!_screen`). Low probability today (PSRAM headroom), but it converts a transient OOM into a persistent leak.
3. **LVGL pool = the true scarcity.** 64 KB, no expansion, shared by the persistent menu (~65 objects) + splash leak + StatusBar + the active app's whole widget tree (Grapher ~70 objects + table cell strings). No instrumentation exists (`lv_mem_monitor` not surfaced anywhere; `LV_USE_MEM_MONITOR 0`, `lv_conf.h:272`). Exhaustion is a silent `while(1)`.
4. **Internal-heap churn from Arduino `String`.** `Token::text` (`Tokenizer.h:59`) — re-tokenized per Grapher edit; `SystemApp.cpp` legacy draw paths build `String`s per redraw (`SystemApp.cpp:347,362,434`). Small blocks (< 4 KB ⇒ internal), alloc/free churn fragments the same heap that must satisfy any future internal/DMA allocation (and there is **no reserve**, §2 mech. 5).
5. **Arena reset ≠ shrink.** `SymExprArena::reset()` keeps block 0 (by design) but frees extra blocks (`SymExprArena.h:136-140`) — good. `CasMemoryPool::reset()` never returns overflow allocations (§4.4) — leak. `NeoArena::reset()` frees everything (`NeoAST.h:174-185`) — good, and it is the only arena that correctly handles oversize requests (`NeoAST.h:146-149`).
6. **Giac is uncapped and permanent.** The global context accretes symbol-table/cache state in PSRAM for the life of the device (`GiacBridge.cpp:44,192-211`); a hostile/huge input can consume PSRAM until `bad_alloc` (caught, `GiacBridge.cpp:435-439`) — but the allocation pressure transiently starves any concurrent `SPIRAM` request, and fallback paths (mech. 2/3/4 all fall back to `MALLOC_CAP_8BIT`) then bite into **internal** RAM.
7. **History entries are unbounded per-entry** (§4.2): 50 deep AST clones of arbitrarily complex expressions.
8. **Two variable stores** persist independently (`/vars.dat` 379 B vs NVS blob 246 B) — trivial memory, but double writes on flash.
9. **`_stepRenderers` growth**: one `StepRenderData` (AST + MathCanvas widget) per solver step with no cap (`EquationsApp.h:154` et al.); each MathCanvas is an LVGL object → **step count also drains the 64 KB LVGL pool**, not just PSRAM.
10. **Fractal render task** (32 KB stack, core 1) mutates the PSRAM canvas while `lv_timer_handler` reads it on core 0 — memory-coherency risk class, guarded by flags/timeouts (`FractalApp.cpp:513-531`).

## 6. Boot-time memory budget (static, derived)

Internal `.bss`/`.data` (largest known contributors): LVGL pool 64 KB + TFT_eSPI/framework/statics — exact totals in §9. Internal heap at `setup()` exit: 32 KB draw + 32 KB staging gone from DMA class. PSRAM at menu idle ≈ 288 KB arenas + 20 app objects (order 10–50 KB with in-object arrays: CircuitCore 34 KB undo, Fractal 32 KB atlas, NeuralLab 8 KB samples) + LittleFS/NVS metadata ⇒ **order 350–400 KB of 8 MB committed at boot**; ≥ 7.5 MB free for sessions. Boot log prints free PSRAM (`main.cpp:116-121`) and heartbeat prints internal heap only (`main.cpp:233-236`) — PSRAM is invisible at runtime today.

## 7. What cannot be measured statically — and how to measure it

| Quantity | Why static analysis can't give it | Measurement (on-device) |
|---|---|---|
| Internal heap floor after boot | framework/task/Bluetooth-off overheads vary by SDK | add to heartbeat: `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`, `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` (pattern already exists in `EquationsApp.cpp:487-518`) |
| LVGL pool high-water | depends on screen contents | `lv_mem_monitor(lv_mem_monitor_t*)` per app entry/exit; or `LV_USE_MEM_MONITOR 1` overlay (dev builds) — note LVGL-9 name in `lv_conf.h` (`LV_USE_SYSMON` block, `lv_conf.h:266-273`) |
| Giac per-query footprint | data-dependent | wrap `solveWithGiac` with `ESP.getFreePsram()`/`heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` deltas + high-water via `heap_caps_get_minimum_free_size` |
| Fragmentation | allocator state | `heap_caps_get_largest_free_block(caps)` vs `heap_caps_get_free_size(caps)` ratio, logged per app switch |
| Stack high-water (loopTask, fractalTask) | runtime | `uxTaskGetStackHighWaterMark(nullptr)` in heartbeat; for Giac: sample before/after `solveWithGiac` |
| True `.bss`/static internal usage | needs link map | `pio run -e esp32s3_n16r8 -v`, read "RAM/Flash" lines; symbol-level: `xtensa-esp32s3-elf-nm --size-sort -S .pio/build/esp32s3_n16r8/firmware.elf \| tail -40` |

Emulator/CI limitations: the emulator runs libc malloc + `LV_STDLIB_CLIB` (`platformio.ini:176`) — **no** heap_caps, no PSRAM, no 64 KB pool, no GiacAlloc override. It can validate lifecycle logic (leaks via ASAN/valgrind) but neither domain routing nor exhaustion behavior. CI compiles firmware but executes nothing (`compile-and-release.yml`).

## 8. Measured build figures (this audit)

Emulator: `pio run -e emulator_pc` SUCCESS (20.9 s, Linux, SDL2 2.30.0, LVGL 9.5.0).

Firmware (`pio run -e esp32s3_n16r8`, espressif32 @ IDF 4.4.7 / GCC 8.4.0, SUCCESS in 321 s):

```
RAM:   [====      ]  35.4% (used 115896 bytes from 327680 bytes)
Flash: [========  ]  79.4% (used 5203645 bytes from 6553600 bytes)
```

- **Static internal DRAM: 115,896 B of 327,680 B** ⇒ ≈ 211.8 KB left for the internal heap (before FreeRTOS/runtime task allocations; measure the true boot floor per §7). The LVGL pool alone is 64 KB = 56 % of the static figure — ELF-verified: `work_mem_int$8270` = `0x10000` B in `.bss` @ `0x3fc98108` (`xtensa-esp32s3-elf-nm -S firmware.elf`).
- **Flash: 79.4 % used — 1,349,955 B headroom** in the 6.25 MB app slot (`default_16MB.csv`). This answers ground-truth open question P-“does it fit”: yes, but flash is the tighter axis (PROJECT_BIBLE’s “23.2 % flash” long predates Giac). ~80 KB of that is the legacy `Icons.h` bitmaps kept alive by the legacy menu table (`SystemApp.cpp:163-177`; ELF: 10 × 8,192 B `icon_*` symbols in DROM).
- **Global `operator new` override is linked**: the map attributes `_Znwj`/`_ZdlPv` to `.pio/build/esp32s3_n16r8/src/math/giac/GiacAlloc.cpp.o` (firmware.map) — direct proof of §1.2.
- Toolchain: GCC 8.4.0 has no `<memory_resource>` (verified: `#include <memory_resource>` fails to preprocess) ⇒ `CAS_HAS_PMR=0` on firmware, the `cas::pmr` shim path is the live one (§4.4).
- `esp32s3_n16r8_validate` also built SUCCESS during this audit (239 s): RAM identical 35.4 %; Flash 79.8 % (5,228,473 B, +24,828 B over production for the visual-verify fixture). This clears ground-truth open question P-09's "do the validate envs still compile" for the primary validate env.

## 9. Corrections to `NUMOS_ARCHITECTURE_GROUND_TRUTH.md` (memory claims)

| Claim in ground-truth doc | Correction | Evidence |
|---|---|---|
| "LVGL allocations >512 B" go to PSRAM; "heap_caps/PSRAM cutoff at 512 B, `lv_conf.h:74`" (§E, §J) | Dead config. Firmware LVGL heap = 64 KB builtin internal pool; the `LV_MEM_CUSTOM` block is LVGL-8 syntax ignored by LVGL 9 on **both** targets | §1.1 above |
| Fluid2D / NeuralLab / Fractal use "lv_canvas" (§F) | No `lv_canvas` anywhere; `LV_USE_CANVAS 0`; all sims use `lv_image` over own PSRAM buffers or `LV_EVENT_DRAW_MAIN` | `lv_conf.h:190`; `Fluid2DApp.h:33` etc. |
| ParticleLab memory "~MBs" (MEM-3) | Statically bounded: 230,400 B (+76,800 transient) | §4.8 |
| Fluid2D "~200 KB" | 158,740 B | §4.8 |
| Fractal "~280 KB buffers" | 171,008 B peak + 138,240 B transient + 32 KB in-object atlas | §4.8 |
| `SerialBridge` "16-event queue" (§B) | 32-slot buffer | `SerialBridge.h:63-64` |
| "MathAST … PSRAM `operator new`" presented as the PSRAM story | True but subsumed: the **global** override in `GiacAlloc.cpp` routes *all* C++ `new` to PSRAM on firmware | §1.2 |
| GiacAlloc = "optional … for Giac allocations" (file's own comment, `GiacAlloc.cpp:17`) | Misleading: override is global, affects every subsystem | §1.2 |

## 10. Top-10 findings (ranked)

1. Firmware LVGL heap is a fixed 64 KB internal pool; `lv_conf.h`'s PSRAM policy is dead config (§1.1). Exhaustion = silent hang.
2. `GiacAlloc.cpp` globally overrides `operator new` → all C++ heap is PSRAM-first on firmware (§1.2); emulator diverges (never compiled).
3. 32 KB internal-DMA staging buffer is allocated but unreachable (`_dmaEnabled` forced false) (§1.4).
4. `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=0`: no protected internal floor; `malloc` < 4 KB (Strings, bignum limbs) can erode DMA-capable headroom (§2 mech. 5).
5. Arena-resident `CASInt` BigInt promotions leak `mbedtls_mpi` structs+limbs on `SymExprArena::reset()` — destructors never run; the designed registry has zero callers (§4.4).
6. `CasMemoryPool` overflow allocations (past 256 KB) are never reclaimed by `reset()` (§4.4).
7. `SymExprArena::allocRaw` lacks an oversize guard — a single > 64 KB request corrupts memory past the block (§4.4).
8. OOM during `begin()` in Fluid2D/Bridge/NeuralLab strands partial allocations because `end()` is gated on `_screen` (§5.2).
9. ~288 KB of PSRAM arenas are committed at boot inside constructors of apps that may never open (§4.1); Python adds 1 MB and NeoLang up to ~1.16 MB per session; deferred teardown makes budgets two-app-wide (§5.1).
10. Zero runtime memory instrumentation: heartbeat logs internal heap only (`main.cpp:233-236`); no PSRAM, no LVGL-pool, no largest-free-block, no stack high-water anywhere.
