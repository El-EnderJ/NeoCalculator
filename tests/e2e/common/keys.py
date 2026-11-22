"""Keystroke projections of an expression tree.

Cursor semantics modelled here were OBSERVED on the emulator (probe logs in
out/correctness-e2e-01/probe/), not assumed:

  * `/` (the DIV/fraction key) captures the immediately preceding atom as the
    numerator; at the start of a row or after an operator it opens an empty
    fraction. RIGHT (or DOWN) at the end of the numerator moves to the
    denominator, RIGHT at the end of the denominator leaves the fraction.
  * `^` captures the preceding atom as the base; RIGHT leaves the exponent.
  * sqrt / sin / ln / log / log_b / ( open a slot; RIGHT at its end leaves it.
  * In Calculation `)` is plain RIGHT (CursorController::moveRight); in Grapher
    it is closeParen() (exits the nearest enclosing round group, ADV-H01 fix).
  * EXP inserts "×10^□" with the cursor in the exponent.
  * `neg` and `-` are the same SUB key: a minus at row start/after an operator
    is a unary minus.
  * Abs and indexed roots have no key: Toolbox search `abs` / `nthroot`.
    Keyboard-shortcut entries (fraction, power, sqrt, parentheses, sin/cos/tan)
    are deliberately NOT discoverable in the Toolbox (ToolboxCatalog
    discoverable()), so the toolbox projection only replaces ln/log/asin/acos/
    atan/log_b, which do have discoverable entries.

Projections:
  direct  – canonical navigation (RIGHT to leave every slot).
  toolbox – same tree, inserting log_b / ln / log / inverse trig via Toolbox.
  edited  – same tree with a typo that is corrected (DEL), or a detour.
  naive   – a human who presses `)` to close a bracket and expects it to close
            everything inside (absorbing fraction/power exits). This path is
            NOT expected to denote the tree; it is compared separately.
"""
from __future__ import annotations

import random

from .expr import (Abs, Const, Empty, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row,
                   Sqrt, Var)

TOOLBOX_QUERY = {
    "abs": "abs", "nthroot": "nthroot", "logb": "Logarithm",
    "ln": "Natural logarithm", "log": "Decimal logarithm",
    "asin": "asin", "acos": "acos", "atan": "atan",
}
VAR_KEY = {"A": "1", "B": "2", "C": "3", "D": "4", "E": "5", "F": "6"}
FN_KEYS = {"sin": ["sin"], "cos": ["cos"], "tan": ["tan"], "ln": ["ln"], "log": ["log"],
           "asin": ["shift", "sin"], "acos": ["shift", "cos"], "atan": ["shift", "tan"]}
# Exits of these slots are "closing a bracket" in a human's linear reading.
PAREN_LIKE = {"paren", "fn", "logb_arg"}


def toolbox_keys(query: str) -> list:
    # TOOLBOX opens on group 0; UP RIGHT×3 ENTER focuses the search query
    # (tests/wasm/toolbox.mjs uses the same product controls), SDL text types
    # the query, DOWN focuses the first match, ENTER inserts it.
    return [("key", "toolbox"), ("key", "up"), ("key", "right"), ("key", "right"),
            ("key", "right"), ("key", "enter"), ("text", query), ("key", "down"),
            ("key", "enter")]


class _Typer:
    def __init__(self, mode: str):
        self.mode = mode  # 'direct' | 'toolbox'
        self.out = []

    def k(self, *names):
        for n in names:
            self.out.append(("key", n))

    def exit(self, kind):
        self.out.append(("EXIT", kind))

    def row(self, r: Row):
        items = r.items
        for i, e in enumerate(items):
            if isinstance(e, Op):
                if e.kind == "-":
                    unary = i == 0 or isinstance(items[i - 1], Op)
                    self.k("neg" if unary else "-")
                else:
                    self.k({"+": "+", "*": "*", "/": "divide"}[e.kind])
            else:
                self.atom(e)

    def atom(self, a):
        if isinstance(a, Empty):
            return
        if isinstance(a, Num):
            for ch in a.text:
                self.k(ch)
        elif isinstance(a, Const):
            self.k("pi" if a.name == "pi" else "e")
        elif isinstance(a, Var):
            if a.name == "x":
                self.k("x")
            elif a.name == "Ans":
                self.k("ans")
            else:
                self.k("alpha", VAR_KEY[a.name])
        elif isinstance(a, Paren):
            self.k("(")
            self.row(a.body)
            self.exit("paren")
        elif isinstance(a, Frac):
            if a.capture:
                self.row(a.num)
                self.k("/")
            else:
                self.k("/")
                self.row(a.num)
                self.exit("frac_num")
            self.row(a.den)
            self.exit("frac_den")
        elif isinstance(a, Pow):
            if a.sci:
                self.k("exp")
            else:
                self.atom(a.base)
                self.k("^")
            self.row(a.exp)
            self.exit("pow")
        elif isinstance(a, Sqrt):
            self.k("sqrt")
            self.row(a.body)
            self.exit("sqrt")
        elif isinstance(a, Root):
            self.out += toolbox_keys(TOOLBOX_QUERY["nthroot"])
            self.row(a.index)
            self.exit("root_index")
            self.row(a.body)
            self.exit("root_body")
        elif isinstance(a, Abs):
            self.out += toolbox_keys(TOOLBOX_QUERY["abs"])
            self.row(a.body)
            self.exit("abs")
        elif isinstance(a, Fn):
            if self.mode == "toolbox" and a.name in TOOLBOX_QUERY:
                self.out += toolbox_keys(TOOLBOX_QUERY[a.name])
            else:
                self.k(*FN_KEYS[a.name])
            self.row(a.arg)
            self.exit("fn")
        elif isinstance(a, LogB):
            if self.mode == "toolbox":
                self.out += toolbox_keys(TOOLBOX_QUERY["logb"])
            else:
                self.k("logbase")
            self.row(a.base)
            self.exit("logb_base")
            self.row(a.arg)
            self.exit("logb_arg")
        else:
            raise TypeError(a)


