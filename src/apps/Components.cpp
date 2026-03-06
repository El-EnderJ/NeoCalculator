/**
 * Components.cpp — Circuit component implementations.
 *
 * Each component implements:
 *   stampMatrix() — MNA matrix stamping
 *   draw()        — LVGL vector drawing (replaceable with images)
 *   updateFromSolution() — post-solve state update (LEDs)
 *
 * New components added:
 *   - Sensors: LDR, Thermistor, FlexSensor, FSR (variable resistance)
 *   - Semiconductors: NPN BJT, PNP BJT, N-MOSFET, Op-Amp
 *   - Outputs: Buzzer, 7-Segment Display
 */

#include "CircuitComponent.h"
#include <cmath>
#include <cstdio>

// ── Drawing helper: pixel position from grid coords + offset ────────────────
static inline int px(int gridCoord, int offset) {
    return gridCoord + offset;
}

// ── Helper: draw a broken component (dark gray rect with "X") ───────────────
static void drawBrokenOverlay(lv_layer_t* layer, int cx, int cy) {
    lv_draw_rect_dsc_t rdsc;
    lv_draw_rect_dsc_init(&rdsc);
    rdsc.bg_color = lv_color_hex(0x333333);
    rdsc.bg_opa   = LV_OPA_COVER;
    rdsc.border_color = lv_color_hex(0x666666);
    rdsc.border_width = 1;
    rdsc.radius = 2;
    lv_area_t area;
    area.x1 = cx - 12; area.y1 = cy - 10;
    area.x2 = cx + 12; area.y2 = cy + 10;
    lv_draw_rect(layer, &rdsc, &area);

    lv_draw_line_dsc_t ldsc;
    lv_draw_line_dsc_init(&ldsc);
    ldsc.color = lv_color_hex(0xFF4444);
    ldsc.width = 2;
    ldsc.p1.x = cx - 8; ldsc.p1.y = cy - 6;
    ldsc.p2.x = cx + 8; ldsc.p2.y = cy + 6;
    lv_draw_line(layer, &ldsc);
    ldsc.p1.x = cx + 8; ldsc.p1.y = cy - 6;
    ldsc.p2.x = cx - 8; ldsc.p2.y = cy + 6;
    lv_draw_line(layer, &ldsc);
}

// ── Base class: checkStress default implementation ──────────────────────────
void CircuitComponent::checkStress(MnaMatrix& mna) {
    if (_isBroken) return;
    float vA = mna.nodeVoltage(_nodeA);
    float vB = mna.nodeVoltage(_nodeB);
    float vDrop = fabsf(vA - vB);
    if (vDrop > _maxVoltage) { _isBroken = true; return; }
}

// ══════════════════════════════════════════════════════════════════════════════
// Resistor
// ══════════════════════════════════════════════════════════════════════════════

Resistor::Resistor(int gridX, int gridY, float ohms)
    : CircuitComponent(CompType::RESISTOR, gridX, gridY)
    , _resistance(ohms)
{}

void Resistor::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    // G_aa += 1/R, G_bb += 1/R, G_ab -= 1/R, G_ba -= 1/R
    mna.stampResistor(_nodeA, _nodeB, _resistance);
}

void Resistor::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0x00CC66);  // green
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Lead-in line (left)
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 14; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Zigzag body (6 segments)
    static const int8_t zigX[] = { -14, -10, -6, -2, 2, 6, 10, 14 };
    static const int8_t zigY[] = {   0,  -6,  6, -6, 6, -6, 6,  0 };
    for (int i = 0; i < 7; ++i) {
        dsc.p1.x = cx + zigX[i]; dsc.p1.y = cy + zigY[i];
        dsc.p2.x = cx + zigX[i + 1]; dsc.p2.y = cy + zigY[i + 1];
        lv_draw_line(layer, &dsc);
    }

    // Lead-out line (right)
    dsc.p1.x = cx + 14; dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
}

// ══════════════════════════════════════════════════════════════════════════════
// VoltageSource
// ══════════════════════════════════════════════════════════════════════════════

VoltageSource::VoltageSource(int gridX, int gridY, float volts)
    : CircuitComponent(CompType::VOLTAGE_SOURCE, gridX, gridY)
    , _voltage(volts)
    , _vsIndex(0)
{}

void VoltageSource::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    mna.stampVoltageSource(_nodeA, _nodeB, _voltage, _vsIndex);
}

void VoltageSource::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Lead-in (left to center-4)
    dsc.color = lv_color_hex(0xFF4444);
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 4;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Long line (positive terminal)
    dsc.color = lv_color_hex(0xFF4444);
    dsc.width = 2;
    dsc.p1.x = cx - 4; dsc.p1.y = cy - 8;
    dsc.p2.x = cx - 4; dsc.p2.y = cy + 8;
    lv_draw_line(layer, &dsc);

    // Short line (negative terminal)
    dsc.color = lv_color_hex(0x4488FF);
    dsc.p1.x = cx + 4; dsc.p1.y = cy - 5;
    dsc.p2.x = cx + 4; dsc.p2.y = cy + 5;
    lv_draw_line(layer, &dsc);

    // Lead-out (center+4 to right)
    dsc.color = lv_color_hex(0x4488FF);
    dsc.p1.x = cx + 4;  dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
}

// ══════════════════════════════════════════════════════════════════════════════
// LED (Piecewise Linear Model)
// ══════════════════════════════════════════════════════════════════════════════

