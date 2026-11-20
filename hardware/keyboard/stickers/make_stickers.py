#!/usr/bin/env python3
"""
NeoCalculator keycap label stickers — print-ready vector PDF at true 1:1 scale.

    python hardware/keyboard/stickers/make_stickers.py            # build the PDF
    python hardware/keyboard/stickers/make_stickers.py --verify   # build + check + PNG previews
    python hardware/keyboard/stickers/make_stickers.py --measure  # re-derive key size from key.stl
    python hardware/keyboard/stickers/make_stickers.py --labels   # print the legend table (Markdown)

Requirements: Python 3.9+, reportlab, PyYAML, fontTools, numpy.
Optional: PyMuPDF (`pip install pymupdf`) for --verify and the PNG previews.

Everything configurable lives in stickers.yaml next to this file. Legends are
taken from the canonical keypad layout JSON and cross-checked, key by key and
plane by plane, against the firmware's generated keymap
(src/input/generated/ProductionKeypadMap.generated.h).
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import math
import os
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

import yaml
from fontTools.pens.boundsPen import BoundsPen
from fontTools.ttLib import TTFont as FTFont
from reportlab.lib.colors import HexColor
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas as rl_canvas

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
PT_PER_MM = 72.0 / 25.4
PLANES = ("primary", "shift", "alpha")

# Layout-JSON action names that the firmware generator renames.
JSON_TO_FIRMWARE_SEMANTIC = {
    "delete": "key_delete",
    "enter_deep_sleep": "deep_sleep_off",
}


def mm(v: float) -> float:
    return v * PT_PER_MM


# ═════════════════════════════════════════════════════════════════════════════
# Configuration and fonts
# ═════════════════════════════════════════════════════════════════════════════

def load_config(path: Path) -> dict:
    with open(path, encoding="utf-8") as fh:
        cfg = yaml.safe_load(fh)
    st = cfg["sticker"]
    if st.get("corner_radius", "auto") == "auto":
        st["corner_radius"] = max(0.0, cfg["key_top"]["corner_radius"] - st["inset"])
    return cfg


def _font_candidates(paths: list[str], filename: str) -> list[Path]:
    out = [Path(os.path.expanduser(p)) for p in paths]
    local = os.environ.get("LOCALAPPDATA")
    if local:
        out.append(Path(local) / "Microsoft/Windows/Fonts" / filename)
    try:
        import matplotlib  # ships DejaVu Sans

        out.append(Path(matplotlib.get_data_path()) / "fonts/ttf" / filename)
    except Exception:
        pass
    return out


class Font:
    """A TrueType font registered with reportlab plus fontTools ink metrics."""

    def __init__(self, name: str, path: Path):
        self.name, self.path = name, path
        pdfmetrics.registerFont(TTFont(name, str(path)))
        self.ft = FTFont(str(path))
        self.cmap = self.ft.getBestCmap()
        self.upm = self.ft["head"].unitsPerEm
        self.glyphset = self.ft.getGlyphSet()
        self.hmtx = self.ft["hmtx"]
        self._bounds: dict[str, tuple] = {}

    def has(self, ch: str) -> bool:
        return ord(ch) in self.cmap

    def _glyph_bounds(self, gname: str):
        if gname not in self._bounds:
            pen = BoundsPen(self.glyphset)
            self.glyphset[gname].draw(pen)
            self._bounds[gname] = pen.bounds  # None for blank glyphs
        return self._bounds[gname]

    def ink(self, text: str, size: float):
        """(advance, ink_x0, ink_x1, ink_ymin, ink_ymax) in points."""
        x = 0.0
        x0 = y0 = math.inf
        x1 = y1 = -math.inf
        for ch in text:
            if ord(ch) not in self.cmap:
                raise MissingGlyph(f"U+{ord(ch):04X} {ch!r} is not in {self.path.name}")
            g = self.cmap[ord(ch)]
            b = self._glyph_bounds(g)
            if b:
                x0 = min(x0, x + b[0]); x1 = max(x1, x + b[2])
                y0 = min(y0, b[1]); y1 = max(y1, b[3])
            x += self.hmtx[g][0]
        s = size / self.upm
        if x0 == math.inf:  # only blank glyphs
            return x * s, 0.0, x * s, 0.0, 0.0
        return x * s, x0 * s, x1 * s, y0 * s, y1 * s


class MissingGlyph(Exception):
    pass


def load_fonts(cfg: dict) -> dict[str, Font]:
    fonts = {}
    for key, fname in (("regular", "DejaVuSans.ttf"), ("bold", "DejaVuSans-Bold.ttf")):
        for cand in _font_candidates(cfg["fonts"][key], fname):
            if cand.is_file():
                fonts[key] = Font(f"Sticker-{key}", cand)
                break
        else:
            sys.exit(f"ERROR: no {fname} found. Install DejaVu Sans or `pip install matplotlib`, "
                     f"or set fonts.{key} in stickers.yaml.")
    return fonts


# ═════════════════════════════════════════════════════════════════════════════
# Tiny math-legend typesetter (subset of the LaTeX used in the layout JSON:
# \frac \sqrt[..]{..} \square \lvert \rvert ^{..} \, plus Unicode text)
# All metrics in points; origin on the baseline at the left edge, y up.
# ═════════════════════════════════════════════════════════════════════════════

class Node:
    w = 0.0      # advance width
    x0 = 0.0     # ink left
    x1 = 0.0     # ink right
    asc = 0.0    # ink above baseline
    desc = 0.0   # ink below baseline (positive)

    def draw(self, c, x, y):
        raise NotImplementedError


class TextN(Node):
    def __init__(self, s: str, font: Font, size: float):
        self.s, self.font, self.size = s, font, size
        self.w, self.x0, self.x1, ymin, ymax = font.ink(s, size)
        self.asc, self.desc = ymax, -ymin

    def draw(self, c, x, y):
        c.setFont(self.font.name, self.size)
        c.drawString(x, y, self.s)


class GlueN(Node):
    def __init__(self, w: float):
        self.w, self.x0, self.x1 = w, 0.0, 0.0

    def draw(self, c, x, y):
        pass


class SquareN(Node):
    """Empty-slot placeholder box."""

    def __init__(self, size: float):
        self.side = 0.56 * size
        self.lw = max(0.3, 0.075 * size)
        m = 0.07 * size
        self.m = m
        self.w = self.side + 2 * m
        self.x0, self.x1 = m, m + self.side
        self.asc, self.desc = self.side, 0.0

    def draw(self, c, x, y):
        c.setLineWidth(self.lw)
        h = self.lw / 2
        c.rect(x + self.m + h, y + h, self.side - self.lw, self.side - self.lw, stroke=1, fill=0)


class RowN(Node):
    def __init__(self, items: list[Node]):
        self.items = items
        x = 0.0
        self.x0, self.x1 = math.inf, -math.inf
        for it in items:
            if not isinstance(it, GlueN):
                self.x0 = min(self.x0, x + it.x0); self.x1 = max(self.x1, x + it.x1)
            self.asc = max(self.asc, it.asc); self.desc = max(self.desc, it.desc)
            x += it.w
        self.w = x
        if self.x0 == math.inf:
            self.x0, self.x1 = 0.0, x

    def draw(self, c, x, y):
        for it in self.items:
            it.draw(c, x, y)
            x += it.w


class SupN(Node):
    def __init__(self, base: Node, exp: Node, size: float):
        self.base, self.exp = base, exp
        self.raise_ = 0.40 * size
        self.gap = 0.04 * size
        self.w = base.w + self.gap + exp.w
        self.x0 = min(base.x0, base.w + self.gap + exp.x0)
        self.x1 = max(base.x1, base.w + self.gap + exp.x1)
        self.asc = max(base.asc, self.raise_ + exp.asc)
        self.desc = max(base.desc, exp.desc - self.raise_)

    def draw(self, c, x, y):
        self.base.draw(c, x, y)
        self.exp.draw(c, x + self.base.w + self.gap, y + self.raise_)


class FracN(Node):
    def __init__(self, num: Node, den: Node, size: float):
        self.num, self.den = num, den
        self.axis = 0.30 * size
        self.t = max(0.3, 0.07 * size)
        self.gap = 0.10 * size
        self.pad = 0.08 * size
        inner = max(num.x1 - num.x0, den.x1 - den.x0)
        self.w = inner + 2 * self.pad
        self.x0, self.x1 = 0.0, self.w
        self.num_base = self.axis + self.t / 2 + self.gap + num.desc
        self.den_base = self.axis - self.t / 2 - self.gap - den.asc
        self.asc = self.num_base + num.asc
        self.desc = max(0.0, -(self.den_base - den.desc))

    def draw(self, c, x, y):
        c.setLineWidth(self.t)
        c.line(x + self.pad * 0.5, y + self.axis, x + self.w - self.pad * 0.5, y + self.axis)
        for n, b in ((self.num, self.num_base), (self.den, self.den_base)):
            nx = x + (self.w - (n.x1 - n.x0)) / 2 - n.x0
            n.draw(c, nx, y + b)


class SqrtN(Node):
    def __init__(self, body: Node, index: Node | None, size: float):
        self.body, self.index, self.size = body, index, size
        self.t = max(0.3, 0.07 * size)
        self.sign_w = 0.50 * size
        body_asc = max(body.asc, 0.62 * size)
        self.top = body_asc + 0.12 * size
        self.bottom = -max(body.desc, 0.06 * size)
        self.lead = 0.0
        if index is not None:
            self.lead = max(0.0, (index.x1 - index.x0) - 0.22 * size)
        body_w = max(body.w, 0.30 * size)
        self.body_x = self.lead + self.sign_w + 0.04 * size
        self.w = self.body_x + body_w + 0.08 * size
        self.x0, self.x1 = 0.0, self.w
        self.asc = self.top + self.t / 2
        self.desc = -self.bottom + self.t / 2
        if index is not None:
            self.index_base = 0.42 * self.top + index.desc + 0.05 * size
            self.asc = max(self.asc, self.index_base + index.asc)

    def draw(self, c, x, y):
        s, L = self.size, x + self.lead
        p = c.beginPath()
        p.moveTo(L + 0.02 * s, y + 0.30 * self.top)
        p.lineTo(L + 0.13 * s, y + 0.36 * self.top)
        p.lineTo(L + 0.27 * s, y + self.bottom)
        p.lineTo(L + self.sign_w, y + self.top)
        p.lineTo(x + self.w, y + self.top)
        c.setLineWidth(self.t)
        c.setLineJoin(1); c.setLineCap(1)
        c.drawPath(p, stroke=1, fill=0)
        c.setLineCap(0)
        self.body.draw(c, x + self.body_x, y)
        if self.index is not None:
            self.index.draw(c, x - self.index.x0, y + self.index_base)


_CMD_TEXT = {"times": "×", "div": "÷", "pi": "π", "infty": "∞", "circ": "°",
             "lvert": "|", "rvert": "|", "theta": "θ", "leq": "≤", "geq": "≥", "neq": "≠"}


def typeset(src: str, font: Font, size: float) -> Node:
    """Parse a label (Unicode text with optional LaTeX subset) into a Node."""
    if not re.search(r"\\[A-Za-z,]|\^", src):
        return TextN(src, font, size)      # plain text: "{", "}", "\", "x,t,θ,n" … literally
    src = src.replace("-", "−")            # math minus in exponents such as x^{-1}
    pos = 0

    def parse_until(end: str | None, sz: float) -> Node:
        nonlocal pos
        items: list[Node] = []
        buf = ""

        def flush():
            nonlocal buf
            if buf:
                items.append(TextN(buf, font, sz)); buf = ""

        def arg(asz: float) -> Node:
            nonlocal pos
            if pos < len(src) and src[pos] == "{":
                pos += 1
                return parse_until("}", asz)
            if pos < len(src) and src[pos] == "\\":
                sub = parse_one_command(asz)
                return sub
            ch = src[pos]; pos += 1
            return TextN(ch, font, asz)

        def parse_one_command(csz: float) -> Node:
            nonlocal pos
            pos += 1
            m = re.match(r"[A-Za-z]+|.", src[pos:])
            if m is None:                      # a lone trailing "\" is a literal backslash
                return TextN("\\", font, csz)
            name = m.group(0); pos += len(name)
            if name == "\\":
                return TextN("\\", font, csz)
            if name == "frac":
                a = arg(csz * 0.72); b = arg(csz * 0.72)
                return FracN(a, b, csz)
            if name == "sqrt":
                idx = None
                if pos < len(src) and src[pos] == "[":
                    pos += 1
                    idx = parse_until("]", csz * 0.55)
                return SqrtN(arg(csz), idx, csz)
            if name == "square":
                return SquareN(csz)
            if name == ",":
                return GlueN(0.17 * csz)
            if name in _CMD_TEXT:
                return TextN(_CMD_TEXT[name], font, csz)
            raise ValueError(f"unsupported command \\{name} in label {src!r}")

        while pos < len(src):
            ch = src[pos]
            if end is not None and ch == end:
                pos += 1
                break
            if ch == "\\":
                flush(); items.append(parse_one_command(sz))
            elif ch == "^":
                pos += 1
                if buf:
                    base: Node = TextN(buf[-1], font, sz); buf = buf[:-1]
                    flush()
                elif items:
                    base = items.pop()
                else:
                    raise ValueError(f"superscript without base in {src!r}")
                items.append(SupN(base, arg(sz * 0.64), sz))
            elif ch == "{":
                flush(); pos += 1
                items.append(parse_until("}", sz))
            else:
                buf += ch; pos += 1
        flush()
        return items[0] if len(items) == 1 else RowN(items)

    return parse_until(None, size)


# ═════════════════════════════════════════════════════════════════════════════
# Key data: layout JSON + firmware cross-check
# ═════════════════════════════════════════════════════════════════════════════

@dataclass
class Key:
    id: str
    row: int
    col: int
    category: str
    primary: str
    shift: str
    alpha: str
    semantics: dict
    hidden: dict = field(default_factory=dict)   # legends not printed by layout policy


def load_keys(cfg: dict) -> dict[str, Key]:
    with open(REPO / cfg["sources"]["layout_json"], encoding="utf-8") as fh:
        layout = json.load(fh)
    keys: dict[str, Key] = {}
    for k in layout["keys"]:
        p = k["planes"]
        sem = {}
        for name in PLANES:
            d = p[name]
            if d.get("kind") == "inherit":
                src = p[d.get("inheritFrom", "primary")]
                s = src.get("semanticId") or src.get("action")
            else:
                s = d.get("semanticId") or d.get("action")
            sem[name] = JSON_TO_FIRMWARE_SEMANTIC.get(s, s)
        labels, hidden = {}, {}
        for name in PLANES:
            d = p[name]
            lab = d.get("label", "") if d.get("kind") != "inherit" else ""
            if name == "primary":
                labels[name] = lab          # the main legend is always printed
            elif d.get("printLegend") or cfg.get("print_hidden_legends"):
                labels[name] = lab
            else:
                labels[name] = ""
                if lab:
                    hidden[name] = lab
        key = Key(k["id"], k["visual"]["row"], k["visual"]["col"], k["category"],
                  labels["primary"], labels["shift"], labels["alpha"], sem, hidden)
        for name, val in (cfg.get("label_overrides") or {}).get(key.id, {}).items():
            setattr(key, name, val)
        keys[key.id] = key
    return keys


_MAP_RE = re.compile(
    r"\{(\d+), (\d+), (\d+), (\d+), (\d+), (-?\d+), (-?\d+), (-?\d+), SemanticId::(\w+), "
    r"KeyCode::(\w+), \"((?:[^\"\\]|\\.)*)\", (true|false)\}")
_DEF_RE = re.compile(r"\{SemanticId::(\w+), KeyCode::(\w+), \"((?:[^\"\\]|\\.)*)\", (true|false)\}")


def _unescape(s: str) -> str:
    return re.sub(r"\\(.)", r"\1", s)  # C string literal → text (only \\ and \" occur)


def firmware_audit(cfg: dict, keys: dict[str, Key]) -> tuple[list[str], list[str]]:
    """Return (mismatches, unhandled) comparing printed legends with the firmware map."""
    text = (REPO / cfg["sources"]["firmware_keymap"]).read_text(encoding="utf-8")
    entries = [m.groups() for m in _MAP_RE.finditer(text)]
    math_block = text.split("kMathPlaneDefinitions", 1)[1].split("}};", 1)[0]
    defs = [_DEF_RE.findall(line) for line in math_block.splitlines() if "{{{" in line]
    if len(entries) != 50 or len(defs) != 50:
        return [f"could not parse firmware keymap ({len(entries)} keys, {len(defs)} plane rows)"], []

    # Which SemanticIds have explicit handlers outside the generated header?
    referenced: set[str] = set()
    src_root = REPO / cfg["sources"]["firmware_src"]
    for path in src_root.rglob("*"):
        if path.suffix in (".cpp", ".h", ".hpp", ".inc") and "generated" not in path.parts:
            try:
                referenced.update(re.findall(r"SemanticId::(\w+)", path.read_text(errors="ignore")))
            except OSError:
                pass

    by_visual = {(int(e[3]), int(e[4])): (i, e) for i, e in enumerate(entries)}
    mismatches, unhandled = [], []
    for key in keys.values():
        i, e = by_visual[(key.row, key.col)]
        fw_primary_label = _unescape(e[10])
        if fw_primary_label != key.primary and key.id not in (cfg.get("label_overrides") or {}):
            mismatches.append(f"{key.id}: layout primary {key.primary!r} != firmware {fw_primary_label!r}")
        for pi, plane in enumerate(PLANES):
            fw_sem, fw_code, fw_text, _ = defs[i][pi]
            if fw_sem != key.semantics[plane]:
                mismatches.append(f"{key.id} {plane}: layout '{key.semantics[plane]}' != firmware '{fw_sem}'")
            label = getattr(key, plane)
            if not label or key.category == "modifier":
                continue
            if fw_code == "NONE" and fw_text == "" and fw_sem not in referenced:
                unhandled.append(f"{key.id} {plane} {label!r} → SemanticId::{fw_sem} "
                                 f"(KeyCode::NONE, no insert text, no handler found in src/)")
    return mismatches, unhandled


# ═════════════════════════════════════════════════════════════════════════════
# Drawing
# ═════════════════════════════════════════════════════════════════════════════

class Page:
    """Thin wrapper: millimetres, origin at the top-left corner of the page."""

    def __init__(self, c, h_mm: float):
        self.c, self.h = c, h_mm

    def Y(self, y_top_mm: float) -> float:
        return mm(self.h - y_top_mm)

    def text(self, x, y, s, font: Font, size, color="#000000", anchor="left"):
        c = self.c
        c.setFillColor(HexColor(color)); c.setFont(font.name, size)
        if anchor == "left":
            c.drawString(mm(x), self.Y(y), s)
        elif anchor == "center":
            c.drawCentredString(mm(x), self.Y(y), s)
        else:
            c.drawRightString(mm(x), self.Y(y), s)

    def line(self, x1, y1, x2, y2, width_pt=0.3, color="#000000"):
        c = self.c
        c.setStrokeColor(HexColor(color)); c.setLineWidth(width_pt)
        c.line(mm(x1), self.Y(y1), mm(x2), self.Y(y2))


def _inside_rounded(px, py, w, h, r, margin) -> bool:
    """Point (sticker-local, mm, top-left origin) inside the rounded rect shrunk by margin."""
    x0, y0, x1, y1 = margin, margin, w - margin, h - margin
    rr = max(0.0, r - margin)
    if not (x0 - 1e-9 <= px <= x1 + 1e-9 and y0 - 1e-9 <= py <= y1 + 1e-9):
        return False
    cx = min(max(px, x0 + rr), x1 - rr)
    cy = min(max(py, y0 + rr), y1 - rr)
    return math.hypot(px - cx, py - cy) <= rr + 1e-9


@dataclass
class Placed:
    node: Node
    size: float
    sx: float        # horizontal scale
    x: float         # pen x (mm, sticker-local)
    y: float         # baseline y (mm from sticker top)
    color: str

    def ink_rect(self):  # mm, sticker-local, top-left origin
        return (self.x + self.node.x0 * self.sx / PT_PER_MM,
                self.y - self.node.asc / PT_PER_MM,
                self.x + self.node.x1 * self.sx / PT_PER_MM,
                self.y + self.node.desc / PT_PER_MM)


class LayoutError(Exception):
    pass


def _fit(label: str, font: Font, max_size: float, min_size: float, avail_w: float,
         avail_h: float, min_sx: float) -> tuple[Node, float, float]:
    """Largest size (and horizontal scale) at which the label's ink fits avail (mm)."""
    size = max_size
    while size >= min_size - 1e-9:
        node = typeset(label, font, size)
        iw = (node.x1 - node.x0) / PT_PER_MM
        ih = (node.asc + node.desc) / PT_PER_MM
        if ih <= avail_h + 1e-9:
            sx = min(1.0, avail_w / iw) if iw > 0 else 1.0
            if sx >= min_sx - 1e-9:
                return node, size, sx
        size -= 0.1
    raise LayoutError(f"label {label!r} does not fit {avail_w:.2f} x {avail_h:.2f} mm "
                      f"even at {min_size} pt")


