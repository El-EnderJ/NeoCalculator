# CAS Constitution: Giac/KhiCAS Migration for NumOS (ESP32-S3)

Version: 1.1
Status: Adopted and executed (migration complete)
Scope: `src/math`, `src/apps/EquationsApp.*`, `src/ui/MathRenderer.*`, PlatformIO build and memory policy
Supersedes: Any previous roadmap claiming the custom CAS stack is the long-term final architecture

Execution update (April 2026):

1. Big Switch to Giac completed through `src/math/giac/GiacBridge.cpp`.
2. Embedded baseline locked: `-DDOUBLEVAL` and `-DARDUINO_LOOP_STACK_SIZE=65536`.
3. Real-style defaults locked: `complex_mode(false)` with preserved `i^2 = -1` behavior.
4. UART hardware validation completed for `sum`, `int`, `solve`, and `simplify`.

---

## 1. Vision

NumOS will stop evolving a custom symbolic core as the primary CAS and will adopt Giac/KhiCAS as the canonical mathematics engine.

This migration is not cosmetic. It is a strategic move to:

1. Reach modern symbolic capability without reinventing decades of algebra research.
2. Preserve NumOS strengths (fast UI, LVGL experience, educational steps, hardware fit).
3. Build on open-source mathematics instead of locking the project into a maintenance trap.

Target outcome:

1. Giac/KhiCAS executes symbolic math.
2. NumOS remains responsible for interaction, pedagogy, rendering, and hardware reliability.
3. Existing VPAM visual quality is preserved and upgraded.

---

## 2. Constitutional Rules (Non-Negotiable)

1. Giac/KhiCAS is the source of mathematical truth for symbolic operations once migration is complete.
2. Display DMA buffers remain in internal RAM. CAS heap and large symbolic objects move to PSRAM.
3. UI responsiveness is mandatory: no long CAS operation may block the render loop.
4. Step rendering must stay incremental and memory-bounded, aligned with current staged step pipeline behavior.
5. Migration must remain reversible until Phase 6 sign-off via a compile-time switch.
6. Open-source license obligations are part of the architecture, not an afterthought.

---

## 3. Technical Architecture

### 3.1 Current Assets to Preserve

Keep and evolve these layers:

1. `src/math/MathAST.*` for semantic visual layout objects.
2. `src/ui/MathRenderer.*` (`MathCanvas`) for high-quality expression rendering in LVGL.
3. `src/apps/EquationsApp.*` staged steps pipeline (`PARSE -> SOLVE -> RENDER_CHUNK -> FINALIZE`) and memory guardrails.
4. Existing native desktop path (`[env:emulator_pc]` in `platformio.ini`) as high-speed debug loop.

### 3.2 Target Layering

```
Keyboard/Input -> App Layer (Equations/Calculation/Tutor)
             -> GiacBridge (async command/eval/step API)
             -> Giac/KhiCAS Kernel (context + gen + eval + gen2tex)
             -> TeX-to-MathAST Adapter (new)
             -> MathRenderer (existing, upgraded)
             -> LVGL + DisplayDriver
```

### 3.3 Planned New Components

1. `src/math/giac/GiacBridge.h/.cpp`
2. `src/math/giac/GiacWorker.h/.cpp` (optional worker task)
3. `src/math/tex/TeXTokenizer.h/.cpp`
4. `src/math/tex/TeXParser.h/.cpp`
5. `src/math/tex/TeXToMathAST.h/.cpp`
6. `src/math/giac/GiacStepAdapter.h/.cpp`

---

## 4. Memory Constitution: PSRAM-First CAS, Internal-Only DMA

ESP-IDF guidance confirms:

1. External RAM can be routed through capability allocator (`MALLOC_CAP_SPIRAM`).
2. `malloc()` can be configured to use external RAM with internal reserve thresholds.
3. DMA-capable buffers have strict constraints; in practice display-critical buffers stay internal unless carefully validated.

NumOS policy:

1. Keep display draw buffers in `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA` (already implemented in `src/main.cpp`).
2. Route Giac-heavy allocations to PSRAM.
3. Reserve internal heap headroom for LVGL metadata, stacks, ISR-safe allocations, and fragmentation safety.