LEDComponent::LEDComponent(int gridX, int gridY, float vForward)
    : CircuitComponent(CompType::LED, gridX, gridY)
    , _vForward(vForward)
    , _current(0.0f)
    , _isOn(false)
    , _vsIndex(0)
{ _maxPower = 0.06f; }

void LEDComponent::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    // Iterative PWL model:
    // If Vanode - Vcathode > Vf → stamp voltage source + 10Ω series
    // Else → stamp 1MΩ off-resistor
    if (_isOn) {
        // ON state: voltage source Vf + series resistance
        mna.stampVoltageSource(_nodeA, _nodeB, _vForward, _vsIndex);
        mna.stampResistor(_nodeA, _nodeB, ON_RESISTANCE);
    } else {
        // OFF state: very high resistance (practically open circuit)
        mna.stampResistor(_nodeA, _nodeB, OFF_RESISTANCE);
    }
}

void LEDComponent::updateFromSolution(MnaMatrix& mna) {
    float vA = mna.nodeVoltage(_nodeA);
    float vB = mna.nodeVoltage(_nodeB);
    float vDrop = vA - vB;

    bool wasOn = _isOn;
    _isOn = (vDrop > _vForward);

    if (_isOn) {
        _current = (vDrop - _vForward) / ON_RESISTANCE;
        if (_current < 0) _current = 0;
    } else {
        _current = vDrop / OFF_RESISTANCE;
    }

    // If state changed, we need another solve iteration
    (void)wasOn;
}

void LEDComponent::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    // Brightness based on current (0-20mA typical)
    float brightness = _isOn ? fminf(_current / 0.020f, 1.0f) : 0.0f;

    // Color interpolation: dim red → bright red
    uint8_t r = (uint8_t)(60 + brightness * 195);
    uint8_t g = (uint8_t)(brightness * 30);
    uint8_t b = (uint8_t)(brightness * 10);
    lv_color_t ledColor = lv_color_make(r, g, b);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Triangle (anode side) — 3 lines
    dsc.color = ledColor;

    // Left lead
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 6;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Triangle top
    dsc.p1.x = cx - 6; dsc.p1.y = cy - 6;
    dsc.p2.x = cx + 4; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Triangle bottom
    dsc.p1.x = cx - 6; dsc.p1.y = cy + 6;
    dsc.p2.x = cx + 4; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Triangle base
    dsc.p1.x = cx - 6; dsc.p1.y = cy - 6;
    dsc.p2.x = cx - 6; dsc.p2.y = cy + 6;
    lv_draw_line(layer, &dsc);

    // Cathode bar
    dsc.p1.x = cx + 4; dsc.p1.y = cy - 6;
    dsc.p2.x = cx + 4; dsc.p2.y = cy + 6;
    lv_draw_line(layer, &dsc);

    // Right lead
    dsc.p1.x = cx + 4;  dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
}

// ══════════════════════════════════════════════════════════════════════════════
// Wire (Union-Find — stamps nothing)
// ══════════════════════════════════════════════════════════════════════════════

WireComponent::WireComponent(int gridX, int gridY)
    : CircuitComponent(CompType::WIRE, gridX, gridY)
{}

void WireComponent::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    // Wire: merge nodes via Union-Find (done before stamping other components)
    mna.ufUnion(_nodeA, _nodeB);
}

void WireComponent::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0x58A6FF);  // wire blue
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Simple horizontal line
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
}

// ══════════════════════════════════════════════════════════════════════════════
// Ground Node
// ══════════════════════════════════════════════════════════════════════════════

GroundNode::GroundNode(int gridX, int gridY)
    : CircuitComponent(CompType::GROUND, gridX, gridY)
{
    _nodeA = 0;  // ground is always node 0
    _nodeB = 0;
}

void GroundNode::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    // Ground node: connect nodeA to node 0 via Union-Find
    mna.ufUnion(_nodeA, 0);
}

void GroundNode::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0x808080);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Vertical lead
    dsc.p1.x = cx; dsc.p1.y = cy - 10;
    dsc.p2.x = cx; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Three horizontal bars (decreasing width)
    for (int i = 0; i < 3; ++i) {
        int w = 10 - i * 3;
        int y = cy + i * 4;
        dsc.p1.x = cx - w; dsc.p1.y = y;
        dsc.p2.x = cx + w; dsc.p2.y = y;
        lv_draw_line(layer, &dsc);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// MCU (Microcontroller)
// ══════════════════════════════════════════════════════════════════════════════

MCUComponent::MCUComponent(int gridX, int gridY)
    : CircuitComponent(CompType::MCU, gridX, gridY)
{
    for (int i = 0; i < PIN_COUNT; ++i) {
        _pinNodes[i] = 0;
        _pinVoltage[i] = 0.0f;
        _pinActive[i] = false;
        _pinVsIndex[i] = 0;
    }
}

int MCUComponent::pinNode(int idx) const {
    if (idx < 0 || idx >= PIN_COUNT) return 0;
    return _pinNodes[idx];
}

void MCUComponent::setPinNode(int idx, int node) {
    if (idx >= 0 && idx < PIN_COUNT)
        _pinNodes[idx] = node;
}

void MCUComponent::setPin(int idx, float voltage) {
    if (idx >= 0 && idx < PIN_COUNT) {
        _pinVoltage[idx] = voltage;
        _pinActive[idx] = true;
    }
}

void MCUComponent::clearPins() {
    for (int i = 0; i < PIN_COUNT; ++i)
        _pinActive[i] = false;
}

void MCUComponent::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    // Stamp active pins as temporary voltage sources
    for (int i = 0; i < PIN_COUNT; ++i) {
        if (_pinActive[i] && _pinNodes[i] > 0) {
            mna.stampVoltageSource(_pinNodes[i], 0, _pinVoltage[i], _pinVsIndex[i]);
        }
    }
}

