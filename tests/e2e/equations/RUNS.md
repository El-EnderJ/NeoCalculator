# Equations surface (CORRECTNESS-E2E-01, worker W5): deterministic runs

All commands run from the worktree root (`C:/numos-wt/audit-e2e-01`, ASCII path)
with Python 3.12 (`py -3.12`; the default `python` on this machine is 3.14) and
the read-only emulator `C:/.piobuild/e2e-01/emulator_pc/program.exe`.
`common/emulator.py` prepends `C:/mingw64/bin` and `C:/SDL2/x86_64-w64-mingw32/bin`
to PATH. Outputs are git-ignored under `out/correctness-e2e-01/equations/`.

## 1. `)` behaviour (ADV-H01 pattern table)

    py -3.12 tests/e2e/equations/adv_h01.py

-> `out/correctness-e2e-01/equations/adv-h01/adv_h01.{md,json,stdout.txt}`
(30 patterns, naive `)` keys vs canonical RIGHT keys, one emulator process).

## 2. Seeded batches (task 4)

    py -3.12 tests/e2e/run.py --surface equations --seed S --count 5000 --jobs 4 --batch 40 \
        --emulator C:/.piobuild/e2e-01/emulator_pc/program.exe

Seeds 1, 2, 3, 4 (5000 cases each; seeds 1-2 ran before the shared 10 s per-case timeout
commit a959381 (20 s single-case budget), seeds 3-4 and the seed-1 re-analysis after it) -> `out/correctness-e2e-01/equations/seed<S>-n5000/`
(`results.jsonl`, `findings.json`, `summary.json`, `stats.json`). Case `q<S>-<i>`
is generated from sha256("S:i") alone, so any case can be re-run on its own:

    py -3.12 tests/e2e/run.py --surface equations --seed 3 --start 1234 --count 1 --jobs 1

To continue beyond what was run, use a new seed or `--start`.

Seed-1 re-analysis (analysis bugs fixed after the first seed-1 pass: decimal residual
tolerance, display-check keyword clash), counted instead of the first pass:

    py -3.12 tests/e2e/run.py --surface equations --seed 1 --count 5000 --jobs 4 --batch 40 --name seed1-n5000-r2

## 2b. Targeted cursor-edit / template batch (re-plan)

    py -3.12 tests/e2e/run.py --surface equations.edit --seed 51 --count 1500 --jobs 3 --batch 40         --out out/correctness-e2e-01/equations/edit-seed51-n1500

(`edit-seed51-n1500-run1-freebug`, `-run2-keysbug` are superseded runs kept for the record:
I1 disabled by a free-variable check bug, then a keyword clash; the final run is `edit-seed51-n1500`.)

## 3. Driver self-test (task 3)

    py -3.12 tests/e2e/equations/selftest.py --seed 900 --count 400

-> `out/correctness-e2e-01/equations/selftest/selftest.json`

## 4. Finding reproducers (task 5)

    py -3.12 tests/e2e/equations/repro.py            # all classes, each re-run twice (fresh processes)
    py -3.12 tests/e2e/equations/repro.py --only W5-F01

-> `out/correctness-e2e-01/equations/findings/<ID>/{repro.numos,finding.json}`.
A `repro.numos` also runs stand-alone:

    PATH=/c/mingw64/bin:/c/SDL2/x86_64-w64-mingw32/bin:$PATH \
      C:/.piobuild/e2e-01/emulator_pc/program.exe --headless --deterministic --quiet \
      --frames 3000 --script out/correctness-e2e-01/equations/findings/W5-F01/repro.numos \
      --fs-sandbox-dir <empty dir>

## 5. Class aggregation over the seeded batches

    py -3.12 tests/e2e/equations/classify.py out/correctness-e2e-01/equations/seed1-n5000 \
        out/correctness-e2e-01/equations/seed2-n5000 out/correctness-e2e-01/equations/seed3-n5000 \
        out/correctness-e2e-01/equations/seed4-n5000

(use `seed1-n5000-r2` in place of `seed1-n5000`)
-> `out/correctness-e2e-01/equations/classes.json`, `classes.md`; planned vs achieved
coverage per family: `out/correctness-e2e-01/equations/coverage.md`.
