# Keycap label stickers

Print-ready labels for the 50 flat key tops of the NeoCalculator prototype, at true 1:1 scale.

| File | What it is |
|:--|:--|
| `neocalculator-keycap-stickers.pdf` | The sheet: 7 complete keyboards and 40 spares (390 stickers) on one A4 page, with a small 50 mm scale bar. |
| `neocalculator-keycap-stickers-1-keyboard.pdf` | A single keyboard (50 stickers) in the top-left corner of the page. |
| `neocalculator-keycap-fit-test.pdf` | Fit test: sample stickers at three inset values and the real key outline. |
| `neocalculator-keycap-stickers-preview.png`, `-detail.png` | Previews of the sheet (150 dpi) and of one set (600 dpi). |
| `make_stickers.py`, `stickers.yaml` | Generator and its settings (geometry, type sizes, colours, overrides). |
| `fonts/` | Subsets of Noto Sans (variable) and Noto Sans Math, SIL OFL 1.1 (`fonts/OFL.txt`). Computer Modern comes with matplotlib. |

## Printing

1. Print the **fit test** first, on plain paper. In the print dialog choose **100 % / "Actual size"**. Turn **off** "Fit to page" and "Shrink oversized pages".
2. Check the scale bar with a ruler. It must measure **50 mm**, end tick to end tick. If it doesn't, fix the scaling in the print dialog and print again.
3. Cut a few test stickers and try them on a key. Each row uses a different inset: 0.2, 0.4 or 0.6 mm per side. If you prefer a row other than 0.4, set `sticker.inset` in `stickers.yaml` and regenerate.
4. Print the **sheet** on A4 sticker paper with the same settings.
5. Cut each sticker along its outline: the thin darker edge is the cut line. The gaps are about 3 mm, so a craft knife and a steel ruler along whole rows and columns, then scissors for the corners, work best.
6. Stick each label centred on its key, display side up. The background tint shows the key group, so you can sort the stickers after cutting.

### Sheet layout

The page is one uniform grid of 15 × 26 stickers with 2.93 mm gaps. That is the widest gap that still fits three keyboards across inside 10 mm margins, and no 8th keyboard can fit: it would need 400 slots.

- **Keyboards 1–6:** three across and two down, each laid out like the keypad.
- **Keyboard 7:** in the band below, split in two: its top half (rows 1–5) on the left and its bottom half (rows 6–10) in the middle.
- **Spares:** another bottom half on the right, then a last row with the digit and operator rows.

## Design

- **Geometry:** key top 10.73 × 8.33 mm, r 1.765 mm. Measured from `hardware/enclosure/key.stl` and cross-checked against the 50 openings in `case-top.stl`. The sticker is 0.4 mm smaller per side: 9.93 × 7.53 mm. Re-measure with `--measure`.
- **Legends:** taken from `neocalculator-v1-final-5x10-revision-c-canonical.json` and checked plane by plane (primary / SHIFT / ALPHA) against `src/input/generated/ProductionKeypadMap.generated.h`. Any mismatch stops the build.
- **Math in TeX:** every mathematical legend is typeset by matplotlib's `mathtext` engine, which follows TeX's layout rules. This covers variables in math italic (x, y, t, θ, n), `\sin`, `\log` and `\ln` as upright operators, and TeX superscripts, subscripts, fractions and radicals.
  - **Font:** **Computer Modern**, the LaTeX fonts, with a very light added stroke (`tex.embolden_em`). More than that fills the letters' counters when printed small. `tex.font: stix-bold` is a sturdier alternative.
  - **Constants:** `e` and `i` are upright (`\mathrm`, ISO 80000-2); set `tex.upright_constants: ""` for italic.
  - **Empty slots:** laid out by TeX, then made geometric and drawn as outline boxes:
    - numerator and denominator are the same size, sit the same distance from the bar and stay centred on it;
    - the whole part of a mixed fraction is the same box, centred on the bar;
    - in `|□|` the box is centred between the bars.

    Boxes are at least 0.85 mm (0.95 mm in fractions), and rules and boxes are never thinner than 0.13 mm.
  - **Fractions:** always stacked. The main legend uses a display-style fraction. In a corner, a stacked fraction such as the mixed-fraction legend may hang lower than the corner line (`typography.secondary.max_drop`), so it keeps a readable size. When that happens, the key's main legend is placed whichever of two ways gives it the larger size:
    - it uses the whole key height at the same visual centre as the neighbouring keys, moved right just enough to keep `secondary.clearance` (0.6 mm) from the corner legends, but never more than `secondary.max_shift` (0.8 mm), so the □/□ key keeps a big fraction close to centre;
    - it sits underneath the corner legends.
