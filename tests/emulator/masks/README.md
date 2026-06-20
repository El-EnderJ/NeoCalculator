# Ignore-rect masks — known volatile screenshot regions

This directory holds **ignore-rect mask files** for the NumOS SDL2 emulator visual
tests. A mask lets [`scripts/compare-ppm.py`](../../../scripts/compare-ppm.py)
tolerate *one known, justified* class of non-deterministic pixels — the StatusBar
`HH:MM` clock on app screens (`calc_1_plus_2`, `calc_fraction_sum`,
`settings_smoke`, `math_showcase_smoke`) — without weakening byte-comparison
anywhere else.

Masking is the **narrow exception** to byte-exact comparison — not the rule. It
exists only because the clock is genuinely nondeterministic across separate
launches; it is **not** a way to paper over a rendering bug. The renderer is frozen
(see [`docs/math-renderer-acceptance.md`](../../../docs/math-renderer-acceptance.md)),
so the clock jitter is fixed in the *comparator*, never in `src/`.

## Why this exists (the StatusBar clock finding)

`launcher_smoke.ppm` is byte-stable across launches (the launcher has no StatusBar,
hence no clock — so it carries **no** mask and is gated byte-for-byte). Every screen
that hosts a `ui::StatusBar` is **not** byte-stable: the bar draws a live `HH:MM`
clock from the real wall clock (`StatusBar::updateClock()` →
`time()`/`localtime`/`"%02d:%02d"`, [`src/ui/StatusBar.cpp`](../../../src/ui/StatusBar.cpp)),
so a golden blessed at one time vs a candidate generated at another differ inside
the clock glyphs — and only there.

The clock label is aligned `LV_ALIGN_LEFT_MID,6` in `lv_font_montserrat_12`
([`StatusBar.cpp:61-65`](../../../src/ui/StatusBar.cpp#L61)) and is always exactly 5
glyphs (`"HH:MM"`), so its box spans ≈ `x[6..38] y[8..16]`. Because **both** the
hour and the minute digits vary across arbitrary launches, the mask must cover the
*whole* label, not just the minutes. All four app masks therefore share one rect:

```
4,6,37,13      → x[4..40] y[6..18]  (full HH:MM label + ~2px margin)
```

That band touches only the clock — the app title is centered (`x~160`), the
battery/angle/modifier are right-aligned (`x>240`), and the expression/result and
app body render below the `y=24` StatusBar separator.

> **Note — the calc masks are clock masks, not cursor masks.** Earlier revisions
> described `calc_1_plus_2` / `calc_fraction_sum` as masking the CalculationApp
> *blink cursor*. That was a misdiagnosis: in `--deterministic` mode the blink phase
> is a function of the frame index, so the cursor is byte-identical run-to-run; the
> only cross-launch variation is the wall-clock-driven clock. The original calc rect
> `8,6,30,13` (`x[8..37]`) missed the clock's left edge `x[6..7]`, which leaked
> outside the mask and **failed CI** once a candidate was generated at a different
> hour than the golden was blessed. Widening both calc masks to the shared
> `4,6,37,13` (a superset of the old rect) fixed it.

## Naming

| Role | Location | Committed? |
|:--|:--|:--|
| Replay script | `tests/emulator/scripts/<stem>.numos` | yes |
| Accepted golden | `tests/emulator/golden/<stem>.ppm` | yes |
| **Ignore-rect mask** | `tests/emulator/masks/<stem>.mask` | **yes** |
| Candidate (machine-generated) | `out/emulator-candidates/<stem>.ppm` | no (`out/` git-ignored) |

The stem matches the script / golden exactly, so CI can auto-discover a mask:
golden `calc_1_plus_2.ppm` ↔ mask `calc_1_plus_2.mask`. **A mask is optional per
golden.** A golden with no `.mask` is compared byte-for-byte — absence of a mask
never silently weakens a gate. Do **not** add a mask for a byte-stable screenshot
(e.g. `launcher_smoke`).

## File format

Plain text, one ignore-rect per line:

```
# comment lines start with '#'; blank lines are ignored
x,y,w,h        # top-left origin, pixels; trailing comment allowed
```

Rules enforced by `compare-ppm.py`:

- `x,y` must be `>= 0` and `w,h` must be `> 0`, else the comparison errors (exit 2).
- A rect extending past the 320×240 frame is **clipped** to the frame; an off-image
  origin simply ignores nothing.
- Pixels inside any rect are excluded from the mismatch count, the bounding box,
  and the equality decision. In a `--write-diff` image they render flat **teal**.

## Authoring rules (load-bearing)

1. **No tool ever writes here.** Masks enter the tree only through a deliberate
   human `git commit`, alongside the golden they cover. `compare-ppm.py` only
   *reads* masks; `generate-emulator-candidates.py` only writes `out/`.
2. **Keep masks tight.** Cover only the proven-volatile region. Over-masking hides
   real regressions — never mask the expression or result pixels (the `3`, the `5/6`).
3. **Justify every mask.** Each rect needs a comment saying *why* it exists, and
   the mask should be backed by a reproduced jitter diff (regenerate the same
   candidate twice and `compare-ppm.py A B --write-diff jitter.ppm`; the magenta
   region is the minimum the mask must cover).

## Reviewer checklist (so a bad image is not blessed)

- Confirm the candidate is **semantically correct** *before* masking (a human still
  asserts "1+2 really renders 3" — masking the clock does not relax this).
- Confirm the masked region is genuinely nondeterministic, not a bug being hidden.
- Confirm the mask does not cover the expression/result area.
- Reject any mask covering a large fraction of the frame.

See [`tests/emulator/golden/README.md`](../golden/README.md) for the golden+mask
promotion workflow and [`docs/emulator-sdl2-quickstart.md`](../../../docs/emulator-sdl2-quickstart.md)
for the comparator CLI.
