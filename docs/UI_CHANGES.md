# UI Changes — MainMenu Refactor & Recent UI Notes

**Last update:** 8 March, 2026

This document summarizes recent UI/layout changes and provides developer guidance
for adding new apps, reproducing the launcher, and debugging startup scroll issues.

## Summary

- The `MainMenu` launcher was refactored from `LV_LAYOUT_GRID` (static col/row descriptors)
  to `LV_LAYOUT_FLEX` with `LV_FLEX_FLOW_ROW_WRAP`. This removes the risk of grid descriptor
  overflow and allows dynamic wrapping across rows.
- Cards now use an explicit fixed size (recommended `CARD_W = 94`, `CARD_H = 78`) and the
  container uses a pad/gap based layout (`lv_obj_set_style_pad_gap(_menuContainer, 10, 0)`).
- Important startup fix: after building the container call `lv_obj_update_layout(container)`
  before calling `lv_group_focus_obj()` or `lv_obj_scroll_to_view()`.

## Developer checklist — Adding a new app (short)

1. Add `AppEntry` to `src/ui/MainMenu.cpp` `APPS[]`:

```cpp
// Example — Fluid2D
{ 14, "Fluid 2D", 0x1E88E5, 0x64B5F6 },
```

2. Ensure `SystemApp.h` exposes the corresponding pointer/instance and `Mode` enum.
3. In `SystemApp::begin()` instantiate the app object (lazy LVGL creation allowed).
4. Rebuild and test: on first load the launcher will call `lv_obj_update_layout()` and
   snap the focused card into view using `lv_obj_scroll_to_view(..., LV_ANIM_OFF)`.

## Key migration points

- If you previously relied on `lv_obj_set_grid_dsc_array()` or `col_dsc[]`/`row_dsc[]`,
  migrate to explicit `lv_obj_set_size(card, CARD_W, CARD_H)` and `LV_LAYOUT_FLEX`.
- Use `lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START)`
  to center cards within rows but anchor rows to the top of the container.
- Call `lv_obj_add_flag(container, LV_OBJ_FLAG_SCROLL_ELASTIC)` for better UX.

## Startup visibility bug (fixed)

Symptoms:
- On power-on, the launcher showed cards vertically centered (hiding the first row).
- `lv_obj_scroll_to_view()` was a no-op because LVGL had not yet computed flex coordinates.

Fix applied:
1. `lv_obj_update_layout(_menuContainer);` // forces immediate layout computation
2. `lv_group_focus_obj(_firstCard);`
3. `lv_obj_scroll_to_view(_firstCard, LV_ANIM_OFF);`

This sequence guarantees `Calculation` (App ID 0) is visible and focused at startup.

## Notes on shadows & performance

- Reduce `shadow_width` on many small cards (e.g., `4` instead of `12`) to avoid expensive
  draw surfaces being created during scroll; use `onScrollBegin`/`onScrollEnd` callbacks to
  temporarily disable shadows while scrolling.

## Cross references

- MainMenu implementation: `src/ui/MainMenu.h` / `src/ui/MainMenu.cpp`
- System coordinator: `src/SystemApp.h` / `src/SystemApp.cpp`
- Fluid2D plan: `docs/fluid2d_plan.md`
- Migration notes (LVGL): `MIGRACIÓN_LVGL.md`

## New apps to note

- `OpticsLab` (App ID 17) was added and registered in `src/ui/MainMenu.cpp` as:

```cpp
{ 17, "OpticsLab",  0x00838F,   0x4DD0E1 },   // Teal (Optics)
```

Ensure `SystemApp` includes `OpticsLabApp* _opticsLabApp;` and the deferred teardown
case `Mode::APP_OPTICS_LAB` exists (see `src/SystemApp.cpp`). See `docs/OPTICS_LAB.md` for
developer and user notes.

