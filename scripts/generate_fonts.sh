#!/usr/bin/env bash
# Compatibility wrapper. The canonical generator owns ranges, BPP settings,
# deterministic provenance headers, and repository-relative output paths.
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/generate_stix_math_font.sh" "$@"
