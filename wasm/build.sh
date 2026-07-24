#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
configuration="${1:-Release}"
case "$configuration" in
  Debug|Release) ;;
  *) echo "usage: wasm/build.sh [Debug|Release]" >&2; exit 2 ;;
esac

required_version=$(tr -d '[:space:]' < wasm/emscripten.version)
actual_version=$(emcc --version | sed -n '1s/.*emcc.* \([0-9][0-9.]*\).*/\1/p')
if [[ "$actual_version" != "$required_version" ]]; then
  echo "emulator_web requires Emscripten $required_version (found ${actual_version:-unknown})" >&2
  exit 2
fi

configuration_lower=$(printf '%s' "$configuration" | tr '[:upper:]' '[:lower:]')
build_dir="out/wasm/build-${configuration_lower}"
emcmake cmake -S wasm -B "$build_dir" -DCMAKE_BUILD_TYPE="$configuration"
cmake --build "$build_dir" --target emulator_web --parallel "${JOBS:-8}"
