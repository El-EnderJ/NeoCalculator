# W3 Grapher runs (CORRECTNESS-E2E-01 Phase 2) — deterministic re-run commands

All commands from the worktree root `C:/numos-wt/audit-e2e-01`, Python 3.12 (`py -3.12`),
emulator `C:/.piobuild/e2e-01/emulator_pc/program.exe` (read-only; SHA-256 in each `stats.json`).
Outputs under `out/correctness-e2e-01/grapher/`.

| step | command | output |
|---|---|---|
| ADV-H01 `)` table (230 runs) | `py -3.12 tests/e2e/grapher/adv_h01.py` | `adv-h01/adv_h01.md`, `records.jsonl` |
| adjacent-number probe (110 runs) | `py -3.12 tests/e2e/grapher/adv_h01.py --set adjacent --out out/correctness-e2e-01/grapher/adjacent` | `adjacent/` |
| oracle self-test | `E2E_GRAPHER_PERTURB=sign py -3.12 tests/e2e/grapher/run_w3.py --seed 5 --count 200 --batch 25 --jobs 3 --case-timeout 10 --no-confirm --out out/correctness-e2e-01/grapher/selftest-sign` (same with `coef`, and unset for `selftest-base`); then `py -3.12 tests/e2e/grapher/w3_compare.py selftest` | `selftest-*` |
| isolation | `py -3.12 tests/e2e/grapher/run_w3.py --seed 5 --count 200 --batch 1 --jobs 3 --case-timeout 10 --no-confirm --out out/correctness-e2e-01/grapher/isolation-batch1`; `py -3.12 tests/e2e/grapher/w3_compare.py isolation` | `isolation-batch1/` |
| main campaign (10,000) | `py -3.12 tests/e2e/grapher/run_w3.py --seed 10 --count 2500 --start S --jobs 2 --batch 25 --case-timeout 10 --out out/correctness-e2e-01/grapher/main-seed10-sS` for S in 0 2500 5000 7500 | `main-seed10-s*/` |
| offline re-analysis + classes | `py -3.12 tests/e2e/grapher/w3_reanalyze.py out/correctness-e2e-01/grapher/main-seed10-s*` | `*/reanalysis.json` |
| Table/Trace OCR check (520 bulk cases) | `py -3.12 tests/e2e/grapher/w3_tabletrace.py --seed 10 --count 520 --jobs 3` (resumed with `--skip 432 --jobs 2` after a SymPy stall; 45 s oracle guard added) | `tabletrace/summary-seed10-combined.json` |
| HANG re-check | `py -3.12 tests/e2e/grapher/w3_hang.py g10-8735 g10-9431` | `findings/W3-F00-hang/` |
| finding reproducers (each run twice) | `py -3.12 tests/e2e/grapher/w3_findings.py` | `findings/W3-F*/` |

Family mix per 20 consecutive indices: 8 bulk y=f(x) (direct), 5 edited, 2 template (Toolbox log_b),
1 naive `)`, 4 edge. Seeds/indices fully determine cases and keys (`surface.generate`).

## Overrides of `tests/e2e/common/` (kept inside `grapher/`)

* `run_w3.py` replaces `Runner._run_lines` at import time to parse implicit/inequality residuals
  (`assert_graph_eval_near` PASS lines after `log R|case|slot|x|y`) into `by_tag["__residuals__"]`.
* `gcore.merge_nums`: adjacent `num` nodes read as one digit string (the Grapher serializer joins them;
  common/evaluator multiplies).
* `gcore.point_value`: x/y substituted as exact float32 rationals BEFORE evaluation; real-plot policy
  (non-real intermediate ⇒ "undefined" accepted); IEEE-double second oracle `float_eval` used only to label
  float artefacts (class W3-F06, needs policy), never to accept a value.
* Toolbox in Grapher: only `Logarithm` (log_b) is discoverable/available (abs, nthroot → "Unavailable in
  this editor"); `_GTyper` uses it and types `y` with the `y` key.
