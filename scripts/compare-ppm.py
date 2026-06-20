#!/usr/bin/env python3
# compare-ppm.py — dependency-free P6 PPM comparator for the NumOS SDL2 emulator.
#
# Phase 4B-A visual-test tooling (Phase 4B-B adds cursor-safe masking). Compares
# the candidate screenshot produced by the native emulator (`--screenshot` /
# in-script `screenshot`, see src/hal/NativeHal.cpp:679-702) against an
# expected/golden PPM, byte-for-byte — optionally ignoring known volatile regions
# (the blinking text cursor) so a candidate can be compared despite UI nondeterminism.
#
# Scope: this tool only proves that two PPMs are or are not identical (and how
# they differ). It does NOT assert semantic correctness ("1+2 = 3"); that is a
# human review decision when a candidate is promoted to a golden. See
# tests/emulator/golden/README.md and tests/emulator/masks/README.md.
#
# Format handled: binary Netpbm P6 only (magic "P6", maxval 255, 3 bytes/pixel,
# RGB, top-to-bottom) — exactly what the emulator writes. Stdlib only: no PIL,
# no numpy, no PNG. Works identically on Windows and Linux.
#
# Usage:
#   python scripts/compare-ppm.py EXPECTED.ppm ACTUAL.ppm [--write-diff DIFF.ppm]
#                                 [--ignore-rect X,Y,W,H ...] [--mask-file PATH]
#
# Masking:
#   --ignore-rect X,Y,W,H   ignore a rectangle (top-left origin, pixels). Repeatable.
#   --mask-file PATH        text file of ignore-rects: one "x,y,w,h" per line;
#                           '#' comments and blank lines ignored. Combined (unioned)
#                           with any --ignore-rect flags.
#   Pixels inside any ignore-rect are excluded from the mismatch count, the
#   bounding box, and the equality decision. Rects with x<0, y<0, w<=0 or h<=0 are
#   an error (exit 2); rects that extend past the image edge are CLIPPED to the
#   frame (a rect whose origin is off-image simply ignores nothing).
#
# Exit codes:
#   0  identical (byte-identical, OR all remaining differences fall inside masks)
#   1  both valid P6 but different outside any mask (dimensions or pixels)
#   2  invalid input / I/O error (not P6, truncated, maxval != 255, missing file,
#      malformed ignore-rect / mask file)

import argparse
import hashlib
import sys


class PpmError(Exception):
    """Raised when a file is not a valid 8-bit P6 PPM we can compare, or when an
    ignore-rect / mask file is malformed. Funnels to exit code 2."""


def _sha256(data):
    h = hashlib.sha256()
    h.update(data)
    return h.hexdigest()


def _read_token(data, pos):
    """Read one whitespace-delimited ASCII token from the PPM header.

    Skips leading whitespace and '#' comments (to end of line) per the Netpbm
    spec. Returns (token_bytes, new_pos). Raises PpmError on EOF before a token.
    The emulator always writes a canonical header, but tolerating comments and
    arbitrary whitespace keeps the tool a correct general P6 reader.
    """
    n = len(data)
    while pos < n:
        c = data[pos]
        if c in b" \t\r\n\x0b\x0c":
            pos += 1
            continue
        if c == 0x23:  # '#': comment to end of line
            while pos < n and data[pos] not in b"\r\n":
                pos += 1
            continue
        break
    if pos >= n:
        raise PpmError("unexpected end of file while reading header")
    start = pos
    while pos < n and data[pos] not in b" \t\r\n\x0b\x0c":
        pos += 1
    return data[start:pos], pos