def layout_sticker(key: Key, w: float, h: float, r: float, cfg: dict,
                   fonts: dict[str, Font]) -> list[Placed]:
    st, fs, col = cfg["sticker"], cfg["font_sizes"], cfg["colors"]
    pad = st["text_padding"]
    f1 = fonts[cfg["fonts"]["primary_weight"]]
    f2 = fonts[cfg["fonts"]["secondary_weight"]]
    min_sx = fs["min_horizontal_scale"]
    placed: list[Placed] = []
    primary_color = col["modifier_text"].get(key.primary, col["text"]) \
        if key.category == "modifier" else col["text"]

    has_secondary = bool(key.shift or key.alpha)
    band_bottom = pad
    if has_secondary:
        # Both corner legends share one size, one condensation and one baseline.
        size = fs["secondary"]
        gap = 0.6
        # side padding needed so an ink box whose top is at `pad` clears the corner arc
        side_pad = pad
        while not _inside_rounded(side_pad, pad, w, h, r, pad * 0.6):
            side_pad += 0.02
        while True:
            nodes = [typeset(s, f2, size) if s else None for s in (key.shift, key.alpha)]
            widths = [((n.x1 - n.x0) / PT_PER_MM if n else 0.0) for n in nodes]
            avail = w - 2 * side_pad - (gap if all(nodes) else 0.0)
            sx = min(1.0, avail / sum(widths)) if sum(widths) else 1.0
            if sx >= min_sx or size <= fs["secondary_min"]:
                break
            size -= 0.1
        if sx < min_sx - 1e-9:
            raise LayoutError(f"{key.id}: secondary legends {key.shift!r}/{key.alpha!r} do not fit")
        asc = max(n.asc for n in nodes if n) / PT_PER_MM
        baseline = pad + asc
        for n, side, color in ((nodes[0], "left", col["shift_legend"]),
                               (nodes[1], "right", col["alpha_legend"])):
            if n is None:
                continue
            iw = (n.x1 - n.x0) * sx / PT_PER_MM
            hp = side_pad
            # nudge inwards until the ink box clears the rounded corner
            while True:
                x_ink = hp if side == "left" else w - hp - iw
                pl = Placed(n, size, sx, x_ink - n.x0 * sx / PT_PER_MM, baseline, color)
                x0, y0, x1, y1 = pl.ink_rect()
                if all(_inside_rounded(px, py, w, h, r, pad * 0.6)
                       for px, py in ((x0, y0), (x1, y0), (x0, y1), (x1, y1))):
                    break
                hp += 0.05
                if hp > w / 2:
                    raise LayoutError(f"{key.id}: corner legend cannot clear the corner radius")
            placed.append(pl)
            band_bottom = max(band_bottom, y1)
        # fixed primary area top, so the main legends line up across keys
        band_bottom = max(band_bottom, pad + fs["secondary"] * 0.86 / PT_PER_MM)

    area_top = band_bottom + (0.35 if has_secondary else 0.0)
    area_bot = h - pad
    if has_secondary:
        # keep the visual centre of all keys' main legends on the same line
        area_top = max(area_top, pad + fs["secondary"] * 0.86 / PT_PER_MM + 0.35)
    node, size, sx = _fit(key.primary, f1, fs["primary_max"], fs["primary_min"],
                          w - 2 * pad, area_bot - area_top, min_sx)
    cy = (area_top + area_bot) / 2
    ink_w = (node.x1 - node.x0) * sx / PT_PER_MM
    x_pen = w / 2 - ink_w / 2 - node.x0 * sx / PT_PER_MM
    baseline = cy + (node.asc - node.desc) / 2 / PT_PER_MM
    placed.append(Placed(node, size, sx, x_pen, baseline, primary_color))

    # Final checks: every ink box inside the safe rounded rect, no overlaps.
    rects = [p.ink_rect() for p in placed]
    for (x0, y0, x1, y1) in rects:
        for px, py in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
            if not _inside_rounded(px, py, w, h, r, pad * 0.6):
                raise LayoutError(f"{key.id}: ink at ({px:.2f},{py:.2f}) leaves the sticker safe area")
    for i in range(len(rects)):
        for j in range(i + 1, len(rects)):
            a, b = rects[i], rects[j]
            if a[0] < b[2] + 0.2 and b[0] < a[2] + 0.2 and a[1] < b[3] + 0.2 and b[1] < a[3] + 0.2:
                raise LayoutError(f"{key.id}: legends overlap")
    return placed