### 4.1 Build and Config Example

```ini
; platformio.ini (example additions)
[env:esp32s3_n16r8]
build_flags =
    ; existing flags omitted
    -DNUMOS_USE_GIAC=1
    -DGIAC_KHICAS_PROFILE=1
    -DNO_GUI=1
    -DNO_FILESYSTEM=1
    -fexceptions
    -frtti

; If using sdkconfig defaults in mixed Arduino/IDF mode, set:
; CONFIG_SPIRAM_USE_MALLOC=y
; CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=1024
; CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=<platform-tested value>
```

### 4.2 Global Allocation Redirection Example

```cpp
// src/math/giac/GiacAlloc.cpp
#include <new>
#include "esp_heap_caps.h"

void* operator new(std::size_t size) {
    void* p = heap_caps_malloc_prefer(
        size,
        2,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    heap_caps_free(p);
}

void* operator new[](std::size_t size) {
    void* p = heap_caps_malloc_prefer(
        size,
        2,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    heap_caps_free(p);
}
```

### 4.3 Runtime Guardrails

1. Log `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)` before and during step rendering.
2. Keep hard limits equivalent to current step policy in `EquationsApp` (`STEPS_LABEL_BUDGET`, `STEPS_CANVAS_BUDGET`, child cap).
3. Fail closed with a warning step when memory thresholds are crossed.

---

## 5. Giac/KhiCAS Version Policy (1.9+ Baseline)

### 5.1 Baseline Decision

NumOS targets Giac/KhiCAS version line `>= 1.9.0` for first production integration.

`2.0.x` is allowed when the build is stable on ESP32-S3 and phase gates pass.

### 5.2 Why Not 1.5-era Branches

Public timeline entries indicate major evolution after 1.5, including:

1. `1.6.17`: calculator compatibility and interpreter integration improvements.
2. `1.7.x`: stronger Numworks connectivity and polynomial system solver updates.
3. `1.9.0-x`: optimization and solver improvements, documentation modernization, bootloader compatibility updates.
4. `2.0.0-x`: full-featured browser/offline workflows and ongoing packaging modernization.

### 5.3 Version Comparison (Strategic)

| Dimension | Giac 1.5-era baseline | Giac 1.9.x | Giac 2.0.x |
|---|---|---|---|
| Modern packaging cadence | Low | High | High |
| Calculator ecosystem maintenance | Legacy | Active | Active |
| Optimization updates (post-2015) | Limited | Significant | Significant |
| Integration confidence for new ports | Medium | High | High, but verify per release |
| Recommended for NumOS | No | Yes (initial) | Yes (after validation) |

---

## 6. C++ Wrapper Bridge Contract

Giac must never be called ad hoc from UI widgets. All calls go through a single bridge contract.

### 6.1 API Shape

```cpp
// src/math/giac/GiacBridge.h
struct GiacStep {
    std::string description;
    std::string latex;
};

struct GiacRequest {
    std::string command;
    bool withSteps = false;
};

struct GiacResponse {
    bool ok = false;
    std::string latex;
    std::string plain;
    std::vector<GiacStep> steps;
    std::string error;
};

class GiacBridge {
public:
    bool begin();
    void resetContext();
    GiacResponse evaluate(const GiacRequest& req);
};
```

### 6.2 Execution Model

1. App state enqueues request.
2. Worker executes Giac eval in isolated context.
3. Response contains final result + optional steps payload.
4. UI thread only receives rendered-ready payload; never manipulates Giac internals.

This aligns with existing non-blocking behavior in the `EquationsApp` staged pipeline.

---

## 7. LVGL Rendering Evolution for Giac LaTeX Output

Current renderer is AST-based, not TeX-based. This is a strength.

Migration rule: do not replace `MathRenderer`; feed it better trees.

### 7.1 New Render Flow

1. `gen` result from Giac.
2. `gen2tex(...)` to canonical TeX-like string.
3. TeX adapter parses output into `MathAST`.
4. Existing `MathCanvas` draws with current quality and cursor conventions.