void MCUComponent::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    // Draw a rectangle with pin stubs
    lv_draw_rect_dsc_t rdsc;
    lv_draw_rect_dsc_init(&rdsc);
    rdsc.bg_color = lv_color_hex(0x2D2D2D);
    rdsc.bg_opa   = LV_OPA_COVER;
    rdsc.border_color = lv_color_hex(0x58A6FF);
    rdsc.border_width = 2;
    rdsc.radius = 2;

    lv_area_t rect;
    rect.x1 = cx - 14;
    rect.y1 = cy - 14;
    rect.x2 = cx + 14;
    rect.y2 = cy + 14;
    lv_draw_rect(layer, &rdsc, &rect);

    // Pin stubs (4 pins: top, bottom, left, right)
    lv_draw_line_dsc_t ldsc;
    lv_draw_line_dsc_init(&ldsc);
    ldsc.color = lv_color_hex(0x58A6FF);
    ldsc.width = 2;

    // Left pin
    ldsc.p1.x = cx - 20; ldsc.p1.y = cy;
    ldsc.p2.x = cx - 14; ldsc.p2.y = cy;
    lv_draw_line(layer, &ldsc);

    // Right pin
    ldsc.p1.x = cx + 14; ldsc.p1.y = cy;
    ldsc.p2.x = cx + 20; ldsc.p2.y = cy;
    lv_draw_line(layer, &ldsc);

    // Top pin
    ldsc.p1.x = cx; ldsc.p1.y = cy - 20;
    ldsc.p2.x = cx; ldsc.p2.y = cy - 14;
    lv_draw_line(layer, &ldsc);

    // Bottom pin
    ldsc.p1.x = cx; ldsc.p1.y = cy + 14;
    ldsc.p2.x = cx; ldsc.p2.y = cy + 20;
    lv_draw_line(layer, &ldsc);

    // "μC" label
    lv_draw_label_dsc_t labdsc;
    lv_draw_label_dsc_init(&labdsc);
    labdsc.color = lv_color_hex(0xFFFFFF);
    labdsc.font  = &lv_font_montserrat_10;
    labdsc.text  = "uC";

    lv_area_t labArea;
    labArea.x1 = cx - 8;
    labArea.y1 = cy - 6;
    labArea.x2 = cx + 8;
    labArea.y2 = cy + 6;
    lv_draw_label(layer, &labdsc, &labArea);
}

// ══════════════════════════════════════════════════════════════════════════════
// Potentiometer (Adjustable Resistor)
// ══════════════════════════════════════════════════════════════════════════════

Potentiometer::Potentiometer(int gridX, int gridY, float ohms)
    : CircuitComponent(CompType::POTENTIOMETER, gridX, gridY)
    , _resistance(ohms)
{}

void Potentiometer::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    mna.stampResistor(_nodeA, _nodeB, _resistance);
}

void Potentiometer::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0xFFAA00);  // orange for potentiometer
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Lead-in line (left)
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 14; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Zigzag body (same as resistor)
    static const int8_t zigX[] = { -14, -10, -6, -2, 2, 6, 10, 14 };
    static const int8_t zigY[] = {   0,  -6,  6, -6, 6, -6, 6,  0 };
    for (int i = 0; i < 7; ++i) {
        dsc.p1.x = cx + zigX[i]; dsc.p1.y = cy + zigY[i];
        dsc.p2.x = cx + zigX[i + 1]; dsc.p2.y = cy + zigY[i + 1];
        lv_draw_line(layer, &dsc);
    }

    // Lead-out line (right)
    dsc.p1.x = cx + 14; dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Arrow indicator (distinguishes from fixed resistor)
    dsc.color = lv_color_hex(0xFFDD44);
    dsc.p1.x = cx; dsc.p1.y = cy - 10;
    dsc.p2.x = cx; dsc.p2.y = cy - 4;
    lv_draw_line(layer, &dsc);
    // Arrow head
    dsc.p1.x = cx - 3; dsc.p1.y = cy - 6;
    dsc.p2.x = cx;     dsc.p2.y = cy - 4;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 3; dsc.p1.y = cy - 6;
    dsc.p2.x = cx;     dsc.p2.y = cy - 4;
    lv_draw_line(layer, &dsc);
}

// ══════════════════════════════════════════════════════════════════════════════
// Push-Button (Switch)
// ══════════════════════════════════════════════════════════════════════════════

PushButton::PushButton(int gridX, int gridY)
    : CircuitComponent(CompType::PUSH_BUTTON, gridX, gridY)
    , _pressed(false)
{}

void PushButton::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    float r = _pressed ? CLOSED_RESISTANCE : OPEN_RESISTANCE;
    mna.stampResistor(_nodeA, _nodeB, r);
}

