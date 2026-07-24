#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
configuration="${1:-Release}"
./wasm/build.sh "$configuration"
variant=$(printf '%s' "$configuration" | tr '[:upper:]' '[:lower:]')
NUMOS_WASM_VARIANT="$variant" npm --prefix tests/wasm run smoke
