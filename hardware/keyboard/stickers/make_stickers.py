#!/usr/bin/env python3
"""
NeoCalculator keycap label stickers: print-ready vector PDF at true 1:1 scale.

    python hardware/keyboard/stickers/make_stickers.py            # sheet + fit test
    python hardware/keyboard/stickers/make_stickers.py --verify   # ... + checks + PNG previews
    python hardware/keyboard/stickers/make_stickers.py --measure  # re-derive key size from key.stl
    python hardware/keyboard/stickers/make_stickers.py --labels   # legend table (Markdown)

Requirements: Python 3.9+, reportlab, PyYAML, fontTools, uharfbuzz, numpy.
Optional: PyMuPDF (pip install pymupdf) for --verify and the PNG previews.

Typography: every legend is shaped with HarfBuzz (kerning, case-sensitive forms)
from Noto Sans (variable weight/width) and converted to vector outlines, so the
PDF prints identically everywhere. Width is narrowed on the font's own width axis
(never by stretching). Math legends are typeset: italic variables, real
superscripts, fractions, radicals and placeholder boxes drawn at the stem weight
of the surrounding text. Legend sizes come from per-role cap heights
(stickers.yaml), shared across a role so keys read evenly.
"""

from __future__ import annotations

import argparse
import colorsys
import datetime as _dt
import json
import math
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

import uharfbuzz as hb
import yaml
from fontTools.pens.basePen import BasePen
from fontTools.pens.boundsPen import BoundsPen
from fontTools.ttLib import TTFont
from reportlab.lib.colors import HexColor
from reportlab.pdfgen import canvas as rl_canvas

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
PT_PER_MM = 72.0 / 25.4
PLANES = ("primary", "shift", "alpha")
FILL_NON_ZERO = 1

JSON_TO_FIRMWARE_SEMANTIC = {"delete": "key_delete", "enter_deep_sleep": "deep_sleep_off"}
SUPERSCRIPTS = {"ˣ": "x", "⁻": "−", "¹": "1", "²": "2", "³": "3", "ⁿ": "n", "⁰": "0"}


def mm(v: float) -> float:
    return v * PT_PER_MM


class LayoutError(Exception):
    pass


class MissingGlyph(Exception):
    pass


# ═════════════════════════════════════════════════════════════════════════════
# Fonts: HarfBuzz shaping + fontTools outlines (variable-font aware)
# ═════════════════════════════════════════════════════════════════════════════

class Face:
    def __init__(self, path: Path):
        self.path = path
        self.ft = TTFont(str(path))
        self.cmap = self.ft.getBestCmap()
        self.upm = self.ft["head"].unitsPerEm
        self.order = self.ft.getGlyphOrder()
        self.axes = ({a.axisTag: (a.minValue, a.defaultValue, a.maxValue) for a in self.ft["fvar"].axes}
                     if "fvar" in self.ft else {})
        self.hbface = hb.Face(hb.Blob.from_file_path(str(path)))
        os2 = self.ft["OS/2"]
        self.cap, self.xh = os2.sCapHeight, os2.sxHeight
        self._gs, self._hb, self._bounds, self._stem = {}, {}, {}, {}

    def loc(self, weight: float, width: float) -> tuple:
        out = []
        for tag, val in (("wght", weight), ("wdth", width)):
            if tag in self.axes:
                lo, _, hi = self.axes[tag]
                out.append((tag, float(min(max(val, lo), hi))))
        return tuple(out)

    def glyphset(self, loc: tuple):
        if loc not in self._gs:
            self._gs[loc] = self.ft.getGlyphSet(location=dict(loc)) if loc else self.ft.getGlyphSet()
        return self._gs[loc]

    def hbfont(self, loc: tuple):
        if loc not in self._hb:
            f = hb.Font(self.hbface)
            if loc:
                f.set_variations(dict(loc))
            self._hb[loc] = f
        return self._hb[loc]

    def bounds(self, name: str, loc: tuple):
        k = (name, loc)
        if k not in self._bounds:
            pen = BoundsPen(self.glyphset(loc))
            self.glyphset(loc)[name].draw(pen)
            self._bounds[k] = pen.bounds
        return self._bounds[k]

    def stem(self, loc: tuple) -> float:
        """Vertical stem width (font units) measured on 'l'."""
        if loc not in self._stem:
            b = self.bounds(self.cmap[ord("l")], loc)
            self._stem[loc] = b[2] - b[0]
        return self._stem[loc]


@dataclass(frozen=True)
class Style:
    weight: float
    width: float
    italic: bool = False
    case: bool = False


class Fonts:
    def __init__(self, cfg: dict):
        f = cfg["fonts"]
        self.upright = Face(HERE / f["upright"])
        self.italic = Face(HERE / f["italic"])
        self.math = Face(HERE / f["math"])
        self.cap_ratio = self.upright.cap / self.upright.upm
        self.axis_ratio = self.math.ft["MATH"].table.MathConstants.AxisHeight.Value / self.math.upm

    def chain(self, st: Style):
        primary = self.italic if st.italic else self.upright
        loc = primary.loc(st.weight, st.width)
        out = [(primary, loc, 0.0)]
        if st.italic:
            out.append((self.upright, self.upright.loc(st.weight, st.width), 0.0))
        # Noto Sans Math has one weight: embolden its glyphs to the text stem.
        target = primary.stem(loc) * self.math.upm / primary.upm
        out.append((self.math, (), max(0.0, target - self.math.stem(()))))
        return out

    def size_for_cap(self, cap_mm: float) -> float:
        return mm(cap_mm) / self.cap_ratio

    def rule(self, st: Style, size: float) -> float:
        """Line weight for drawn math (fraction bars, boxes, radicals) in pt."""
        loc = self.upright.loc(st.weight, st.width)
        return 0.62 * self.upright.stem(loc) / self.upright.upm * size


class _CanvasPen(BasePen):
    def __init__(self, glyphset, path):
        super().__init__(glyphset)
        self.p = path

    def _moveTo(self, pt):
        self.p.moveTo(*pt)

    def _lineTo(self, pt):
        self.p.lineTo(*pt)

    def _curveToOne(self, a, b, c):
        self.p.curveTo(a[0], a[1], b[0], b[1], c[0], c[1])

    def _closePath(self):
        self.p.close()


# ═════════════════════════════════════════════════════════════════════════════
# Layout boxes (points, baseline origin, y up)
# ═════════════════════════════════════════════════════════════════════════════

