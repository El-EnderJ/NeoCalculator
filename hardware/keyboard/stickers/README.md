# Keycap label stickers

Print-ready labels for the 50 flat key tops of the NeoCalculator prototype, at true 1:1 scale.

| File | What it is |
|:--|:--|
| `neocalculator-keycap-stickers.pdf` | The sheet: four full keyboard sets on A4 and a small 50 mm scale bar. |
| `neocalculator-keycap-fit-test.pdf` | Fit test: sample stickers at three inset values and the real key outline. |
| `neocalculator-keycap-stickers-preview.png`, `-detail.png` | Previews of the sheet (150 dpi) and of one set (600 dpi). |
| `make_stickers.py`, `stickers.yaml` | Generator and its settings (geometry, type sizes, colours, overrides). |
| `fonts/` | Subsets of Noto Sans (variable) and Noto Sans Math, SIL OFL 1.1 (`fonts/OFL.txt`). |

## Printing

1. Print the **fit test** first, on plain paper. In the print dialog choose **100 % / "Actual size"**. Turn **off** "Fit to page" and "Shrink oversized pages".
2. Check the scale bar with a ruler. It must measure **50 mm**, end tick to end tick. If it doesn't, fix the scaling in the print dialog and print again.
3. Cut a few test stickers and try them on a key. Each row uses a different inset: 0.2, 0.4 or 0.6 mm per side. If you prefer a row other than 0.4, set `sticker.inset` in `stickers.yaml` and regenerate.
4. Print the **sheet** on A4 sticker paper with the same settings.
5. Cut each sticker along its outline. The thin darker edge is the cut line, and the gaps are 5 mm, so scissors are enough.
6. Stick each label centred on its key. Each set is laid out like the keypad, display side up. The background tint shows the key group, so you can sort the stickers after cutting.

## Design

- **Geometry:** key top 10.73 × 8.33 mm, r 1.765 mm. Measured from `hardware/enclosure/key.stl` and cross-checked against the 50 openings in `case-top.stl`. The sticker is 0.4 mm smaller per side: 9.93 × 7.53 mm. Re-measure with `--measure`.
- **Legends:** taken from `neocalculator-v1-final-5x10-revision-c-canonical.json` and checked plane by plane (primary / SHIFT / ALPHA) against `src/input/generated/ProductionKeypadMap.generated.h`. Any mismatch stops the build.
- **Type:** all text is shaped with HarfBuzz (kerning, case forms) and converted to vector outlines, so the PDF looks the same in every viewer and printer.
- **Width:** long words are narrowed on Noto Sans's own width axis instead of being stretched. Arrows and math operators come from Noto Sans Math, thickened to the same stem weight as the text.
- **Sizes by key type:** each key type has a fixed cap height and shares one size, so a row of keys reads evenly. The types are digits, operators, punctuation, function names (sin, log …), short words (DEL, AC, EXE, VAR), long words (SHIFT, HOME, FORMAT …) and math templates. All of these are set in `stickers.yaml`.
- **Math:** typeset rather than written as text:
  - variables in italic (x, y, t, θ, n), while e, i and π stay upright per ISO 80000-2;
  - real superscripts (`x²`, `sin⁻¹`, `10ˣ`), true minus signs;
  - fractions, radicals and placeholder boxes drawn at the text's stem weight.
- **Secondary legends:** SHIFT is upper-left in amber and ALPHA is upper-right in orange, as the layout JSON specifies. They hang from the same top line and use the corner space that the rounded corners leave free.
- **Not printed:** legends the layout marks `printLegend: false`. These are SHIFT+1…6 (F1–F6), the SHIFT/ALPHA locks and the combined SHIFT+ALPHA plane. Set `print_hidden_legends: true` to print them anyway.
- **Overrides:** change a legend with `label_overrides`, e.g. `r6c3: {primary: "⌫"}`.

The build also lists printed legends with no handler in `src/`. At the time of writing these are SHIFT+VAR `MGR`, SHIFT+TOOLS `CATALOG`, SHIFT+STO `RCL` and SHIFT+DEL `UNDO`. SHIFT+AC `OFF` resolves, but production OFF is deliberately deferred in `SystemApp::powerOff()`.

Full legend table: `python hardware/keyboard/stickers/make_stickers.py --labels`

## Regenerating

```
pip install reportlab pyyaml fonttools uharfbuzz numpy pymupdf
python hardware/keyboard/stickers/make_stickers.py --verify
```

`--verify` checks the following, then rewrites the previews:

- the page is exactly 210 × 297 mm;
- every sticker matches the configured size to within 0.01 mm;
- gaps are at least the configured minimum and margins at least 10 mm;
- every legend outline lies inside its sticker, at least 0.3 mm from the edge;
- there is no live text.