### 7.2 Parser Scope (Phase-Gated)

Phase 1 TeX subset:

1. Fractions (`\\frac`)
2. Powers/subscripts (`^`, `_`)
3. Parentheses and group braces
4. Roots (`\\sqrt`)
5. Core Greek/constants/operators used in school workflows

Unsupported tokens must degrade gracefully:

1. Fallback to plain label representation.
2. Log unsupported token for parser coverage tracking.

---

## 8. step_by_step Tutor Mapping Constitution

NumOS will map Giac step workflows into the existing educational UI model.

### 8.1 Input Strategy

The adapter must support both:

1. Explicit step command path (`step_by_step(...)`) where available.
2. Alternative step-rich outputs driven by solver commands and context flags.

### 8.2 Canonical Internal Step Record

```cpp
struct TutorStepRecord {
    std::string description;
    std::string latex;
    bool isResult = false;
};
```

### 8.3 Mapping Rules

| Giac payload shape | Adapter behavior | UI output |
|---|---|---|
| Scalar expression | `gen2tex` and mark as single result | One MathCanvas row |
| Vector of expressions | Iterate and convert each expression | Multi-step canvases |
| Mixed vector (strings + expressions) | Strings become descriptions, expressions become formula rows | Label + canvas pairs |
| Nested vectors | Flatten with stable order | Deterministic sequence |

### 8.4 Integration with Existing Pipeline

Map directly to `EquationsApp` staged rendering behavior:

1. `PARSE`: parse request and detect step mode.
2. `SOLVE`: run Giac and collect raw step payload.
3. `RENDER_CHUNK`: convert each step to AST and emit chunks using existing memory budgets.
4. `FINALIZE`: append hint/footer and hide progress label.

---

## 9. Six-Phase Integration Roadmap (Realistic)

## Phase 1 - Build Skeleton and Legal Baseline

Goals:

1. Bring Giac/KhiCAS source snapshot into `lib/third_party/giac`.
2. Establish compile profile for `emulator_pc` and ESP32 target.
3. Add license files and attribution documentation.

Exit criteria:

1. Bridge compiles and returns result for `2+2` in both native and ESP32 builds.
2. CI job reports third-party version hash and license notice.

## Phase 2 - Memory Redirection and Stability

Goals:

1. Route CAS-heavy allocations to PSRAM.
2. Preserve internal RAM for DMA and latency-sensitive allocations.
3. Add heap telemetry in debug mode.

Exit criteria:

1. Large symbolic operations do not exhaust internal heap.
2. No display corruption/regression under concurrent UI + CAS workload.

## Phase 3 - GiacBridge and Command Contract

Goals:

1. Implement `GiacBridge` API with robust error propagation.
2. Add context reset and deterministic evaluation paths.
3. Add smoke tests for simplify/solve/diff/integrate commands.

Exit criteria:

1. `EquationsApp` can request final symbolic results through bridge only.
2. No direct CAS UI calls bypassing bridge remain in active path.

## Phase 4 - TeX-to-MathAST Rendering Bridge

Goals:

1. Implement tokenizer/parser for Giac TeX subset.
2. Convert TeX output to existing `MathAST` nodes.
3. Integrate with `MathCanvas` in result views.

Exit criteria:

1. Fraction/power/root expressions from Giac render correctly in VPAM.
2. Unsupported TeX degrades gracefully with logs, not crashes.

## Phase 5 - Tutor Step Integration

Goals:

1. Implement step adapter for vector/mixed Giac outputs.
2. Map steps into staged incremental renderer with memory caps.
3. Preserve highlight semantics for affected expression regions where possible.

Exit criteria:

1. Step view supports long derivations without UI lockups.
2. Progress and fail-closed behavior mirrors current reliability guarantees.

## Phase 6 - Decommission and Cutover

Goals:

1. Turn Giac path into default symbolic backend.
2. Freeze then remove obsolete custom symbolic engines from primary build.
3. Keep one emergency fallback build profile for one release cycle.

Exit criteria:

1. All symbolic user flows pass through Giac in production profile.
2. Legacy custom CAS code is either archived or guarded behind explicit legacy flag.

---

## 10. Decommission Plan (Legacy CAS)

This plan avoids a big-bang deletion and prevents accidental regressions.

### 10.1 Freeze Immediately (No New Features)

1. `src/math/cas/RuleEngine.*`
2. `src/math/cas/AlgebraicRules.*`
3. `src/math/cas/SingleSolver.*`
4. `src/math/cas/SystemSolver.*`
5. `src/math/cas/OmniSolver.*`
6. `src/math/cas/SystemTutor.*`

### 10.2 Transitional Adapters (Temporary)

1. `src/math/cas/SymExprToAST.*`
2. `src/math/cas/SymToAST.*`
3. `src/math/cas/CasToVpam.*`

These remain during migration as compatibility layers until TeX bridge reaches feature parity.

### 10.3 Removal Trigger

Remove or archive legacy symbolic modules after:

1. Two consecutive release candidates pass with Giac backend enabled.
2. Step tutorial parity is confirmed on supported equation classes.
3. Memory safety metrics remain within target thresholds on real hardware.

---

## 11. Capability Comparison Table

### 11.1 Custom CAS vs Giac-Based NumOS

| Capability | Current custom stack | Giac/KhiCAS target |
|---|---|---|
| Algebra breadth | Medium (rule-limited) | High (mature symbolic engine) |
| Exact arithmetic depth | Good in scoped paths | High and broad |
| Symbolic integration breadth | Limited heuristics | Much broader built-in coverage |
| Nonlinear solving robustness | Mixed by class | Stronger mature heuristics |
| Step-by-step pedagogy | Good for implemented rules | Good with adapter + richer backend |
| Rendering quality | High (VPAM) | High (same VPAM, richer inputs) |
| Maintenance burden | High internal burden | Lower algorithm burden, higher integration discipline |

### 11.2 Historical Baseline Comparison (Version Track)

| Dimension | Older 1.5-era deployments | Giac 1.9+ / 2.0 track |
|---|---|---|
| Active maintenance | Lower | Higher |
| Post-2015 algorithm updates | Limited | Significant |
| Packaging and compatibility updates | Limited | Ongoing |
| Recommended for NumOS | No | Yes |

---

## 12. Open-Source Covenant

NumOS adopts this migration as an open engineering collaboration, not a closed fork strategy.

Commitments:

1. Keep Giac/KhiCAS source provenance explicit in repository metadata.
2. Publish local patches as clean, reviewable commits.
3. Contribute upstream fixes when they are generic and reusable.
4. Keep educational step UX as a first-class NumOS differentiator.

License policy:

1. Giac is distributed under GPL terms in upstream sources.
2. Distribution and derivative work obligations must be satisfied before production release.
3. Third-party notices are required in release artifacts.

---

## 13. Definition of Done for Migration

Migration is complete only when all are true:

1. Giac backend is default in production profile.
2. Result rendering path is `gen -> TeX -> MathAST -> MathCanvas` for supported expressions.
3. Step view supports long sessions with no hard UI lockups.
4. Internal heap floor and largest-block thresholds remain healthy during stress tests.
5. Legacy custom symbolic engine is removed from default build path.

---

## 14. Immediate Next Actions

1. Create `NUMOS_USE_GIAC` feature flag and scaffold `GiacBridge`.
2. Add TeX adapter skeleton with unit tests for fractions, powers, and roots.
3. Wire `EquationsApp` single-equation solve path to bridge behind runtime toggle.
4. Capture baseline memory/performance telemetry before and after bridge activation.

---

## 15. Source Notes (Research Basis)

This constitution is grounded on:

1. Current NumOS architecture in `src/ui/MathRenderer.*`, `src/math/MathAST.*`, `src/apps/EquationsApp.*`, and `platformio.ini`.
2. ESP-IDF memory and external RAM guidance for capability allocator and PSRAM restrictions.
3. Giac/Xcas public installation and release timeline information (including 1.9.x and 2.0.x evolution and KhiCAS calculator packaging context).
