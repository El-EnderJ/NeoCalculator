# NumOS — MathRenderer v2 Phase 0 Acceptance & Rollback Plan (MR-00 / MR-01 / MR-02)

> Operational plan covering Phase 0 as **one phase, four PRs**. Companion execution detail:
> `NUMOS_MATHRENDERER_MR00_DECISION_RECORDS.md`,
> `NUMOS_MATHRENDERER_MR01_OBSERVABILITY_AND_CAPS_EXECUTION_PLAN.md`,
> `NUMOS_MATHRENDERER_MR02_GEOMETRY_HARNESS_EXECUTION_PLAN.md`,
> `NUMOS_MATHRENDERER_PHASE0_IMPLEMENTATION_PROMPTS.md` (ready-to-paste prompts).
> Verified against `b234e15` (2026-07-03).

---

## A. Phase objective

Make MathRenderer v2 **measurable before it is touched**:

1. Decisions frozen and human-ratified (MR-00 ADRs).
2. Editor bounded (depth 16 / nodes 512 / digits 32) and renderer observable
   (tree stats, layout passes, glyph-fallback counters, budget detection) with zero pixel
   change (MR-01).
3. Geometry provable: host snapshot harness over ~94 expressions + committed metrics fixture +
   CI gates (MR-02).

Phase exit gate (tickets phase map, P0): host harness + math asserts green in CI; all **19**
existing goldens byte-identical. (NumIR round-trip belongs to MR-03, which is Phase 0's last
ticket but **out of scope for this plan's PRs** — it follows the same discipline with its own
execution plan.)

---

## B. PR sequence, allowed/forbidden files, gates, rollback

### PR-A — MR-00: decision ratification (docs only)

- **Allowed files**: `docs/specs/NUMOS_MATHRENDERER_*.md` (the Phase-0 set + amendments the
  ratification forces into the six MRV2 spec docs).
- **Forbidden**: everything else. Zero code, zero tests, zero CI.
- **Acceptance gate**: the maintainer has read the ratification sheet
  (`NUMOS_MATHRENDERER_MR00_DECISION_RECORDS.md` top) and either ratified all eight ADRs in the
  PR discussion or requested amendments (which are applied in the same PR, propagated to every
  affected spec doc). Nothing in PR-B/PR-C may begin until PR-A is merged.
- **Rollback**: `git revert` — pure docs, no consumer.

### PR-B — MR-01: observability + editor caps

- **Allowed files** (exact; MR-01 plan §C):
  `src/math/MathAST.h`, `src/math/MathAST.cpp`, `src/math/CursorController.h`,
  `src/math/CursorController.cpp`, `src/ui/MathRenderer.h`, `src/ui/MathRenderer.cpp`,
  `src/apps/CalculationApp.h`, `src/apps/CalculationApp.cpp`, `src/hal/NativeHal.cpp`,
  `tests/emulator/scripts/mathv2_*.numos` (7 new scripts).
- **Forbidden**: `src/math/MathEvaluator.*`, `src/math/MathTypography.h`,
  `src/ui/MathTypography.*`, `src/math/font/**`, `src/fonts/**`, `src/apps/GrapherApp.*`,
  `src/input/**`, `platformio.ini`, `tests/emulator/golden/**`, `tests/emulator/masks/**`,
  `scripts/**`, `.github/**` (CI wiring is PR-D), `lib/**`, `docs/**` (except a changelog note
  if the repo keeps one).
- **Acceptance gates** (all hard; commands in §D):
  1. All 7 new scripts exit 0; negative control exits 4.
  2. Candidate sweep: all 19 goldens compare identical (masked where masked).
  3. Full existing CI suites green (8C semantic, 9A input, 9B menu, 9F grapher).
  4. KeyCode guard green.
  5. `esp32s3_n16r8` and `esp32s3_n16r8_validate` compile; flash delta ≤ +2 KB recorded.
  6. Review rules: draw-function hunks all `#ifdef NATIVE_SIM`; `ScriptCmdType` append-only;
     no forbidden file touched.
- **Rollback**: revert the single merge commit. Commands/scripts/caps all revert together; no
  persisted state exists. Partial revert unsupported (MR-01 plan §L).

### PR-C — MR-02: geometry harness + snapshots

- **Allowed files**: `tests/host/mathlayout_geometry_test.cpp`,
  `tests/host/mathlayout_fixed_metrics.h`, `tests/host/golden_geometry/mathv2_geometry.jsonl`,
  `scripts/build-mathlayout-host-tests.sh`, `scripts/check-math-metrics-fixture.py`.
- **Forbidden**: **all of `src/**`** (hard rule — the emptiness of the src diff *is* the
  no-golden-impact proof), goldens/masks, `platformio.ini`, `.github/**` (PR-D),
  existing `scripts/*.py`.
- **Acceptance gates**: harness footer `mismatched=0`, negative control exits 1, double-pass
  identity, fixture parity green locally, `git diff --stat` shows zero `src/**` lines
  (MR-02 plan §L/§N).
- **Rollback**: revert; deletes five files. Nothing references them except PR-D.