void PushButton::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Left lead
    dsc.color = lv_color_hex(0xC0C0C0);
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 8;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Right lead
    dsc.p1.x = cx + 8;  dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Left contact dot
    lv_draw_rect_dsc_t rdsc;
    lv_draw_rect_dsc_init(&rdsc);
    rdsc.bg_color = lv_color_hex(0xC0C0C0);
    rdsc.bg_opa = LV_OPA_COVER;
    rdsc.radius = 3;
    lv_area_t dot;
    dot.x1 = cx - 10; dot.y1 = cy - 2;
    dot.x2 = cx - 6;  dot.y2 = cy + 2;
    lv_draw_rect(layer, &rdsc, &dot);

    // Right contact dot
    dot.x1 = cx + 6;  dot.y1 = cy - 2;
    dot.x2 = cx + 10; dot.y2 = cy + 2;
    lv_draw_rect(layer, &rdsc, &dot);

    // Switch bar (angled when open, flat when pressed)
    if (_pressed) {
        dsc.color = lv_color_hex(0x3FB950);  // green = closed
        dsc.p1.x = cx - 8; dsc.p1.y = cy;
        dsc.p2.x = cx + 8; dsc.p2.y = cy;
    } else {
        dsc.color = lv_color_hex(0xFF4444);  // red = open
        dsc.p1.x = cx - 8; dsc.p1.y = cy;
        dsc.p2.x = cx + 6; dsc.p2.y = cy - 8;
    }
    lv_draw_line(layer, &dsc);
}

// ══════════════════════════════════════════════════════════════════════════════
// Capacitor (Transient Euler Integration)
// ══════════════════════════════════════════════════════════════════════════════

Capacitor::Capacitor(int gridX, int gridY, float farads)
    : CircuitComponent(CompType::CAPACITOR, gridX, gridY)
    , _capacitance(farads)
    , _vPrev(0.0f)
    , _current(0.0f)
{}

void Capacitor::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    // Backward Euler companion model: stamp equivalent conductance + current source
    mna.stampCapacitor(_nodeA, _nodeB, _capacitance, _vPrev);
}

void Capacitor::updateFromSolution(MnaMatrix& mna) {
    float vA = mna.nodeVoltage(_nodeA);
    float vB = mna.nodeVoltage(_nodeB);
    float vNow = vA - vB;

    // Compute current: I = C * dV/dt
    float dt = mna.timeStep();
    if (dt > 0.0f) {
        _current = _capacitance * (vNow - _vPrev) / dt;
    } else {
        _current = 0.0f;
    }
    _vPrev = vNow;
}

void Capacitor::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0x44BBFF);  // light blue
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Left lead
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 3;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Left plate (vertical bar)
    dsc.width = 3;
    dsc.p1.x = cx - 3; dsc.p1.y = cy - 7;
    dsc.p2.x = cx - 3; dsc.p2.y = cy + 7;
    lv_draw_line(layer, &dsc);

    // Right plate (vertical bar)
    dsc.p1.x = cx + 3; dsc.p1.y = cy - 7;
    dsc.p2.x = cx + 3; dsc.p2.y = cy + 7;
    lv_draw_line(layer, &dsc);

    // Right lead
    dsc.width = 2;
    dsc.p1.x = cx + 3;  dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
}

// ══════════════════════════════════════════════════════════════════════════════
// Diode (Piecewise-Linear Rectifier)
// ══════════════════════════════════════════════════════════════════════════════

Diode::Diode(int gridX, int gridY, float vForward)
    : CircuitComponent(CompType::DIODE, gridX, gridY)
    , _vForward(vForward)
    , _current(0.0f)
    , _conducting(false)
{}

void Diode::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    if (_conducting) {
        // ON state: low resistance (forward biased)
        mna.stampResistor(_nodeA, _nodeB, ON_RESISTANCE);
    } else {
        // OFF state: high resistance (reverse biased)
        mna.stampResistor(_nodeA, _nodeB, OFF_RESISTANCE);
    }
}

void Diode::updateFromSolution(MnaMatrix& mna) {
    float vA = mna.nodeVoltage(_nodeA);
    float vB = mna.nodeVoltage(_nodeB);
    float vDrop = vA - vB;

    _conducting = (vDrop > _vForward);

    if (_conducting) {
        _current = (vDrop - _vForward) / ON_RESISTANCE;
        if (_current < 0) _current = 0;
    } else {
        _current = vDrop / OFF_RESISTANCE;
    }
}

void Diode::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    uint32_t color = _conducting ? 0x3FB950 : 0x808080;

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(color);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Left lead
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 6;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Triangle (anode)
    dsc.p1.x = cx - 6; dsc.p1.y = cy - 6;
    dsc.p2.x = cx + 4; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    dsc.p1.x = cx - 6; dsc.p1.y = cy + 6;
    dsc.p2.x = cx + 4; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    dsc.p1.x = cx - 6; dsc.p1.y = cy - 6;
    dsc.p2.x = cx - 6; dsc.p2.y = cy + 6;
    lv_draw_line(layer, &dsc);

    // Cathode bar
    dsc.p1.x = cx + 4; dsc.p1.y = cy - 6;
    dsc.p2.x = cx + 4; dsc.p2.y = cy + 6;
    lv_draw_line(layer, &dsc);

    // Right lead
    dsc.p1.x = cx + 4;  dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
}

// ════════════════════════════════════════════════════════════════════════════
// SENSORS — Variable Resistance Components
// ════════════════════════════════════════════════════════════════════════════