class Node:
    w = 0.0
    x0 = 0.0
    x1 = 0.0
    asc = 0.0
    desc = 0.0
    italic_tail = 0.0

    def draw(self, c, x, y):
        raise NotImplementedError


@dataclass
class _G:
    face: Face
    loc: tuple
    name: str
    x: float
    y: float
    scale: float
    emb: float


class Run(Node):
    """A shaped string, possibly spanning several fonts of the fallback chain."""

    def __init__(self, text: str, st: Style, size: float, fonts: Fonts):
        chain = fonts.chain(st)
        segs: list[list] = []
        for ch in text:
            for i, (face, _, _) in enumerate(chain):
                if ord(ch) in face.cmap:
                    break
            else:
                raise MissingGlyph(f"U+{ord(ch):04X} {ch!r} not in any font")
            if segs and segs[-1][0] == i:
                segs[-1][1] += ch
            else:
                segs.append([i, ch])
        feats = {"kern": True}
        if st.case:
            feats["case"] = True
        x = 0.0
        self.glyphs: list[_G] = []
        for i, seg in segs:
            face, loc, emb = chain[i]
            buf = hb.Buffer()
            buf.add_str(seg)
            buf.guess_segment_properties()
            hb.shape(face.hbfont(loc), buf, feats)
            s = size / face.upm
            for info, pos in zip(buf.glyph_infos, buf.glyph_positions):
                if info.codepoint == 0:
                    raise MissingGlyph(f"{seg!r} shaped to .notdef in {face.path.name}")
                self.glyphs.append(_G(face, loc, face.order[info.codepoint],
                                      x + pos.x_offset * s, pos.y_offset * s, s, emb))
                x += pos.x_advance * s
        self.w = x
        x0 = y0 = math.inf
        x1 = y1 = -math.inf
        for g in self.glyphs:
            b = g.face.bounds(g.name, g.loc)
            if not b:
                continue
            e = g.emb / 2 * g.scale
            x0 = min(x0, g.x + b[0] * g.scale - e)
            x1 = max(x1, g.x + b[2] * g.scale + e)
            y0 = min(y0, g.y + b[1] * g.scale - e)
            y1 = max(y1, g.y + b[3] * g.scale + e)
        if x0 == math.inf:
            x0, x1, y0, y1 = 0.0, x, 0.0, 0.0
        self.x0, self.x1, self.asc, self.desc = x0, x1, y1, -y0
        self.italic_tail = 0.06 * size if st.italic else 0.0

    def draw(self, c, x, y):
        for g in self.glyphs:
            gs = g.face.glyphset(g.loc)
            c.saveState()
            c.translate(x + g.x, y + g.y)
            c.scale(g.scale, g.scale)
            path = c.beginPath()
            gs[g.name].draw(_CanvasPen(gs, path))
            if g.emb > 0:
                c.setLineWidth(g.emb)
                c.setLineJoin(1)
                c.drawPath(path, stroke=1, fill=1, fillMode=FILL_NON_ZERO)
            else:
                c.drawPath(path, stroke=0, fill=1, fillMode=FILL_NON_ZERO)
            c.restoreState()


class Glue(Node):
    def __init__(self, w):
        self.w, self.x0, self.x1 = w, 0.0, 0.0

    def draw(self, c, x, y):
        pass


class Slot(Node):
    """Empty-slot placeholder box, drawn at the text's stem weight."""

    def __init__(self, size: float, rule: float):
        self.side = 0.57 * size
        self.lw = rule * 0.9
        self.m = 0.05 * size
        self.w = self.side + 2 * self.m
        self.x0, self.x1 = self.m, self.m + self.side
        self.asc, self.desc = self.side, 0.0

    def draw(self, c, x, y):
        h = self.lw / 2
        c.setLineWidth(self.lw)
        c.setLineJoin(0)
        c.rect(x + self.m + h, y + h, self.side - self.lw, self.side - self.lw, stroke=1, fill=0)


class Row(Node):
    def __init__(self, items: list[Node]):
        self.items = [i for i in items if i is not None]
        x = 0.0
        self.x0, self.x1 = math.inf, -math.inf
        self.asc = self.desc = 0.0
        for it in self.items:
            if not isinstance(it, Glue):
                self.x0 = min(self.x0, x + it.x0)
                self.x1 = max(self.x1, x + it.x1)
            self.asc = max(self.asc, it.asc)
            self.desc = max(self.desc, it.desc)
            x += it.w
        self.w = x
        if self.x0 == math.inf:
            self.x0, self.x1 = 0.0, x
        self.italic_tail = self.items[-1].italic_tail if self.items else 0.0

    def draw(self, c, x, y):
        for it in self.items:
            it.draw(c, x, y)
            x += it.w


class Sup(Node):
    def __init__(self, base: Node, exp: Node, size: float):
        self.base, self.exp = base, exp
        self.shift = 0.39 * size
        # ink-to-ink spacing: exponent starts a fixed distance after the base's ink
        self.dx = max(base.x1 + 0.05 * size - exp.x0, base.w * 0.6)
        self.w = self.dx + exp.w
        self.x0 = min(base.x0, self.dx + exp.x0)
        self.x1 = max(base.x1, self.dx + exp.x1)
        self.asc = max(base.asc, self.shift + exp.asc)
        self.desc = max(base.desc, exp.desc - self.shift)

    def draw(self, c, x, y):
        self.base.draw(c, x, y)
        self.exp.draw(c, x + self.dx, y + self.shift)


class Frac(Node):
    def __init__(self, num: Node, den: Node, size: float, rule: float, axis: float):
        self.num, self.den, self.t = num, den, rule
        self.axis = axis
        gap = 0.11 * size
        self.over = 0.07 * size
        inner = max(num.x1 - num.x0, den.x1 - den.x0)
        self.w = inner + 2 * self.over + 0.06 * size
        self.pad = 0.03 * size
        self.x0, self.x1 = self.pad, self.w - self.pad
        self.nb = axis + rule / 2 + gap + num.desc
        self.db = axis - rule / 2 - gap - den.asc
        self.asc = self.nb + num.asc
        self.desc = max(0.0, den.desc - self.db)

    def draw(self, c, x, y):
        c.setLineWidth(self.t)
        c.setLineCap(0)
        c.line(x + self.pad, y + self.axis, x + self.w - self.pad, y + self.axis)
        for n, b in ((self.num, self.nb), (self.den, self.db)):
            n.draw(c, x + (self.w - (n.x1 - n.x0)) / 2 - n.x0, y + b)


