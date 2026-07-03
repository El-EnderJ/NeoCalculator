# NumOS — MathRenderer v2 Phase 0: Ready-to-Paste Implementation Prompts

> Six prompts for later model sessions (Opus/Sonnet implementation, Fable adversarial review,
> emergency triage, human review). Paste each verbatim as the session's task; the prompts assume
> the repo is checked out and the Phase-0 spec pack (this directory) is merged.
> Source of truth: `NUMOS_MATHRENDERER_MR01_OBSERVABILITY_AND_CAPS_EXECUTION_PLAN.md` (MR01-PLAN),
> `NUMOS_MATHRENDERER_MR02_GEOMETRY_HARNESS_EXECUTION_PLAN.md` (MR02-PLAN),
> `NUMOS_MATHRENDERER_MR00_DECISION_RECORDS.md` (ADRs),
> `NUMOS_MATHRENDERER_PHASE0_ACCEPTANCE_AND_ROLLBACK_PLAN.md` (ACC-PLAN).

---

## Prompt 1 — MR-01 implementation (Opus/Sonnet)

```
You are implementing ticket MR-01 (renderer observability + editor caps) in the NumOS repo.
Read first, in order:
  docs/specs/NUMOS_FABLE_CONTEXT_HEADER.md
  docs/specs/NUMOS_MATHRENDERER_MR00_DECISION_RECORDS.md          (ADR-MR00-03/05 govern you)
  docs/specs/NUMOS_MATHRENDERER_MR01_OBSERVABILITY_AND_CAPS_EXECUTION_PLAN.md   (your exact spec)
  docs/specs/NUMOS_GRAPHER_ASSERT_HOOKS_SPEC.md                   (the .numos grammar conventions)

Implement EXACTLY what MR01-PLAN §C-§I specifies. Do not invent architecture; where the plan
gives a formula (the structural-insert guard, §C.2), implement that formula; where it gives a
grammar (§H), implement that grammar including parse-time validation and error messages.

ALLOWED FILES (touch nothing else):
  src/math/MathAST.h  src/math/MathAST.cpp
  src/math/CursorController.h  src/math/CursorController.cpp
  src/ui/MathRenderer.h  src/ui/MathRenderer.cpp
  src/apps/CalculationApp.h  src/apps/CalculationApp.cpp
  src/hal/NativeHal.cpp
  tests/emulator/scripts/mathv2_obs_normal.numos
  tests/emulator/scripts/mathv2_caps_deep_fraction.numos
  tests/emulator/scripts/mathv2_caps_deep_power.numos
  tests/emulator/scripts/mathv2_caps_digits.numos
  tests/emulator/scripts/mathv2_boss_subtree_depth.numos
  tests/emulator/scripts/mathv2_glyph_vector_delims.numos
  tests/emulator/scripts/mathv2_metrics_dump.numos

FORBIDDEN (hard): src/math/MathEvaluator.*, src/math/MathTypography.h, src/ui/MathTypography.*,
src/math/font/**, src/fonts/**, src/apps/GrapherApp.*, src/input/**, platformio.ini,
tests/emulator/golden/**, tests/emulator/masks/**, scripts/**, .github/**, lib/**.
PROHIBITED SCOPE: no LayoutBudget/⚠ box/draw-cap change (MR-05), no NumIR/type_numir (MR-03),
no eval refusal chips (MR-09), no new node kinds, no pixel-affecting change of any kind.

RULES: every MathRenderer.cpp hunk inside draw functions must be enclosed in #ifdef NATIVE_SIM;
ScriptCmdType/ScriptCmd changes are append-only; refusal guards run BEFORE any tree mutation;
log lines exactly as MR01-PLAN §C.3/§C.4 ([INPUT] depth-cap max=16, [LVGL] layout-budget …,
[MATHMETRICS] …); every pinned number in a script carries its derivation as a comment.

RUN (all must pass; paste outputs in your report):
  export PLATFORMIO_BUILD_DIR=.pio/build && pio run -e emulator_pc
  # the 7 scripts (exit 0 each), the negative control (exit 4, uncommitted),
  # the golden sweep (all 19 identical), the 8C/9A/9B/9F suites,
  # the keycode guard, pio run -e esp32s3_n16r8 and _validate (record flash delta)
  # — verbatim command blocks: MR01-PLAN §J.

STOP CONDITIONS (stop, report, do not work around): any golden differs; any existing script
fails; a forbidden file seems necessary; flash delta > +2 KB; the guard formula contradicts an
observed case; firmware build breaks for unrelated reasons.

FINAL REPORT (required): files changed with line counts; the §M checklist with every box and
pasted evidence; flash delta; the derivation table for the two refusal ladders; any deviation
from MR01-PLAN with justification (deviations without justification are rejected).
Commit to a fresh branch cut from main; do NOT push goldens; do NOT open the PR as merged.
```