def sticker_background(key: Key, cfg: dict) -> str:
    col = cfg["colors"]
    if key.category == "modifier":
        return col["modifier_background"].get(key.primary, "#FFFFFF")
    return col["category_background"].get(key.category, "#FFFFFF")


def draw_sticker(pg: Page, key: Key, x: float, y: float, w: float, h: float, r: float,
                 cfg: dict, fonts: dict[str, Font], report: list) -> None:
    c = pg.c
    st = cfg["sticker"]
    placed = layout_sticker(key, w, h, r, cfg, fonts)
    c.setFillColor(HexColor(sticker_background(key, cfg)))
    c.setStrokeColor(HexColor(st["cut_line_color"]))
    c.setLineWidth(st["cut_line_width_pt"])
    c.roundRect(mm(x), pg.Y(y + h), mm(w), mm(h), mm(r), stroke=1, fill=1)
    for p in placed:
        c.saveState()
        c.setFillColor(HexColor(p.color)); c.setStrokeColor(HexColor(p.color))
        c.translate(mm(x + p.x), pg.Y(y + p.y))
        c.scale(p.sx, 1.0)
        p.node.draw(c, 0.0, 0.0)
        c.restoreState()
    report.append({"key": key.id, "x": x, "y": y, "w": w, "h": h,
                   "legends": [(round(p.size, 1), round(p.sx, 2)) for p in placed]})