class Radical(Node):
    def __init__(self, body: Node, index: Node | None, size: float, rule: float):
        self.body, self.index, self.size = body, index, size
        self.t = rule * 1.05
        s = size
        self.top = max(body.asc, 0.60 * s) + 0.13 * s
        self.bottom = -max(body.desc, 0.0)
        self.sign = 0.50 * s
        self.lead = 0.0 if index is None else max(0.0, (index.x1 - index.x0) - 0.20 * s)
        self.bx = self.lead + self.sign + 0.05 * s
        self.w = self.bx + max(body.w, 0.25 * s) + 0.06 * s
        self.x0, self.x1 = 0.0, self.w
        self.asc = self.top + self.t / 2
        self.desc = -self.bottom + self.t * 0.8
        if index is not None:
            self.ib = 0.46 * self.top + index.desc
            self.asc = max(self.asc, self.ib + index.asc)

    def draw(self, c, x, y):
        s, L = self.size, x + self.lead
        p = c.beginPath()
        p.moveTo(L + 0.01 * s, y + 0.40 * self.top)
        p.lineTo(L + 0.11 * s, y + 0.47 * self.top)
        p.lineTo(L + 0.26 * s, y + self.bottom)
        p.lineTo(L + self.sign, y + self.top)
        p.lineTo(x + self.w, y + self.top)
        c.setLineWidth(self.t)
        c.setLineJoin(0)
        c.setMiterLimit(3)
        c.setLineCap(0)
        c.drawPath(p, stroke=1, fill=0)
        self.body.draw(c, x + self.bx, y)
        if self.index is not None:
            self.index.draw(c, x - self.index.x0, y + self.ib)


# ═════════════════════════════════════════════════════════════════════════════
# Legend typesetter: Unicode text + the LaTeX subset used by the layout JSON
# (\frac \sqrt[..]{..} \square \lvert \rvert ^{..} \,)
# ═════════════════════════════════════════════════════════════════════════════

_CMD_TEXT = {"times": "×", "div": "÷", "pi": "π", "infty": "∞", "circ": "°", "lvert": "|",
             "rvert": "|", "theta": "θ", "leq": "≤", "geq": "≥", "neq": "≠"}


def normalise(label: str, cfg: dict) -> str:
    """Typographic normalisation: true minus, Unicode superscripts → real superscripts."""
    if cfg["typography"].get("true_minus", True):
        label = label.replace("-", "−")
    return re.sub("[" + "".join(SUPERSCRIPTS) + "]+",
                  lambda m: "^{" + "".join(SUPERSCRIPTS[ch] for ch in m.group(0)) + "}", label)


def is_plain(label: str) -> bool:
    return not re.search(r"\\[A-Za-z,]|\^", label)


def typeset(src: str, base: Style, size: float, fonts: Fonts, cfg: dict) -> Node:
    variables = cfg["typography"]["italic_variables"]
    var_list = bool(re.fullmatch(r"[a-zθ](,[a-zθ])+", src))
    math_mode = not is_plain(src) or var_list

    def text_nodes(s: str, sz: float) -> list[Node]:
        out: list[Node] = []
        for m in re.finditer(r"[A-Za-zθ]+|[^A-Za-zθ]+", s):
            t = m.group(0)
            italic = len(t) == 1 and t in variables
            out.append(Run(t, Style(base.weight, base.width, italic, base.case), sz, fonts))
            if var_list and t == ",":
                out.append(Glue(0.10 * sz))
        return out

    if not math_mode:
        nodes = text_nodes(src, size)
        return nodes[0] if len(nodes) == 1 else Row(nodes)

    pos = 0

    def parse_until(end, sz) -> Node:
        nonlocal pos
        items: list[Node] = []
        buf = ""

        def flush():
            nonlocal buf
            if buf:
                items.extend(text_nodes(buf, sz))
                buf = ""

        def arg(asz) -> Node:
            nonlocal pos
            if src[pos] == "{":
                pos += 1
                return parse_until("}", asz)
            if src[pos] == "\\":
                return command(asz)
            ch = src[pos]
            pos += 1
            return Row(text_nodes(ch, asz))

        def command(csz) -> Node:
            nonlocal pos
            pos += 1
            m = re.match(r"[A-Za-z]+|.", src[pos:])
            if m is None:
                return Run("\\", base, csz, fonts)
            name = m.group(0)
            pos += len(name)
            r = fonts.rule(base, csz)
            if name == "frac":
                a, b = arg(csz * 0.80), arg(csz * 0.80)
                return Frac(a, b, csz, r, fonts.axis_ratio * csz)
            if name == "sqrt":
                idx = None
                if pos < len(src) and src[pos] == "[":
                    pos += 1
                    idx = parse_until("]", csz * 0.55)
                return Radical(arg(csz), idx, csz, r)
            if name == "square":
                return Slot(csz, r)
            if name == ",":
                return Glue(0.17 * csz)
            if name == "\\":
                return Run("\\", base, csz, fonts)
            if name in _CMD_TEXT:
                return Row(text_nodes(_CMD_TEXT[name], csz))
            raise ValueError(f"unsupported command \\{name} in {src!r}")

        while pos < len(src):
            ch = src[pos]
            if end is not None and ch == end:
                pos += 1
                break
            if ch == "\\":
                flush()
                items.append(command(sz))
            elif ch == "^":
                pos += 1
                if buf:
                    # base = the trailing word ("sin" in sin⁻¹) or the last character
                    m = re.search(r"[A-Za-zθ]+$", buf)
                    last = m.group(0) if m else buf[-1]
                    buf = buf[: len(buf) - len(last)]
                    flush()
                    b = Row(text_nodes(last, sz))
                elif items:
                    b = items.pop()
                else:
                    raise ValueError(f"superscript without base in {src!r}")
                items.append(Sup(b, arg(sz * 0.66), sz))
            elif ch == "{":
                flush()
                pos += 1
                items.append(parse_until("}", sz))
            else:
                buf += ch
                pos += 1
        flush()
        return items[0] if len(items) == 1 else Row(items)

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
    raw: dict = field(default_factory=dict)
    hidden: dict = field(default_factory=dict)