## Prompt 2 — MR-02 implementation (Opus/Sonnet)

```
You are implementing ticket MR-02 (host math-layout geometry harness) in the NumOS repo.
Read first: docs/specs/NUMOS_FABLE_CONTEXT_HEADER.md, NUMOS_MATHRENDERER_MR00_DECISION_RECORDS.md
(ADR-MR00-06 governs you), NUMOS_MATHRENDERER_MR02_GEOMETRY_HARNESS_EXECUTION_PLAN.md (your
exact spec), NUMOS_CAS_HOST_HARNESS_SPEC.md (build-style precedent).

PREREQUISITE: MR-01 is merged (you need tests/emulator/scripts/mathv2_metrics_dump.numos and
the [MATHMETRICS] dump). Verify before starting; stop if absent.

ALLOWED FILES (create only these; touch nothing else):
  tests/host/mathlayout_geometry_test.cpp
  tests/host/mathlayout_fixed_metrics.h
  tests/host/golden_geometry/mathv2_geometry.jsonl
  scripts/build-mathlayout-host-tests.sh
  scripts/check-math-metrics-fixture.py

FORBIDDEN (hard): ALL of src/** — you may not change ONE LINE of renderer behavior; if the
harness "needs" a src change, the harness design is wrong — re-read MR02-PLAN §D/§E and stop if
still blocked. Also forbidden: goldens/masks, platformio.ini, .github/** (CI wiring is PR-D),
existing scripts/*.py, tests/emulator/**.

BUILD MODEL (proven, do not deviate): plain g++ -std=c++17 -O1 -Wall -Wextra
-Wno-unused-parameter -Wno-unused-variable -I src over exactly:
  tests/host/mathlayout_geometry_test.cpp src/math/MathAST.cpp
  src/math/font/MathGlyphAssembly.cpp src/math/MathRenderVisualCases.cpp
  src/math/MathStressExpressions.cpp
No LVGL, no SDL, no defines. FontMetrics come ONLY from the committed fixture header, captured
by the exact procedure MR02-PLAN §D.4 (run it; transcribe; hash; record git sha).

IMPLEMENT: the corpus (94 built cases + 4 future SKIPs, ids exactly as MR02-PLAN §F), the JSONL
schema (§E, field names and order frozen), double-pass identity (§H), the comparator with change
classification (§G), the CLI (§D.6), the parity checker script (§D.4), the --cache-off build
flag pass-through (§H).

RUN (paste outputs): scripts/build-mathlayout-host-tests.sh (footer:
"[MATH-GEO] cases=98 built=94 skipped_future=4 mismatched=0 exit=0"); the negative control
(corrupt one baseline digit -> exit 1 with path/field diagnostics; revert); the fixture-corruption
control (checker exit 1); the metrics parity run against the emulator; the full golden sweep
(unchanged — prove it); git diff --stat (must show zero src/** lines).

STOP CONDITIONS: any src/** edit appears necessary; the two-TU compile fails (report the exact
error — do NOT patch src to fix it); pass-2 differs from pass-1 anywhere; a corpus builder can't
be expressed with existing factories (mark it future/SKIP and report, don't approximate).

FINAL REPORT: files created with line counts; the MR02-PLAN §N checklist with pasted evidence;
the fixture values and capture sha; corpus count by category; runtime numbers.
Commit to a fresh branch from main.
```

## Prompt 3 — Fable adversarial review of the MR-01 PR