def parse_ppm(path):
    """Parse a P6 PPM file. Returns a dict with raw bytes, dims, header_len,
    pixel payload, and total size. Raises PpmError on anything we cannot compare.
    """
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as exc:
        raise PpmError("cannot read '%s': %s" % (path, exc))

    if len(data) < 2 or data[0:2] != b"P6":
        raise PpmError("'%s' is not a binary P6 PPM (bad magic)" % path)

    pos = 2
    try:
        w_tok, pos = _read_token(data, pos)
        h_tok, pos = _read_token(data, pos)
        m_tok, pos = _read_token(data, pos)
    except PpmError as exc:
        raise PpmError("'%s': %s" % (path, exc))

    try:
        width = int(w_tok)
        height = int(h_tok)
        maxval = int(m_tok)
    except ValueError:
        raise PpmError("'%s': non-integer width/height/maxval in header" % path)

    if width <= 0 or height <= 0:
        raise PpmError("'%s': non-positive dimensions %dx%d" % (path, width, height))
    if maxval != 255:
        raise PpmError(
            "'%s': unsupported maxval %d (only 8-bit maxval 255 is supported)"
            % (path, maxval)
        )

    # Per the spec exactly one whitespace byte follows the maxval; the pixel
    # payload starts immediately after it. Do NOT .split() the whole file: raw
    # RGB bytes may legitimately contain 0x20/0x0A.
    if pos >= len(data) or data[pos] not in b" \t\r\n\x0b\x0c":
        raise PpmError("'%s': missing whitespace after maxval" % path)
    pos += 1

    header_len = pos
    pixels = data[pos:]
    expected = width * height * 3
    if len(pixels) != expected:
        raise PpmError(
            "'%s': pixel payload is %d bytes, expected %d (%dx%d x3) — "
            "truncated or trailing garbage" % (path, len(pixels), expected, width, height)
        )

    return {
        "path": path,
        "data": data,
        "width": width,
        "height": height,
        "maxval": maxval,
        "header_len": header_len,
        "pixels": pixels,
        "size": len(data),
    }


def parse_rect(spec, source="--ignore-rect"):
    """Parse an 'X,Y,W,H' ignore-rect into a (x, y, w, h) int tuple.

    Raises PpmError (exit 2) on a malformed spec, a non-integer field, a negative
    origin (x<0 or y<0), or a non-positive size (w<=0 or h<=0). Bounds-clipping
    against the actual image happens later in build_ignore_mask().
    """
    parts = spec.split(",")
    if len(parts) != 4:
        raise PpmError(
            "bad %s '%s': expected exactly 4 comma-separated integers X,Y,W,H"
            % (source, spec)
        )
    try:
        x, y, w, h = (int(p.strip()) for p in parts)
    except ValueError:
        raise PpmError("bad %s '%s': non-integer field in X,Y,W,H" % (source, spec))
    if x < 0 or y < 0:
        raise PpmError(
            "bad %s '%s': negative origin (x,y must be >= 0)" % (source, spec)
        )
    if w <= 0 or h <= 0:
        raise PpmError(
            "bad %s '%s': non-positive size (w,h must be > 0)" % (source, spec)
        )
    return (x, y, w, h)


