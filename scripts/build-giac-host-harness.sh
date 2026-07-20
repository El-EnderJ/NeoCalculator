#!/usr/bin/env bash
# Build + run the GIAC-A01 production Giac engine host harness.
# Usage: scripts/build-giac-host-harness.sh [--build-only]
#
# Compiles the vendored embedded Giac (lib/giac, KhiCAS profile) natively
# with the same macro set the esp32s3_n16r8 firmware uses, plus the
# PRODUCTION seam (src/math/giac/GiacEngine.cpp) and the harness
# (tests/host/giac_engine_suite_main.cpp) plus the focused GIAC-E01 calculus
# executable. Successor of the GIAC-FEAS-01 spike script; there is no
# experimental adapter anymore.
#
# -DHAVE_CONFIG_H: on firmware the Arduino-ESP32 platform injects it; native
# builds must pass it explicitly (its absence was the old "native Giac is
# broken / SIZEOF_INT" verdict).
#
# The engine reads vpam::g_angleMode through AngleModeRuntime.h, so the
# harness links MathEvaluator.cpp and its (emulator-proven) closure — which
# also proves the custom CAS and Giac coexist in one native binary, exactly
# what emulator_pc now does.
#
# Object files are cached in $OUT/obj across runs.
set -euo pipefail
cd "$(dirname "$0")/.."   # repo root
OUT="${GIAC_HOST_OUT:-/tmp/giac-host-harness}"
mkdir -p "$OUT/obj"

CXX=${CXX:-g++}
CC=${CC:-gcc}
JOBS=${JOBS:-8}

# Macro parity with [env:esp32s3_n16r8] + lib/giac/library.json:
GIAC_DEFS="-DHAVE_CONFIG_H -DIN_GIAC -DGIAC_KHICAS -DNO_GUI -DGIAC_GENERIC -DEMBEDDED -DUSE_GMP_REPLACEMENTS -DUMAP -DDOUBLEVAL"
INC="-Ilib/giac -Ilib/giac/src -Ilib/libtommath -Isrc"
# -ffunction-sections + --gc-sections mirrors the ESP32 link so unreachable
# KhiCAS console/UI code drops out; src/math/giac/GiacHostStubs.cpp covers
# the two symbols PE/COFF cannot strip (see that file's header).
GIAC_CXXFLAGS="-std=gnu++17 -O1 -fexceptions -ffunction-sections -fdata-sections -D_USE_MATH_DEFINES $GIAC_DEFS $INC -w"
CFLAGS="-O1 -ffunction-sections -fdata-sections -D_USE_MATH_DEFINES -Ilib/libtommath -w"
PROJ_CXXFLAGS="-std=gnu++17 -O1 -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections -D_USE_MATH_DEFINES -DNUMOS_GIAC_HOST_HARNESS=1 -Isrc"
LDFLAGS="-Wl,--gc-sections"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    # -static: avoid Git Bash's mismatched libstdc++-6.dll (segfaults in
    # iostreams); -lpsapi for the RSS probe. -D__MINGW_H: Giac guards its
    # POSIX access() calls with it, but it is only set once a Windows header
    # is included; the compiler predefine is __MINGW32__. -fpermissive: one
    # LP64 pointer->unsigned long debug print in kusual.cc.
    LDFLAGS="$LDFLAGS -static -lpsapi"
    GIAC_CXXFLAGS="$GIAC_CXXFLAGS -D__MINGW_H -fpermissive"
    ;;
  *)
    # config.h #undefs HAVE_UNISTD_H (embedded profile) but Giac's access()
    # guard still calls POSIX access() off-Windows; force the declaration.
    # GCC 13's C++ locale implementation also expects the gettext declarations
    # that the embedded Giac profile suppresses.
    GIAC_CXXFLAGS="$GIAC_CXXFLAGS -include unistd.h"
    if [[ -f /usr/include/libintl.h ]]; then
      GIAC_CXXFLAGS="$GIAC_CXXFLAGS -include libintl.h"
    fi
    ;;
esac

