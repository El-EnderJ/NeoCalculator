/**
 * PowerSystems.cpp — Battery and regulator models.
 *
 * All batteries stamp as ideal voltage source + small internal resistance.
 * This models real battery voltage sag under load.
 *
 * Part of: NumOS — Circuit Core Module
 */

#include "PowerSystems.h"
#include <cstdio>

// ── Drawing helper ──────────────────────────────────────────────────────────
static inline int px(int gridCoord, int offset) {
    return gridCoord + offset;
}

// ══ Battery Base Class ══════════════════════════════════════════════════════

Battery::Battery(CompType type, int gridX, int gridY, float volts, float internalR)
    : CircuitComponent(type, gridX, gridY)
    , _voltage(volts)
    , _internalR(internalR)
    , _vsIndex(0)
{}

void Battery::stampMatrix(MnaMatrix& mna) {
    // Ideal voltage source + internal resistance
    mna.stampVoltageSource(_nodeA, _nodeB, _voltage, _vsIndex);
    if (_internalR > 0.0f) {
        mna.stampResistor(_nodeA, _nodeB, _internalR);
    }
}

void Battery::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    char buf[8];
    snprintf(buf, sizeof(buf), "%.1fV", (double)_voltage);
    drawBattery(layer, cx, cy, buf);
}

void Battery::drawBattery(lv_layer_t* layer, int cx, int cy, const char* label) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Lead-in (left)
    dsc.color = lv_color_hex(0xFF4444);
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 4;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Long line (positive terminal)
    dsc.p1.x = cx - 4; dsc.p1.y = cy - 8;
    dsc.p2.x = cx - 4; dsc.p2.y = cy + 8;
    lv_draw_line(layer, &dsc);

    // Short line (negative terminal)
    dsc.color = lv_color_hex(0x4488FF);
    dsc.p1.x = cx + 4; dsc.p1.y = cy - 5;
    dsc.p2.x = cx + 4; dsc.p2.y = cy + 5;
    lv_draw_line(layer, &dsc);

    // Lead-out (right)
    dsc.p1.x = cx + 4;  dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Voltage label
    lv_draw_label_dsc_t labdsc;
    lv_draw_label_dsc_init(&labdsc);
    labdsc.color = lv_color_hex(0xFFD700);
    labdsc.font  = &lv_font_unscii_8;
    labdsc.text  = label;

    lv_area_t labArea;
    labArea.x1 = cx - 12;
    labArea.y1 = cy - 16;
    labArea.x2 = cx + 12;
    labArea.y2 = cy - 6;
    lv_draw_label(layer, &labdsc, &labArea);
}

// ══ AA Battery (1.5V) ═══════════════════════════════════════════════════════

BatteryAA::BatteryAA(int gridX, int gridY)
    : Battery(CompType::BATTERY_AA, gridX, gridY, 1.5f, 0.3f)
{}

void BatteryAA::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    drawBattery(layer, cx, cy, "1.5V");
}

// ══ Coin Cell Battery (3V) ══════════════════════════════════════════════════

BatteryCoin::BatteryCoin(int gridX, int gridY)
    : Battery(CompType::BATTERY_COIN, gridX, gridY, 3.0f, 1.0f)
{}

void BatteryCoin::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    drawBattery(layer, cx, cy, "3V");
}

// ══ 9V Battery ══════════════════════════════════════════════════════════════

Battery9V::Battery9V(int gridX, int gridY)
    : Battery(CompType::BATTERY_9V, gridX, gridY, 9.0f, 0.5f)
{}

void Battery9V::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    drawBattery(layer, cx, cy, "9V");
}