def draw_crop_marks(pg: Page, xs: list[tuple[float, float]], ys: list[tuple[float, float]],
                    bx0, by0, bx1, by1, cfg: dict) -> None:
    off, ln = cfg["page"]["crop_mark_offset"], cfg["page"]["crop_mark_length"]
    color = cfg["sticker"]["cut_line_color"]
    for a, b in xs:
        for xv in (a, b):
            pg.line(xv, by0 - off - ln, xv, by0 - off, 0.25, color)
            pg.line(xv, by1 + off, xv, by1 + off + ln, 0.25, color)
    for a, b in ys:
        for yv in (a, b):
            pg.line(bx0 - off - ln, yv, bx0 - off, yv, 0.25, color)
            pg.line(bx1 + off, yv, bx1 + off + ln, yv, 0.25, color)


def draw_calibration(pg: Page, x: float, y: float, cfg: dict, fonts: dict[str, Font]) -> float:
    """Calibration square + ruler with its top-left at (x, y). Returns the bottom y."""
    c = pg.c
    sq, rl = cfg["calibration"]["square"], cfg["calibration"]["ruler"]
    fb, fr = fonts["bold"], fonts["regular"]
    pg.text(x, y + 3.2, "Print at 100% / Actual size — this square must measure "
            f"{sq:g} mm", fb, 9)
    pg.text(x, y + 7.0, "Imprime al 100 % / «Tamaño real» (sin «Ajustar a página»). "
            "Mide el cuadrado con una regla antes de recortar.", fr, 7, "#444444")
    top = y + 10.0
    c.setStrokeColor(HexColor("#000000")); c.setLineWidth(0.5)
    c.rect(mm(x), pg.Y(top + sq), mm(sq), mm(sq), stroke=1, fill=0)
    pg.text(x + sq / 2, top + sq / 2 - 1, f"{sq:.1f} × {sq:.1f} mm", fb, 9, anchor="center")
    pg.text(x + sq / 2, top + sq / 2 + 3.5, "(measure between the centres of the lines)", fr, 5.5,
            "#555555", anchor="center")
    # ruler
    rx, ry = x + sq + 12.0, top + 14.0
    pg.line(rx, ry, rx + rl, ry, 0.4)
    for i in range(int(round(rl)) + 1):
        ln = 4.0 if i % 10 == 0 else (2.6 if i % 5 == 0 else 1.6)
        pg.line(rx + i, ry, rx + i, ry + ln, 0.25 if i % 5 else 0.4)
        if i % 10 == 0:
            pg.text(rx + i, ry + 7.0, f"{i // 10}", fr, 6, anchor="center")
    pg.text(rx, ry - 2.0, f"{rl:g} mm ruler (numbers in cm, ticks every 1 mm)", fr, 7)
    # key-size reference under the ruler
    kt = cfg["key_top"]
    pg.text(rx, ry + 16.0, f"Key top (measured from key.stl): {kt['width']:.2f} × "
            f"{kt['height']:.2f} mm, corner radius {kt['corner_radius']:.2f} mm", fr, 7)
    st = cfg["sticker"]
    sw, sh = kt["width"] - 2 * st["inset"], kt["height"] - 2 * st["inset"]
    pg.text(rx, ry + 20.0, f"Sticker: {sw:.2f} × {sh:.2f} mm (inset {st['inset']:.2f} mm per "
            f"side), corner radius {st['corner_radius']:.2f} mm", fr, 7)
    return top + sq