- **Everything else in Noto Sans:** words, digits and arithmetic keys use Noto Sans, shaped with HarfBuzz (kerning, case forms). All text is converted to vector outlines, so the PDF looks the same in every viewer and printer.
- **Width:** long words are narrowed on Noto Sans's own width axis instead of being stretched. Arrows and math operators come from Noto Sans Math, thickened to the same stem weight as the text.
- **Sizes by key type:** each key type has a fixed cap height and shares one size, so a row of keys reads evenly. The types are digits, operators, punctuation, function names (sin, log …), short words (DEL, AC, EXE, VAR), long words (SHIFT, HOME, FORMAT …) and math templates. All of these are set in `stickers.yaml`.
- **Root template:** the nth-root template shows both slots (index and radicand), because that is what the key inserts.
- **Secondary legends:** SHIFT is upper-left in amber and ALPHA is upper-right in orange, as the layout JSON specifies. They hang from the same top line and use the corner space that the rounded corners leave free.
- **Not printed:** legends the layout marks `printLegend: false`. These are SHIFT+1…6 (F1–F6), the SHIFT/ALPHA locks and the combined SHIFT+ALPHA plane. Set `print_hidden_legends: true` to print them anyway.
- **Icons:** `icon:<name>` in `label_overrides` draws a vector line icon instead of text, at the weight of the legends around it. Available: `toolbox` (TOOLS), `space` (ALPHA+0, the space-bar sign ⎵), `back` (BACK, a curved return arrow) and `home` (HOME, a house; its SHIFT legend SETUP stays).
- **Template boxes:** placeholder boxes are laid out by TeX and then made geometric. Numerator and denominator are the same size and sit the same distance from the bar. A mixed fraction's whole-part box is the same size and centred on the bar.
- **Dark keys (EXE):** corner legends are larger, heavier and set in the sans, because thin light strokes get swallowed by the black ink around them. The ≈ comes from Noto Sans Math exactly as designed. It is not thickened like other math-font symbols, because that closes the gap between its two waves (`typography.keep_as_designed`).
- **Overrides:** change a legend with `label_overrides`, e.g. `r6c3: {primary: "⌫"}`.

The build also lists printed legends with no handler in `src/`. At the time of writing these are SHIFT+VAR `MANAGER`, SHIFT+TOOLS `CATALOG`, SHIFT+STO `RCL` and SHIFT+DEL `UNDO`. SHIFT+AC `OFF` resolves, but production OFF is deliberately deferred in `SystemApp::powerOff()`.

Full legend table: `python hardware/keyboard/stickers/make_stickers.py --labels`

## Regenerating

```
pip install reportlab pyyaml fonttools uharfbuzz numpy matplotlib pymupdf
python hardware/keyboard/stickers/make_stickers.py --verify
```

For a sheet with a fixed number of keyboards and no spares, use `--sets` and `--out`. The keyboards are placed in the top-left corner, so the rest of a sticker sheet stays usable:

```
python hardware/keyboard/stickers/make_stickers.py --sets 1 --out neocalculator-keycap-stickers-1-keyboard.pdf
```

`--verify` checks the following, then rewrites the previews:

- the page is exactly 210 × 297 mm;
- every sticker matches the configured size to within 0.01 mm;
- gaps are at least the configured minimum and margins at least 10 mm;
- every legend outline lies inside its sticker, at least 0.3 mm from the edge;
- there is no live text.
