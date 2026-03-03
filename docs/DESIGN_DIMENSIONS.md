# 📐 NumOS — 3D Design Specifications (Chassis V1.0)

> Critical dimensions for calculator chassis design. All values are **minimum recommended** based on actual hardware pieces. Always measure your specific components before completing the design.

---

## 1. Main Components — Dimensions

### ESP32-S3 N16R8 CAM

| Dimension | Value |
|:----------|:-----:|
| Length | ~40 mm |
| Width | ~27 mm |
| Height (with USB-C) | ~8 mm |
| USB-C Port | Centered on short end — cavity needed: **12 mm × 6 mm** |
| Mounting | No standard holes — use lateral snap-fit or support posts |

> The CAM module includes integrated WiFi/BT antenna + camera connector. If not using the camera, cover the connector to prevent dust accumulation.

### 3.2" IPS TFT Display (ILI9341)

| Dimension | Value |
|:----------|:-----:|
| **Active area (LCD panel)** | 64.8 mm × 48.6 mm |
| **Recommended window in enclosure** | ~66 mm × 50 mm |
| **Total PCB** | ~89 mm × 55 mm *(varies by manufacturer — measure actual piece)* |
| **Depth from front face** | minimum **7 mm** (panel + PCB + FPC connector) |
| **Viewing angle** | IPS: 170° H/V — no ergonomic angle restriction |

> Recommended to tilt display ~12° relative to chassis horizontal plane for desktop ergonomics.

### Keyboard — 6×6 mm Tactile Buttons

| Dimension | Value |
|:----------|:-----:|
| Button body | 6.0 mm × 6.0 mm |
| Total height (body + stem) | ~7 mm (5 mm body + 2 mm travel) |
| **Printed cap** | 7.5 mm × 7.5 mm × 3 mm height |
| **Cavity in enclosure for cap** | 7.9 mm × 7.9 mm (0.4 mm tolerance) |

---

## 2. 6×8 Keyboard Geometry

### Basic Dimensions

| Parameter | Value |
|:----------|:-----:|
| Horizontal center-to-center spacing (X) | **11 mm** |
| Vertical center-to-center spacing (Y) | **11 mm** |
| Printed key size | 7.5 mm × 7.5 mm |
| Free space between keys | ~3.5 mm (comfortable for fingers) |
| **Total keyboard area** | ~88 mm (width) × ~66 mm (height) |

### Keyboard Functional Zones

```
 ┌──────────────────────────────────────────┐
 │   R0: SHIFT │ ALPHA │ MODE │ SETUP │ F1-F4  │  ← Modes row (top)
 ├──────────────────────────────────────────┤
 │   R1: ON │ AC │ DEL │ FREE_EQ │ ← ↑ ↓ →   │  ← Control + Navigation
 ├──────────────────────────────────────────┤
 │   R2: X │ Y= │ TABLE │ GRAPH │ ZOOM…      │  ← Scientific apps
 ├──────────────────────────────────────────┤
 │   R3: 7 │ 8 │ 9 │ ( │ ) │ ÷ │ ^ │ √      │  ← Upper operators
 ├──────────────────────────────────────────┤
 │   R4: 4 │ 5 │ 6 │ × │ - │ sin │ cos │ tan │  ← Trig
 ├──────────────────────────────────────────┤
 │   R5: 1 │ 2 │ 3 │ + │ (-) │ 0 │ . │ = │  ← Base numeric
 └──────────────────────────────────────────┘
```

### Recommended Chassis Distribution

- Display in upper half of front.
- 8-10 mm gap between lower edge of display and top row of keyboard.
- Function keys (F1-F4) aligned below display bottom edge, like physical Casio calculators.

---

## 3. Overall Chassis Dimensions

### Version V1.0 (Landscape & Compact)

| Dimension | Min | Recommended | Max |
|:----------|:---:|:-----------:|:---:|
| **Width** | 92 mm | **98 mm** | 105 mm |
| **Height** | 175 mm | **185 mm** | 200 mm |
| **Thickness (display area)** | 12 mm | **15 mm** | 20 mm |
| **Thickness (keyboard area)** | 10 mm | **13 mm** | 18 mm |

> The 185 mm × 98 mm range matches the Casio fx-570/991 form factor, which is comfortable to hold and familiar to users.

### Thickness Breakdown

```
Front:
  ├── 1.5 mm  3D printed front panel
  ├── 7.0 mm  ILI9341 display (panel + PCB)
  ├── 2.0 mm  Space for FPC/flex cables
  ├── 2.5 mm  ESP32-S3 CAM
  └── 2.0 mm  Rear enclosure
  ─────────────────────────
  Total: ~15 mm (without battery)

With 18650 battery:
  ├── 15 mm   PCB + components
  ├── 19 mm   18650 battery diameter
  └── 2.0 mm  Rear cover
  ─────────────────────────
  Total: ~36 mm
```

---

## 4. Ergonomics & Design Details

### Display Tilt

- Recommend **12°** tilt in display area relative to base for desktop use.
- Optional: hinged rear base (like TI-84) to adjust angle between 5° and 20°.

### Key Labels

- Secondary function labels (SHIFT in orange, ALPHA in green like Casio) can be achieved with **filament change** on 3D printer at label layer.
- Alternative: use **printed vinyl stickers** for secondary function labels.
- Function keys F1-F4 should have function name embossed/engraved in enclosure **above** each key.

### Battery Cover (if applicable)

| Parameter | Value |
|:----------|:-----:|
| 18650 hole | 18.5 mm diameter × 70 mm length |
| Closure type | Slide or M3 screw |
| Location | Bottom rear of chassis |

### Fasteners

- **M3 × 4 mm threaded inserts** in posts at 4 internal corners.
- **M3 × 8 mm screws** with Phillips head, nickel-plated.
- Minimum 4 screws (one per corner); 6 if chassis exceeds 190 mm height.

---

## 5. Design & Print Checklist

### Before Printing

- [ ] Does the display window measure exactly the **active area** of your actual panel?
- [ ] Is the USB-C cavity oriented correctly (horizontal, on short bottom end)?
- [ ] Do the ESP32-S3 mounting posts have correct height (module height + 1 mm clearance)?
- [ ] Are button columns aligned on 11 mm × 11 mm grid?
- [ ] Do key cavities measure 7.9 × 7.9 mm (0.4 mm tolerance)?

### Partial Test (recommended before printing full piece)

- [ ] Print 2×3 key section to verify tolerances.
- [ ] Print display frame to verify panel fit.
- [ ] Verify ESP32-S3 USB-C connector is accessible.

### After Assembly

- [ ] Do keys move freely without sticking?
- [ ] Is TFT panel centered with no light leakage at edges?
- [ ] Do cables not obstruct closing both halves?
- [ ] Do screws close without forcing plastic?

---

## 6. Size References — Commercial Calculators

| Model | Width | Height | Thickness |
|:-------|:-----:|:------:|:---------:|
| Casio fx-991EX | 77 mm | 165 mm | 11 mm |
| Casio fx-CG50 | 89 mm | 188 mm | 18 mm |
| TI-84 Plus CE | 82 mm | 189 mm | 19 mm |
| NumWorks N0110 | 68 mm | 166 mm | 12 mm |
| **NumOS V1.0 (target)** | **98 mm** | **185 mm** | **15 mm** |

---

*Last updated: February 2026*