```
You are adversarially reviewing the MR-01 PR (renderer observability + editor caps) against
docs/specs/NUMOS_MATHRENDERER_MR01_OBSERVABILITY_AND_CAPS_EXECUTION_PLAN.md and
ADR-MR00-03/05/07. Your job is to FIND the defect, not to approve. Assume the implementer was
competent and still made 2-3 mistakes. Read the full diff, then attack these surfaces in order:

1. PIXEL SAFETY: enumerate every hunk in src/ui/MathRenderer.cpp; for each, prove it is inside
   #ifdef NATIVE_SIM or is a comment. Any executable firmware-visible line in a draw/layout
   path = finding. Then actually run the golden sweep yourself (ACC-PLAN §D) — do not trust the
   pasted output.
2. GUARD CORRECTNESS: derive by hand the refusal points for (a) the FRAC ladder, (b) the POW
   ladder, (c) FRAC capturing a depth-12 subtree, (d) FRAC at cursor depth 13 with a leaf left
   operand, (e) insertParen then FRAC inside. Compare each against the code's projected-depth
   formula and against the scripts' pinned numbers. Check the guard runs before ANY mutation
   (search each inserter for removeChild/appendChild preceding the guard). Check backspace and
   navigation are unguarded. Check the H-5 invariant: construct (mentally or via a throwaway
   script) the worst accepted tree and verify max depth ≤ 16.
3. GRAMMAR SAFETY: verify ScriptCmdType values appended only (compare enum order to main);
   verify every new command parse-validates all keys (feed it a bad key -> exit 2); verify
   verdict strings go through assertPass/assertFail; verify gating FAILs (not no-ops) outside
   Calculation/MathShowcase; verify no existing command branch was edited.
4. COUNTER SEMANTICS: per-draw latching (two canvases drawing in one refresh must not corrupt
   each other's latches); layout-pass counter increments at all three sites (MathRenderer.cpp
   591/629/737 equivalents post-diff); budget check is setExpression-only on firmware and
   per-draw on emulator as specified; walker saturation at 64 handled in every assert path.
5. LIFECYCLE: toast label+timer destroyed/nulled in end(); no timer leak across app exits
   (run grapher_home_return-style enter/exit of Calculation twice with a forced refusal).
6. DETERMINISM: run every mathv2_* script twice, SHA-256 the stdout — must match.
7. FLASH: verify the claimed delta by building esp32s3_n16r8 yourself if the toolchain exists;
   otherwise inspect for accidental firmware-side <cstdio>/<string> pulls in the new helpers.

FORBIDDEN to you: pushing fixes to the PR branch (report findings; the implementer fixes),
re-blessing anything, editing goldens.
FINAL REPORT: numbered findings with file:line and a one-line repro each, severity
(blocker/major/minor), plus explicit "attacked and held" notes for each of the 7 surfaces.
An empty findings list requires the held-notes to show real work (commands run, derivations).
```

## Prompt 4 — Fable adversarial review of the MR-02 PR

```
You are adversarially reviewing the MR-02 PR (geometry harness) against
docs/specs/NUMOS_MATHRENDERER_MR02_GEOMETRY_HARNESS_EXECUTION_PLAN.md and ADR-MR00-06.
Attack surfaces, in order:

1. SRC PURITY: git diff --stat origin/main -- src/  must be EMPTY. Any src line = blocker.
2. SECOND-RENDERER CREEP: search the harness for reimplemented policy — any arithmetic that
   mirrors fractionBarGaps/superscriptShiftMetrics/interAtomSpacing walks is a finding; the
   only allowed derived quantity is Row gapSum = w − Σ(child w). Verify extras come from public
   getters only.
3. FIXTURE HONESTY: re-run the capture procedure (MR02-PLAN §D.4) yourself; diff against the
   committed header; verify the hash; verify the parity checker fails on a mutated field and on
   a missing profile line. Verify the harness asserts g_delimiterAssemblyRenderable == false at
   startup and records it in the header line.
4. BASELINE QUALITY: pick 5 cases across categories; hand-check plausibility (frac root height
   ≈ num+den+gaps; pow width ≈ base+exp(scaled)+spaceAfterScript; paren taller than content).
   Verify per-node paths are unique per case (duplicate-path detector: sort|uniq -d).
   Verify the 4 future rows SKIP visibly in the log and are absent from the JSONL.
5. COMPARATOR RIGOR: corrupt (a) one digit, (b) delete a case line, (c) append a bogus case,
   (d) change the header fixture hash — each must exit 1 with the correct classification
   (geometry-drift / corpus-change / corpus-change / fixture-mismatch). --update must refuse on
   (d) and must be absent from all CI/step code.
6. DETERMINISM: run the harness 3× and on a second machine/compiler if available; byte-compare
   candidates. Verify pass-1/pass-2 identity is checked per case, not globally.
7. PORTABILITY TRAPS: script must not write inside the repo (candidate under $OUT), must honor
   CXX, must not require bash-isms beyond what run-emulator-linux.sh already uses.

FINAL REPORT: same format as Prompt 3 (numbered findings + held-notes per surface).
```

## Prompt 5 — Emergency rollback / bug triage (goldens shifted)