def load_keys(cfg: dict) -> dict[str, Key]:
    layout = json.loads((REPO / cfg["sources"]["layout_json"]).read_text(encoding="utf-8"))
    keys: dict[str, Key] = {}
    for k in layout["keys"]:
        p = k["planes"]
        sem, labels, hidden = {}, {}, {}
        for name in PLANES:
            d = p[name]
            src = p[d.get("inheritFrom", "primary")] if d.get("kind") == "inherit" else d
            s = src.get("semanticId") or src.get("action")
            sem[name] = JSON_TO_FIRMWARE_SEMANTIC.get(s, s)
            lab = d.get("label", "") if d.get("kind") != "inherit" else ""
            if name == "primary" or d.get("printLegend") or cfg.get("print_hidden_legends"):
                labels[name] = lab
            else:
                labels[name] = ""
                if lab:
                    hidden[name] = lab
        key = Key(k["id"], k["visual"]["row"], k["visual"]["col"], k["category"],
                  labels["primary"], labels["shift"], labels["alpha"], sem, dict(labels), hidden)
        for name, val in (cfg.get("label_overrides") or {}).get(key.id, {}).items():
            setattr(key, name, val)
        keys[key.id] = key
    return keys


_MAP_RE = re.compile(
    r"\{(\d+), (\d+), (\d+), (\d+), (\d+), (-?\d+), (-?\d+), (-?\d+), SemanticId::(\w+), "
    r"KeyCode::(\w+), \"((?:[^\"\\]|\\.)*)\", (true|false)\}")
_DEF_RE = re.compile(r"\{SemanticId::(\w+), KeyCode::(\w+), \"((?:[^\"\\]|\\.)*)\", (true|false)\}")


def firmware_audit(cfg: dict, keys: dict[str, Key]):
    text = (REPO / cfg["sources"]["firmware_keymap"]).read_text(encoding="utf-8")
    entries = [m.groups() for m in _MAP_RE.finditer(text)]
    block = text.split("kMathPlaneDefinitions", 1)[1].split("}};", 1)[0]
    defs = [_DEF_RE.findall(line) for line in block.splitlines() if "{{{" in line]
    if len(entries) != 50 or len(defs) != 50:
        return [f"could not parse firmware keymap ({len(entries)} keys, {len(defs)} rows)"], []
    referenced: set[str] = set()
    src_root = REPO / cfg["sources"]["firmware_src"]
    scan = src_root.is_dir() and any(src_root.rglob("*.cpp"))
    if scan:
        for path in src_root.rglob("*"):
            if path.suffix in (".cpp", ".h", ".hpp", ".inc") and "generated" not in path.parts:
                try:
                    referenced.update(re.findall(r"SemanticId::(\w+)", path.read_text(errors="ignore")))
                except OSError:
                    pass
    by_visual = {(int(e[3]), int(e[4])): (i, e) for i, e in enumerate(entries)}
    mismatches, unhandled = [], []
    overrides = cfg.get("label_overrides") or {}
    for key in keys.values():
        i, e = by_visual[(key.row, key.col)]
        fw_label = re.sub(r"\\(.)", r"\1", e[10])
        if fw_label != key.raw["primary"] and key.id not in overrides:
            mismatches.append(f"{key.id}: layout {key.raw['primary']!r} != firmware {fw_label!r}")
        for pi, plane in enumerate(PLANES):
            fw_sem, fw_code, fw_text, _ = defs[i][pi]
            if fw_sem != key.semantics[plane]:
                mismatches.append(f"{key.id} {plane}: layout {key.semantics[plane]} != firmware {fw_sem}")
            label = getattr(key, plane)
            if scan and label and key.category != "modifier" and fw_code == "NONE" \
                    and fw_text == "" and fw_sem not in referenced:
                unhandled.append(f"{key.id} {plane} {label!r} → SemanticId::{fw_sem} "
                                 "(KeyCode::NONE, no insert text, no handler in src/)")
    return mismatches, unhandled


# ═════════════════════════════════════════════════════════════════════════════
# Sticker layout
# ═════════════════════════════════════════════════════════════════════════════

def role_of(key: Key, cfg: dict) -> str:
    forced = (cfg["typography"].get("role_overrides") or {}).get(key.id)
    if forced:
        return forced
    lab = key.primary
    if re.fullmatch(r"[0-9]", lab):
        return "digit"
    if not is_plain(lab) or re.search("[" + "".join(SUPERSCRIPTS) + "]", lab) \
            or re.fullmatch(r"[a-zθ](,[a-zθ])+", lab):
        return "math"
    if re.fullmatch(r"[a-z]{2,}", lab):
        return "func"
    if re.fullmatch(r"[A-Z]{2,3}", lab):
        return "short"
    if re.fullmatch(r"[A-Z]{2,}\W?", lab):
        return "word"
    if lab in ("+", "-", "−", "×", "÷", "=", "<", ">"):
        return "op"
    if lab in (",", ".", ";", ":"):
        return "punct"
    return "glyph"


def _inside_rounded(px, py, w, h, r, margin) -> bool:
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
    x: float      # pen x, mm from sticker left
    y: float      # baseline, mm from sticker top
    color: str
    size: float
    width: float

    def ink(self):
        n = self.node
        return (self.x + n.x0 / PT_PER_MM, self.y - n.asc / PT_PER_MM,
                self.x + n.x1 / PT_PER_MM, self.y + n.desc / PT_PER_MM)


def _luma(hexcol: str) -> float:
    c = HexColor(hexcol)
    return 0.2126 * c.red + 0.7152 * c.green + 0.0722 * c.blue


def background(key: Key, cfg: dict) -> str:
    col = cfg["colors"]
    return (col.get("key_background") or {}).get(key.primary) or col["background"].get(key.category, "#FFFFFF")


def darken(hexcol: str, amount: float) -> str:
    c = HexColor(hexcol)
    h, l, s = colorsys.rgb_to_hls(c.red, c.green, c.blue)
    r, g, b = colorsys.hls_to_rgb(h, max(0.0, l * (1 - amount)), min(1.0, s * 1.1))
    return "#%02X%02X%02X" % (round(r * 255), round(g * 255), round(b * 255))