// ══ LDR (Light Dependent Resistor) ══════════════════════════════════════════
// Mathematically equivalent to a potentiometer: R varies with light level.
// Dark: ~100kΩ, Bright: ~200Ω. Follows inverse power law.

LDR::LDR(int gridX, int gridY, float darkR)
    : CircuitComponent(CompType::LDR, gridX, gridY)
    , _darkResistance(darkR)
    , _resistance(darkR)
    , _lightLevel(0.0f)
{}

void LDR::setLightLevel(float level) {
    _lightLevel = (level < 0.0f) ? 0.0f : (level > 1.0f) ? 1.0f : level;
    // Inverse power law: R = R_dark * (1 - 0.998 * level)
    // At level 0.0: R = 100kΩ, at level 1.0: R ≈ 200Ω
    float factor = 1.0f - 0.998f * _lightLevel;
    if (factor < 0.002f) factor = 0.002f;
    _resistance = _darkResistance * factor;
}

void LDR::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    mna.stampResistor(_nodeA, _nodeB, _resistance);
}

void LDR::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0xCCCC00);  // yellow for LDR
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Same zigzag body as resistor (shared drawing pattern)
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 14; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    static const int8_t zigX[] = { -14, -10, -6, -2, 2, 6, 10, 14 };
    static const int8_t zigY[] = {   0,  -6,  6, -6, 6, -6, 6,  0 };
    for (int i = 0; i < 7; ++i) {
        dsc.p1.x = cx + zigX[i]; dsc.p1.y = cy + zigY[i];
        dsc.p2.x = cx + zigX[i + 1]; dsc.p2.y = cy + zigY[i + 1];
        lv_draw_line(layer, &dsc);
    }

    dsc.p1.x = cx + 14; dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Light arrows (two diagonal arrows pointing at body)
    dsc.color = lv_color_hex(0xFFFF44);
    dsc.width = 1;
    dsc.p1.x = cx - 8; dsc.p1.y = cy - 12;
    dsc.p2.x = cx - 4; dsc.p2.y = cy - 8;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx - 2; dsc.p1.y = cy - 12;
    dsc.p2.x = cx + 2; dsc.p2.y = cy - 8;
    lv_draw_line(layer, &dsc);
}

// ══ Thermistor (TMP36 Temperature Sensor) ═══════════════════════════════════

Thermistor::Thermistor(int gridX, int gridY, float nominalR)
    : CircuitComponent(CompType::THERMISTOR, gridX, gridY)
    , _nominalR(nominalR)
    , _resistance(nominalR)
    , _tempC(25.0f)
{}

void Thermistor::setTemperature(float tempC) {
    _tempC = tempC;
    static constexpr float B = 3950.0f;
    static constexpr float T_NOM = 298.15f;
    float tK = _tempC + 273.15f;
    if (tK < 1.0f) tK = 1.0f;
    _resistance = _nominalR * expf(B * (1.0f / tK - 1.0f / T_NOM));
    if (_resistance < 10.0f) _resistance = 10.0f;
    if (_resistance > 1e6f)  _resistance = 1e6f;
}

void Thermistor::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    mna.stampResistor(_nodeA, _nodeB, _resistance);
}

void Thermistor::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0xFF6633);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 14; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    static const int8_t zigX[] = { -14, -10, -6, -2, 2, 6, 10, 14 };
    static const int8_t zigY[] = {   0,  -6,  6, -6, 6, -6, 6,  0 };
    for (int i = 0; i < 7; ++i) {
        dsc.p1.x = cx + zigX[i]; dsc.p1.y = cy + zigY[i];
        dsc.p2.x = cx + zigX[i + 1]; dsc.p2.y = cy + zigY[i + 1];
        lv_draw_line(layer, &dsc);
    }

    dsc.p1.x = cx + 14; dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // "T" marker
    dsc.color = lv_color_hex(0xFF4444);
    dsc.width = 1;
    dsc.p1.x = cx - 4; dsc.p1.y = cy - 12;
    dsc.p2.x = cx + 4; dsc.p2.y = cy - 12;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx; dsc.p1.y = cy - 12;
    dsc.p2.x = cx; dsc.p2.y = cy - 8;
    lv_draw_line(layer, &dsc);
}

// ══ Flex Sensor ═════════════════════════════════════════════════════════════

FlexSensor::FlexSensor(int gridX, int gridY, float flatR)
    : CircuitComponent(CompType::FLEX_SENSOR, gridX, gridY)
    , _flatResistance(flatR)
    , _resistance(flatR)
    , _bendLevel(0.0f)
{}

void FlexSensor::setBendLevel(float level) {
    _bendLevel = (level < 0.0f) ? 0.0f : (level > 1.0f) ? 1.0f : level;
    _resistance = _flatResistance * (1.0f + 4.0f * _bendLevel);
}

void FlexSensor::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    mna.stampResistor(_nodeA, _nodeB, _resistance);
}

void FlexSensor::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0x88CC44);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 10; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    int bend = (int)(4.0f * _bendLevel);
    dsc.p1.x = cx - 10; dsc.p1.y = cy;
    dsc.p2.x = cx;      dsc.p2.y = cy - bend;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx;      dsc.p1.y = cy - bend;
    dsc.p2.x = cx + 10; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    dsc.p1.x = cx + 10; dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    dsc.color = lv_color_hex(0xAAEE66);
    dsc.width = 1;
    dsc.p1.x = cx - 3; dsc.p1.y = cy - 10;
    dsc.p2.x = cx + 3; dsc.p2.y = cy - 10;
    lv_draw_line(layer, &dsc);
}

