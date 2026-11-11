#!/usr/bin/env python3
"""Check the generated C++ lookup against independent source-font interval queries."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    subprocess.run([__import__("sys").executable, str(ROOT / "scripts/generate-stix-math-kern.py"), "--check"], check=True)
    font = TTFont(ROOT / "assets/fonts/STIXTwoMath-Regular.ttf")
    source = font["MATH"].table.MathGlyphInfo.MathKernInfo
    mapping = dict(zip(source.MathKernCoverage.glyphs, source.MathKernInfoRecords))
    compiled = []
    for size in (18, 12, 8):
        text = (ROOT / f"src/fonts/stix_math_{size}.c").read_text(encoding="utf-8")
        compiled.append(set(int(x, 16) for x in re.findall(r"/\* U\+([0-9A-Fa-f]+) ", text)))
    codepoints = set.intersection(*compiled)
    lines = ['#include <cstdio>', '#include "math/font/stix_math_kern.h"', 'int main() { unsigned count=0;']
    corners = ("TopRight", "TopLeft", "BottomRight", "BottomLeft")
    # Test every retained glyph plus source glyphs whose four tables are NULL,
    # and every boundary from below, at equality, and above (off-by-one detector).
    for cp, glyph in font.getBestCmap().items():
        if cp not in codepoints:
            continue
        record = mapping.get(glyph)
        for corner in corners:
            table = getattr(record, corner + "MathKern") if record else None
            heights = [v.Value for v in table.CorrectionHeight] if table else []
            values = [v.Value for v in table.KernValue] if table else [0]
            sample_heights = {-32768, 0, 32767}
            for h in heights:
                sample_heights.update((h-1, h, h+1))
            for query in sorted(sample_heights):
                expected = values[sum(query >= h for h in heights)]
                lines.append(f'if(vpam::lookupMathKernDu({cp}, vpam::MathKernCorner::{corner}, {query}) != {expected}) {{ std::printf("FAIL U+{cp:04X} {corner} h={query} expected={expected}\\n"); return 1; }} ++count;')
    lines += ['if (vpam::lookupMathKernDu(0x10ffff, vpam::MathKernCorner::TopRight, 0) != 0) return 2;',
              'if (vpam::lookupMathKernDu(0x41, static_cast<vpam::MathKernCorner>(255), 0) != 0) return 3;',
              'std::printf("PASS %u font-derived boundary checks; metadata %u bytes; table %zu glyph %zu\\n",count,vpam::kStixMathKernStorageBytes,sizeof(vpam::MathKernTableRecord),sizeof(vpam::MathKernGlyphRecord));', 'return 0; }']
    with tempfile.TemporaryDirectory(prefix="numos-mathkern-") as temp:
        path = Path(temp)
        (path / "checks.cpp").write_text("\n".join(lines), encoding="utf-8")
        subprocess.run([args.compiler, "-std=c++17", "-O0", "-I", str(ROOT / "src"), str(path / "checks.cpp"), "-o", str(path / "checks.exe")], check=True)
        subprocess.run([str(path / "checks.exe")], check=True)


if __name__ == "__main__":
    main()
