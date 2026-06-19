# Math Renderer Acceptance Report

Date: 2026-06-19

This document freezes the current NumOS natural-display renderer geometry for
fractions and powers after clean validation firmware and hardware photo review.
It is an acceptance record, not a typography redesign plan.

## Accepted Invariants

### Fraction Ink Placement

The ink-aware fraction placement is accepted for the current STIX/LVGL renderer.
The decisive diagnostic is the visible glyph ink gap, not the abstract layout
box gap:

```text
visibleGapAboveBar=3
visibleGapBelowBar=3
visibleGapDelta=0
```

The accepted cases include simple, mixed-row, nested, powered-numerator, powered
denominator, and powered-fraction expressions. The fraction bar is now positioned
between the rendered numerator and denominator ink on the TFT.

### Baseline Contract

The row baseline contract is accepted:

```cpp
childYTop = rowYTop + (rowLayout.ascent - childLayout.ascent)
```

Validation logs showed `baselineMatch=YES` for row children. This remains a
renderer invariant and must not be replaced with a baseline-only `drawRow()`
rewrite.

### Layout/Draw Equality

The layout and draw bounds are accepted for the validated fraction and power
cases:

```text
topDelta=0
botDelta=0
```

The host regression suite allows only the existing +/-1 px integer rounding
tolerance.

### Superscript Policy

The default superscript policy is accepted with no VPAM adjustment:

```text
texShift=6
vpamAdjust=0
expShift=6
clearance=clear
```

The intended policy remains:

```text
LaTeX/OpenType MATH geometry + VPAM calculator readability
```

`NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX` stays opt-in. The production and clean
validation builds use the default value `0`; `+1` is not accepted as the default
unless a future hardware comparison explicitly chooses it.

## Provisional Typography Notes

Nested fractions are accepted provisionally for renderer geometry. More compact
or VPAM-specific nested-fraction typography is a future policy topic, not a
layout correctness bug in this acceptance pass.

Cursor placement inside a denominator or exponent is classified as a
UX/navigation state. It is not evidence of fraction or power geometry failure.

## Debug And Validation Policy

`esp32s3_n16r8_validate` is the clean photo build:

- Math Visual is available.
- Serial diagnostics are enabled.
- Colored ink overlays are disabled.

`esp32s3_n16r8_validate_overlay` is the only build that should show colored
fraction/power guide lines.

Production firmware must not show the Math Visual menu entry, serial trace spam,
or colored validation overlays.

## Future Work

- Add a Math Showcase mode for clean VPAM visual review without diagnostic
  header placeholders.
- Define a nested-fraction typography policy separately from renderer geometry.
- Improve cursor UX when parked inside denominators and exponents.
