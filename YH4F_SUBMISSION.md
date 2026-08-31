# NumOS — YH4F 2026 Submission

## Project summary

**NumOS** is a Free Software firmware/OS for a scientific graphing calculator. It
runs on an ESP32-S3 microcontroller driving a 320×240 colour TFT display, and it
provides a launcher and a growing set of calculator and math apps. It is built in
C++17 on top of the LVGL graphics library and the Arduino/ESP-IDF toolchain, using
PlatformIO.

The original work in this project is the **firmware and OS layer**: the boot and
dispatcher architecture, the LVGL launcher and UI shell, the app lifecycle/system,
the input handling, and the natural-display math rendering. The numeric expression
handling (tokenizer, parser, evaluator) and the math-layout (AST) code are also my
own C++17 work. The **symbolic-math** direction is a mix of my own experiments and
ongoing integration work around **Giac** — an existing GPL computer-algebra
system by Bernard Parisse. I am *not* claiming to have written a CAS from scratch;
the symbolic side is experimental and in progress.

**NeoCalculator** is the open-source physical calculator that NumOS is meant to run
on (an ESP32-S3 board, a colour TFT, and a physical key matrix). The hardware is
still a prototype — a custom PCB is in progress — so this submission focuses on
**NumOS, the software**.

Why this matters: the calculators students are required to buy are almost always
closed, locked devices — you cannot read how they compute, fix a bug, or extend
them. NumOS is the opposite. Its firmware, UI, input layer and math rendering are
open for study and modification under the GPL, so the calculator becomes something
you can learn *from* as well as *with*. That is the Free Software argument — **use,
study, share, improve** — applied to a device millions of students carry every day.

## This submission

This repository is my YH4F 2026 submission for NumOS. The required personal details
are included in my submission email, so I have kept them out of this public file.

- The project started around **mid-February 2026**, during the YH4F 2026 programming period, and has been developed continuously since.
- Repository: https://github.com/El-EnderJ/NeoCalculator
- A specific submission tag is referenced in the submission email.

## What works today

These are the parts that are currently most stable and demonstrable. NumOS is a
work in progress, so "works" means usable in the current demo path, not finished or
production-ready.

- **ESP32-S3 firmware structure** — boot → dispatcher → app lifecycle (`main.cpp` → `SystemApp`), built with PlatformIO for the `esp32s3_n16r8` target.
- **LVGL UI shell** — animated splash screen, a NumWorks-style launcher (`MainMenu`), and a shared status bar.
- **Calculation app** — natural-display entry: stacked fractions, radicals, superscripts, and a 2D editing cursor.
- **Grapher app** — `y = f(x)` plotting with zoom, pan, a value table, and multi-expression entry.
- **Natural-display math rendering** — a custom 2D layout/renderer (`MathCanvas`) that draws expressions the way they appear on paper.
- **Numeric expression pipeline** — tokenizer → parser → evaluator for calculator input, plus an AST used for natural display. *(This is the numeric path; symbolic/CAS work is separate and experimental — see below.)*
- **SerialBridge dev input** — drive the calculator from a PC over the serial monitor (arrow keys, ENTER, DEL, …), so development and testing do not require finished hardware.
- **SDL2 desktop emulator** (`emulator_pc`) — runs the real UI and a **subset** of the app/math code natively on a PC, with a small scripting harness for automated UI smoke tests.
- **Documentation & hardware target definition** — architecture, math-engine and hardware/pinout docs, plus a record of the boot-crash classes found and fixed during bring-up.

### Experimental / in progress

The following exist in the repository but are early or experimental, and should not
be assumed to be stable or complete:

- **Equations app** and **Calculus app** — solver and symbolic differentiation/integration experiments; usable in places but still rough (the Equations "steps" view is alpha).
- **Symbolic / CAS work, including Giac integration** — a symbolic pipeline and a Giac-based backend are being integrated. Treat as in-progress, not a validated CAS. A CAS unit-test suite exists but is compile-time gated and off by default.
- **Other apps** — physics/simulation labs, scripting, statistics/regression/sequences/probability, matrices, and chemistry modules range from prototype to experimental.
- **Physical keyboard / custom PCB** — prototype stage; only a few of the planned key-matrix columns are wired so far.

## How to see or try the project

The jury does **not** need the physical hardware. Recommended order, easiest first:

