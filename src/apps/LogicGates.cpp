/**
 * LogicGates.cpp — Fast digital logic solvers.
 *
 * Digital Sublayer: Logic gates do NOT use MNA for internal logic.
 * Instead, inputs are read as High/Low and the output is stamped
 * as an ideal voltage source (5V or 0V).
 *
 * This prevents the calculator from stalling on transistor-level
 * gate simulations.
 *
 * Part of: NumOS — Circuit Core Module
 */

#include "LogicGates.h"
#include <cstdio>

// ── Drawing helper ──────────────────────────────────────────────────────────
static inline int px(int gridCoord, int offset) {
    return gridCoord + offset;
}

// ══ LogicGate Base ══════════════════════════════════════════════════════════

LogicGate::LogicGate(CompType type, int gridX, int gridY)
    : CircuitComponent(type, gridX, gridY)
    , _nodeOut(0)
    , _vsIndex(0)
    , _outputHigh(false)
{}

void LogicGate::stampMatrix(MnaMatrix& mna) {
    // Read input voltages and evaluate logic
    float vA = mna.nodeVoltage(_nodeA);
    float vB = mna.nodeVoltage(_nodeB);

    bool inA = (vA > LOGIC_HIGH_THRESHOLD);
    bool inB = (vB > LOGIC_HIGH_THRESHOLD);

    _outputHigh = evaluate(inA, inB);

    // Stamp output as ideal voltage source
    float vOut = _outputHigh ? LOGIC_HIGH_V : LOGIC_LOW_V;
    mna.stampVoltageSource(_nodeOut, 0, vOut, _vsIndex);
}

void LogicGate::updateFromSolution(MnaMatrix& mna) {
    // Re-evaluate after solve for display purposes
    float vA = mna.nodeVoltage(_nodeA);
    float vB = mna.nodeVoltage(_nodeB);
    _outputHigh = evaluate(vA > LOGIC_HIGH_THRESHOLD, vB > LOGIC_HIGH_THRESHOLD);
}

void LogicGate::drawGateBody(lv_layer_t* layer, int cx, int cy, const char* label) {
    // Shared gate drawing: rectangle body with label
    lv_draw_rect_dsc_t rdsc;
    lv_draw_rect_dsc_init(&rdsc);
    rdsc.bg_color = lv_color_hex(0x1A2332);
    rdsc.bg_opa   = LV_OPA_COVER;
    rdsc.border_color = lv_color_hex(_outputHigh ? 0x3FB950 : 0x808080);
    rdsc.border_width = 2;
    rdsc.radius = 2;

    lv_area_t rect;
    rect.x1 = cx - 12;
    rect.y1 = cy - 10;
    rect.x2 = cx + 12;
    rect.y2 = cy + 10;
    lv_draw_rect(layer, &rdsc, &rect);

    // Input leads
    lv_draw_line_dsc_t ldsc;
    lv_draw_line_dsc_init(&ldsc);
    ldsc.color = lv_color_hex(0x808080);
    ldsc.width = 2;

    // Input A (top-left)
    ldsc.p1.x = cx - 20; ldsc.p1.y = cy - 5;
    ldsc.p2.x = cx - 12; ldsc.p2.y = cy - 5;
    lv_draw_line(layer, &ldsc);

    // Input B (bottom-left)
    ldsc.p1.x = cx - 20; ldsc.p1.y = cy + 5;
    ldsc.p2.x = cx - 12; ldsc.p2.y = cy + 5;
    lv_draw_line(layer, &ldsc);

    // Output (right)
    ldsc.color = lv_color_hex(_outputHigh ? 0x3FB950 : 0x808080);
    ldsc.p1.x = cx + 12; ldsc.p1.y = cy;
    ldsc.p2.x = cx + 20; ldsc.p2.y = cy;
    lv_draw_line(layer, &ldsc);

    // Label text
    lv_draw_label_dsc_t labdsc;
    lv_draw_label_dsc_init(&labdsc);
    labdsc.color = lv_color_hex(_outputHigh ? 0x3FB950 : 0xC0C0C0);
    labdsc.font  = &lv_font_unscii_8;
    labdsc.text  = label;

    lv_area_t labArea;
    labArea.x1 = cx - 10;
    labArea.y1 = cy - 5;
    labArea.x2 = cx + 10;
    labArea.y2 = cy + 5;
    lv_draw_label(layer, &labdsc, &labArea);
}

// ══ AND Gate ════════════════════════════════════════════════════════════════

AndGate::AndGate(int gridX, int gridY)
    : LogicGate(CompType::AND_GATE, gridX, gridY) {}

void AndGate::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    drawGateBody(layer, px(_gridX, offsetX), px(_gridY, offsetY), "&");
}

// ══ OR Gate ═════════════════════════════════════════════════════════════════

OrGate::OrGate(int gridX, int gridY)
    : LogicGate(CompType::OR_GATE, gridX, gridY) {}

void OrGate::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    drawGateBody(layer, px(_gridX, offsetX), px(_gridY, offsetY), ">=1");
}

// ══ NOT Gate ════════════════════════════════════════════════════════════════

NotGate::NotGate(int gridX, int gridY)
    : LogicGate(CompType::NOT_GATE, gridX, gridY) {}

void NotGate::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);

    // NOT gate uses single input (top lead only)
    drawGateBody(layer, cx, cy, "1");

    // Small inversion circle at output
    lv_draw_rect_dsc_t cdsc;
    lv_draw_rect_dsc_init(&cdsc);
    cdsc.bg_color = lv_color_hex(0x1A2332);
    cdsc.bg_opa = LV_OPA_COVER;
    cdsc.border_color = lv_color_hex(_outputHigh ? 0x3FB950 : 0x808080);
    cdsc.border_width = 1;
    cdsc.radius = LV_RADIUS_CIRCLE;

    lv_area_t circ;
    circ.x1 = cx + 12; circ.y1 = cy - 2;
    circ.x2 = cx + 16; circ.y2 = cy + 2;
    lv_draw_rect(layer, &cdsc, &circ);
}

// ══ NAND Gate ═══════════════════════════════════════════════════════════════

NandGate::NandGate(int gridX, int gridY)
    : LogicGate(CompType::NAND_GATE, gridX, gridY) {}

void NandGate::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);

    drawGateBody(layer, cx, cy, "&");

    // Inversion circle
    lv_draw_rect_dsc_t cdsc;
    lv_draw_rect_dsc_init(&cdsc);
    cdsc.bg_color = lv_color_hex(0x1A2332);
    cdsc.bg_opa = LV_OPA_COVER;
    cdsc.border_color = lv_color_hex(_outputHigh ? 0x3FB950 : 0x808080);
    cdsc.border_width = 1;
    cdsc.radius = LV_RADIUS_CIRCLE;

    lv_area_t circ;
    circ.x1 = cx + 12; circ.y1 = cy - 2;
    circ.x2 = cx + 16; circ.y2 = cy + 2;
    lv_draw_rect(layer, &cdsc, &circ);
}

// ══ XOR Gate ════════════════════════════════════════════════════════════════

XorGate::XorGate(int gridX, int gridY)
    : LogicGate(CompType::XOR_GATE, gridX, gridY) {}

void XorGate::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    drawGateBody(layer, px(_gridX, offsetX), px(_gridY, offsetY), "=1");
}
