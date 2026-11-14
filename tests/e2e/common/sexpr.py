"""Parser for the single-line structural S-expressions printed by the NativeHal
e2e dump hooks (see `e2e::sexpr` in src/hal/NativeHal.cpp).

Grammar:  row := "[" node* "]"     node := row | "(" HEAD arg* ")" | null
          arg := row | node | WORD | "json string"

Parsed form (plain tuples, hashable, comparable):
    row   -> ("row", (child, ...))
    node  -> (HEAD, arg, ...)     e.g. ("num", "12"), ("frac", ROW, ROW)
"""
from __future__ import annotations

import json


class SexprError(ValueError):
    pass


def parse(text: str):
    if text is None:
        return None
    pos = 0
    n = len(text)

    def skip():
        nonlocal pos
        while pos < n and text[pos] == " ":
            pos += 1

    def string():
        nonlocal pos
        # JSON string starting at text[pos] == '"'
        end = pos + 1
        while end < n:
            c = text[end]
            if c == "\\":
                end += 2
                continue
            if c == '"':
                break
            end += 1
        if end >= n:
            raise SexprError("unterminated string")
        value = json.loads(text[pos:end + 1])
        pos = end + 1
        return value

    def word():
        nonlocal pos
        start = pos
        while pos < n and text[pos] not in " ()[]":
            pos += 1
        return text[start:pos]

    def item():
        nonlocal pos
        skip()
        if pos >= n:
            raise SexprError("unexpected end")
        c = text[pos]
        if c == "[":
            pos += 1
            kids = []
            while True:
                skip()
                if pos < n and text[pos] == "]":
                    pos += 1
                    return ("row", tuple(kids))
                kids.append(item())
        if c == "(":
            pos += 1
            skip()
            head = word()
            args = []
            while True:
                skip()
                if pos < n and text[pos] == ")":
                    pos += 1
                    return (head,) + tuple(args)
                if pos < n and text[pos] == '"':
                    args.append(string())
                elif pos < n and text[pos] in "([":
                    args.append(item())
                else:
                    args.append(word())
        if c == '"':
            return string()
        w = word()
        if w == "null":
            return None
        return w

    result = item()
    skip()
    if pos != n:
        raise SexprError(f"trailing text at {pos}: {text[pos:pos+20]!r}")
    return result


def as_row(node):
    """Template slots are rows in input ASTs but bare nodes in some result ASTs."""
    if node is None:
        return ("row", ())
    if isinstance(node, tuple) and node and node[0] == "row":
        return node
    return ("row", (node,))


_OPS = {"Add": "+", "Sub": "-", "Mul": "*", "Div": "/", "PlusMinus": "+-",
        "Eq": "=", "Lt": "<", "Gt": ">", "Le": "<=", "Ge": ">=", "Ne": "!="}


def linear(node) -> str:
    """Human-readable, structure-preserving linear rendering (for reports)."""
    if node is None:
        return "null"
    if isinstance(node, str):
        return node
    head = node[0]
    if head == "row":
        return " ".join(linear(c) for c in node[1]) if node[1] else "□"
    a = node[1:]
    if head == "num":
        return a[0]
    if head == "op":
        return {"Add": "+", "Sub": "-", "Mul": "×", "Div": "÷"}.get(a[0], a[0])
    if head == "empty":
        return "□"
    if head == "frac":
        return "{" + linear(as_row(a[0])) + "}/{" + linear(as_row(a[1])) + "}"
    if head == "pow":
        return "{" + linear(as_row(a[0])) + "}^{" + linear(as_row(a[1])) + "}"
    if head == "sqrt":
        return "√{" + linear(as_row(a[0])) + "}"
    if head == "root":
        return "root[" + linear(as_row(a[1])) + "]{" + linear(as_row(a[0])) + "}"
    if head == "paren":
        l, r = {"paren": "()", "bracket": "[]", "brace": "{}", "bar": "||"}.get(a[0], "()")
        return l + linear(as_row(a[1])) + r
    if head == "fn":
        return a[0] + "(" + linear(as_row(a[1])) + ")"
    if head == "logb":
        return "log_{" + linear(as_row(a[0])) + "}(" + linear(as_row(a[1])) + ")"
    if head == "const":
        return {"pi": "π", "e": "e", "i": "i"}[a[0]]
    if head == "var":
        return a[0]
    if head == "periodic":
        return ("-" if a[0] == "-" else "") + a[1] + "." + a[2] + "(" + a[3] + ")"
    if head in ("sym", "special"):
        return a[0]
    if head == "call":
        return a[0] + "(" + ",".join(linear(as_row(x)) for x in a[1:]) + ")"
    return head + "(" + ",".join(linear(x) for x in a) + ")"
