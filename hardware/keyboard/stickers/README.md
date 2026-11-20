# Keycap label stickers

Print-ready labels for the 50 flat key tops of the NeoCalculator prototype, at true 1:1 scale.

| File | What it is |
|:--|:--|
| `neocalculator-keycap-stickers.pdf` | Page 1: two full keyboard sets + calibration. Page 2: fit test. |
| `neocalculator-keycap-stickers-preview-p1.png`, `-p2.png` | 200 dpi previews of both pages |
| `make_stickers.py` | Generator (reportlab) |
| `stickers.yaml` | Key size, inset, fonts, colours, label overrides, copies per sheet |

## Printing

1. Print **page 2 (fit test) first**, on plain paper.
2. In the print dialog choose **100 % / "Actual size"**. Turn **off** "Fit to page", "Shrink oversized pages" and any borderless mode.
3. Measure the calibration square with a ruler. It must be **50 mm × 50 mm** (measure between the centres of the lines); the ruler beside it must be 100 mm. If not, fix the print scaling and print again. Don't cut anything until it measures right.
4. Cut a test sticker and hold it on a key. Each row on page 2 uses a different inset (0.2 / 0.4 / 0.6 mm per side). If you prefer another row, set `sticker.inset` in `stickers.yaml` and regenerate.
5. Print **page 1** on A4 sticker paper (inkjet or laser, matching your printer), with the same settings.
6. Cut along the grey outlines. The crop marks around each set line up with whole rows and columns, so long cuts can be made with a steel ruler and a craft knife; finish the rounded corners with scissors.
7. Stick each label centred on its key. The sets are laid out like the keypad, with the display side at the top. The background colour shows the key group (digits, operators, functions, navigation, system, SHIFT, ALPHA), so you can sort the stickers after cutting.

## Geometry (measured, not guessed)

- `key.stl` has a flat top face at z = 5.029 mm: **10.73 × 8.33 mm, corner radius 1.765 mm**. The cap walls have a slight draft (10.79 × 8.39 mm at the flange), and the flange below is 11.6 × 9.2 mm. ASSEMBLY.md uses one `key.stl` for all 50 keys, so there is only one key size.
- Cross-check: `case-top.stl` has 50 identical 11.2 × 8.8 mm openings on a 13.2 × 9.8 mm pitch. That matches the switch pitch in `production-keypad-map.generated.csv`.
- Sticker = key top minus 0.4 mm per side = **9.93 × 7.53 mm**, corner radius 1.365 mm.
- To re-measure: `python hardware/keyboard/stickers/make_stickers.py --measure`

## Legends

The legends come from `neocalculator-v1-final-5x10-revision-c-canonical.json`. Every printed legend is cross-checked, key by key and plane by plane (primary / SHIFT / ALPHA), against the firmware map `src/input/generated/ProductionKeypadMap.generated.h`. Any mismatch stops the build.

- **Large, centred:** the primary function.
- **Upper-left, amber:** what the key does after SHIFT.
- **Upper-right, orange:** what the key does after ALPHA.
- **Not printed:** legends the layout marks `printLegend: false`. These are SHIFT+1…6 (soft keys F1–F6), the SHIFT/ALPHA lock gestures and the combined SHIFT+ALPHA plane. Set `print_hidden_legends: true` to print them anyway.
- **To change a legend:** add it to `label_overrides`, e.g. `r6c3: {primary: "⌫"}`.

The build also lists printed secondary legends that have no handler anywhere in `src/`. At the time of writing these are SHIFT+VAR `MGR`, SHIFT+TOOLS `CATALOG`, SHIFT+STO `RCL` and SHIFT+DEL `UNDO`. In the firmware they resolve to a semantic ID with `KeyCode::NONE` and no insert text. SHIFT+AC `OFF` resolves, but production OFF is deliberately deferred in `SystemApp::powerOff()`: it only turns the backlight off.

Full legend table: `python hardware/keyboard/stickers/make_stickers.py --labels`

## Regenerating

```
pip install reportlab pyyaml fonttools numpy pymupdf
python hardware/keyboard/stickers/make_stickers.py --verify
```

`--verify` checks that both pages are exactly 210 × 297 mm. It also checks that every sticker matches the configured size to within 0.01 mm, that gaps are at least 3 mm and margins at least 10 mm, that text stays inside its sticker and that all fonts are embedded. It then rewrites the PNG previews. The font is DejaVu Sans (embedded subset), which covers every glyph used. If it isn't installed, the script falls back to the copy bundled with matplotlib.