def draw_legend(pg: Page, x: float, y: float, cfg: dict, fonts: dict[str, Font]) -> None:
    col = cfg["colors"]
    fr = fonts["regular"]
    items = [("digits", col["category_background"]["number"]),
             ("operators", col["category_background"]["operator"]),
             ("functions", col["category_background"]["function"]),
             ("navigation", col["category_background"]["navigation"]),
             ("system", col["category_background"]["system"]),
             ("SHIFT", col["modifier_background"]["SHIFT"]),
             ("ALPHA", col["modifier_background"]["ALPHA"])]
    c = pg.c
    cx = x
    for name, bg in items:
        c.setFillColor(HexColor(bg)); c.setStrokeColor(HexColor("#A8A8A8")); c.setLineWidth(0.25)
        c.roundRect(mm(cx), pg.Y(y + 3.2), mm(5.0), mm(3.2), mm(0.6), stroke=1, fill=1)
        pg.text(cx + 6.2, y + 2.6, name, fr, 7, "#333333")
        cx += 6.4 + pdfmetrics.stringWidth(name, fr.name, 7) / PT_PER_MM + 4.0
    y2 = y + 7.5
    pg.text(x, y2, "Upper-left legend = SHIFT +key", fonts["bold"], 7, col["shift_legend"])
    off = pdfmetrics.stringWidth("Upper-left legend = SHIFT +key", fonts["bold"].name, 7) / PT_PER_MM
    pg.text(x + off + 5, y2, "Upper-right legend = ALPHA +key", fonts["bold"], 7, col["alpha_legend"])


