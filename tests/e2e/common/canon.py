"""Canonical Giac text written directly from the expression model.

Used ONLY as the third voice for triage (`giac_eval` hook): it bypasses keys,
cursor, MathAST and the NumOS serializer. Variables are substituted by the
exact values the preamble stored, so the Giac context state is irrelevant.
"""
from __future__ import annotations

from .expr import Abs, Const, Empty, Fn, Frac, LogB, Num, Op, Paren, Pow, Root, Row, Sqrt, Var

_FN = {"sin": "sin", "cos": "cos", "tan": "tan", "asin": "asin", "acos": "acos",
       "atan": "atan", "ln": "ln", "log": "log10"}


def giac(x, env_text: dict) -> str:
    if isinstance(x, Row):
        out, expect, neg = [], True, False
        for e in x.items:
            if isinstance(e, Op):
                if expect:
                    if e.kind == "-":
                        neg = not neg
                    continue
                out.append({"+": "+", "-": "-", "*": "*", "/": "/"}[e.kind])
                expect = True
                continue
            if isinstance(e, Pow) and e.sci:
                out.append("*")
            elif not expect:
                out.append("*")
            a = giac(e, env_text)
            out.append(f"(-({a}))" if neg else a)
            neg = False
            expect = False
        return "(" + "".join(out) + ")"
    if isinstance(x, Num):
        t = x.text
        if "." in t:
            ip, fp = t.split(".")
            digits = (ip + fp).lstrip("0") or "0"
            return f"({digits}/10^{len(fp)})" if fp else f"({ip or '0'})"
        return f"({t})"
    if isinstance(x, Const):
        return "(pi)" if x.name == "pi" else "(exp(1))"
    if isinstance(x, Var):
        if x.name == "x":
            return "(x)"
        return "(" + env_text[x.name] + ")"
    if isinstance(x, Paren):
        return giac(x.body, env_text)
    if isinstance(x, Frac):
        return f"({giac(x.num, env_text)}/{giac(x.den, env_text)})"
    if isinstance(x, Pow):
        return f"({giac(x.base, env_text)}^{giac(x.exp, env_text)})"
    if isinstance(x, Sqrt):
        return f"sqrt({giac(x.body, env_text)})"
    if isinstance(x, Root):
        return f"surd({giac(x.body, env_text)},{giac(x.index, env_text)})"
    if isinstance(x, Abs):
        return f"abs({giac(x.body, env_text)})"
    if isinstance(x, Fn):
        return f"{_FN[x.name]}({giac(x.arg, env_text)})"
    if isinstance(x, LogB):
        return f"logb({giac(x.arg, env_text)},{giac(x.base, env_text)})"
    if isinstance(x, Empty):
        raise ValueError("empty slot has no Giac text")
    raise TypeError(x)
