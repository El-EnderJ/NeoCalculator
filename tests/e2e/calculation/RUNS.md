# W1 (Calculation core) runs — CORRECTNESS-E2E-01 phase 2

All commands run from the worktree root (`C:/numos-wt/audit-e2e-01`) with
Python 3.12 (`py -3.12`; the default `python` on this machine is 3.14) and
`PYTHONIOENCODING=utf-8`. Emulator: `C:/.piobuild/e2e-01/emulator_pc/program.exe`
(SHA-256 29c5f10a…db977ba0, not rebuilt). Every run is deterministic for a
given surface/seed/count (case RNG = sha256(seed:index)); outputs go to
`out/correctness-e2e-01/calculation/<run>/` (git-ignored).

## Task 1 — `)` close-bracket probe (49 literal key sequences, RAD)

```
py -3.12 tests/e2e/calculation/close_paren_probe.py
```
→ `out/correctness-e2e-01/calculation/close-paren/close_paren_probe.{md,json}`,
`shots/*.png` (input + result screenshot of every pattern).

## Main generated budget (default surface, all 4 keystroke paths, I4 every 2nd case)

```
py -3.12 tests/e2e/run.py --surface calculation --seed 101 --count 5000 --jobs 4 --batch 40 --out out/correctness-e2e-01/calculation/w1-main-seed101-n5000
py -3.12 tests/e2e/run.py --surface calculation --seed 102 --count 5000 --jobs 4 --batch 40 --out out/correctness-e2e-01/calculation/w1-main-seed102-n5000
py -3.12 tests/e2e/run.py --surface calculation --seed 103 --count 5000 --jobs 4 --batch 40 --out out/correctness-e2e-01/calculation/w1-main-seed103-n5000
py -3.12 tests/e2e/run.py --surface calculation --seed 104 --count 5000 --jobs 4 --batch 40 --out out/correctness-e2e-01/calculation/w1-main-seed104-n5000
```
(also `out/correctness-e2e-01/scratch/w1/main_runs.sh`).

## Dedicated runs (W1 surfaces, `tests/e2e/calculation/run_w1.py`)

FORMAT-heavy (two-phase: real FORMAT menu read first; FIX at 3 places per case
incl. the place nearest a rounding tie, SCI, ENG ± shift, Decimal, Recurring,
Extended, Mixed, Polar/Exponential; back to Standard; Ans):
```
py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.fmt --seed 201 --count 2500 --jobs 4 --batch 40 --workers 8 --i4-every 1 --out out/correctness-e2e-01/calculation/w1-fmt-seed201-n2500
py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.fmt --seed 202 --count 2500 --jobs 4 --batch 40 --workers 8 --i4-every 1 --out out/correctness-e2e-01/calculation/w1-fmt-seed202-n2500
```
Edge families (huge/tiny magnitudes, int64-boundary rationals, periodic,
domain boundaries, poles, branch cuts, DEG/RAD trig incl. huge arguments,
empty slots), all 4 paths, I4 on every case:
```
py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.edge --seed 301 --count 2500 --jobs 4 --batch 40 --workers 8 --i4-every 1 --out out/correctness-e2e-01/calculation/w1-edge-seed301-n2500
py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.edge --seed 302 --count 2500 --jobs 4 --batch 40 --workers 8 --i4-every 1 --out out/correctness-e2e-01/calculation/w1-edge-seed302-n2500
```
I4 round trips (history UP UP + re-evaluate, S⇔D, STO into a random A–F from
the decimal view + recall, 2×Ans, restore):
```
py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.rt --seed 401 --count 3000 --jobs 4 --batch 40 --workers 8 --i4-every 1 --out out/correctness-e2e-01/calculation/w1-rt-seed401-n3000
```
(also `out/correctness-e2e-01/scratch/w1/dedicated_runs.sh`).

## Classification and reproducers

```
py -3.12 tests/e2e/calculation/w1_classify.py out/correctness-e2e-01/calculation/w1-*-n*      # signature groups
py -3.12 tests/e2e/calculation/w1_findings.py [--only W1-F01,...] --runs 2                       # findings/<ID>/
```
`w1_findings.py` re-runs every reproducer twice in fresh processes (fresh
sandbox each) and records whether both runs were identical.

