/**
 * Components.cpp — Circuit component implementations.
 *
 * Each component implements:
 *   stampMatrix() — MNA matrix stamping
 *   draw()        — LVGL vector drawing (replaceable with images)
 *   updateFromSolution() — post-solve state update (LEDs)
 */

#include "CircuitComponent.h"
#include <cmath>
#include <cstdio>

// ── Drawing helper: pixel position from grid coords + offset ────────────────
static inline int px(int gridCoord, int offset) {
    return gridCoord + offset;
}

// ══════════════════════════════════════════════════════════════════════════════
// Resistor
// ══════════════════════════════════════════════════════════════════════════════

Resistor::Resistor(int gridX, int gridY, float ohms)
    : CircuitComponent(CompType::RESISTOR, gridX, gridY)
    , _resistance(ohms)
{}

void Resistor::stampMatrix(MnaMatrix& mna) {
    // G_aa += 1/R, G_bb += 1/R, G_ab -= 1/R, G_ba -= 1/R
    mna.stampResistor(_nodeA, _nodeB, _resistance);
}

void Resistor::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    // Draw a zigzag resistor symbol (horizontal, 40px wide)
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);

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
    mna.stampVoltageSource(_nodeA, _nodeB, _voltage, _vsIndex);
}

void VoltageSource::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    // Draw as two parallel lines (long + short) = battery symbol
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);

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
{}

void LEDComponent::stampMatrix(MnaMatrix& mna) {
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
    // Wire: merge nodes via Union-Find (done before stamping other components)
    mna.ufUnion(_nodeA, _nodeB);
}

void WireComponent::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);

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
    // Ground node: connect nodeA to node 0 via Union-Find
    mna.ufUnion(_nodeA, 0);
}

void GroundNode::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);

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
    mna.stampResistor(_nodeA, _nodeB, _resistance);
}

void Potentiometer::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);

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
    float r = _pressed ? CLOSED_RESISTANCE : OPEN_RESISTANCE;
    mna.stampResistor(_nodeA, _nodeB, r);
}

void PushButton::draw(lv_layer_t* layer, int offsetX, int offsetY) {
    int cx = px(_gridX, offsetX);
    int cy = px(_gridY, offsetY);

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