def build_pdf(cfg: dict, fonts: dict[str, Font], keys: dict[str, Key], out: Path,
              date: str) -> list:
    pw, ph = cfg["page"]["size"]
    margin, gap = cfg["page"]["margin"], cfg["page"]["sticker_gap"]
    kt, st = cfg["key_top"], cfg["sticker"]
    sw, sh, r = kt["width"] - 2 * st["inset"], kt["height"] - 2 * st["inset"], st["corner_radius"]
    rows = 1 + max(k.row for k in keys.values())
    cols = 1 + max(k.col for k in keys.values())
    grid = {(k.row, k.col): k for k in keys.values()}
    fb, fr = fonts["bold"], fonts["regular"]

    c = rl_canvas.Canvas(str(out), pagesize=(mm(pw), mm(ph)), invariant=1, pageCompression=1,
                         initialFontName=fr.name, initialFontSize=8)  # no stray Helvetica
    c.setTitle("NeoCalculator keycap stickers (1:1)")
    c.setAuthor("NeoCalculator / NumOS")
    c.setSubject(f"Keycap label stickers, key top {kt['width']} x {kt['height']} mm")
    c.setCreator("hardware/keyboard/stickers/make_stickers.py")
    pg = Page(c, ph)
    report: list = []

    # ── Page 1: full sets ───────────────────────────────────────────────────
    block_w = cols * sw + (cols - 1) * gap
    block_h = rows * sh + (rows - 1) * gap
    crop = cfg["page"]["crop_mark_offset"] + cfg["page"]["crop_mark_length"]
    avail_w = pw - 2 * margin - 2 * crop
    set_gap = cfg["page"]["set_gap"]
    copies = cfg["page"]["copies_per_sheet"]
    per_row = max(1, int((avail_w + set_gap + 1e-9) // (block_w + set_gap)))
    copies_fit = min(copies, per_row)  # one row of sets above the calibration block
    if copies_fit < copies:
        print(f"WARNING: only {copies_fit} set(s) fit side by side; printing {copies_fit}.")
    total_w = copies_fit * block_w + (copies_fit - 1) * set_gap
    x_start = (pw - total_w) / 2

    pg.text(margin, margin + 4, "NeoCalculator — keycap label stickers (1:1)", fb, 12)
    pg.text(margin, margin + 9,
            f"{date}  ·  key top {kt['width']:.2f} × {kt['height']:.2f} mm (r {kt['corner_radius']:.2f}, "
            f"measured from hardware/enclosure/key.stl)  ·  sticker {sw:.2f} × {sh:.2f} mm "
            f"(inset {st['inset']:.2f} mm)  ·  {copies_fit} sets", fr, 7, "#333333")
    set_top = margin + 15 + crop + 3
    for s in range(copies_fit):
        bx = x_start + s * (block_w + set_gap)
        pg.text(bx, set_top - crop - 1.5, f"Set {chr(65 + s)}  ▲ top (display side)", fb, 7, "#555555")
        xs, ys = [], []
        for rr in range(rows):
            y = set_top + rr * (sh + gap)
            ys.append((y, y + sh))
            for cc in range(cols):
                x = bx + cc * (sw + gap)
                if rr == 0:
                    xs.append((x, x + sw))
                draw_sticker(pg, grid[(rr, cc)], x, y, sw, sh, r, cfg, fonts, report)
        draw_crop_marks(pg, xs, ys, bx, set_top, bx + block_w, set_top + block_h, cfg)
    y = set_top + block_h + crop + 6
    draw_legend(pg, margin, y, cfg, fonts)
    y += 15
    draw_calibration(pg, margin, y, cfg, fonts)
    pg.text(margin, ph - margin - 3.5, "Cut along the grey outlines; the crop marks around each set "
            "line up with whole rows and columns, so long cuts can be made with a steel ruler.",
            fr, 6.5, "#444444")
    pg.text(margin, ph - margin, "Peel and centre each sticker on its key: the inset leaves "
            f"{st['inset']:.1f} mm of bare key all round so the edges don't lift.", fr, 6.5, "#444444")
    c.showPage()

    # ── Page 2: fit test ────────────────────────────────────────────────────
    pg.text(margin, margin + 4, "Fit test — print this page first", fb, 12)
    pg.text(margin, margin + 9, f"{date}  ·  check the 50 mm square, then try the stickers on a key "
            "and pick the inset that looks best (set sticker.inset in stickers.yaml).", fr, 7, "#333333")
    y = margin + 16
    bottom = draw_calibration(pg, margin, y, cfg, fonts)
    y = bottom + 10
    # key-top outline with the default sticker inside it
    pg.text(margin, y, "Key top outline (dashed, actual key size) with the default sticker inside:",
            fb, 8)
    y += 4
    kx = margin + 2
    c.setDash(1.2, 1.0); c.setStrokeColor(HexColor("#000000")); c.setLineWidth(0.3)
    c.roundRect(mm(kx), pg.Y(y + kt["height"]), mm(kt["width"]), mm(kt["height"]),
                mm(kt["corner_radius"]), stroke=1, fill=0)
    c.setDash()
    sample = keys[cfg["fit_test"]["sample_key"]]
    draw_sticker(pg, sample, kx + st["inset"], y + st["inset"], sw, sh, r, cfg, fonts, report)
    pg.text(kx + kt["width"] + 5, y + 3.5, f"dashed = key top {kt['width']:.2f} × {kt['height']:.2f} mm, "
            f"r {kt['corner_radius']:.2f} mm (from key.stl)", fr, 7)
    pg.text(kx + kt["width"] + 5, y + 7.0, "Measure one printed key with calipers; if it differs, "
            "edit key_top in stickers.yaml.", fr, 7, "#555555")
    y += kt["height"] + 10
    pg.text(margin, y, "Test stickers per inset value (one size per row):", fb, 8)
    y += 4
    test_keys = [sample, keys["r9c3"], keys["r0c0"], keys["r2c1"]]
    for inset in cfg["fit_test"]["insets"]:
        w_i, h_i = kt["width"] - 2 * inset, kt["height"] - 2 * inset
        r_i = max(0.0, kt["corner_radius"] - inset)
        x = margin + 2
        for k in test_keys:
            draw_sticker(pg, k, x, y, w_i, h_i, r_i, cfg, fonts, report)
            x += w_i + gap
        pg.text(x + 3, y + h_i / 2 + 1.2, f"inset {inset:.1f} mm  →  {w_i:.2f} × {h_i:.2f} mm, "
                f"r {r_i:.2f} mm", fr, 7.5)
        y += max(h_i, 6) + gap + 2
    c.showPage()
    c.save()
    return report


# ═════════════════════════════════════════════════════════════════════════════
# --measure : re-derive the key-top size from key.stl (numpy only)
# ═════════════════════════════════════════════════════════════════════════════

def load_stl(path: Path):
    import numpy as np
    data = path.read_bytes()
    if data[:5] == b"solid" and b"facet" in data[:400]:
        v = re.findall(rb"vertex\s+(\S+)\s+(\S+)\s+(\S+)", data)
        return np.array(v, dtype=float).reshape(-1, 3, 3)
    n = struct.unpack("<I", data[80:84])[0]
    dt = np.dtype([("n", "<3f4"), ("v", "<9f4"), ("a", "<u2")])
    return np.frombuffer(data, dt, count=n, offset=84)["v"].reshape(-1, 3, 3).astype(float)


def measure_key_top(path: Path) -> dict:
    import numpy as np
    tri = load_stl(path)
    nrm = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    nrm /= np.linalg.norm(nrm, axis=1)[:, None]
    zmax = tri[:, :, 2].max()
    top = tri[(nrm[:, 2] > 0.999) & (np.abs(tri[:, :, 2] - zmax).max(axis=1) < 1e-3)]
    pts = top.reshape(-1, 3)[:, :2]
    q = np.round(pts, 4)
    uniq, inv = np.unique(q, axis=0, return_inverse=True)
    inv = inv.reshape(-1, 3)
    edges = np.sort(np.concatenate([inv[:, [0, 1]], inv[:, [1, 2]], inv[:, [2, 0]]]), axis=1)
    e, cnt = np.unique(edges, axis=0, return_counts=True)
    bpts = uniq[np.unique(e[cnt == 1])]
    xmin, ymin = bpts.min(0); xmax, ymax = bpts.max(0)
    w, h = xmax - xmin, ymax - ymin
    cx, cy = (xmin + xmax) / 2, (ymin + ymax) / 2
    rel = np.abs(bpts - [cx, cy])
    arc = rel[(rel[:, 0] < w / 2 - 1e-3) & (rel[:, 1] < h / 2 - 1e-3)]
    best = (None, math.inf)
    for r in np.arange(0.05, min(w, h) / 2, 0.005):
        if len(arc) == 0:
            break
        err = np.abs(np.hypot(arc[:, 0] - (w / 2 - r), arc[:, 1] - (h / 2 - r)) - r).max()
        if err < best[1]:
            best = (r, err)
    return {"width": float(w), "height": float(h), "corner_radius": best[0],
            "fit_error": best[1], "z": float(zmax), "arc_points": int(len(arc))}


# ═════════════════════════════════════════════════════════════════════════════
# --verify : check the PDF and render previews (PyMuPDF)
# ═════════════════════════════════════════════════════════════════════════════

def verify(cfg: dict, pdf: Path, keys: dict[str, Key]) -> bool:
    try:
        import pymupdf as fitz
    except ImportError:
        try:
            import fitz  # type: ignore
        except ImportError:
            print("verify: PyMuPDF not installed (pip install pymupdf); skipping.")
            return True
    ok = True

    def fail(msg):
        nonlocal ok
        ok = False
        print("  FAIL:", msg)

    doc = fitz.open(str(pdf))
    pw, ph = cfg["page"]["size"]
    kt, st = cfg["key_top"], cfg["sticker"]
    sw, sh = kt["width"] - 2 * st["inset"], kt["height"] - 2 * st["inset"]
    bgs = {HexColor(v).rgb() for v in cfg["colors"]["category_background"].values()}
    bgs |= {HexColor(v).rgb() for v in cfg["colors"]["modifier_background"].values()}
    tol = 0.01  # mm
    print(f"verify: {pdf.name}, {len(doc)} pages")
    for pno, page in enumerate(doc):
        w_mm, h_mm = page.rect.width / PT_PER_MM, page.rect.height / PT_PER_MM
        print(f"  page {pno + 1}: {w_mm:.3f} × {h_mm:.3f} mm")
        if abs(w_mm - pw) > tol or abs(h_mm - ph) > tol:
            fail(f"page {pno + 1} is not {pw} × {ph} mm")
        stickers = []
        squares = []
        legend_swatch_h = 3.2
        for d in page.get_drawings():
            rct = d["rect"]
            wm, hm = rct.width / PT_PER_MM, rct.height / PT_PER_MM
            fill = d.get("fill")
            if fill is not None and any(all(abs(a - b) < 0.004 for a, b in zip(fill, bg)) for bg in bgs):
                if abs(hm - legend_swatch_h) < 0.05 and abs(wm - 5.0) < 0.05:
                    continue  # colour legend swatch
                stickers.append((rct, wm, hm))
            elif fill is None and abs(wm - cfg["calibration"]["square"]) < 0.5:
                squares.append((wm, hm))
        if not squares:
            fail(f"page {pno + 1}: calibration square not found")
        for wm, hm in squares:
            print(f"    calibration square: {wm:.3f} × {hm:.3f} mm (path box incl. no stroke)")
            if abs(wm - cfg["calibration"]["square"]) > tol or abs(hm - cfg["calibration"]["square"]) > tol:
                fail("calibration square size off")
        if pno == 0:
            n_exp = 50 * cfg["page"]["copies_per_sheet"]
            print(f"    stickers found: {len(stickers)} (expected {n_exp})")
            if len(stickers) != n_exp:
                fail("sticker count")
            dw = max(abs(wm - sw) for _, wm, _ in stickers)
            dh = max(abs(hm - sh) for _, _, hm in stickers)
            print(f"    sticker size: expected {sw:.3f} × {sh:.3f} mm, max deviation "
                  f"{dw:.4f} / {dh:.4f} mm")
            if dw > tol or dh > tol:
                fail("sticker size deviates from config")
            # overlaps / gaps
            mg = math.inf
            rects = [s[0] for s in stickers]
            for i in range(len(rects)):
                for j in range(i + 1, len(rects)):
                    a, b = rects[i], rects[j]
                    if a.intersects(b) and (a & b).get_area() > 0:
                        fail("stickers overlap")
                    dx = max(b.x0 - a.x1, a.x0 - b.x1) / PT_PER_MM
                    dy = max(b.y0 - a.y1, a.y0 - b.y1) / PT_PER_MM
                    mg = min(mg, max(dx, dy))
            print(f"    minimum gap between stickers: {mg:.3f} mm")
            if mg < cfg["page"]["sticker_gap"] - tol:
                fail("gap smaller than configured")
            # margins
            allr = fitz.Rect()
            for d in page.get_drawings():
                allr |= d["rect"]
            for b in page.get_text("dict")["blocks"]:
                allr |= fitz.Rect(b["bbox"])
            m = min(allr.x0, allr.y0, page.rect.width - allr.x1, page.rect.height - allr.y1) / PT_PER_MM
            print(f"    smallest page margin to any content: {m:.2f} mm")
            if m < 10.0 - tol:
                fail("content closer than 10 mm to the page edge")
            # text inside stickers
            inside = 0
            for b in page.get_text("dict")["blocks"]:
                for ln in b.get("lines", []):
                    for sp in ln["spans"]:
                        o = fitz.Point(sp["origin"])
                        host = [r for r in rects if r.contains(o)]
                        if host:
                            inside += 1
                            bb = fitz.Rect(sp["bbox"])
                            if bb.x0 < host[0].x0 - 0.1 or bb.x1 > host[0].x1 + 0.1:
                                fail(f"text {sp['text']!r} spills out of its sticker horizontally")
            print(f"    text spans inside stickers: {inside}")
        else:
            sizes = sorted({(round(wm, 2), round(hm, 2)) for _, wm, hm in stickers})
            print(f"    fit-test sticker sizes (mm): {sizes}")
            exp = sorted({(round(kt['width'] - 2 * i, 2), round(kt['height'] - 2 * i, 2))
                          for i in cfg["fit_test"]["insets"]} | {(round(sw, 2), round(sh, 2))})
            if sizes != exp:
                fail(f"fit-test sizes {sizes} != expected {exp}")
        fonts = page.get_fonts()
        for f in fonts:
            if f[1] in ("n/a", "") or "Type3" in f[2]:
                fail(f"font not embedded: {f}")
        print(f"    fonts: {sorted({f[3] for f in fonts})} (all embedded TrueType subsets)")
        # previews
        dpi = cfg["output"]["preview_dpi"]
        png = pdf.parent / f"{cfg['output']['preview_prefix']}-p{pno + 1}.png"
        page.get_pixmap(dpi=dpi).save(str(png))
        print(f"    preview: {png.name} ({dpi} dpi)")
    print("verify:", "OK" if ok else "FAILED")
    return ok


# ═════════════════════════════════════════════════════════════════════════════

def label_table(keys: dict[str, Key]) -> str:
    def esc(s):
        return s.replace("|", "\\|") if s else "—"
    lines = ["| Key | Row,Col | Group | Primary | SHIFT (upper-left) | ALPHA (upper-right) |",
             "|:--|:--|:--|:--|:--|:--|"]
    for k in sorted(keys.values(), key=lambda k: (k.row, k.col)):
        lines.append(f"| {k.id} | {k.row},{k.col} | {k.category} | `{esc(k.primary)}` | "
                     f"`{esc(k.shift)}` | `{esc(k.alpha)}` |")
    return "\n".join(lines).replace("`—`", "—")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", default=str(HERE / "stickers.yaml"))
    ap.add_argument("--out", default=None, help="output PDF (default: next to this script)")
    ap.add_argument("--date", default=None, help="date printed in the title (default: today)")
    ap.add_argument("--verify", action="store_true", help="check the PDF and write PNG previews")
    ap.add_argument("--measure", action="store_true", help="measure the key top from key.stl")
    ap.add_argument("--labels", action="store_true", help="print the legend table (Markdown)")
    args = ap.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    cfg = load_config(Path(args.config))

    if args.measure:
        m = measure_key_top(REPO / cfg["sources"]["key_stl"])
        kt = cfg["key_top"]
        print(f"key.stl flat top face at z={m['z']:.3f} mm: {m['width']:.3f} × {m['height']:.3f} mm, "
              f"corner radius {m['corner_radius']:.3f} mm (fit error {m['fit_error']:.4f} mm, "
              f"{m['arc_points']} arc points)")
        print(f"stickers.yaml key_top: {kt['width']} × {kt['height']} mm, r {kt['corner_radius']}")
        bad = (abs(m["width"] - kt["width"]) > 0.02 or abs(m["height"] - kt["height"]) > 0.02
               or abs(m["corner_radius"] - kt["corner_radius"]) > 0.05)
        print("MISMATCH — update key_top in stickers.yaml" if bad else "config matches the STL")
        return 1 if bad else 0

    keys = load_keys(cfg)
    if args.labels:
        print(label_table(keys))
        return 0

    fonts = load_fonts(cfg)
    # every character of every printed legend must exist in the embedded fonts
    missing = set()
    for k in keys.values():
        for plane in PLANES:
            txt = getattr(k, plane)
            if re.search(r"\\[A-Za-z,]|\^", txt):   # math label: check only the literal text
                txt = re.sub(r"\\[A-Za-z]+|\\.|[{}^\[\]]", "", txt).replace("-", "−")
            for ch in txt:
                for f in fonts.values():
                    if not f.has(ch):
                        missing.add((ch, f.path.name))
    if missing:
        print("ERROR: glyphs missing from the fonts:", sorted(missing))
        return 2

    mismatches, unhandled = firmware_audit(cfg, keys)
    print(f"firmware cross-check: {len(keys)} keys, {len(mismatches)} mismatches")
    for m in mismatches:
        print("  MISMATCH:", m)
    if unhandled:
        print("legends printed but with no firmware handler found (check before trusting them):")
        for u in unhandled:
            print("  ", u)
    hidden = [(k.id, p, v) for k in keys.values() for p, v in k.hidden.items()]
    if hidden:
        print("secondary legends defined but not printed (layout printLegend: false):")
        for kid, p, v in sorted(hidden):
            print(f"   {kid} {p} {v!r}")

    out = Path(args.out) if args.out else HERE / cfg["output"]["pdf"]
    date = args.date or _dt.date.today().isoformat()
    try:
        report = build_pdf(cfg, fonts, keys, out, date)
    except (LayoutError, MissingGlyph) as e:
        print("ERROR:", e)
        return 2
    sizes = sorted({lg[0] for r in report for lg in r["legends"]})
    condensed = sorted({(r["key"], lg[1]) for r in report for lg in r["legends"] if lg[1] < 0.999})
    print(f"wrote {out} ({len(report)} stickers drawn); legend sizes used: {sizes[0]}–{sizes[-1]} pt")
    if condensed:
        print("condensed legends (key, horizontal scale):", sorted(set(condensed)))
    if mismatches:
        return 3
    if args.verify:
        return 0 if verify(cfg, out, keys) else 4
    return 0


if __name__ == "__main__":
    sys.exit(main())
