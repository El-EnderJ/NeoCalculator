# Keyboard Layout — Physical Hardware (5 × 3 wired, Phase 7)

## Wiring Summary

| Signal | GPIO | Notes |
|--------|------|-------|
| **Row 0** (OUTPUT) | 1 | Drive LOW to scan row |
| **Row 1** (OUTPUT) | 2 | |
| **Row 2** (OUTPUT) | 41 | |
| **Row 3** (OUTPUT) | 42 | |
| **Row 4** (OUTPUT) | 40 | |
| **Col 0** (INPUT_PULLUP) | 6 | OK — free |
| **Col 1** (INPUT_PULLUP) | 7 | OK — free |
| **Col 2** (INPUT_PULLUP) | 8 | OK — free |
| **Col 3 – 9** | 3,15,16,17,18,21,47 | Not wired yet |

> ✅ **GPIO conflict resolved (2026-03-02):** C0 and C1 have been reassigned from GPIO 4 (`TFT_DC`) and GPIO 5 (`TFT_RST`) to GPIO 6 and GPIO 7 respectively. All three currently wired columns (GPIO 6, 7, 8) are free of TFT or row conflicts.

---

## Key Map — 5 rows × 3 columns (15 keys)

```
        Col 0 (GPIO 6)   Col 1 (GPIO 7)   Col 2 (GPIO 8)
         ┌──────────────┬──────────────┬──────────────┐
Row 0    │      7       │      8       │      9       │   GPIO 1
(GPIO 1) │  NUM_7       │  NUM_8       │  NUM_9       │
         ├──────────────┼──────────────┼──────────────┤
Row 1    │      4       │      5       │      6       │   GPIO 2
(GPIO 2) │  NUM_4       │  NUM_5       │  NUM_6       │
         ├──────────────┼──────────────┼──────────────┤
Row 2    │      1       │      2       │      3       │   GPIO 41
(GPIO41) │  NUM_1       │  NUM_2       │  NUM_3       │
         ├──────────────┼──────────────┼──────────────┤
Row 3    │      0       │     AC       │    EXE       │   GPIO 42
(GPIO42) │  NUM_0       │  AC          │  ENTER       │
         ├──────────────┼──────────────┼──────────────┤
Row 4    │      +       │      −       │      ×       │   GPIO 40
(GPIO40) │  ADD         │  SUB         │  MUL         │
         └──────────────┴──────────────┴──────────────┘
```

### Key functions

| (row, col) | GPIO pair | `KeyCode` | Label | Function |
|:----------:|:----------:|:---------:|:-----:|:---------|
| (0, 0) | R:1  C:6 | `NUM_7` | **7** | Digit 7 |
| (0, 1) | R:1  C:7 | `NUM_8` | **8** | Digit 8 |
| (0, 2) | R:1  C:8 | `NUM_9` | **9** | Digit 9 |
| (1, 0) | R:2  C:6 | `NUM_4` | **4** | Digit 4 |
| (1, 1) | R:2  C:7 | `NUM_5` | **5** | Digit 5 |
| (1, 2) | R:2  C:8 | `NUM_6` | **6** | Digit 6 |
| (2, 0) | R:41 C:6 | `NUM_1` | **1** | Digit 1 |
| (2, 1) | R:41 C:7 | `NUM_2` | **2** | Digit 2 |
| (2, 2) | R:41 C:8 | `NUM_3` | **3** | Digit 3 |
| (3, 0) | R:42 C:6 | `NUM_0` | **0** | Digit 0 |
| (3, 1) | R:42 C:7 | `AC`    | **AC** | All Clear — clear expression & result |
| (3, 2) | R:42 C:8 | `ENTER` | **EXE** | Execute / confirm |
| (4, 0) | R:40 C:6 | `ADD`   | **+** | Addition |
| (4, 1) | R:40 C:7 | `SUB`   | **−** | Subtraction |
| (4, 2) | R:40 C:8 | `MUL`   | **×** | Multiplication |

---

## Reserved slots (Cols 3–9, not wired yet)

The keymap array in `Keyboard.cpp` already has 10 column slots.  
All unconnected positions are set to `KeyCode::NONE`.

When you wire more columns, simply update `_map[][]` in `src/drivers/Keyboard.cpp`
and increase `CONNECTED_COLS` in `src/drivers/Keyboard.h`.

| Col | GPIO | Suggested future use |
|:---:|:----:|:---------------------|
| 3 | 3 | DIV, DOT, NEG |
| 4 | 15 | SIN, COS, TAN |
| 5 | 16 | SQRT, POW, LOG |
| 6 | 17 | LEFT, RIGHT, UP, DOWN |
| 7 | 18 | SHIFT, ALPHA, F1..F4 |
| 8 | 21 | DEL, ANS, cursor |
| 9 | 47 | spare |

---

## How debounce works

1. Each cell has its own `_rawState`, `_debState`, and `_debTimer`.
2. When the physical read differs from `_rawState`, the timer resets.
3. When the same physical state has been stable for **≥ 20 ms**, `_debState` is updated and a `PRESS` or `RELEASE` event is pushed to the queue.
4. While a key stays pressed, `REPEAT` events are fired after **500 ms** delay and then every **80 ms**.
5. `update()` only runs the scan loop every **5 ms** (non-blocking).