// ══ FSR (Force Sensitive Resistor) ══════════════════════════════════════════

FSRComponent::FSRComponent(int gridX, int gridY, float maxR)
    : CircuitComponent(CompType::FSR, gridX, gridY)
    , _maxResistance(maxR)
    , _resistance(maxR)
    , _forceLevel(0.0f)
{}

void FSRComponent::setForceLevel(float level) {
    _forceLevel = (level < 0.0f) ? 0.0f : (level > 1.0f) ? 1.0f : level;
    _resistance = _maxResistance / (1.0f + 4999.0f * _forceLevel);
    if (_resistance < 200.0f) _resistance = 200.0f;
}

void FSRComponent::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    mna.stampResistor(_nodeA, _nodeB, _resistance);
}

void FSRComponent::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_rect_dsc_t rdsc;
    lv_draw_rect_dsc_init(&rdsc);
    rdsc.bg_color = lv_color_hex(0x663399);
    rdsc.bg_opa   = LV_OPA_60;
    rdsc.border_color = lv_color_hex(0x9966CC);
    rdsc.border_width = 2;
    rdsc.radius = LV_RADIUS_CIRCLE;

    lv_area_t pad;
    pad.x1 = cx - 8;  pad.y1 = cy - 8;
    pad.x2 = cx + 8;  pad.y2 = cy + 8;
    lv_draw_rect(layer, &rdsc, &pad);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0x9966CC);
    dsc.width = 2;
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 8;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 8;  dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
}

// ════════════════════════════════════════════════════════════════════════════
// SEMICONDUCTORS — Active Components
// ════════════════════════════════════════════════════════════════════════════

// ══ NPN BJT ═════════════════════════════════════════════════════════════════

NpnBjt::NpnBjt(int gridX, int gridY, float beta)
    : CircuitComponent(CompType::NPN_BJT, gridX, gridY)
    , _nodeC(0), _beta(beta), _vbe(0.0f), _saturated(false) {}

void NpnBjt::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    if (_vbe > VBE_ON) {
        mna.stampResistor(_nodeA, _nodeB, R_ON);
        float rCE = R_ON * (1.0f + 1.0f / _beta);
        mna.stampResistor(_nodeC, _nodeB, rCE);
    } else {
        mna.stampResistor(_nodeA, _nodeB, R_OFF);
        mna.stampResistor(_nodeC, _nodeB, R_OFF);
    }
}

void NpnBjt::updateFromSolution(MnaMatrix& mna) {
    float vB = mna.nodeVoltage(_nodeA);
    float vE = mna.nodeVoltage(_nodeB);
    _vbe = vB - vE;
    _saturated = (_vbe > VBE_ON);
}

void NpnBjt::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }
    uint32_t color = _saturated ? 0x3FB950 : 0xC0C0C0;

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(color);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Base lead
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 6;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
    // Base bar
    dsc.p1.x = cx - 6; dsc.p1.y = cy - 8;
    dsc.p2.x = cx - 6; dsc.p2.y = cy + 8;
    lv_draw_line(layer, &dsc);
    // Collector
    dsc.p1.x = cx - 6; dsc.p1.y = cy - 4;
    dsc.p2.x = cx + 8; dsc.p2.y = cy - 12;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 8;  dsc.p1.y = cy - 12;
    dsc.p2.x = cx + 20; dsc.p2.y = cy - 12;
    lv_draw_line(layer, &dsc);
    // Emitter with arrow
    dsc.p1.x = cx - 6; dsc.p1.y = cy + 4;
    dsc.p2.x = cx + 8; dsc.p2.y = cy + 12;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 8;  dsc.p1.y = cy + 12;
    dsc.p2.x = cx + 20; dsc.p2.y = cy + 12;
    lv_draw_line(layer, &dsc);
    // Arrow
    dsc.width = 1;
    dsc.p1.x = cx + 4; dsc.p1.y = cy + 6;
    dsc.p2.x = cx + 8; dsc.p2.y = cy + 12;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 2; dsc.p1.y = cy + 12;
    dsc.p2.x = cx + 8; dsc.p2.y = cy + 12;
    lv_draw_line(layer, &dsc);
}

// ══ PNP BJT ═════════════════════════════════════════════════════════════════

PnpBjt::PnpBjt(int gridX, int gridY, float beta)
    : CircuitComponent(CompType::PNP_BJT, gridX, gridY)
    , _nodeC(0), _beta(beta), _veb(0.0f), _saturated(false) {}

void PnpBjt::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    if (_veb > VEB_ON) {
        mna.stampResistor(_nodeA, _nodeB, R_ON);
        float rCE = R_ON * (1.0f + 1.0f / _beta);
        mna.stampResistor(_nodeC, _nodeB, rCE);
    } else {
        mna.stampResistor(_nodeA, _nodeB, R_OFF);
        mna.stampResistor(_nodeC, _nodeB, R_OFF);
    }
}

void PnpBjt::updateFromSolution(MnaMatrix& mna) {
    float vE = mna.nodeVoltage(_nodeB);
    float vB = mna.nodeVoltage(_nodeA);
    _veb = vE - vB;
    _saturated = (_veb > VEB_ON);
}