```
ALERT CONTEXT: after a Phase-0 MRV2 merge, CI's golden compare step fails (or a human reports a
visual diff). Phase-0 tickets are byte-identical-by-contract (ADR-MR00-07): any golden diff is
a DEFECT, never a re-bless candidate. Your job: contain, localize, decide revert-vs-fix.

STEP 1 — CONTAIN (first 10 minutes):
  git log --oneline -10           # identify the suspect merge(s) since last green
  export PLATFORMIO_BUILD_DIR=.pio/build && pio run -e emulator_pc
  python scripts/generate-emulator-candidates.py
  # for each failing stem:
  python scripts/compare-ppm.py tests/emulator/golden/<stem>.ppm \
    out/emulator-candidates/<stem>.ppm --write-diff /tmp/<stem>_diff.ppm
  # record: which stems, mismatch pixel counts, bounding boxes (comparator output).

STEP 2 — LOCALIZE:
  scripts/build-mathlayout-host-tests.sh        # if MR-02 is merged: does GEOMETRY also drift?
  #   geometry drifts too  -> measurement change: suspect MathAST/typography/fonts hunks.
  #   geometry clean       -> draw-path change: suspect MathRenderer draw hunks / LVGL version.
  pip list 2>/dev/null | grep -i platformio; grep -n "lvgl" platformio.ini
  cat .pio/libdeps/emulator_pc/lvgl/library.json | grep version   # LVGL 9.5.0 expected; a float
  #   past ^9.2.0 is a known environmental cause (GT open question) — if so, the fix is pinning,
  #   not code, and goldens were never wrong.
  git checkout <merge>^ -- . is FORBIDDEN; instead: git stash-based bisect of the suspect PR's
  hunks, or git bisect between last-green and HEAD running the sweep as the test.

STEP 3 — DECIDE:
  - Defect confirmed in a Phase-0 merge -> git revert <merge-sha> on a branch, push, PR titled
    "Revert MR-0x: golden regression <stems>", attach diff PPMs and the localization evidence.
    Rollback safety per ticket: MR-01/MR-02 have no persisted state (ACC-PLAN §B).
  - Environmental (LVGL bump / runner image) -> do NOT revert repo code; open an issue with the
    version evidence and propose the pin; CI stays red until the pin lands (that is correct —
    red is telling the truth).
ABSOLUTE PROHIBITIONS: promote-emulator-golden.py, editing goldens/masks, adding masks to hide
the diff, force-pushing main.
FINAL REPORT: failing stems + pixel counts; localization verdict (measurement vs draw vs
environment) with evidence; the revert/fix decision and its PR link; time-to-contain.
```

## Prompt 6 — Human PR review checklist (maintainer)

```
Phase-0 MRV2 PR review — 15-minute human pass (models already ran Prompts 3/4).

IDENTITY
[ ] PR maps to exactly one ticket (MR-00/01/02/CI) and its branch was cut from current main.
[ ] Diff file list == the ticket's allowed list (ACC-PLAN §B). Anything extra: reject.

FOR MR-01
[ ] Open src/ui/MathRenderer.cpp diff: every hunk in a draw function is #ifdef NATIVE_SIM.
[ ] Open src/math/CursorController.cpp diff: guards precede mutations; constants are 16/512/32.
[ ] Run locally OR trust two independent pasted sweeps: all 19 goldens identical.
[ ] Flash delta pasted and ≤ +2 KB.
[ ] Hold DIV on device/emulator ~20×: toast appears, nothing crashes, AC recovers.

FOR MR-02
[ ] git diff --stat shows zero src/** lines.
[ ] Fixture header: has capture sha + hash; values match a fresh dump if you have 5 minutes.
[ ] Baseline JSONL: header line sane; spot-open one visual/* case and sanity-check numbers.
[ ] Negative-control outputs pasted (snapshot corruption exit 1; fixture corruption exit 1).

FOR CI WIRING (PR-D)
[ ] Steps appended, none edited; golden-compare fail/warn semantics untouched
    (emulator-build.yml lines around 575-607 unchanged).
[ ] Full CI run green on the PR; new artifacts present.

DECISION RULES
- Any golden/mask/generated-font change in a Phase-0 PR: reject without further reading.
- Any "temporary" cap/counter behavior difference between emulator and firmware not specified
  in MR01-PLAN §D: reject.
- Deviations from the plans are acceptable ONLY with a written justification block in the PR
  and a matching amendment to the spec doc in the same PR.
SIGN-OFF: comment "MR-0x accepted per Phase-0 checklist" — this is the human acceptance record
(MR-00's ratification rule).
```

*End of Phase-0 implementation prompts.*