### PR-D (optional but recommended) — CI wiring

- **Allowed files**: `.github/workflows/emulator-build.yml`,
  `scripts/generate-emulator-candidates.py` (append the `mathv2_boss_subtree_depth` stem as a
  warning-only candidate).
- **Forbidden**: everything else.
- **Content**: three appended steps — MR-01 suite runner (after Phase 9F block,
  `emulator-build.yml:531`), geometry harness (after KeyCode guard, `:104`), metrics parity
  (after emulator build). Steps verbatim in MR-01 plan §C.7 and MR-02 plan §I.
- **Acceptance gates**: CI run green end-to-end on the PR; golden compare step behavior
  unchanged (fail-on-mismatch / warn-on-missing semantics untouched, `emulator-build.yml:575-607`);
  the new candidate stem produces a warning (no golden), not a failure.
- **Rollback**: revert; local scripts continue to work ungated.

Sequencing is strict: **PR-A → PR-B → PR-C → PR-D** (PR-C consumes PR-B's
`dump_math_metrics`; PR-D gates both).

---

## C. Exact git commands (per PR)

```bash
# one branch per PR, cut from up-to-date main
git fetch origin main
git checkout -B claude/mrv2-pr-b-mr01-observability origin/main   # adjust letter per PR
# … implement …
git add -A && git status                    # verify ONLY allowed files staged
git diff --stat origin/main                 # paste into PR description
git commit -m "MR-01: renderer observability + editor caps (depth16/nodes512/digits32); NATIVE_SIM accessors; assert_math_* .numos commands"
git push -u origin claude/mrv2-pr-b-mr01-observability
# open PR; after review + green CI, merge; then cut the next PR's branch from new main
```

Never stack PR-C on PR-B's branch — always re-cut from merged `main` (branch hygiene §I).

## D. Exact local commands (the full pre-push battery)

```bash
export PLATFORMIO_BUILD_DIR=.pio/build
BIN=.pio/build/emulator_pc/program

# build
pio run -e emulator_pc

# guards
python scripts/check-keycode-digit-patterns.py --selftest && python scripts/check-keycode-digit-patterns.py
g++ -std=c++17 -Wall -Wextra tests/host/keycode_digit_test.cpp -o /tmp/kt && /tmp/kt

# MR-01 scripts (PR-B onward)
for s in tests/emulator/scripts/mathv2_*.numos; do
  SDL_VIDEODRIVER=dummy timeout 60s "$BIN" --headless --deterministic \
    --script "$s" --frames 1600 --quiet || { echo "FAIL $s"; exit 1; }
done

# geometry harness + parity (PR-C onward)
scripts/build-mathlayout-host-tests.sh
SDL_VIDEODRIVER=dummy timeout 60s "$BIN" --headless --deterministic \
  --script tests/emulator/scripts/mathv2_metrics_dump.numos --frames 800 --quiet \
  | python scripts/check-math-metrics-fixture.py /dev/stdin

# golden sweep (every PR, even docs-only — it is cheap insurance)
python scripts/generate-emulator-candidates.py
for g in tests/emulator/golden/*.ppm; do n=$(basename "$g" .ppm); m=tests/emulator/masks/$n.mask
  if [ -f "$m" ]; then python scripts/compare-ppm.py "$g" out/emulator-candidates/$n.ppm --mask-file "$m"
  else python scripts/compare-ppm.py "$g" out/emulator-candidates/$n.ppm; fi \
  || { echo "GOLDEN DIFF: $n"; exit 1; }
done

# existing semantic suites (PR-B)
for s in tests/emulator/scripts/calc_semantic_*.numos \
         tests/emulator/scripts/calc_error_div_by_zero.numos \
         tests/emulator/scripts/calc_store_variable.numos; do
  SDL_VIDEODRIVER=dummy timeout 60s "$BIN" --headless --deterministic --script "$s" \
    --frames 1400 --quiet > /tmp/l.log 2>&1 && grep -q "PASS - " /tmp/l.log && ! grep -q "FAIL - " /tmp/l.log \
    || { echo "FAIL $s"; exit 1; }
done

# firmware (record sizes in the PR)
pio run -e esp32s3_n16r8
pio run -e esp32s3_n16r8_validate
```

CI equivalents run automatically (`emulator-build.yml`; firmware `compile-and-release.yml` on
main). CI cannot run `--update` or promotion — those are human-only by construction.

---

## E. Manual review checklist (reviewer runs this, not the author)

- [ ] `git diff --stat` matches the PR's allowed-file list exactly.
- [ ] PR-B: search the diff for `lv_draw` — zero new draw calls; search for
      `#else` inside NATIVE_SIM blocks — counters must not have firmware halves.
- [ ] PR-B: `ScriptCmdType`/`ScriptCmd` changes are strictly appended; doc block at
      `NativeHal.cpp:1317-1343` extended in place.
- [ ] PR-B: guard order inside inserters — refusal checks precede any `removeChild`/`appendChild`.
- [ ] PR-C: open the baseline JSONL; header sane (`assembly_renderable:false`, fixture hash
      present); spot-check 3 cases against intuition (a `frac(1;2)` root box ~glyph-height×2).