void PnpBjt::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }
    uint32_t color = _saturated ? 0x3FB950 : 0xC0C0C0;

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(color);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 6;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx - 6; dsc.p1.y = cy - 8;
    dsc.p2.x = cx - 6; dsc.p2.y = cy + 8;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx - 6; dsc.p1.y = cy - 4;
    dsc.p2.x = cx + 8; dsc.p2.y = cy - 12;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 8;  dsc.p1.y = cy - 12;
    dsc.p2.x = cx + 20; dsc.p2.y = cy - 12;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx - 6; dsc.p1.y = cy + 4;
    dsc.p2.x = cx + 8; dsc.p2.y = cy + 12;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 8;  dsc.p1.y = cy + 12;
    dsc.p2.x = cx + 20; dsc.p2.y = cy + 12;
    lv_draw_line(layer, &dsc);
    // PNP arrow towards base
    dsc.width = 1;
    dsc.p1.x = cx - 2; dsc.p1.y = cy + 2;
    dsc.p2.x = cx - 6; dsc.p2.y = cy + 4;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx - 6; dsc.p1.y = cy + 8;
    dsc.p2.x = cx - 6; dsc.p2.y = cy + 4;
    lv_draw_line(layer, &dsc);
}

// ══ N-Channel MOSFET ════════════════════════════════════════════════════════

NmosFet::NmosFet(int gridX, int gridY, float vThreshold)
    : CircuitComponent(CompType::NMOS, gridX, gridY)
    , _nodeD(0), _vThreshold(vThreshold), _vgs(0.0f), _conducting(false) {}

void NmosFet::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    if (_conducting) {
        mna.stampResistor(_nodeD, _nodeB, R_ON);
    } else {
        mna.stampResistor(_nodeD, _nodeB, R_OFF);
    }
    mna.stampResistor(_nodeA, _nodeB, 1e8f);
}

void NmosFet::updateFromSolution(MnaMatrix& mna) {
    float vG = mna.nodeVoltage(_nodeA);
    float vS = mna.nodeVoltage(_nodeB);
    _vgs = vG - vS;
    _conducting = (_vgs > _vThreshold);
}

void NmosFet::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }
    uint32_t color = _conducting ? 0x3FB950 : 0xC0C0C0;

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(color);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Gate lead
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 8;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
    // Gate plate
    dsc.p1.x = cx - 8; dsc.p1.y = cy - 8;
    dsc.p2.x = cx - 8; dsc.p2.y = cy + 8;
    lv_draw_line(layer, &dsc);
    // Channel
    dsc.p1.x = cx - 4; dsc.p1.y = cy - 8;
    dsc.p2.x = cx - 4; dsc.p2.y = cy + 8;
    lv_draw_line(layer, &dsc);
    // Drain
    dsc.p1.x = cx - 4; dsc.p1.y = cy - 6;
    dsc.p2.x = cx + 8; dsc.p2.y = cy - 6;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 8;  dsc.p1.y = cy - 6;
    dsc.p2.x = cx + 20; dsc.p2.y = cy - 6;
    lv_draw_line(layer, &dsc);
    // Source
    dsc.p1.x = cx - 4; dsc.p1.y = cy + 6;
    dsc.p2.x = cx + 8; dsc.p2.y = cy + 6;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 8;  dsc.p1.y = cy + 6;
    dsc.p2.x = cx + 20; dsc.p2.y = cy + 6;
    lv_draw_line(layer, &dsc);
}

// ══ Op-Amp (uA741) ═════════════════════════════════════════════════════════

OpAmp::OpAmp(int gridX, int gridY, float gain)
    : CircuitComponent(CompType::OP_AMP, gridX, gridY)
    , _nodeInN(0), _gain(gain), _vOut(0.0f), _vsIndex(0) {}

void OpAmp::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    mna.stampResistor(_nodeA, 0, 1e8f);
    mna.stampResistor(_nodeInN, 0, 1e8f);
    mna.stampVoltageSource(_nodeB, 0, _vOut, _vsIndex);
}

void OpAmp::updateFromSolution(MnaMatrix& mna) {
    float vP = mna.nodeVoltage(_nodeA);
    float vN = mna.nodeVoltage(_nodeInN);
    _vOut = _gain * (vP - vN);
    if (_vOut > V_SAT) _vOut = V_SAT;
    if (_vOut < -V_SAT) _vOut = -V_SAT;
}

void OpAmp::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0xFF8C00);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    // Triangle body
    dsc.p1.x = cx - 10; dsc.p1.y = cy - 12;
    dsc.p2.x = cx - 10; dsc.p2.y = cy + 12;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx - 10; dsc.p1.y = cy - 12;
    dsc.p2.x = cx + 10; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx - 10; dsc.p1.y = cy + 12;
    dsc.p2.x = cx + 10; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // Leads
    dsc.p1.x = cx - 20; dsc.p1.y = cy - 6;
    dsc.p2.x = cx - 10; dsc.p2.y = cy - 6;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx - 20; dsc.p1.y = cy + 6;
    dsc.p2.x = cx - 10; dsc.p2.y = cy + 6;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 10; dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    // "+"/"-" labels
    lv_draw_label_dsc_t labdsc;
    lv_draw_label_dsc_init(&labdsc);
    labdsc.color = lv_color_hex(0xFFFFFF);
    labdsc.font  = &lv_font_unscii_8;

    labdsc.text = "+";
    lv_area_t labArea;
    labArea.x1 = cx - 9; labArea.y1 = cy - 10;
    labArea.x2 = cx - 3; labArea.y2 = cy - 2;
    lv_draw_label(layer, &labdsc, &labArea);

    labdsc.text = "-";
    labArea.x1 = cx - 9; labArea.y1 = cy + 2;
    labArea.x2 = cx - 3; labArea.y2 = cy + 10;
    lv_draw_label(layer, &labdsc, &labArea);
}

