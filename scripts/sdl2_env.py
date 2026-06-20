#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# sdl2_env.py — Portable SDL2 discovery for the NumOS native emulator build.
#
# PlatformIO `pre:` extra_script, wired ONLY into [env:emulator_pc]
# (see platformio.ini). It removes the need for hardcoded, machine-specific
# SDL2 paths in build_flags by discovering SDL2 per-OS at build time:
#
#   Windows : an SDL2 *development* root (the folder that contains
#             include/, lib/ and bin/). Resolved, in order, from:
#               1. env var NUMOS_SDL2_ROOT
#               2. env var SDL2_DIR
#               3. env var SDL2_ROOT
#               4. the historical project default C:/SDL2/x86_64-w64-mingw32
#                  (kept so the original working dev build keeps working with
#                   zero configuration)
#             Appends: -I<root>/include  -L<root>/lib  -lmingw32 -lSDL2main -lSDL2
#             (matches the flags this project linked before Phase 2; SDL_main.h,
#              pulled in by <SDL2/SDL.h>, already renames main -> SDL_main, so no
#              -Dmain=SDL_main is added — that would double-define it.)
#
#   Linux / macOS : system SDL2 via `pkg-config sdl2`, falling back to
#             `sdl2-config`, falling back to a bare `-lSDL2`. This is what makes
#             a clean `apt-get install libsdl2-dev` (or Homebrew) build work with
#             no source or config edits.
#
# This script NEVER touches the firmware envs ([env:esp32s3_n16r8*]): it is only
# referenced from the emulator env, and it only *appends* flags.
#
# Override examples:
#   Windows : set NUMOS_SDL2_ROOT=C:\path\to\SDL2-2.30.x\x86_64-w64-mingw32
#   Linux   : (usually nothing) — install libsdl2-dev so pkg-config finds it.
#
import os
import sys
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)


def _log(msg):
    sys.stdout.write("[sdl2_env] " + msg + "\n")


def _fail(msg):
    sys.stderr.write("[sdl2_env] ERROR: " + msg + "\n")
    env.Exit(1)  # noqa: F821


def _is_windows():
    return sys.platform.startswith("win")


def _is_macos():
    return sys.platform == "darwin"


# ---------------------------------------------------------------------------
# Windows: development root (include/ + lib/ [+ bin/])
# ---------------------------------------------------------------------------
def _configure_windows():
    candidates = [
        ("NUMOS_SDL2_ROOT", os.environ.get("NUMOS_SDL2_ROOT")),
        ("SDL2_DIR",        os.environ.get("SDL2_DIR")),
        ("SDL2_ROOT",       os.environ.get("SDL2_ROOT")),
        ("default",         "C:/SDL2/x86_64-w64-mingw32"),
    ]

    root = None
    source = None
    for name, value in candidates:
        if value and os.path.isdir(value):
            root, source = value, name
            break

    if not root:
        _fail(
            "SDL2 development root not found.\n"
            "         Install the SDL2 MinGW dev package and point one of these\n"
            "         environment variables at the x86_64-w64-mingw32 folder\n"
            "         (the one containing include/, lib/ and bin/):\n"
            "             set NUMOS_SDL2_ROOT=C:\\path\\to\\SDL2\\x86_64-w64-mingw32\n"
            "         See docs/emulator-sdl2-quickstart.md."
        )

    inc = os.path.join(root, "include")
    lib = os.path.join(root, "lib")
    if not os.path.isdir(inc) or not os.path.isdir(lib):
        _fail(
            "SDL2 root '%s' (from %s) has no include/ or lib/ subfolder.\n"
            "         Expected an SDL2 *development* package layout."
            % (root, source)
        )

    # <SDL2/SDL.h> needs <root>/include; <SDL.h> would need <root>/include/SDL2.
    # Add both so either include style resolves.
    env.Append(CPPPATH=[inc, os.path.join(inc, "SDL2")])  # noqa: F821
    env.Append(LIBPATH=[lib])                             # noqa: F821
    env.Append(LIBS=["mingw32", "SDL2main", "SDL2"])      # noqa: F821

    _log("Windows SDL2 root: %s (via %s)" % (root, source))
    bindir = os.path.join(root, "bin")
    dll = os.path.join(bindir, "SDL2.dll")
    if os.path.isfile(dll):
        _log("Runtime SDL2.dll present: %s" % dll)
        _log("Run with scripts/run-emulator-windows.ps1 (it resolves SDL2.dll).")
    else:
        _log("NOTE: SDL2.dll not found under %s — the emulator will need it at "
             "runtime (PATH or next to the .exe)." % bindir)


# ---------------------------------------------------------------------------
# Linux / macOS: pkg-config -> sdl2-config -> bare -lSDL2
# ---------------------------------------------------------------------------
def _run(cmd):
    try:
        out = subprocess.check_output(cmd, stderr=subprocess.DEVNULL)
        return out.decode("utf-8", "replace").strip()
    except Exception:
        return None


def _add_include_and_parent(flag_str):
    # MergeFlags handles -I/-L/-l etc. Additionally, for every -I<dir> add the
    # PARENT dir too, so <SDL2/SDL.h> resolves even with a non-default prefix
    # (pkg-config/sdl2-config typically emit -I<prefix>/include/SDL2).
    env.MergeFlags(flag_str)  # noqa: F821
    extra = []
    for tok in flag_str.split():
        if tok.startswith("-I"):
            d = tok[2:]
            parent = os.path.dirname(d.rstrip("/\\"))
            if parent and os.path.isdir(parent):
                extra.append(parent)
    if extra:
        env.Append(CPPPATH=extra)  # noqa: F821


def _configure_unix():
    osname = "macOS" if _is_macos() else "Linux"

    cflags = _run(["pkg-config", "--cflags", "sdl2"])
    libs = _run(["pkg-config", "--libs", "sdl2"])
    if cflags is not None and libs is not None:
        _add_include_and_parent(cflags)
        env.MergeFlags(libs)  # noqa: F821
        _log("%s SDL2 via pkg-config: %s %s" % (osname, cflags, libs))
        return

    both = _run(["sdl2-config", "--cflags", "--libs"])
    if both is not None:
        _add_include_and_parent(both)
        _log("%s SDL2 via sdl2-config: %s" % (osname, both))
        return

    # Last resort: hope the headers/libs are on the default search path.
    env.Append(LIBS=["SDL2"])  # noqa: F821
    _log("%s SDL2: pkg-config and sdl2-config both unavailable — falling back "
         "to a bare -lSDL2. If the build fails, install libsdl2-dev (Debian/"
         "Ubuntu), SDL2-devel (Fedora), or sdl2 (Homebrew/Arch). "
         "See docs/emulator-sdl2-quickstart.md." % osname)


# ---------------------------------------------------------------------------
if _is_windows():
    _configure_windows()
else:
    _configure_unix()