class StickerLayout:
    """Lays out all keys for one sticker size, with role-uniform type sizes."""

    def __init__(self, keys: dict[str, Key], w: float, h: float, r: float, cfg: dict, fonts: Fonts):
        self.keys, self.w, self.h, self.r, self.cfg, self.fonts = keys, w, h, r, cfg, fonts
        st, ty = cfg["sticker"], cfg["typography"]
        self.pad, self.clear = st["edge_padding"], st["corner_clearance"]
        self.steps = ty["width_steps"]
        sec = ty["secondary"]
        self.sec_size = fonts.size_for_cap(sec["cap"])
        # Secondary band: a cap-height line hanging from the top padding.
        self.band_base = self.pad + sec["cap"]
        self.side_pad = self.pad
        while not _inside_rounded(self.side_pad, self.pad, w, h, r, self.clear):
            self.side_pad += 0.01
        self.primary_top = self.band_base + 0.58
        self.sizes = self._role_sizes()

    def _area(self, key: Key):
        top = self.primary_top if (key.shift or key.alpha) else self.pad
        return top, self.h - self.pad

    def _place_xy(self, key: Key, node: Node, role: str, size: float):
        rc = self.cfg["typography"]["roles"][role]
        top, bot = self._area(key)
        cy = (top + bot) / 2
        if rc["align"] == "cap":
            base = cy + size * self.fonts.cap_ratio / PT_PER_MM / 2
        else:
            base = cy + (node.asc - node.desc) / 2 / PT_PER_MM
        return self.w / 2 - (node.x0 + node.x1) / 2 / PT_PER_MM, base

    def _ok(self, key: Key, node: Node, role: str, size: float) -> bool:
        top, bot = self._area(key)
        x, base = self._place_xy(key, node, role, size)
        x0, y0, x1, y1 = Placed(node, x, base, "", 0, 0).ink()
        if y0 < top - 1e-6 or y1 > bot + 1e-6 or x0 < self.pad - 1e-6 or x1 > self.w - self.pad + 1e-6:
            return False
        return all(_inside_rounded(px, py, self.w, self.h, self.r, self.clear)
                   for px, py in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)))

    def _try(self, key: Key, role: str, size: float):
        rc = self.cfg["typography"]["roles"][role]
        label = normalise(key.primary, self.cfg)
        for wd in [s for s in self.steps if rc["min_width"] - 1e-9 <= s <= rc["width"] + 1e-9]:
            node = typeset(label, Style(rc["weight"], wd, False, role in ("word", "short")), size, self.fonts, self.cfg)
            if self._ok(key, node, role, size):
                return node, wd
        return None

    def _max_size(self, key: Key, role: str) -> float:
        size = self.fonts.size_for_cap(self.cfg["typography"]["roles"][role]["cap"])
        while size > 3.0:
            if self._try(key, role, size):
                return size
            size *= 0.985
        raise LayoutError(f"{key.id}: primary {key.primary!r} does not fit")

    def _role_sizes(self) -> dict:
        roles: dict[str, list] = {}
        for k in self.keys.values():
            roles.setdefault(role_of(k, self.cfg), []).append(k)
        sizes = {}
        for role, members in roles.items():
            if self.cfg["typography"]["roles"][role].get("uniform"):
                s = min(self._max_size(k, role) for k in members)
                sizes.update({k.id: s for k in members})
            else:
                sizes.update({k.id: self._max_size(k, role) for k in members})
        return sizes

    def secondary(self, key: Key, on_dark: bool) -> list[Placed]:
        col, sec = self.cfg["colors"], self.cfg["typography"]["secondary"]
        labels = [normalise(s, self.cfg) if s else "" for s in (key.shift, key.alpha)]
        if not any(labels):
            return []
        size = self.sec_size
        avail = self.w - 2 * self.side_pad
        widths_ok = [s for s in self.steps if sec["min_width"] - 1e-9 <= s <= sec["width"] + 1e-9]
        while True:
            for wd in widths_ok:
                st = Style(sec["weight"], wd, False, False)
                nodes = [typeset(s, st, size, self.fonts, self.cfg) if s else None for s in labels]
                need = sum((n.x1 - n.x0) / PT_PER_MM for n in nodes if n) + (sec["gap"] if all(nodes) else 0)
                if need <= avail + 1e-9:
                    break
            else:
                size *= 0.97
                if size < self.sec_size * 0.7:
                    raise LayoutError(f"{key.id}: secondary legends do not fit")
                continue
            break
        sc = col["shift_on_dark"] if on_dark else col["shift"]
        ac = col["alpha_on_dark"] if on_dark else col["alpha"]
        out = []
        for n, side, color in ((nodes[0], "L", sc), (nodes[1], "R", ac)):
            if n is None:
                continue
            x = self.side_pad - n.x0 / PT_PER_MM if side == "L" else self.w - self.side_pad - n.x1 / PT_PER_MM
            # legends that rise above cap height (arrows, exponents, brackets) hang from the
            # same top line instead of poking above it
            over = max(0.0, n.asc / PT_PER_MM - size * self.fonts.cap_ratio / PT_PER_MM)
            out.append(Placed(n, x, self.band_base + over, color, size, wd))
        return out

    def place(self, key: Key) -> list[Placed]:
        col = self.cfg["colors"]
        bg = background(key, self.cfg)
        on_dark = _luma(bg) < 0.4
        placed = self.secondary(key, on_dark)
        role = role_of(key, self.cfg)
        size = self.sizes[key.id]
        res = self._try(key, role, size)
        if res is None:
            raise LayoutError(f"{key.id}: {key.primary!r} does not fit at its role size")
        node, wd = res
        x, base = self._place_xy(key, node, role, size)
        ink = (col.get("key_ink") or {}).get(key.primary) or (col["ink_on_dark"] if on_dark else col["ink"])
        placed.append(Placed(node, x, base, ink, size, wd))
        self._check(key, placed)
        return placed

    def _check(self, key: Key, placed: list[Placed]):
        rects = [p.ink() for p in placed]
        for (x0, y0, x1, y1) in rects:
            for px, py in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
                if not _inside_rounded(px, py, self.w, self.h, self.r, min(self.clear, self.pad) - 1e-6):
                    raise LayoutError(f"{key.id}: ink at ({px:.2f}, {py:.2f}) mm leaves the safe area")
        for i in range(len(rects)):
            for j in range(i + 1, len(rects)):
                a, b = rects[i], rects[j]
                if a[0] < b[2] + 0.15 and b[0] < a[2] + 0.15 and a[1] < b[3] + 0.15 and b[1] < a[3] + 0.15:
                    raise LayoutError(f"{key.id}: legends collide")


