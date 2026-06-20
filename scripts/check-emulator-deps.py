#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# check-emulator-deps.py — Optional diagnostic for the NumOS SDL2 emulator.
#
# Verifies that the host has what it needs to BUILD and RUN the native emulator
# (`pio run -e emulator_pc`). Standard library only; never imported or required
# by any build. Safe to run anywhere:
#
#     python scripts/check-emulator-deps.py
#
# It only reports — it changes nothing. Exit code 0 if no blocking problems were
# found, 1 otherwise. This script is NOT involved in firmware builds.
#
import os
import re
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IS_WIN = sys.platform.startswith("win")
IS_MAC = sys.platform == "darwin"

OK, WARN, BAD = "OK  ", "WARN", "FAIL"
_problems = 0


def report(level, msg):
    global _problems
    if level == BAD:
        _problems += 1
    print("  [%s] %s" % (level, msg))


def have(cmd):
    return shutil.which(cmd) is not None


def run(cmd):
    try:
        out = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        return out.decode("utf-8", "replace").strip()
    except Exception:
        return None


def section(title):
    print("\n== %s ==" % title)


def check_toolchain():
    section("Toolchain")
    if have("pio"):
        report(OK, "PlatformIO (pio) found: %s" % (run(["pio", "--version"]) or "?"))
    elif have("platformio"):
        report(OK, "PlatformIO (platformio) found.")
    else:
        report(BAD, "PlatformIO not found. Install: pip install -U platformio")

    compilers = ["g++", "clang++"] if not IS_WIN else ["g++", "clang++", "cl"]
    found = [c for c in compilers if have(c)]
    if found:
        report(OK, "C++ compiler(s): %s" % ", ".join(found))
    else:
        report(BAD, "No C++ compiler found (need g++/clang++ for platform=native).")


def check_sdl2_windows():
    section("SDL2 (Windows dev root)")
    candidates = [
        ("NUMOS_SDL2_ROOT", os.environ.get("NUMOS_SDL2_ROOT")),
        ("SDL2_DIR", os.environ.get("SDL2_DIR")),
        ("SDL2_ROOT", os.environ.get("SDL2_ROOT")),
        ("default", "C:/SDL2/x86_64-w64-mingw32"),
    ]
    root = None
    for name, val in candidates:
        if val and os.path.isdir(val):
            report(OK, "SDL2 root (%s): %s" % (name, val))
            root = val
            break
        elif val:
            report(WARN, "%s set to '%s' but that folder does not exist." % (name, val))
    if not root:
        report(BAD, "No SDL2 dev root found. Set NUMOS_SDL2_ROOT to the "
                    "x86_64-w64-mingw32 folder (contains include/, lib/, bin/).")
        return
    for sub in ("include", "lib"):
        p = os.path.join(root, sub)
        report(OK if os.path.isdir(p) else BAD, "%s/ %s" % (sub, "present" if os.path.isdir(p) else "MISSING"))
    dll = os.path.join(root, "bin", "SDL2.dll")
    if os.path.isfile(dll):
        report(OK, "Runtime SDL2.dll: %s" % dll)
    else:
        report(WARN, "SDL2.dll not under %s\\bin — needed at runtime (PATH or next to .exe)." % root)


def check_sdl2_unix():
    section("SDL2 (system)")
    pc = run(["pkg-config", "--modversion", "sdl2"]) if have("pkg-config") else None
    sc = run(["sdl2-config", "--version"]) if have("sdl2-config") else None
    if pc:
        report(OK, "pkg-config sees SDL2 %s" % pc)
    if sc:
        report(OK, "sdl2-config reports SDL2 %s" % sc)
    if not pc and not sc:
        report(BAD, "SDL2 not discoverable (no pkg-config sdl2 / sdl2-config).")
        if IS_MAC:
            report(WARN, "Install: brew install sdl2")
        else:
            report(WARN, "Install: sudo apt-get install libsdl2-dev  (Debian/Ubuntu)")
            report(WARN, "     or: sudo dnf install SDL2-devel        (Fedora)")
            report(WARN, "     or: sudo pacman -S sdl2                 (Arch)")


def check_build_artifacts():
    section("Build configuration")
    ini = os.path.join(REPO_ROOT, "platformio.ini")
    build_dir = os.environ.get("PLATFORMIO_BUILD_DIR")
    if build_dir:
        report(OK, "build_dir from PLATFORMIO_BUILD_DIR: %s" % build_dir)
    elif os.path.isfile(ini):
        m = None
        with open(ini, "r", encoding="utf-8", errors="replace") as f:
            for line in f:
                mm = re.match(r"\s*build_dir\s*=\s*(.+?)\s*$", line)
                if mm:
                    m = mm.group(1)
                    break
        if m:
            build_dir = m
            note = ""
            if not IS_WIN and re.match(r"^[A-Za-z]:", m):
                note = "  (Windows path — on this OS use PLATFORMIO_BUILD_DIR=.pio/build)"
            report(OK if not note else WARN, "build_dir from platformio.ini: %s%s" % (m, note))
        else:
            build_dir = os.path.join(REPO_ROOT, ".pio", "build")
            report(OK, "build_dir: PlatformIO default (.pio/build)")
    else:
        report(BAD, "platformio.ini not found at %s" % ini)
        return

    exe = "program.exe" if IS_WIN else "program"
    # Normalize a possibly Windows-style build_dir on non-Windows.
    if not IS_WIN and re.match(r"^[A-Za-z]:", build_dir or ""):
        build_dir = os.path.join(REPO_ROOT, ".pio", "build")
    binpath = os.path.join(build_dir, "emulator_pc", exe)
    if os.path.isfile(binpath):
        report(OK, "Emulator binary built: %s" % binpath)
    else:
        report(WARN, "Emulator binary not built yet (%s). Run: pio run -e emulator_pc" % binpath)


def main():
    print("NumOS emulator dependency check  (%s)" % sys.platform)
    check_toolchain()
    if IS_WIN:
        check_sdl2_windows()
    else:
        check_sdl2_unix()
    check_build_artifacts()
    print("\n%s" % ("All required dependencies look OK."
                     if _problems == 0 else
                     "%d blocking problem(s) found — see [FAIL] lines above." % _problems))
    return 1 if _problems else 0


if __name__ == "__main__":
    sys.exit(main())
