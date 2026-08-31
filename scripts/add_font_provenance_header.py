#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepend deterministic OFL provenance to an lv_font_conv C output."""

from __future__ import annotations

import argparse
from pathlib import Path


FONT_METADATA = {
    "montserrat": {
        "project": "Montserrat",
        "source": "assets/fonts/Montserrat-Regular.ttf",
        "copyright": (
            "Copyright 2011 The Montserrat Project Authors "
            "(https://github.com/JulietaUla/Montserrat)"
        ),
        "license": "assets/fonts/LICENSES/Montserrat-OFL-1.1.txt",
        "regenerate": "bash scripts/generate_montserrat_math_font.sh",
    },
    "stix": {
        "project": "STIX Two Math",
        "source": "assets/fonts/STIXTwoMath-Regular.ttf",
        "copyright": (
            "Copyright 2001-2021 The STIX Fonts Project Authors "
            "(https://github.com/stipub/stixfonts)"
        ),
        "license": "assets/fonts/LICENSES/STIXTwoMath-OFL-1.1.txt",
        "regenerate": "bash scripts/generate_stix_math_font.sh",
    },
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=sorted(FONT_METADATA))
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    metadata = FONT_METADATA[args.kind]
    body = args.output.read_text(encoding="utf-8")
    generator_script = metadata["regenerate"].split()[-1]
    header = f"""\
/*
 * Generated font data derived from {metadata['project']} Font Software.
 * Source: {metadata['source']}
 * {metadata['copyright']}
 * SPDX-License-Identifier: OFL-1.1
 * Full licence: {metadata['license']}
 * Generator: lv_font_conv, invoked by {generator_script}
 * Regenerate: {metadata['regenerate']}
 * DO NOT EDIT MANUALLY.
 */

"""
    args.output.write_text(header + body, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