def draw_sticker_form(c, name: str, key: Key, lay: StickerLayout, cfg: dict) -> list[Placed]:
    """Draw one sticker as a reusable PDF form (origin: sticker bottom-left)."""
    w, h, r = lay.w, lay.h, lay.r
    placed = lay.place(key)
    bg = background(key, cfg)
    c.beginForm(name, lowerx=-2, lowery=-2, upperx=mm(w) + 2, uppery=mm(h) + 2)
    c.setFillColor(HexColor(bg))
    c.setStrokeColor(HexColor(darken(bg, cfg["sticker"]["outline_darken"])))
    c.setLineWidth(cfg["sticker"]["outline_pt"])
    c.roundRect(0, 0, mm(w), mm(h), mm(r), stroke=1, fill=1)
    for p in placed:
        c.saveState()
        c.setFillColor(HexColor(p.color))
        c.setStrokeColor(HexColor(p.color))
        p.node.draw(c, mm(p.x), mm(h - p.y))
        c.restoreState()
    c.endForm()
    return placed


# ═════════════════════════════════════════════════════════════════════════════
# Sheets
# ═════════════════════════════════════════════════════════════════════════════

def plan_sheet(cfg: dict, sw: float, sh: float, cols: int, rows: int):
    pg = cfg["page"]
    pw, ph = pg["size"]
    m, sg = pg["margin"], pg["set_gap"]
    reserve = 9.0 if pg.get("calibration_mm") else 0.0
    aw, ah = pw - 2 * m, ph - 2 * m - reserve
    want = pg.get("sets", "auto")
    best = None
    for nc in range(1, 5):
        for nr in range(1, 5):
            if want != "auto" and nc * nr != int(want):
                continue
            gw = (aw - (nc - 1) * sg - nc * cols * sw) / (nc * (cols - 1))
            gh = (ah - (nr - 1) * sg - nr * rows * sh) / (nr * (rows - 1))
            g = min(gw, gh, pg["max_gap"])
            if g >= pg["min_gap"] - 1e-9:
                cand = (nc * nr, g, nc, nr)
                if best is None or cand[:2] > best[:2]:
                    best = cand
    if best is None:
        raise LayoutError("not even one set fits on the page with the configured gaps")
    n, g, nc, nr = best
    bw = nc * (cols * sw + (cols - 1) * g) + (nc - 1) * sg
    bh = nr * (rows * sh + (rows - 1) * g) + (nr - 1) * sg
    return {"sets": n, "gap": g, "nc": nc, "nr": nr, "bw": bw, "bh": bh,
            "x0": (pw - bw) / 2, "y0": m + (ph - 2 * m - reserve - bh) / 2}


def draw_calibration(c, ph: float, x_center: float, y: float, length: float, fonts: Fonts):
    """Hairline scale bar with end ticks and a tiny label. y = bar line, mm from top."""
    grey = HexColor("#8C8C8C")
    c.setStrokeColor(grey)
    c.setLineWidth(0.35)
    xa, xb = x_center - length / 2, x_center + length / 2
    c.line(mm(xa), mm(ph - y), mm(xb), mm(ph - y))
    for xv in (xa, xb):
        c.line(mm(xv), mm(ph - y + 1.2), mm(xv), mm(ph - y - 1.2))
    label = Run(f"{length:g} mm  ·  imprimir al 100 %", Style(500, 87.5), 6.0, fonts)
    c.setFillColor(grey)
    label.draw(c, mm(x_center) - (label.x0 + label.x1) / 2, mm(ph - y - 3.6))


