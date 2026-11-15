# Calculus surface (W4, CORRECTNESS-E2E-01): deterministic re-run commands

All commands from the worktree root (`C:/numos-wt/audit-e2e-01`), Git Bash,
`PYTHONIOENCODING=utf-8`, emulator `C:/.piobuild/e2e-01/emulator_pc/program.exe`
(read-only, not rebuilt). Outputs are git-ignored under `out/correctness-e2e-01/calculus/`.

```sh
export PYTHONIOENCODING=utf-8
EMU=C:/.piobuild/e2e-01/emulator_pc/program.exe

# Task 1: ADV-H01 close-bracket probe, 27 patterns x 2 tabs x {naive, RIGHT}
python tests/e2e/calculus/adv_h01.py --emulator $EMU
#   -> out/correctness-e2e-01/calculus/adv-h01/adv_h01.{md,json}

# Task 3: checker self-test (perturbed oracles + planted wrong answers)
python tests/e2e/calculus/selftest.py --seed 7 --count 200 --emulator $EMU
#   -> out/correctness-e2e-01/calculus/selftest/selftest-seed7-n200.json
#   (--reuse re-judges the stored dumps without the emulator)

# Task 4: the 20,000-case budget (4 paths per case), one seed at a time
for s in 11 12 13 14 15; do
  python tests/e2e/run.py --surface calculus --seed $s --count 4000 --jobs 4 --batch 40 --emulator $EMU
done
#   -> out/correctness-e2e-01/calculus/seed<S>-n4000/{results.jsonl,findings.json,summary.json,stats.json}
#   continue a partial run with --start N --count M (case ids c<seed>-<index> are independent)

# Task 5: dedupe by class, shrink, reproducers, 2 fresh re-runs each
python tests/e2e/calculus/findings.py out/correctness-e2e-01/calculus/seed1{1,2,3,4,5}-n4000 --emulator $EMU
#   -> out/correctness-e2e-01/calculus/findings/W4-Fnn/{repro.numos,finding.json}
#   -> out/correctness-e2e-01/calculus/findings-summary.json
```

## Driver design (files in this directory)

| file | role |
|---|---|
| `model.py` | common VPAM model + Calculus-only Toolbox templates `DefInt` (NodeDefIntegral, dump `node13`) and `DiffCall` (`call "diff"`); expected dump structure; keystroke projections (direct / toolbox / edited / naive with the extra template slot moves); canonical Giac text with DEG made explicit (third voice) |
| `gen.py` | seeded typed generator, 15 families (poly, rational, power_root, exp_log, trig, invtrig, abs, composition (depth <= 5), product_chain, parameter (y), defint (regular / integrable singularity / divergent / discontinuity / odd-symmetric; integer, decimal, expression, a>b, a=b limits), ftc (variable limits), diffcall, domain, incomplete); op = Derivative/Integral tab; angle RAD/DEG |
| `semantics.py` | oracle: SymPy over real x/y, common evaluator extended (bound integration variable, diff(), result calls incl. Giac's `NTHROOT`, root/power readings); constant definite integrals by SymPy exact (divergent -> undefined) cross-checked with mpmath quad; independent Giac-text reader; per-point mpmath comparison (40 digits) at x in {-23/10, -7/11, 3/8, 5/7, 17/10, 29/10} |
| `check.py` | per-dump judgement: I1 (derivative: shown vs d/dv f; integral: d/dv shown vs f, any antiderivative form; `+ C` present/absent), I2b (Giac-bound text denotes the displayed input), I5 (refusal / unevaluated vs oracle; SymPy's own antiderivative attempt for unevaluated integrals), variable rule and bound-variable choice; triage readings (rad / principal-root / Giac exact text) to separate DEG-display, radical-reading, display-conversion and Giac-value classes |
| `surface.py` | runner adapter: per case and path `key ac, key ac, key f1|f2, set_angle_mode, <keys>, key enter, dump_calculus`; I2/I3 structure checks; third voice `giac_eval diff(...)/integrate(...)` on canonical text |
| `adv_h01.py` | task 1 table |
| `selftest.py` | task 3 |
| `findings.py` | task 5 |

## Planned vs achieved (close-out, 2026-11-15)

Budget was cut twice by the orchestrator (20,000 -> 12,000 -> stop bulk now). Achieved:

| run | checker | cases | path executions (direct/edited/toolbox/naive) | cases/s | wall | status |
|---|---|---|---|---|---|---|
| seed 11 n4000 (`seed11-n4000-v1`) | v1 (all sample points) | 4,000 | 3995/3995/962/347 = 9,299 | 2.86 | 1,399 s | complete; v1 counts value/root classes over non-real regions |
| seed 12 n4000 (`seed12-n4000`) | v2 (real-domain points, 0^0 policy) | 4,000 | 3994/3994/1027/340 = 9,355 | 3.79 | 1,054 s | complete |
| seed 13 | v2 | 0 of 4,000 | - | - | - | started, killed on re-plan (jobs 4 -> 3); not relaunched (scope stop) |
| seeds 14, 15 | - | 0 | - | - | - | dropped by re-plan |
| seed 11 v2 re-run | v2 | 0 | - | - | - | not run (scope stop) |
| edge-family run (seed >= 100) | v2 | 0 of 2,000 | - | - | - | not run (scope stop); generator support exists |
| total | | **8,000** (planned 20,000, then 12,000) | **18,654** (edited 7,989; Toolbox 1,989 + Toolbox templates inside direct/edited) | | | |

Per family achieved (seed 11 + seed 12; planned = generator weights x 20,000):
abs 307, composition 809, defint 784, diffcall 223, domain 489, exp_log 731, ftc 339, incomplete 165,
invtrig 519, parameter 255, poly 551, power_root 637, product_chain 777, rational 615, trig 788,
(analysis timeouts 11). Edge families (domain+defint+ftc+incomplete+diffcall+parameter+abs) = 2,562.
op/angle: diff/rad 3,032, diff/deg 961, int/rad 3,000, int/deg 996.

Self-test: v1 completed (`selftest/selftest-seed7-n200.json`, 0 misses). The v2 re-run with `--reuse`
(started 11:13) produced no output in >20 min and was killed: no v2 self-test numbers.

Targeted probes: `adv_h01.py` (27 patterns x 2 tabs x 2 key paths), `cursor_probe.py` (21 cursor-insertion
patterns x 2 tabs, `out/correctness-e2e-01/calculus/cursor/`).

Reproducers: `python tests/e2e/calculus/findings.py out/correctness-e2e-01/calculus/seed12-n4000 --max-rounds 0 --jobs 1`
(one unshrunk representative per class, 2 fresh re-runs each) + W4-F16 written from the cursor probe.
Findings on this surface are CANDIDATES (not shrunk).
