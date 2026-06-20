#!/bin/sh
# ============================================================================
# run-emulator-linux.sh — Launch the NumOS SDL2 desktop emulator on Linux.
#
# Locates the built native binary, sanity-checks that SDL2 runtime is present,
# and runs it. POSIX sh; no bashisms.
#
# Build first (system SDL2 is discovered automatically by scripts/sdl2_env.py):
#     sudo apt-get install -y libsdl2-dev     # Debian/Ubuntu
#     PLATFORMIO_BUILD_DIR=.pio/build pio run -e emulator_pc
# Then run:
#     ./scripts/run-emulator-linux.sh
#
# Any extra arguments are forwarded to the emulator.
#
# Build-dir resolution order (to find the binary):
#   1. $PLATFORMIO_BUILD_DIR
#   2. `build_dir` from platformio.ini  (ignored if it is a Windows C:\ path)
#   3. .pio/build  (PlatformIO default)
# ============================================================================
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd -P)

# ── 1. Resolve build dir ────────────────────────────────────────────────────
resolve_build_dir() {
    if [ -n "${PLATFORMIO_BUILD_DIR:-}" ]; then
        printf '%s\n' "$PLATFORMIO_BUILD_DIR"
        return
    fi
    ini="$REPO_ROOT/platformio.ini"
    if [ -f "$ini" ]; then
        # Grab the build_dir value, trim whitespace.
        bd=$(sed -n 's/^[[:space:]]*build_dir[[:space:]]*=[[:space:]]*\(.*\)$/\1/p' "$ini" \
             | head -n1 | sed 's/[[:space:]]*$//')
        case "$bd" in
            [A-Za-z]:* | "")
                # Windows-style path (e.g. C:/.piobuild) or empty — not usable here.
                ;;
            *)
                printf '%s\n' "$bd"
                return
                ;;
        esac
    fi
    printf '%s\n' "$REPO_ROOT/.pio/build"
}

BUILD_DIR=$(resolve_build_dir)
BIN="$BUILD_DIR/emulator_pc/program"

if [ ! -x "$BIN" ]; then
    # PlatformIO default fallback.
    ALT="$REPO_ROOT/.pio/build/emulator_pc/program"
    if [ -x "$ALT" ]; then
        BIN="$ALT"
    else
        echo "ERROR: emulator binary not found." >&2
        echo "  Looked for: $BIN" >&2
        echo "          and: $ALT" >&2
        echo "Build it first:" >&2
        echo "  PLATFORMIO_BUILD_DIR=.pio/build pio run -e emulator_pc" >&2
        exit 1
    fi
fi
echo "[run-emulator] Binary: $BIN"

# ── 2. Sanity-check SDL2 runtime ────────────────────────────────────────────
sdl2_runtime_ok() {
    # Prefer ldd on the actual binary: catches a missing/unresolved libSDL2.
    if command -v ldd >/dev/null 2>&1; then
        if ldd "$BIN" 2>/dev/null | grep -qi 'libSDL2.*not found'; then
            return 1
        fi
        if ldd "$BIN" 2>/dev/null | grep -qi 'libSDL2'; then
            return 0
        fi
    fi
    # Fall back to the loader cache.
    if command -v ldconfig >/dev/null 2>&1; then
        ldconfig -p 2>/dev/null | grep -qi 'libSDL2' && return 0
    fi
    # Couldn't determine — don't block; let the loader decide.
    return 0
}

if ! sdl2_runtime_ok; then
    echo "ERROR: SDL2 runtime library not found for $BIN." >&2
    echo "Install the SDL2 runtime/dev package:" >&2
    echo "  Debian/Ubuntu : sudo apt-get install -y libsdl2-2.0-0   (or libsdl2-dev)" >&2
    echo "  Fedora        : sudo dnf install -y SDL2                 (or SDL2-devel)" >&2
    echo "  Arch          : sudo pacman -S sdl2" >&2
    echo "  macOS         : brew install sdl2" >&2
    echo "See docs/emulator-sdl2-quickstart.md (Troubleshooting)." >&2
    exit 1
fi

# ── 3. Run ──────────────────────────────────────────────────────────────────
echo "[run-emulator] Launching NumOS emulator... (close the window to exit)"
exec "$BIN" "$@"