def build_sheet(cfg, keys, fonts, out: Path, date: str):
    kt, st = cfg["key_top"], cfg["sticker"]
    sw, sh = kt["width"] - 2 * st["inset"], kt["height"] - 2 * st["inset"]
    rows = 1 + max(k.row for k in keys.values())
    cols = 1 + max(k.col for k in keys.values())
    lay = StickerLayout(keys, sw, sh, st["corner_radius"], cfg, fonts)
    plan = plan_sheet(cfg, sw, sh, cols, rows)
    pw, ph = cfg["page"]["size"]
    c = rl_canvas.Canvas(str(out), pagesize=(mm(pw), mm(ph)), invariant=1, pageCompression=1)
    c.setTitle("NeoCalculator keycap stickers (1:1)")
    c.setSubject(f"{date} · key top {kt['width']} x {kt['height']} mm (r {kt['corner_radius']}), "
                 f"measured from hardware/enclosure/key.stl · sticker {sw:.2f} x {sh:.2f} mm "
                 f"(inset {st['inset']} mm) · {plan['sets']} sets")
    c.setCreator("hardware/keyboard/stickers/make_stickers.py")
    report = {k.id: draw_sticker_form(c, f"K{k.id}", k, lay, cfg) for k in keys.values()}
    g, sg = plan["gap"], cfg["page"]["set_gap"]
    set_w, set_h = cols * sw + (cols - 1) * g, rows * sh + (rows - 1) * g
    for si in range(plan["sets"]):
        bx = plan["x0"] + (si % plan["nc"]) * (set_w + sg)
        by = plan["y0"] + (si // plan["nc"]) * (set_h + sg)
        for k in keys.values():
            c.saveState()
            c.translate(mm(bx + k.col * (sw + g)), mm(ph - (by + k.row * (sh + g)) - sh))
            c.doForm(f"K{k.id}")
            c.restoreState()
    if cfg["page"].get("calibration_mm"):
        draw_calibration(c, ph, pw / 2, plan["y0"] + plan["bh"] + 5.0, cfg["page"]["calibration_mm"], fonts)
    c.showPage()
    c.save()
    return plan, lay, report


def build_fit_test(cfg, keys, fonts, out: Path, date: str):
    kt = cfg["key_top"]
    pw, ph = cfg["page"]["size"]
    m = cfg["page"]["margin"] + 8
    c = rl_canvas.Canvas(str(out), pagesize=(mm(pw), mm(ph)), invariant=1, pageCompression=1)
    c.setTitle("NeoCalculator keycap stickers: fit test")
    c.setSubject(date)
    grey = HexColor("#8C8C8C")
    y = m
    sizes = []
    for inset in cfg["fit_test"]["insets"]:
        w, h = kt["width"] - 2 * inset, kt["height"] - 2 * inset
        lay = StickerLayout(keys, w, h, max(0.0, kt["corner_radius"] - inset), cfg, fonts)
        x = m
        for kid in cfg["fit_test"]["sample_keys"]:
            name = f"F{kid}_{int(round(inset * 100))}"
            draw_sticker_form(c, name, keys[kid], lay, cfg)
            c.saveState()
            c.translate(mm(x), mm(ph - y - h))
            c.doForm(name)
            c.restoreState()
            x += w + 4.0
        c.setFillColor(grey)
        lab = Run(f"{inset:.1f} mm  →  {w:.2f} × {h:.2f} mm", Style(500, 87.5), 7.0, fonts)
        lab.draw(c, mm(x + 2), mm(ph - y - h / 2) - (lab.asc - lab.desc) / 2)
        sizes.append((round(w, 2), round(h, 2)))
        y += h + 6.0
    c.setDash(1.0, 0.9)
    c.setStrokeColor(grey)
    c.setLineWidth(0.35)
    c.roundRect(mm(m), mm(ph - y - kt["height"]), mm(kt["width"]), mm(kt["height"]),
                mm(kt["corner_radius"]), stroke=1, fill=0)
    c.setDash()
    lab = Run(f"tecla real {kt['width']:.2f} × {kt['height']:.2f} mm", Style(500, 87.5), 7.0, fonts)
    c.setFillColor(grey)
    lab.draw(c, mm(m + kt["width"] + 6), mm(ph - y - kt["height"] / 2) - (lab.asc - lab.desc) / 2)
    y += kt["height"] + 12
    draw_calibration(c, ph, m + cfg["page"]["calibration_mm"] / 2, y, cfg["page"]["calibration_mm"], fonts)
    c.showPage()
    c.save()
    return sizes


# ═════════════════════════════════════════════════════════════════════════════
# --measure
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
    pts = np.round(top.reshape(-1, 3)[:, :2], 4)
    uniq, inv = np.unique(pts, axis=0, return_inverse=True)
    inv = inv.reshape(-1, 3)
    edges = np.sort(np.concatenate([inv[:, [0, 1]], inv[:, [1, 2]], inv[:, [2, 0]]]), axis=1)
    e, cnt = np.unique(edges, axis=0, return_counts=True)
    b = uniq[np.unique(e[cnt == 1])]
    w, h = np.ptp(b[:, 0]), np.ptp(b[:, 1])
    rel = np.abs(b - [(b[:, 0].min() + b[:, 0].max()) / 2, (b[:, 1].min() + b[:, 1].max()) / 2])
    arc = rel[(rel[:, 0] < w / 2 - 1e-3) & (rel[:, 1] < h / 2 - 1e-3)]
    best = (None, math.inf)
    for r in np.arange(0.05, min(w, h) / 2, 0.005):
        err = np.abs(np.hypot(arc[:, 0] - (w / 2 - r), arc[:, 1] - (h / 2 - r)) - r).max()
        if err < best[1]:
            best = (float(r), float(err))
    return {"width": float(w), "height": float(h), "corner_radius": best[0], "fit_error": best[1],
            "z": float(zmax), "arc_points": int(len(arc))}


# ═════════════════════════════════════════════════════════════════════════════
# --verify
# ═════════════════════════════════════════════════════════════════════════════

def verify(cfg: dict, sheet: Path, fit: Path, plan: dict, fit_sizes: list) -> bool:
    try:
        import pymupdf as fitz
    except ImportError:
        print("verify: PyMuPDF not installed (pip install pymupdf); skipping.")
        return True
    ok = True

    def fail(msg):
        nonlocal ok
        ok = False
        print("  FAIL:", msg)

    pw, ph = cfg["page"]["size"]
    kt, st = cfg["key_top"], cfg["sticker"]
    sw, sh = kt["width"] - 2 * st["inset"], kt["height"] - 2 * st["inset"]
    col = cfg["colors"]
    fills = {HexColor(v).rgb() for v in col["background"].values()}
    fills |= {HexColor(v).rgb() for v in (col.get("key_background") or {}).values()}

    def is_bg(f):
        return f is not None and any(all(abs(a - b) < 0.003 for a, b in zip(f, bg)) for bg in fills)

    tol = 0.01
    for pdf, kind in ((sheet, "sheet"), (fit, "fit test")):
        doc = fitz.open(str(pdf))
        page = doc[0]
        wmm, hmm = page.rect.width / PT_PER_MM, page.rect.height / PT_PER_MM
        print(f"verify {pdf.name}: {len(doc)} page, {wmm:.3f} × {hmm:.3f} mm")
        if abs(wmm - pw) > tol or abs(hmm - ph) > tol:
            fail("page is not A4")
        draws = page.get_drawings()
        stickers = [d["rect"] for d in draws if is_bg(d.get("fill"))]
        others = [d for d in draws if not is_bg(d.get("fill"))]
        if kind == "sheet":
            exp = plan["sets"] * 50
            print(f"  stickers: {len(stickers)} (expected {exp} = {plan['sets']} full sets)")
            if len(stickers) != exp:
                fail("sticker count")
            dw = max(abs(r.width / PT_PER_MM - sw) for r in stickers)
            dh = max(abs(r.height / PT_PER_MM - sh) for r in stickers)
            print(f"  sticker size: {sw:.3f} × {sh:.3f} mm expected, max deviation {dw:.4f} / {dh:.4f} mm")
            if dw > tol or dh > tol:
                fail("sticker size")
            mg = math.inf
            for i in range(len(stickers)):
                for j in range(i + 1, len(stickers)):
                    a, b = stickers[i], stickers[j]
                    if (a & b).get_area() > 0.01:
                        fail("stickers overlap")
                    mg = min(mg, max(b.x0 - a.x1, a.x0 - b.x1, b.y0 - a.y1, a.y0 - b.y1) / PT_PER_MM)
            print(f"  smallest gap between stickers: {mg:.3f} mm")
            if mg < cfg["page"]["min_gap"] - tol:
                fail("gap")
        else:
            sizes = sorted({(round(r.width / PT_PER_MM, 2), round(r.height / PT_PER_MM, 2)) for r in stickers})
            print(f"  fit-test sticker sizes: {sizes}")
            if sizes != sorted(set(fit_sizes)):
                fail(f"fit-test sizes {sizes} != {sorted(set(fit_sizes))}")
        allr = fitz.Rect()
        for d in draws:
            allr |= d["rect"]
        marg = min(allr.x0, allr.y0, page.rect.width - allr.x1, page.rect.height - allr.y1) / PT_PER_MM
        print(f"  smallest margin to any ink: {marg:.2f} mm")
        if marg < 10.0 - tol:
            fail("ink closer than 10 mm to the page edge")
        inside = outside = 0
        clear = math.inf
        for d in others:
            rr = d["rect"]
            ctr = fitz.Point((rr.x0 + rr.x1) / 2, (rr.y0 + rr.y1) / 2)
            host = [s for s in stickers if s.contains(ctr)]
            if not host:
                outside += 1
                continue
            s = host[0]
            inside += 1
            clear = min(clear, (rr.x0 - s.x0) / PT_PER_MM, (s.x1 - rr.x1) / PT_PER_MM,
                        (rr.y0 - s.y0) / PT_PER_MM, (s.y1 - rr.y1) / PT_PER_MM)
        print(f"  legend paths inside stickers: {inside}; nearest to a sticker edge: {clear:.2f} mm; "
              f"marks outside stickers: {outside}")
        if clear < 0.3:
            fail("a legend comes closer than 0.3 mm to a sticker edge")
        if page.get_text().strip():
            fail("live text found (all legends should be vector outlines)")
    print("verify:", "OK" if ok else "FAILED")
    return ok


def previews(cfg: dict, sheet: Path, plan: dict):
    try:
        import pymupdf as fitz
    except ImportError:
        return
    o = cfg["output"]
    page = fitz.open(str(sheet))[0]
    page.get_pixmap(dpi=o["preview_dpi"]).save(str(HERE / o["preview"]))
    kt, st = cfg["key_top"], cfg["sticker"]
    sw = kt["width"] - 2 * st["inset"]
    set_w = 5 * sw + 4 * plan["gap"]
    x0, y0 = plan["x0"], plan["y0"]
    clip = fitz.Rect(mm(x0 - 1.5), mm(y0 - 1.5), mm(x0 + set_w + 1.5), mm(y0 + 52))
    page.get_pixmap(dpi=o["detail_dpi"], clip=clip).save(str(HERE / o["detail"]))
    print(f"previews: {o['preview']}, {o['detail']}")


# ═════════════════════════════════════════════════════════════════════════════

def label_table(keys: dict[str, Key], cfg: dict) -> str:
    def esc(s):
        return s.replace("|", "\\|") if s else "—"
    lines = ["| Key | Group | Role | Primary | SHIFT (upper-left) | ALPHA (upper-right) |",
             "|:--|:--|:--|:--|:--|:--|"]
    for k in sorted(keys.values(), key=lambda k: (k.row, k.col)):
        lines.append(f"| {k.id} | {k.category} | {role_of(k, cfg)} | `{esc(k.primary)}` | "
                     f"`{esc(k.shift)}` | `{esc(k.alpha)}` |")
    return "\n".join(lines).replace("`—`", "—")


def load_config(path: Path) -> dict:
    cfg = yaml.safe_load(path.read_text(encoding="utf-8"))
    st = cfg["sticker"]
    if st.get("corner_radius", "auto") == "auto":
        st["corner_radius"] = max(0.0, cfg["key_top"]["corner_radius"] - st["inset"])
    return cfg


def main() -> int:
    global REPO
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", default=str(HERE / "stickers.yaml"))
    ap.add_argument("--repo", default=None, help=argparse.SUPPRESS)
    ap.add_argument("--date", default=None)
    ap.add_argument("--verify", action="store_true", help="check the PDFs and write PNG previews")
    ap.add_argument("--measure", action="store_true", help="measure the key top from key.stl")
    ap.add_argument("--labels", action="store_true", help="print the legend table (Markdown)")
    args = ap.parse_args()
    if args.repo:
        REPO = Path(args.repo)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    cfg = load_config(Path(args.config))

    if args.measure:
        m = measure_key_top(REPO / cfg["sources"]["key_stl"])
        kt = cfg["key_top"]
        print(f"key.stl top face at z={m['z']:.3f} mm: {m['width']:.3f} × {m['height']:.3f} mm, "
              f"r {m['corner_radius']:.3f} mm (fit error {m['fit_error']:.4f} mm, {m['arc_points']} arc points)")
        print(f"stickers.yaml: {kt['width']} × {kt['height']} mm, r {kt['corner_radius']}")
        bad = (abs(m["width"] - kt["width"]) > 0.02 or abs(m["height"] - kt["height"]) > 0.02
               or abs(m["corner_radius"] - kt["corner_radius"]) > 0.05)
        print("MISMATCH: update key_top in stickers.yaml" if bad else "config matches the STL")
        return 1 if bad else 0

    keys = load_keys(cfg)
    if args.labels:
        print(label_table(keys, cfg))
        return 0

    mismatches, unhandled = firmware_audit(cfg, keys)
    print(f"firmware cross-check: {len(keys)} keys, {len(mismatches)} mismatches")
    for x in mismatches:
        print("  MISMATCH:", x)
    if unhandled:
        print("printed legends with no firmware handler found:")
        for u in unhandled:
            print("  ", u)
    hidden = sorted((k.id, p, v) for k in keys.values() for p, v in k.hidden.items())
    if hidden:
        print("not printed (layout printLegend: false): " + ", ".join(f"{a} {b} {c!r}" for a, b, c in hidden))
    if mismatches:
        return 3

    fonts = Fonts(cfg)
    date = args.date or _dt.date.today().isoformat()
    o = cfg["output"]
    try:
        plan, lay, report = build_sheet(cfg, keys, fonts, HERE / o["pdf"], date)
        fit_sizes = build_fit_test(cfg, keys, fonts, HERE / o["fit_test_pdf"], date)
    except (LayoutError, MissingGlyph) as e:
        print("ERROR:", e)
        return 2
    print(f"wrote {o['pdf']} ({plan['sets']} sets, gap {plan['gap']:.2f} mm) and {o['fit_test_pdf']}")
    by_role: dict[str, set] = {}
    for kid, pl in report.items():
        p = pl[-1]
        by_role.setdefault(role_of(keys[kid], cfg), set()).add(
            (round(p.size / PT_PER_MM * fonts.cap_ratio, 2), p.width))
    for role, vals in sorted(by_role.items()):
        caps = sorted({v[0] for v in vals})
        widths = sorted({v[1] for v in vals}, reverse=True)
        print(f"  {role:5s}: cap height {caps[0]:.2f}–{caps[-1]:.2f} mm, widths {widths}")
    if args.verify:
        good = verify(cfg, HERE / o["pdf"], HERE / o["fit_test_pdf"], plan, fit_sizes)
        previews(cfg, HERE / o["pdf"], plan)
        return 0 if good else 4
    return 0


if __name__ == "__main__":
    sys.exit(main())