1. **Watch the short demo video:** https://youtu.be/IOTeKleOHQY
   It shows the current core NumOS path: launcher, Calculation, natural-display math input, and Grapher.
2. **Read the [README](README.md)** for an overview and the feature map.
3. **Look at the screenshots** in the [README gallery](README.md#photo-gallery) (Calculation, Grapher, and other screens).
4. **Try the desktop emulator** (no ESP32 needed): build the SDL2 `emulator_pc` target to run the real UI on a PC. Setup and troubleshooting are in [`docs/emulator-sdl2-quickstart.md`](docs/emulator-sdl2-quickstart.md); it builds a subset of modules (see `build_src_filter` in [`platformio.ini`](platformio.ini)).
5. **Build the firmware** if you have an ESP32-S3 board (see below and the [README Quick Start](README.md#quick-start)).
6. **Read the source** — the most representative code is [`src/apps/`](src/apps/) (Calculation, Grapher), [`src/math/`](src/math/), and [`src/ui/`](src/ui/) (rendering); the entry point is [`src/main.cpp`](src/main.cpp) → [`src/SystemApp.cpp`](src/SystemApp.cpp).

The video is a short demonstration of the current stable demo path. It does not show
every experimental app in the repository.

## Build / run instructions

Everything goes through **PlatformIO** (no `npm`/`pip`/`make` steps). The two main
targets are firmware for the board and the desktop emulator:

```bash
# Firmware for ESP32-S3 hardware
pio run -e esp32s3_n16r8

# Build + flash + serial monitor (requires the board)
pio run -e esp32s3_n16r8 --target upload
pio device monitor

# Native PC emulator (SDL2) — no ESP32 needed, builds a subset of modules
pio run -e emulator_pc
```

The emulator depends on SDL2; platform-specific setup (Windows/Linux) and a
troubleshooting guide are in
[`docs/emulator-sdl2-quickstart.md`](docs/emulator-sdl2-quickstart.md).

## What I built during the YH4F period

The project started around mid-February 2026 and has been developed throughout the
YH4F 2026 programming period; the commit history shows continuous activity from
February to the end of June 2026. The main work during the period:

- **Firmware architecture** — the boot sequence, the `SystemApp` dispatcher, the lazy app-lifecycle pattern, and the deferred-teardown handling that keeps LVGL stable on a memory-constrained MCU.
- **UI / launcher** — the splash screen, the launcher, the status bar, and the natural-math rendering canvas.
- **App system** — the modular app contract and the apps built on it (most mature: Calculation, Grapher; others experimental).
- **Math work** — the numeric pipeline (tokenizer, parser, evaluator) and AST rendering, plus symbolic experiments and integration work around the Giac backend.
- **Display / input integration** — TFT + LVGL DMA flush, the serial-bridge input path, an LVGL keypad adapter, and the physical key-matrix driver.
- **Desktop emulator + test harness** — the SDL2 native target and scripted UI smoke tests.
- **Documentation and hardware target definition** — architecture, math-engine and hardware docs, the pinout, and the record of bring-up fixes.

## Originality and practical relevance

Most calculator software students meet is either closed commercial firmware
(TI, Casio, HP) or a phone app. NumOS is different in two ways: it is **open
calculator firmware/OS** — the OS itself, including the launcher, input layer and
rendering — and it targets **affordable, hackable hardware** (an ESP32-S3 board),
which makes it realistic for a student or maker to build, flash, and modify.

The practical relevance is education and hackability: a learner can read how
expressions are parsed and rendered, change a key mapping, or add an app following a
documented contract. This is the Free Software promise applied to an everyday school
tool, where today there is almost no open option.

## Technical difficulty

The difficulty is concrete rather than theoretical:

- **Embedded C++17 on a microcontroller** — no OS underneath, manual lifecycle and memory management, and a watchdog that aborts boot if init is too slow.
- **ESP32-S3 constraints** — a fixed RAM/Flash budget and reliance on external PSRAM for heavier structures. Several real boot-crash classes (PSRAM/OPI panic, SPI `StoreProhibited`, display artefacts, an LVGL black screen from DMA buffers in the wrong heap) were diagnosed, fixed, and documented for anyone forking the project.
- **LVGL on a small display** — a smooth UI, a 2D math renderer and a launcher inside a 320×240 frame, with single-buffered DMA flushing to avoid an LVGL 9.x pipelining deadlock.
- **Physical input** — a scanned key matrix with debouncing, a SHIFT/ALPHA modifier state machine, and resolving GPIO conflicts between the keyboard and the display.
- **Math rendering** — natural 2D layout of fractions, radicals and exponents on top of a hand-written numeric pipeline.

The hard part is making all of this coexist reliably on a constrained device, so
much of the work is about memory, timing and bring-up rather than features alone.

## Self-reliance and AI assistance

I used AI coding assistants (Claude / Copilot) during this project — for code
generation, debugging, refactoring, drafting documentation, and exploring
implementation options. I am disclosing this openly; it is allowed under the YH4F
rules.

The direction and the engineering decisions are mine. I defined the project goals
and hardware requirements, chose the platform and libraries, and made the
architecture decisions (the dispatcher/app model, the memory strategy, the math
pipeline). I reviewed the generated code, corrected wrong assumptions, compiled and
tested on real ESP32-S3 hardware, and integrated everything into the working
project. AI was an engineering assistant that helped me move faster — not an
autonomous author. I understand the codebase and I am responsible for it.

## Project status and limitations

NumOS is a **work in progress**. Being clear about that is more honest and more
useful to the jury:

- **Not all apps are finished.** Calculation and Grapher are the most developed; many modules in `src/apps/` are experimental or early prototypes.
- **The hardware is a prototype.** The custom PCB is in progress, and the physical keyboard is partially wired. This submission focuses on the software.
- **The emulator is partial.** It builds a subset of modules and is used mainly for UI smoke tests; some apps run only on real hardware.
- **Symbolic / CAS features are experimental.** The Giac integration is in progress and should not be treated as a finished CAS.

This is the normal state of an ambitious solo embedded project mid-development, and
YH4F explicitly accepts unfinished work.

## Roadmap

- Stabilise the core calculator workflow (input → evaluate → display) end to end.
- Improve the Calculation app (entry edge cases, history, formatting).
- Improve the Grapher (multi-function graphing, tracing, robustness).
- Finish and clean up the natural math rendering.
- Continue the symbolic/Giac integration as an experimental track.
- Bring up and test the first custom PCB prototypes.
- Improve documentation and contributor onboarding (a clear "add an app" guide, emulator-first workflow).

## License

NumOS-authored software carrying the project notice is offered under
**GPL-3.0-or-later** (`LICENSE-SOFTWARE` contains the full GNU GPL v3 text;
`LICENSE.md` records the licence-history qualification). This gives users the
freedom to use, study, share and improve that software. Separately licensed
third-party components retain their own terms.

The hardware design files for NeoCalculator are separately licensed under the
**CERN Open Hardware Licence Version 2 — Strongly Reciprocal (CERN-OHL-S v2)**
(see [`LICENSE-HARDWARE`](LICENSE-HARDWARE) and [`LICENSE.md`](LICENSE.md)). NumOS
builds on the **Giac** symbolic engine by Bernard Parisse, which is also GPL.

## Start here

- **Demo video:** https://youtu.be/IOTeKleOHQY
- **Overview:** [`README.md`](README.md) · this file [`YH4F_SUBMISSION.md`](YH4F_SUBMISSION.md)
- **Build & emulator:** [README Quick Start](README.md#quick-start) · [`docs/emulator-sdl2-quickstart.md`](docs/emulator-sdl2-quickstart.md) · [`platformio.ini`](platformio.ini)
- **Architecture docs:** [`docs/PROJECT_BIBLE.md`](docs/PROJECT_BIBLE.md) · [`docs/MATH_ENGINE.md`](docs/MATH_ENGINE.md) · [`docs/ROADMAP.md`](docs/ROADMAP.md) · [`docs/HARDWARE.md`](docs/HARDWARE.md)
- **Screenshots:** [README gallery](README.md#photo-gallery) · [`info/`](info/)
- **Most representative source:** [`src/apps/`](src/apps/) (Calculation, Grapher) · [`src/math/`](src/math/) · [`src/ui/`](src/ui/)
- **Licenses:** [`LICENSE-SOFTWARE`](LICENSE-SOFTWARE) (GPL-3.0-or-later) · [`LICENSE-HARDWARE`](LICENSE-HARDWARE) (CERN-OHL-S v2) · [`LICENSE.md`](LICENSE.md)