## Re-plan (orchestrator, 2026-11-15 11:30): what was actually run

All with `E2E_CASE_TIMEOUT=10` (runner commit a959381; `run_w1.py`'s W1Runner does not override
`single_timeout` and delegates batches to `Runner.run_cases`), `--jobs 3`:

```
py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.cursor --seed 501 --count 2000 --jobs 3 --batch 40 --workers 6 --i4-every 1 --out out/correctness-e2e-01/calculation/w1-cursor-seed501-n2000
py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.edge   --seed 301 --count 2500 --jobs 3 --batch 40 --workers 6 --i4-every 1 --out out/correctness-e2e-01/calculation/w1-edge-seed301-n2500
py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.rt     --seed 401 --count 1500 --jobs 3 --batch 40 --workers 6 --i4-every 1 --out out/correctness-e2e-01/calculation/w1-rt-seed401-n1500
py -3.12 tests/e2e/calculation/run_w1.py --surface calculation.fmt    --seed 201 --count 2500 --jobs 3 --batch 40 --workers 6 --i4-every 1 --out out/correctness-e2e-01/calculation/w1-fmt-seed201-n2500
```
(queue scripts: `out/correctness-e2e-01/scratch/w1/replan_runs.sh`, `fmt_rerun.sh`). The second seeds of
edge (302) and fmt (202) from the original plan were dropped by the re-plan (planned 5,000+5,000+3,000 ->
2,500+2,500+1,500), and the cursor family (2,000) was added.

`calculation.cursor` (NEW, `tests/e2e/calculation/cursor/surface.py`): base expression typed, then 1-3 edits
(LEFT/RIGHT/UP/DOWN/`)` moves + digit, '.', EXP, (-), ALPHA var, Ans, pi/e, '(', sqrt, fraction, power, ×/+/-,
DEL, Toolbox abs/nthroot/log_b at the cursor) in modes live / history (ENTER UP) / result (ENTER then edit).
Judged on the final input only: eye reading (adjacent numbers concatenated) vs computed AST (I2a),
Giac string vs AST (I2b), result vs oracle(AST) (I1).

Reproducers: `py -3.12 tests/e2e/calculation/w1_findings.py --runs 2` (registry `w1_registry.py`);
root-cause counts: `py -3.12 tests/e2e/calculation/w1_rootcause.py <run dirs> --json <out>`;
ad-hoc probes: `py -3.12 tests/e2e/calculation/w1_probe.py out/correctness-e2e-01/calculation/probes/pN.json`.

## Coverage achieved (planned -> achieved)

| family | seed | planned | achieved | paths | wall s | cases/s | cases w/ confirmed findings |
|---|---|---|---|---|---|---|---|
| main random (default surface) | 101-104 | 20,000 | 20,000 | direct/toolbox/edited/naive + I4 every 2nd | 739/591/1042/669 | 6.76/8.46/4.80/7.48 | 198/175/160/167 |
| cursor / edited-path (NEW) | 501 | 2,000 | 2,000 | edit (live/history/result) | 270 | 7.40 | 268 (196 = W1-F01) |
| edge families | 301 | 2,500 (orig. 5,000) | 2,500 | 4 paths + I4 every case | 292 | 8.56 | 223 |
| FORMAT (FIX/SCI/ENG heavy) | 201 | 2,500 (orig. 5,000) | 2,500 (20,713 format dumps judged) | direct + real-menu formats | 862 | 2.90 | 285 |
| I4 round trips | 401 | 1,500 (orig. 3,000) | 1,500 (1,292 judged) | direct + hist/S<->D/STO A-F/Ans | 316 | 4.75 | 46 |
| total | | | 28,500 | | | | |

Failed attempts (not counted): fmt-201 attempt 1 killed by the session pause in phase A; attempt 2 crashed
at confirmation (KeyError 'Ans' in third_voice, fixed in w1gen.py/fmt surface); attempt 3 completed.