- [ ] Sweep/suite outputs pasted in the PR match the reviewer's own local run.
- [ ] Negative-control outputs pasted (assert FAIL exit 4; snapshot corruption exit 1).
- [ ] Flash delta line pasted (PR-B) and ≤ +2 KB.

## F. Reviewer questions (ask these verbatim in review)

1. "Which existing script comes closest to any cap, and what is its margin?" (expected: depth 5
   of 16 — if the author can't answer from the audit, the cap analysis wasn't done.)
2. "What happens on the 8th nested fraction — exactly which check fires, and what state
   changed?" (expected: `projected = 14+3 > 16`, `InsertRefusal::Depth`, zero mutation.)
3. "Why can't the toast appear in any golden?" (expected: created lazily on first refusal;
   no golden script refuses.)
4. "What breaks if fonts are regenerated tomorrow?" (expected: metrics parity step fails;
   snapshots refuse `--update` until fixture refreshed; goldens unchanged only if metrics
   unchanged.)
5. "Show me the pass-2 identity evidence." (expected: harness log line, not an assurance.)
6. "Which counters are per-canvas and which are per-editor?" (expected: §D table answer,
   refusal is per-editor.)

## G. Known failure patterns (and their signatures)

| Pattern | Signature | Response |
|---|---|---|
| Counter increment accidentally outside `#ifdef NATIVE_SIM` | firmware flash delta > +2 KB, or firmware build error on `std::printf` | move under guard; rebuild |
| Cap fires on a legitimate existing script | one of the 71 scripts exits 4 with `[INPUT] *-cap` in its log | the guard formula is wrong (likely capture depth double-count) — fix formula, never raise the cap to pass |
| Toast leaks into a capture | golden diff in a calc stem showing a bottom-center pill | toast created eagerly by mistake — make creation lazy on first refusal |
| Off-by-one in refusal ladder | `mathv2_caps_deep_fraction` FAILs at k=7 vs 8 | re-derive per MR-01 §E.5; the script comment must carry the derivation, fix whichever is wrong **with the maintainer's sign-off on the invariant** (tree depth ≤ 16 is the contract) |
| Harness diff on unchanged src | `geometry-drift` classification with zero src edits | nondeterminism — check for uninitialized `LayoutResult` extras on kinds the corpus newly exercises; pass-2 check should already have caught it |
| Fixture drift after LVGL bump | parity step red though no repo change | LVGL resolved version moved (`lvgl@^9.2.0` floats — GT open question); pin or re-baseline deliberately, never mask |
| Script flakiness on waits | assert reads stale state, intermittent FAIL under different `--step-ms` | increase settle `wait` per GHOOK §A.1 (30 after commit; counters need a draw first) |

## H. Contingencies

- **A golden changes unexpectedly** (any Phase-0 PR): STOP. Do not re-bless. Run
  `python scripts/compare-ppm.py <golden> <candidate> --write-diff /tmp/d.ppm`, view the
  magenta region, bisect the diff hunks (`git stash` halves). Phase-0 tickets are
  byte-identical-by-contract (ADR-MR00-07); a diff is a bug in the PR, full stop.
- **Firmware flash grows too much** (> +2 KB in PR-B): the pure helpers (`mathTreeStats`) or the
  cap logic pulled something heavy — check the map file for accidental `std::string`/iostream
  inclusion; the tree walker must be header-light. If irreducible, split: caps stay (they are
  the safety payload), accessors move fully behind NATIVE_SIM (they already should be), and
  report the remainder to the maintainer with the map-file lines.
- **Stack/heap risk appears** (e.g. `mathTreeStats` frame too large, or toast allocation in a
  tight path): the walker's explicit stack is 64 × ~16 B ≈ 1 KB stack — fine; if a review or
  HIL shows otherwise, make the frame array `static` (single-threaded loopTask) and note it.
  Heap: MR-01/MR-02 add zero steady-state allocations (toast is one label on demand); any
  `[MEM]` regression on device is a stop-and-report.

## I. Red flags / stop conditions / branch hygiene

**Stop immediately and ask the maintainer when:**
- any golden or mask would need to change;
- a forbidden file "needs" touching (it doesn't — the plan is wrong or the work belongs to a
  later ticket; say which);
- the guard formula and the H-5 invariant conflict on a real case;
- firmware fails to compile for a reason unrelated to the diff (toolchain drift — don't "fix"
  `platformio.ini` in a Phase-0 PR);
- LVGL resolved version differs from 9.5.0 in a fresh CI run.

**Ask (not stop) when:** naming bikesheds (command/key names — defaults in the plans stand
unless overruled); whether PR-D lands now or later; CODEOWNERS for
`tests/host/golden_geometry/**`.

**Branch hygiene:** one ticket = one branch = one PR; branch from fresh `main` only; no force
pushes after review starts except rebase-on-main requested by the reviewer; PR description
carries the checklist from the ticket's execution plan §M/§N with every box ticked and outputs
pasted; commit messages name the ticket (`MR-01: …`).

*End of Phase-0 acceptance & rollback plan.*