def _raw(tree: Row, mode: str) -> list:
    t = _Typer(mode)
    t.row(tree)
    return t.out


def _canonical(raw: list) -> list:
    return [("key", "right") if tok[0] == "EXIT" else tok for tok in raw]


def _naive(raw: list, close_key: str = ")"):
    """Replace each maximal run of slot exits that contains a paren-like close
    by one `close_key` per paren-like close (absorbing fraction/power exits
    before it). Slot-to-slot moves inside a template stay RIGHT."""
    out, absorbed, i = [], 0, 0
    while i < len(raw):
        if raw[i][0] != "EXIT":
            out.append(raw[i])
            i += 1
            continue
        j = i
        while j < len(raw) and raw[j][0] == "EXIT":
            j += 1
        run = [t[1] for t in raw[i:j]]
        last_close = max((n for n, kind in enumerate(run) if kind in PAREN_LIKE), default=-1)
        for n, kind in enumerate(run):
            if n <= last_close:
                if kind in PAREN_LIKE:
                    out.append(("key", close_key))
                elif kind in ("frac_num", "root_index", "logb_base"):
                    out.append(("key", "right"))  # slot change: a human needs it
                else:
                    absorbed += 1
            else:
                out.append(("key", "right"))
        i = j
    return out, absorbed


def project(tree: Row, path: str, rng: random.Random | None = None):
    """Return (tokens, info) for `path` or (None, info) when the path does not
    apply. Tokens: ("key", name) | ("text", query)."""
    if path == "direct":
        return _canonical(_raw(tree, "direct")), {}
    if path == "toolbox":
        raw_t = _raw(tree, "toolbox")
        if raw_t == _raw(tree, "direct"):
            return None, {"skip": "no toolbox-insertable node"}
        return _canonical(raw_t), {}
    if path == "naive":
        toks, absorbed = _naive(_raw(tree, "direct"))
        if not absorbed:
            return None, {"skip": "no absorbed exit"}
        return toks, {"absorbed": absorbed}
    if path == "edited":
        rng = rng or random.Random(0)
        base = _canonical(_raw(tree, "direct"))
        variant = rng.choice(["typo_digit", "trailing", "detour"])
        return edit(base, variant, rng), {"variant": variant}
    raise ValueError(path)


def edit(base: list, variant: str, rng: random.Random) -> list:
    if variant == "typo_digit":
        digits = [i for i, t in enumerate(base) if t[0] == "key" and t[1].isdigit()]
        if digits:
            i = rng.choice(digits)
            wrong = str((int(base[i][1]) + 1 + rng.randrange(8)) % 10)
            # type a wrong digit right after the real one, then delete it
            return base[:i + 1] + [("key", wrong), ("key", "del")] + base[i + 1:]
        variant = "trailing"
    if variant == "trailing":
        # append "+7" after everything, then delete both keys
        return base + [("key", "+"), ("key", "7"), ("key", "del"), ("key", "del")]
    if variant == "detour":
        k = rng.randint(1, 3)
        # walk left k steps, insert pi, delete it, walk back right k steps
        return base + [("key", "left")] * k + [("key", "pi"), ("key", "del")] + [("key", "right")] * k
    raise ValueError(variant)


def to_lines(tokens: list) -> list:
    out = []
    for kind, val in tokens:
        if kind == "key":
            out.append(f"key {val}")
        elif kind == "text":
            out.append(f"sdl_text {val}")
        else:
            raise ValueError(kind)
    return out


def to_compact(tokens: list) -> str:
    return " ".join(v if k == "key" else f"<{v}>" for k, v in tokens)
