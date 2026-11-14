"""Process pool with a HARD per-task timeout.

SymPy can block inside C-level big-integer arithmetic (observed: an oracle
job holding the GIL for minutes), which neither thread interruption nor
concurrent.futures can stop. Here each worker is a separate process; a task
that overruns gets its worker terminated and replaced, and yields `on_timeout`.
Results are returned in input order.
"""
from __future__ import annotations

import multiprocessing as mp
import queue
import time


def _worker(func, tasks, results):
    while True:
        item = tasks.get()
        if item is None:
            return
        idx, arg = item
        results.put(("start", idx, None))
        try:
            out = func(arg)
            results.put(("done", idx, out))
        except BaseException as exc:  # noqa: BLE001
            results.put(("error", idx, repr(exc)))


def map_hard(func, items, workers, timeout, on_timeout):
    ctx = mp.get_context("spawn")
    tasks, results = ctx.Queue(), ctx.Queue()
    for i, it in enumerate(items):
        tasks.put((i, it))
    procs = {}

    def spawn():
        p = ctx.Process(target=_worker, args=(func, tasks, results), daemon=True)
        p.start()
        procs[p.pid] = {"proc": p, "idx": None, "t": None}

    for _ in range(max(1, min(workers, len(items)))):
        spawn()
    out = [None] * len(items)
    done = 0
    started = {}  # idx -> start time (assigned to whichever worker reports it)
    while done < len(items):
        try:
            kind, idx, val = results.get(timeout=0.5)
            if kind == "start":
                started[idx] = time.monotonic()
                continue
            if out[idx] is None:
                out[idx] = val if kind == "done" else on_timeout(items[idx], f"error: {val}")
                done += 1
            started.pop(idx, None)
        except queue.Empty:
            pass
        now = time.monotonic()
        overdue = [i for i, t in started.items() if now - t > timeout]
        if overdue:
            # We cannot tell which worker owns the overdue task from the shared
            # queue, so terminate workers that are still alive but have been
            # silent, i.e. all busy ones, after recording every overdue task.
            for i in overdue:
                started.pop(i)
                if out[i] is None:
                    out[i] = on_timeout(items[i], f"timeout {timeout}s")
                    done += 1
            # Re-queue the other in-flight tasks: they will be recomputed.
            inflight = list(started)
            started.clear()
            for pid, rec in list(procs.items()):
                rec["proc"].terminate()
                rec["proc"].join(5)
                del procs[pid]
            for i in inflight:
                if out[i] is None:
                    tasks.put((i, items[i]))
            for _ in range(max(1, min(workers, len(items) - done))):
                spawn()
    for _ in procs:
        tasks.put(None)
    for rec in procs.values():
        rec["proc"].join(2)
        if rec["proc"].is_alive():
            rec["proc"].terminate()
    return out
