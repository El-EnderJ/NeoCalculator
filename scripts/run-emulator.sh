#!/bin/sh
# ============================================================================
# run-emulator.sh — Build AND run the NumOS SDL2 desktop emulator (macOS/Linux).
#
# The simplest, path-agnostic way to open the emulator window. It delegates to
#
#     pio run -e emulator_pc -t exec
#
# so PlatformIO itself builds (if needed) and then launches the native binary
# from whatever build directory it uses — you never need to know where the
# binary lives. (platformio.ini puts it under ~/.piobuild/numOS-build on
# macOS/Linux, next to PlatformIO's core dir, unless PLATFORMIO_BUILD_DIR is set.)
#
# Usage:
#     ./scripts/run-emulator.sh        # build if needed, then open the window
#
# Prereqs (macOS):  brew install platformio sdl2 pkg-config
# Prereqs (Linux):  sudo apt-get install -y libsdl2-dev   (plus the pio CLI)
# Verify the CLI:   pio --version
#
# NOTE: the `-t exec` target cannot forward arguments to the emulator program.
# For advanced flags (--frames, --headless, --script, --screenshot, ...) build
# first, then use scripts/run-emulator-linux.sh, which runs the built binary
# directly and forwards everything after it. On Windows use
# scripts/run-emulator-windows.ps1 (it also resolves SDL2.dll at runtime).
# ============================================================================
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd -P)

if ! command -v pio >/dev/null 2>&1; then
    echo "ERROR: PlatformIO CLI (pio) not found on PATH." >&2
    echo "  The VS Code PlatformIO extension is NOT the same as the 'pio' CLI;" >&2
    echo "  a normal Terminal needs the standalone tool:" >&2
    echo "    macOS: brew install platformio" >&2
    echo "    any:   pip install -U platformio" >&2
    echo "  Then re-check with:  pio --version" >&2
    echo "  See docs/emulator-sdl2-quickstart.md (macOS)." >&2
    exit 1
fi

echo "[run-emulator] pio run -e emulator_pc -t exec  (build + launch; close the window to exit)"
cd "$REPO_ROOT"
exec pio run -e emulator_pc -t exec "$@"
