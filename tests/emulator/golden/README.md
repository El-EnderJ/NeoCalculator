# Accepted golden screenshots — human review gate

This directory holds **accepted, human-reviewed** golden screenshots (PPM P6,
320×240) for the NumOS SDL2 emulator. A file here is ground truth: CI compares
freshly generated candidate screenshots against it byte-for-byte and **fails on
mismatch**.

Because a golden is authoritative, it must only enter the tree through a
deliberate human review — never automatically. This mirrors the existing
acceptance practice in [`docs/math-renderer-acceptance.md`](../../../docs/math-renderer-acceptance.md)
(geometry is *frozen* only after human review).

## Directory & naming

| Role | Location | Committed? |
|:--|:--|:--|
| Replay script | `tests/emulator/scripts/<name>.numos` | yes |
| **Accepted golden** | `tests/emulator/golden/<name>.ppm` | **yes** |
| Ignore-rect mask (optional) | `tests/emulator/masks/<name>.mask` | yes |
| Candidate (machine-generated) | `out/emulator-candidates/<name>.ppm` | no (`out/` is git-ignored) |

The stem is identical across all of them; they differ only by directory and
extension. Current candidate set: `launcher_smoke`, `calc_1_plus_2`,
`calc_fraction_sum`.

A golden may have an optional **ignore-rect mask** (Phase 4B-B) covering a known
nondeterministic region — today only the CalculationApp blinking cursor. With a
mask, CI ignores those pixels; with no mask the compare is byte-exact. Masks are
also human-gated; see [`tests/emulator/masks/README.md`](../masks/README.md).

## How a candidate becomes a golden (the review gate)

1. **Generate** candidates deterministically (ideally on the pinned reference OS
   — Linux + `SDL_VIDEODRIVER=dummy`, which CI uses):

   ```bash
   pio run -e emulator_pc
   python scripts/generate-emulator-candidates.py
   # -> out/emulator-candidates/*.ppm  (320x240, 230415 bytes each)
   ```

2. **Visually inspect** the candidate and confirm it is actually correct
   (e.g. `calc_1_plus_2` really renders `3`, `calc_fraction_sum` really renders
   `5/6`). View a PPM with GIMP / ImageMagick `display` / IrfanView, or convert:
   `convert out/emulator-candidates/calc_1_plus_2.ppm calc.png`.

3. **Promote** — only after a human confirms correctness — by copying the
   candidate here and committing it. Use the helper (Phase 4B-D) so the copy is
   byte-exact and you get the SHA-256 + the compare mode CI will use:

   ```bash
   # See what's promotable (read-only): stem, sha256, golden/mask state.
   python scripts/promote-emulator-golden.py --list-candidates
   # Preview without writing anything:
   python scripts/promote-emulator-golden.py --dry-run calc_1_plus_2
   # Promote (refuses if the candidate is missing or not a valid 320x240 PPM,
   # and refuses to clobber an existing golden without --force):
   python scripts/promote-emulator-golden.py calc_1_plus_2
   git add tests/emulator/golden/calc_1_plus_2.ppm
   git commit -m "Accept golden: calc_1_plus_2 renders 3"
   ```

   The tool only does the copy + reporting step — it **never** generates images,
   **never** edits masks, and **never** commits. The commit is the human
   acceptance record. The equivalent manual copy is still valid:

   ```bash
   cp out/emulator-candidates/calc_1_plus_2.ppm tests/emulator/golden/calc_1_plus_2.ppm
   ```

   Re-blessing a changed golden follows the same path with `--force`; review the
   old-vs-new diff in the PR (see *Updating a golden* below).

### Promoting a golden that needs a mask (cursor nondeterminism)

Some screens (the calc screens) are not byte-stable across separate launches
because the input cursor blinks. To promote one safely:

1. Prove the volatility: regenerate the same candidate twice and diff them —
   `python scripts/compare-ppm.py a.ppm b.ppm --write-diff jitter.ppm`. The
   magenta region is the minimum the mask must cover (it should be only the cursor
   band, nothing else).
2. Visually confirm the candidate is **semantically correct** (e.g. `1+2` really
   renders `3`). Masking the cursor does not relax this judgement.
3. Author a tight `tests/emulator/masks/<name>.mask` (see that directory's README)
   covering the cursor band, with a comment explaining why.
4. Verify the mask closes the gate: `compare-ppm.py a.ppm b.ppm --mask-file
   tests/emulator/masks/<name>.mask` must report IDENTICAL (exit 0).
5. Commit the golden **and** the mask together so review sees both. The promote
   helper copies the golden and prints `masked compare (…)` when a mask already
   exists for that stem (it never writes the mask — you author and stage that by
   hand):

   ```bash
   python scripts/promote-emulator-golden.py calc_1_plus_2   # copies the golden
   git add tests/emulator/golden/calc_1_plus_2.ppm tests/emulator/masks/calc_1_plus_2.mask
   git commit -m "Accept golden+mask: calc_1_plus_2 renders 3 (cursor region masked)"
   ```

## Updating a golden after an intentional UI / render change

When a deliberate change legitimately alters the rendered pixels, the old golden
*should* now mismatch — that is the gate doing its job. To re-bless it:

1. Regenerate candidates from the changed build
   (`python scripts/generate-emulator-candidates.py`).
2. **Visually re-confirm** the new candidate is correct — the bar is the same as a
   first promotion; a passing-by-construction copy is not a review.
3. Re-promote with `--force` (the tool refuses to clobber a golden otherwise):
   `python scripts/promote-emulator-golden.py --force <stem>`.
4. Commit the changed golden in its own PR; the diff (old vs new bytes) is the
   record reviewers approve. If the mask must also change, stage it in the same
   commit and justify it (see [`../masks/README.md`](../masks/README.md)).

## Guarantees

- **No *automated* step writes here.** `scripts/generate-emulator-candidates.py`
  only writes to `out/emulator-candidates/`; CI only *reads* this directory and
  never invokes the promote tool. The only writer is
  `scripts/promote-emulator-golden.py` — a **human-run** convenience that you
  must invoke by hand with an explicit stem (no implicit "promote all"), and it
  never commits. So a golden still only enters the tree through a deliberate
  human `git commit`.
- **CI cannot ossify a bad screenshot.** When a golden is **absent**, CI prints a
  warning, uploads the candidate, and **does not fail** — so an unreviewed image
  can never silently become a passing gate. When a golden is **present**, CI
  compares against it (`scripts/compare-ppm.py`) and fails on mismatch.
- **Recommended:** require PR review for changes under `tests/emulator/golden/**`
  so blessing/altering a golden is always attributable.

## Scope (Phase 4B-A / 4B-B)

Byte-comparison proves a candidate matches an accepted image; it does **not**
itself assert semantic correctness. Semantic correctness is the human's
judgement at promotion time. Phase 4B-B adds *cursor-safe masking* so calc
screenshots can be compared despite blink nondeterminism — it still does not read
the result. OCR / automated result extraction (proving the pixels actually read
`3` without a hand-reviewed golden) is a later phase (Phase 4B-C).
