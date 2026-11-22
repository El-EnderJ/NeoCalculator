"""Bounded execution of pure-Python work (SymPy) inside a worker process.

SymPy occasionally spends minutes on a pathological exact expression
(observed during development: one oracle job ran > 200 s). The work runs in a
helper thread; if it overruns, a TimeoutError is injected into that thread
(PyThreadState_SetAsyncExc), which pure-Python code honours between bytecodes.
"""
from __future__ import annotations

import ctypes
import threading


class Overrun(Exception):
    pass


def call(fn, seconds, *args, **kwargs):
    box = {}

    def target():
        try:
            box["value"] = fn(*args, **kwargs)
        except Overrun:
            box["overrun"] = True
        except BaseException as exc:  # noqa: BLE001
            box["error"] = exc

    t = threading.Thread(target=target, daemon=True)
    t.start()
    t.join(seconds)
    if t.is_alive():
        ctypes.pythonapi.PyThreadState_SetAsyncExc(ctypes.c_ulong(t.ident), ctypes.py_object(Overrun))
        t.join(5)
        raise Overrun(f"exceeded {seconds}s")
    if box.get("overrun"):
        raise Overrun(f"exceeded {seconds}s")
    if "error" in box:
        raise box["error"]
    return box["value"]