// ════════════════════════════════════════════════════════════════════════════
// OUTPUTS
// ════════════════════════════════════════════════════════════════════════════

// ══ Buzzer ══════════════════════════════════════════════════════════════════

BuzzerComponent::BuzzerComponent(int gridX, int gridY)
    : CircuitComponent(CompType::BUZZER, gridX, gridY)
    , _active(false), _current(0.0f) {}

void BuzzerComponent::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    mna.stampResistor(_nodeA, _nodeB, IMPEDANCE);
}

void BuzzerComponent::updateFromSolution(MnaMatrix& mna) {
    float vDrop = fabsf(mna.nodeVoltage(_nodeA) - mna.nodeVoltage(_nodeB));
    _active = (vDrop > THRESHOLD_V);
    _current = vDrop / IMPEDANCE;
}

void BuzzerComponent::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_rect_dsc_t rdsc;
    lv_draw_rect_dsc_init(&rdsc);
    rdsc.bg_color = lv_color_hex(_active ? 0x3FB950 : 0x555555);
    rdsc.bg_opa   = LV_OPA_80;
    rdsc.border_color = lv_color_hex(0xC0C0C0);
    rdsc.border_width = 2;
    rdsc.radius = LV_RADIUS_CIRCLE;

    lv_area_t body;
    body.x1 = cx - 8;  body.y1 = cy - 8;
    body.x2 = cx + 8;  body.y2 = cy + 8;
    lv_draw_rect(layer, &rdsc, &body);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(0xC0C0C0);
    dsc.width = 2;
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 8;  dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 8;  dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);

    if (_active) {
        dsc.color = lv_color_hex(0x3FB950);
        dsc.width = 1;
        dsc.p1.x = cx + 10; dsc.p1.y = cy - 6;
        dsc.p2.x = cx + 14; dsc.p2.y = cy - 2;
        lv_draw_line(layer, &dsc);
        dsc.p1.x = cx + 14; dsc.p1.y = cy - 2;
        dsc.p2.x = cx + 10; dsc.p2.y = cy + 2;
        lv_draw_line(layer, &dsc);
    }
}

// ══ 7-Segment Display ══════════════════════════════════════════════════════

SevenSegment::SevenSegment(int gridX, int gridY)
    : CircuitComponent(CompType::SEVEN_SEG, gridX, gridY)
    , _segments(0), _voltage(0.0f), _active(false) {}

void SevenSegment::stampMatrix(MnaMatrix& mna) {
    if (_isBroken) return;
    mna.stampResistor(_nodeA, _nodeB, R_INTERNAL);
}

void SevenSegment::updateFromSolution(MnaMatrix& mna) {
    _voltage = mna.nodeVoltage(_nodeA) - mna.nodeVoltage(_nodeB);
    _active = (_voltage > LED_VF);
    if (_active) {
        int digit = (int)(_voltage);
        if (digit < 0) digit = 0;
        if (digit > 9) digit = 9;
        static const uint8_t DIGIT_MAP[] = {
            0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
        };
        _segments = DIGIT_MAP[digit];
    } else {
        _segments = 0;
    }
}

void SevenSegment::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);
    if (_isBroken) { drawBrokenOverlay(layer, cx, cy); return; }

    lv_draw_rect_dsc_t rdsc;
    lv_draw_rect_dsc_init(&rdsc);
    rdsc.bg_color = lv_color_hex(0x1A1A1A);
    rdsc.bg_opa   = LV_OPA_COVER;
    rdsc.border_color = lv_color_hex(0x404040);
    rdsc.border_width = 1;
    rdsc.radius = 2;

    lv_area_t body;
    body.x1 = cx - 10; body.y1 = cy - 14;
    body.x2 = cx + 10; body.y2 = cy + 14;
    lv_draw_rect(layer, &rdsc, &body);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.width = 2;
    dsc.round_start = 1;
    dsc.round_end   = 1;

    struct SegDef { int8_t x1, y1, x2, y2; };
    static const SegDef SEGS[] = {
        { -6, -10,  6, -10 }, {  6, -10,  6,   0 }, {  6,   0,  6,  10 },
        { -6,  10,  6,  10 }, { -6,   0, -6,  10 }, { -6, -10, -6,   0 },
        { -6,   0,  6,   0 },
    };

    for (int s = 0; s < 7; ++s) {
        bool on = (_segments >> s) & 1;
        dsc.color = lv_color_hex(on ? 0xFF2222 : 0x2A2A2A);
        dsc.p1.x = cx + SEGS[s].x1; dsc.p1.y = cy + SEGS[s].y1;
        dsc.p2.x = cx + SEGS[s].x2; dsc.p2.y = cy + SEGS[s].y2;
        lv_draw_line(layer, &dsc);
    }

    dsc.color = lv_color_hex(0x808080);
    dsc.p1.x = cx - 20; dsc.p1.y = cy;
    dsc.p2.x = cx - 10; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
    dsc.p1.x = cx + 10; dsc.p1.y = cy;
    dsc.p2.x = cx + 20; dsc.p2.y = cy;
    lv_draw_line(layer, &dsc);
}
