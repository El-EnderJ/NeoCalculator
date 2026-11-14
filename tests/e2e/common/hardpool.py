"""Process pool with a HARD per-task timeout.

SymPy can block inside C-level big-integer arithmetic (observed: an oracle
job holding the GIL for minutes), which neither thread interruption nor
concurrent.futures can stop. Each worker process has its OWN task pipe (no
shared queue lock that a killed process could leave held); a task that
overruns gets exactly its worker terminated and replaced, and yields
`on_timeout(item, reason)`. Results are returned in input order.
"""
from __future__ import annotations

import multiprocessing as mp
import time
from multiprocessing.connection import wait


def _worker(func, conn):
    while True:
        try:
            msg = conn.recv()
        except EOFError:
            return
        if msg is None:
            return
        idx, arg = msg
        try:
            conn.send(("done", idx, func(arg)))
        except BaseException as exc:  # noqa: BLE001
            conn.send(("error", idx, repr(exc)))


def map_hard(func, items, workers, timeout, on_timeout):
    ctx = mp.get_context("spawn")
    n = len(items)
    out = [None] * n
    pending = list(range(n - 1, -1, -1))  # pop() from the end = in order
    slots = []  # dicts: proc, conn, idx, t

    def spawn():
        parent, child = ctx.Pipe()
        p = ctx.Process(target=_worker, args=(func, child), daemon=True)
        p.start()
        child.close()
        return {"proc": p, "conn": parent, "idx": None, "t": None}

    def feed(slot):
        if pending:
            i = pending.pop()
            slot["idx"], slot["t"] = i, time.monotonic()
            slot["conn"].send((i, items[i]))
        else:
            slot["idx"] = None

    for _ in range(max(1, min(workers, n))):
        s = spawn()
        slots.append(s)
        feed(s)
    done = 0
    while done < n:
        busy = [s for s in slots if s["idx"] is not None]
        ready = wait([s["conn"] for s in busy], timeout=0.5) if busy else []
        for s in busy:
            if s["conn"] in ready:
                try:
                    kind, idx, val = s["conn"].recv()
                except EOFError:
                    kind, idx, val = "error", s["idx"], "worker died"
                out[idx] = val if kind == "done" else on_timeout(items[idx], f"error: {val}")
                done += 1
                feed(s)
        now = time.monotonic()
        for k, s in enumerate(slots):
            if s["idx"] is not None and now - s["t"] > timeout:
                out[s["idx"]] = on_timeout(items[s["idx"]], f"timeout {timeout}s")
                done += 1
                s["proc"].terminate()
                s["proc"].join(5)
                s["conn"].close()
                slots[k] = spawn()
                feed(slots[k])
    for s in slots:
        try:
            s["conn"].send(None)
        except (OSError, BrokenPipeError):
            pass
        s["proc"].join(2)
        if s["proc"].is_alive():
            s["proc"].terminate()
    return out