def load_mask_file(path):
    """Read a text mask file into a list of (x, y, w, h) ignore-rects.

    One 'x,y,w,h' rect per line. Blank lines and '#'-comment lines are ignored;
    a trailing '# ...' comment after a rect is also stripped. Raises PpmError on
    I/O error or any malformed rect."""
    rects = []
    try:
        with open(path, "r", encoding="utf-8") as f:
            lines = f.readlines()
    except OSError as exc:
        raise PpmError("cannot read mask file '%s': %s" % (path, exc))
    for lineno, raw in enumerate(lines, 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        rects.append(parse_rect(line, source="mask file '%s' line %d" % (path, lineno)))
    return rects


def build_ignore_mask(width, height, rects):
    """Build a flat 1-byte-per-pixel ignore mask (1 = ignored) from rects.

    Rects are clipped to the [0,width)x[0,height) frame, so a rect extending past
    the edge ignores only its in-bounds pixels and an off-image origin ignores
    nothing. Overlapping rects are counted once. Returns (mask, ignored_pixels).
    """
    mask = bytearray(width * height)
    one_row_cache = {}
    for (x, y, w, h) in rects:
        x0 = max(0, x)
        y0 = max(0, y)
        x1 = min(width, x + w)
        y1 = min(height, y + h)
        if x1 <= x0 or y1 <= y0:
            continue  # fully clipped away — ignores nothing
        span = x1 - x0
        filler = one_row_cache.get(span)
        if filler is None:
            filler = b"\x01" * span
            one_row_cache[span] = filler
        for yy in range(y0, y1):
            start = yy * width + x0
            mask[start:start + span] = filler
    return mask, mask.count(1)


def diff_metrics(a, b, width, mask=None):
    """Compute byte/pixel mismatch counts and the bounding box of differing
    pixels. `a` and `b` are equal-length pixel payloads (3 bytes/pixel). If
    `mask` is given (1 byte/pixel, 1 = ignore), masked pixels are excluded from
    all counts and from the bounding box."""
    byte_mismatches = 0
    pixel_mismatches = 0
    x_min = y_min = None
    x_max = y_max = None
    npix = len(a) // 3
    for i in range(npix):
        if mask is not None and mask[i]:
            continue
        base = i * 3
        if a[base] != b[base] or a[base + 1] != b[base + 1] or a[base + 2] != b[base + 2]:
            pixel_mismatches += 1
            if a[base] != b[base]:
                byte_mismatches += 1
            if a[base + 1] != b[base + 1]:
                byte_mismatches += 1
            if a[base + 2] != b[base + 2]:
                byte_mismatches += 1
            x = i % width
            y = i // width
            if x_min is None:
                x_min = x_max = x
                y_min = y_max = y
            else:
                if x < x_min:
                    x_min = x
                if x > x_max:
                    x_max = x
                if y < y_min:
                    y_min = y
                if y > y_max:
                    y_max = y
    bbox = None
    if x_min is not None:
        bbox = (x_min, y_min, x_max, y_max)
    return byte_mismatches, pixel_mismatches, bbox


def write_diff(expected, actual, out_path, mask=None):
    """Write a P6 diff image. Matching pixels become a dim grayscale of the
    expected image; differing pixels are bright magenta (255,0,255); pixels inside
    an ignore-rect are flat teal (0,128,128) so masked regions are visually
    distinct and never read as a mismatch. Same dimensions as the inputs.
    Dependency-free."""
    w = expected["width"]
    h = expected["height"]
    ea = expected["pixels"]
    ab = actual["pixels"]
    out = bytearray(len(ea))
    for i in range(w * h):
        base = i * 3
        if mask is not None and mask[i]:
            # Ignored region — flat teal, regardless of whether it differs.
            out[base] = 0
            out[base + 1] = 128
            out[base + 2] = 128
        elif ea[base] == ab[base] and ea[base + 1] == ab[base + 1] and ea[base + 2] == ab[base + 2]:
            # Dim grayscale of the expected pixel (luminance, halved).
            lum = (ea[base] * 30 + ea[base + 1] * 59 + ea[base + 2] * 11) // 100
            g = lum // 2
            out[base] = g
            out[base + 1] = g
            out[base + 2] = g
        else:
            out[base] = 255
            out[base + 1] = 0
            out[base + 2] = 255
    with open(out_path, "wb") as f:
        f.write(b"P6\n%d %d\n255\n" % (w, h))
        f.write(out)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Dependency-free P6 PPM comparator (NumOS emulator Phase 4B)."
    )
    parser.add_argument("expected", help="expected / golden PPM")
    parser.add_argument("actual", help="actual / candidate PPM")
    parser.add_argument(
        "--write-diff",
        metavar="DIFF.ppm",
        default=None,
        help="write a visual diff PPM (magenta = differing, teal = ignored)",
    )
    parser.add_argument(
        "--ignore-rect",
        action="append",
        default=[],
        metavar="X,Y,W,H",
        help="ignore a rectangle of pixels (top-left origin). Repeatable.",
    )
    parser.add_argument(
        "--mask-file",
        default=None,
        metavar="PATH",
        help="text file of ignore-rects (one 'x,y,w,h' per line; '#' comments)",
    )
    args = parser.parse_args(argv)

    try:
        exp = parse_ppm(args.expected)
        act = parse_ppm(args.actual)
        # Gather + validate ignore-rects up front so a malformed mask always
        # exits 2, regardless of whether the images happen to be identical.
        rects = [parse_rect(r) for r in args.ignore_rect]
        if args.mask_file:
            rects.extend(load_mask_file(args.mask_file))
        if rects:
            mask, ignored_px = build_ignore_mask(exp["width"], exp["height"], rects)
        else:
            mask, ignored_px = None, 0
    except PpmError as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        return 2

    exp_sha = _sha256(exp["data"])
    act_sha = _sha256(act["data"])

    print("expected: %s" % exp["path"])
    print("  dimensions: %dx%d  maxval %d" % (exp["width"], exp["height"], exp["maxval"]))
    print("  file size : %d bytes (header %d + pixels %d)"
          % (exp["size"], exp["header_len"], len(exp["pixels"])))
    print("  sha256    : %s" % exp_sha)
    print("actual:   %s" % act["path"])
    print("  dimensions: %dx%d  maxval %d" % (act["width"], act["height"], act["maxval"]))
    print("  file size : %d bytes (header %d + pixels %d)"
          % (act["size"], act["header_len"], len(act["pixels"])))
    print("  sha256    : %s" % act_sha)
    if rects:
        print("  ignore mask: %d rectangle(s), %d pixel(s) ignored" % (len(rects), ignored_px))

    if exp_sha == act_sha:
        print("RESULT: IDENTICAL (byte-for-byte)")
        return 0

    # Files differ. Distinguish geometry mismatch from pixel mismatch.
    if exp["width"] != act["width"] or exp["height"] != act["height"]:
        print("RESULT: DIFFERENT — dimension mismatch "
              "(%dx%d vs %dx%d); cannot compare pixels"
              % (exp["width"], exp["height"], act["width"], act["height"]))
        return 1

    byte_mismatches, pixel_mismatches, bbox = diff_metrics(
        exp["pixels"], act["pixels"], exp["width"], mask
    )
    total_pixels = exp["width"] * exp["height"]
    pct = (100.0 * pixel_mismatches / total_pixels) if total_pixels else 0.0
    label = " (outside mask)" if rects else ""

    if rects and pixel_mismatches == 0:
        print("RESULT: IDENTICAL (after masking %d rectangle(s), %d px ignored)"
              % (len(rects), ignored_px))
        if args.write_diff:
            try:
                write_diff(exp, act, args.write_diff, mask)
                print("  diff image written: %s" % args.write_diff)
            except OSError as exc:
                print("WARNING: could not write diff image: %s" % exc, file=sys.stderr)
        return 0

    print("RESULT: DIFFERENT — same dimensions, pixels differ")
    print("  mismatching bytes%s : %d / %d" % (label, byte_mismatches, len(exp["pixels"])))
    print("  mismatching pixels%s: %d / %d (%.4f%%)" % (label, pixel_mismatches, total_pixels, pct))
    if bbox is not None:
        x0, y0, x1, y1 = bbox
        print("  bounding box%s      : x[%d..%d] y[%d..%d]  (%dx%d px)"
              % (label, x0, x1, y0, y1, x1 - x0 + 1, y1 - y0 + 1))

    if args.write_diff:
        try:
            write_diff(exp, act, args.write_diff, mask)
            print("  diff image written: %s" % args.write_diff)
        except OSError as exc:
            print("WARNING: could not write diff image: %s" % exc, file=sys.stderr)

    return 1


if __name__ == "__main__":
    sys.exit(main())