# --- collect Giac sources (mirror lib/giac/library.json srcFilter) ---
giac_srcs=()
for f in lib/giac/src/*.cc lib/giac/src/*.cpp; do
  base=$(basename "$f")
  case "$base" in
    kdisplay*|k_*|fx*|ti*|Xcas*|Fl_*|Graph*|hist*|test.cpp) continue ;;
  esac
  giac_srcs+=("$f")
done
tommath_srcs=(lib/libtommath/*.c)

compile_one() {
  src="$1"; flags="$2"; compiler="$3"
  obj="$OUT/obj/$(basename "$src").o"
  if [[ -f "$obj" && "$obj" -nt "$src" ]]; then return 0; fi
  echo "CC $src"
  $compiler $flags -c "$src" -o "$obj"
}
export -f compile_one
export OUT

# --- compile Giac + libtommath (parallel, cached) ---
printf '%s\n' "${tommath_srcs[@]}" | \
  xargs -P "$JOBS" -I{} bash -c "compile_one {} '$CFLAGS' '$CC'"
printf '%s\n' "${giac_srcs[@]}" | \
  xargs -P "$JOBS" -I{} bash -c "compile_one {} '$GIAC_CXXFLAGS' '$CXX'"

# --- production engine + host stubs (Giac-aware project TUs) ---
printf '%s\n' src/math/giac/GiacEngine.cpp src/math/giac/GiacHostStubs.cpp | \
  xargs -P "$JOBS" -I{} bash -c "compile_one {} '$PROJ_CXXFLAGS $GIAC_DEFS $INC -Wno-deprecated-declarations' '$CXX'"

# --- vpam::g_angleMode link closure (production MathEvaluator + deps; the
# --- same subset emulator_pc links, proving CAS/Giac coexistence) ---
proj_srcs=(
  src/math/CalculationEngine.cpp
  src/math/MathEvaluator.cpp src/math/MathAST.cpp src/math/VariableManager.cpp
  src/math/font/MathGlyphAssembly.cpp src/hal/FileSystem.cpp
  src/apps/NeoValue.cpp src/apps/NeoUnits.cpp
  src/apps/NeoMathBackend.cpp
  src/apps/NeoEnv.cpp src/apps/NeoLexer.cpp src/apps/NeoParser.cpp
  src/apps/NeoInterpreter.cpp src/apps/NeoStdLib.cpp
  src/apps/NeoStats.cpp src/apps/NeoModules.cpp src/apps/NeoIO.cpp
)
for f in src/math/cas/*.cpp; do proj_srcs+=("$f"); done
printf '%s\n' "${proj_srcs[@]}" | \
  xargs -P "$JOBS" -I{} bash -c "compile_one {} '$PROJ_CXXFLAGS' '$CXX'"

compile_one tests/host/giac_engine_suite_main.cpp "$PROJ_CXXFLAGS" "$CXX"
compile_one tests/host/giac_calculus_suite_main.cpp "$PROJ_CXXFLAGS" "$CXX"
compile_one tests/host/neo_math_backend_suite_main.cpp "$PROJ_CXXFLAGS" "$CXX"
compile_one tests/host/host_rss_probe.cpp "$PROJ_CXXFLAGS" "$CXX"

# --- link the stable regression and focused calculus executables separately.
# Vendored embedded Giac has a host-only, layout-sensitive failure when both
# large suites share one main TU; both binaries still link the exact same
# production objects and firmware macro profile.
common_objs=()
for obj in "$OUT"/obj/*.o; do
  case "$(basename "$obj")" in
    giac_engine_suite_main.cpp.o|giac_calculus_suite_main.cpp.o|neo_math_backend_suite_main.cpp.o) continue ;;
  esac
  common_objs+=("$obj")
done
echo "LINK $OUT/giac_engine_suite"
$CXX -o "$OUT/giac_engine_suite" "${common_objs[@]}" \
  "$OUT/obj/giac_engine_suite_main.cpp.o" $LDFLAGS
echo "LINK $OUT/giac_calculus_suite"
$CXX -o "$OUT/giac_calculus_suite" "${common_objs[@]}" \
  "$OUT/obj/giac_calculus_suite_main.cpp.o" $LDFLAGS
echo "LINK $OUT/neo_math_backend_suite"
$CXX -o "$OUT/neo_math_backend_suite" "${common_objs[@]}" \
  "$OUT/obj/neo_math_backend_suite_main.cpp.o" $LDFLAGS

[[ "${1:-}" == "--build-only" ]] && exit 0
# Run from $OUT so any emulator_data/ a TU creates stays out of the repo.
( cd "$OUT" && ./giac_engine_suite && ./giac_calculus_suite && \
  ./neo_math_backend_suite )
