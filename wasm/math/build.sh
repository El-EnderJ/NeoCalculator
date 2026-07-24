#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."
configuration="${1:-Release}"
case "$configuration" in
  Debug|Release) ;;
  *) echo "usage: wasm/math/build.sh [Debug|Release]" >&2; exit 2 ;;
esac

required_version=$(tr -d '[:space:]' < wasm/emscripten.version)
actual_version=$(emcc --version | sed -n '1s/.*emcc.* \([0-9][0-9.]*\).*/\1/p')
if [[ "$actual_version" != "$required_version" ]]; then
  echo "numos_math_wasm requires Emscripten $required_version (found ${actual_version:-unknown})" >&2
  exit 2
fi

configuration_lower=$(printf '%s' "$configuration" | tr '[:upper:]' '[:lower:]')
build_dir="out/wasm-math/build-${configuration_lower}"
emcmake cmake -S wasm/math -B "$build_dir" -DCMAKE_BUILD_TYPE="$configuration"
cmake --build "$build_dir" --target numos_math_wasm --parallel "${JOBS:-8}"
node wasm/math/package.mjs "$configuration"
