/**
 * CircuitComponent.h — OOP Circuit Components for CircuitCoreApp.
 *
 * Base class CircuitComponent with stamp/draw interface.
 * Derived: Resistor, VoltageSource, LED, Wire, GroundNode, MCU.
 *
 * Drawing uses lv_draw_line on lv_layer_t* (no lv_canvas).
 * Components are designed so 40×40px image icons can replace
 * vector drawing in the future without breaking the stamp logic.
 *
 * Part of: NumOS — Circuit Core Module
 */

#pragma once

#include <lvgl.h>
#include <cstdint>
#include "MnaMatrix.h"

// ── Component type identifiers ──────────────────────────────────────────────
enum class CompType : uint8_t {
    WIRE,
    RESISTOR,
    VOLTAGE_SOURCE,
    LED,
    GROUND,
    MCU,
    POTENTIOMETER,
    PUSH_BUTTON,
    CAPACITOR,
    DIODE,
    // ── Sensors (Variable Resistance) ──
    LDR,               // Light Dependent Resistor
    THERMISTOR,         // TMP36 temperature sensor
    FLEX_SENSOR,        // Flex/bend sensor
    FSR,                // Force Sensitive Resistor
    // ── Semiconductors ──
    NPN_BJT,            // NPN Bipolar Junction Transistor
    PNP_BJT,            // PNP Bipolar Junction Transistor
    NMOS,               // N-Channel MOSFET
    OP_AMP,             // Operational Amplifier (uA741)
    // ── Logic Gates (74HCxx digital sublayer) ──
    AND_GATE,
    OR_GATE,
    NOT_GATE,
    NAND_GATE,
    XOR_GATE,
    // ── Power ──
    BATTERY_AA,         // 1.5V AA battery
    BATTERY_COIN,       // 3V coin cell
    BATTERY_9V,         // 9V battery
    // ── Outputs ──
    BUZZER,             // Piezo buzzer
    SEVEN_SEG           // 7-segment display
};

// ── Base class ──────────────────────────────────────────────────────────────

class CircuitComponent {
public:
    CircuitComponent(CompType type, int gridX, int gridY)
        : _type(type), _gridX(gridX), _gridY(gridY)
        , _nodeA(0), _nodeB(0), _rotation(0)
        , _isBroken(false), _maxPower(0.5f), _maxVoltage(50.0f)
    { _label[0] = '\0'; }
    virtual ~CircuitComponent() = default;

    /** Stamp this component into the MNA matrix. */
    virtual void stampMatrix(MnaMatrix& mna) = 0;

    /**
     * Draw this component using LVGL vector primitives.
     * @param layer   LVGL draw layer from LV_EVENT_DRAW_MAIN
     * @param offsetX  horizontal pixel offset (camera/scroll)
     * @param offsetY  vertical pixel offset (camera/scroll)
     *
     * NOTE: To replace with a 40×40px image in the future, override
     * this method to call lv_draw_image() instead of lv_draw_line().
     */
    virtual void draw(lv_layer_t* layer, int offsetX, int offsetY) = 0;

    /** Update after MNA solve (e.g., LED brightness). */
    virtual void updateFromSolution(MnaMatrix& mna) { (void)mna; }

    /** Check if component is over voltage/power limits; sets _isBroken. */
    virtual void checkStress(MnaMatrix& mna);

    // ── Accessors ────────────────────────────────────────────────────────
    CompType type()   const { return _type; }
    int gridX()       const { return _gridX; }
    int gridY()       const { return _gridY; }
    int nodeA()       const { return _nodeA; }
    int nodeB()       const { return _nodeB; }
    int rotation()    const { return _rotation; }

    bool  isBroken()   const { return _isBroken; }
    void  setIsBroken(bool b) { _isBroken = b; }
    float maxPower()   const { return _maxPower; }
    float maxVoltage() const { return _maxVoltage; }
    const char* label() const { return _label; }
    void setLabel(const char* s) {
        int i = 0;
        if (s) { for (; i < 4 && s[i]; ++i) _label[i] = s[i]; }
        _label[i] = '\0';
    }

    void setGridPos(int x, int y) { _gridX = x; _gridY = y; }
    void setNodes(int a, int b) { _nodeA = a; _nodeB = b; }
    void setRotation(int r) { _rotation = r % 4; }

    /** Component bounding box size in grid cells (for future image swap). */
    static constexpr int CELL_SIZE = 20;  // 20px grid
    static constexpr int ICON_SIZE = 40;  // 40px icon area (2×2 cells)

protected:
    CompType _type;
    int _gridX, _gridY;  // grid position (in pixels, snapped to CELL_SIZE)
    int _nodeA, _nodeB;  // connected MNA nodes
    int _rotation;       // 0-3 (×90°)
    bool  _isBroken;     // true if component has been destroyed by overstress
    float _maxPower;     // maximum power dissipation (Watts)
    float _maxVoltage;   // maximum voltage across component (Volts)
    char  _label[5];     // user label (up to 4 chars + null)
};

// ── Resistor ────────────────────────────────────────────────────────────────

class Resistor : public CircuitComponent {
public:
    Resistor(int gridX, int gridY, float ohms = 1000.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;

    float resistance() const { return _resistance; }
    void setResistance(float r) { _resistance = r; }

private:
    float _resistance;
};

// ── Voltage Source ──────────────────────────────────────────────────────────

class VoltageSource : public CircuitComponent {
public:
    VoltageSource(int gridX, int gridY, float volts = 5.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;

    float voltage() const { return _voltage; }
    void setVoltage(float v) { _voltage = v; }

    int vsIndex() const { return _vsIndex; }
    void setVsIndex(int idx) { _vsIndex = idx; }

private:
    float _voltage;
    int   _vsIndex;  // index in MNA voltage source array
};

// ── LED (Piecewise-Linear Model) ────────────────────────────────────────────

class LEDComponent : public CircuitComponent {
public:
    LEDComponent(int gridX, int gridY, float vForward = 2.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
    void updateFromSolution(MnaMatrix& mna) override;

    float forwardVoltage() const { return _vForward; }
    float current()        const { return _current; }
    bool  isOn()           const { return _isOn; }

    int vsIndex() const { return _vsIndex; }
    void setVsIndex(int idx) { _vsIndex = idx; }

private:
    float _vForward;     // forward voltage threshold
    float _current;      // computed current
    bool  _isOn;         // LED is conducting
    int   _vsIndex;      // voltage source index when ON

    static constexpr float ON_RESISTANCE  = 10.0f;     // 10Ω series when ON
    static constexpr float OFF_RESISTANCE = 1e6f;      // 1MΩ when OFF
};

// ── Wire (stamps nothing — uses Union-Find) ─────────────────────────────────

class WireComponent : public CircuitComponent {
public:
    WireComponent(int gridX, int gridY);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
};

// ── Ground Node ─────────────────────────────────────────────────────────────

class GroundNode : public CircuitComponent {
public:
    GroundNode(int gridX, int gridY);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
};

// ── MCU (Microcontroller placeholder) ───────────────────────────────────────

class MCUComponent : public CircuitComponent {
public:
    MCUComponent(int gridX, int gridY);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;

    /** Pin count and their assigned MNA nodes. */
    static constexpr int PIN_COUNT = 4;
    int pinNode(int idx) const;
    void setPinNode(int idx, int node);

    /** Set a pin as a temporary voltage source for next MNA tick. */
    void setPin(int idx, float voltage);
    float pinVoltage(int idx) const { return _pinVoltage[idx]; }
    bool  pinActive(int idx)  const { return _pinActive[idx]; }
    void  clearPins();

private:
    int   _pinNodes[PIN_COUNT];
    float _pinVoltage[PIN_COUNT];
    bool  _pinActive[PIN_COUNT];
    int   _pinVsIndex[PIN_COUNT];  // voltage source indices
};

// ── Potentiometer (Adjustable Resistor) ─────────────────────────────────────

class Potentiometer : public CircuitComponent {
public:
    Potentiometer(int gridX, int gridY, float ohms = 10000.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;

    float resistance() const { return _resistance; }
    void setResistance(float r) { if (r >= MIN_RESISTANCE && r <= MAX_RESISTANCE) _resistance = r; }

    /** Adjust resistance by a factor (UP/DOWN in EDIT_MODE). */
    void adjustUp()   { setResistance(_resistance * ADJUST_FACTOR); }
    void adjustDown() { setResistance(_resistance / ADJUST_FACTOR); }

    static constexpr float MIN_RESISTANCE = 10.0f;
    static constexpr float MAX_RESISTANCE = 1e6f;
    static constexpr float ADJUST_FACTOR  = 1.2f;

private:
    float _resistance;
};

// ── Push-Button (Switch) ────────────────────────────────────────────────────

class PushButton : public CircuitComponent {
public:
    PushButton(int gridX, int gridY);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;

    bool isPressed() const { return _pressed; }
    void setPressed(bool p) { _pressed = p; }
    void toggle() { _pressed = !_pressed; }

private:
    bool _pressed;
    static constexpr float OPEN_RESISTANCE   = 1e6f;   // 1MΩ when open
    static constexpr float CLOSED_RESISTANCE = 0.001f;  // 1mΩ when pressed
};

// ── Capacitor (Transient Euler Integration) ─────────────────────────────────

class Capacitor : public CircuitComponent {
public:
    Capacitor(int gridX, int gridY, float farads = 100e-6f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
    void updateFromSolution(MnaMatrix& mna) override;

    float capacitance() const { return _capacitance; }
    void setCapacitance(float c) { _capacitance = c; }
    float voltageDrop() const { return _vPrev; }

private:
    float _capacitance;
    float _vPrev;       // voltage across capacitor from previous time step
    float _current;     // computed current through capacitor
};

// ── Diode (Piecewise-Linear Rectifier) ──────────────────────────────────────

class Diode : public CircuitComponent {
public:
    Diode(int gridX, int gridY, float vForward = 0.7f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
    void updateFromSolution(MnaMatrix& mna) override;

    bool  isConducting() const { return _conducting; }
    float current()      const { return _current; }

private:
    float _vForward;
    float _current;
    bool  _conducting;
    static constexpr float ON_RESISTANCE  = 10.0f;     // 10Ω when conducting
    static constexpr float OFF_RESISTANCE = 1e6f;      // 1MΩ when blocking
};

// ════════════════════════════════════════════════════════════════════════════
// SENSORS — Variable Resistance (Template Pattern: all share R-based stamp)
// ════════════════════════════════════════════════════════════════════════════

// ── LDR (Light Dependent Resistor) ──────────────────────────────────────────

class LDR : public CircuitComponent {
public:
    LDR(int gridX, int gridY, float darkR = 100000.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;

    float resistance() const { return _resistance; }
    /** Set light level 0.0 (dark) to 1.0 (bright). R decreases with light. */
    void setLightLevel(float level);
    float lightLevel() const { return _lightLevel; }

private:
    float _darkResistance;   // resistance in full darkness
    float _resistance;       // current resistance (computed from light)
    float _lightLevel;       // 0.0 = dark, 1.0 = bright
};

// ── Thermistor (TMP36 Temperature Sensor Model) ────────────────────────────

class Thermistor : public CircuitComponent {
public:
    Thermistor(int gridX, int gridY, float nominalR = 10000.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;

    float resistance() const { return _resistance; }
    /** Set temperature in °C. Resistance changes via simplified NTC model. */
    void setTemperature(float tempC);
    float temperature() const { return _tempC; }

private:
    float _nominalR;    // resistance at 25°C
    float _resistance;  // current resistance
    float _tempC;       // simulated temperature
};

// ── Flex Sensor ─────────────────────────────────────────────────────────────

class FlexSensor : public CircuitComponent {
public:
    FlexSensor(int gridX, int gridY, float flatR = 25000.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;

    float resistance() const { return _resistance; }
    /** Set bend level 0.0 (flat) to 1.0 (fully bent). R increases with bend. */
    void setBendLevel(float level);
    float bendLevel() const { return _bendLevel; }

private:
    float _flatResistance;
    float _resistance;
    float _bendLevel;
};

// ── FSR (Force Sensitive Resistor) ──────────────────────────────────────────

class FSRComponent : public CircuitComponent {
public:
    FSRComponent(int gridX, int gridY, float maxR = 1000000.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;

    float resistance() const { return _resistance; }
    /** Set force level 0.0 (no force) to 1.0 (max force). R decreases. */
    void setForceLevel(float level);
    float forceLevel() const { return _forceLevel; }

private:
    float _maxResistance;
    float _resistance;
    float _forceLevel;
};

// ════════════════════════════════════════════════════════════════════════════
// SEMICONDUCTORS — Active Components
// ════════════════════════════════════════════════════════════════════════════

// ── NPN BJT (Simplified Ebers-Moll / PWL) ──────────────────────────────────

class NpnBjt : public CircuitComponent {
public:
    NpnBjt(int gridX, int gridY, float beta = 100.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
    void updateFromSolution(MnaMatrix& mna) override;

    int nodeC() const { return _nodeC; }
    void setNodeC(int n) { _nodeC = n; }
    float beta() const { return _beta; }

    /** Nodes: _nodeA=Base, _nodeB=Emitter, _nodeC=Collector */
private:
    int   _nodeC;        // collector node (3rd terminal)
    float _beta;         // current gain
    float _vbe;          // base-emitter voltage
    bool  _saturated;    // operating region
    static constexpr float VBE_ON = 0.7f;
    static constexpr float R_ON   = 10.0f;
    static constexpr float R_OFF  = 1e6f;
};

// ── PNP BJT ─────────────────────────────────────────────────────────────────

class PnpBjt : public CircuitComponent {
public:
    PnpBjt(int gridX, int gridY, float beta = 100.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
    void updateFromSolution(MnaMatrix& mna) override;

    int nodeC() const { return _nodeC; }
    void setNodeC(int n) { _nodeC = n; }

private:
    int   _nodeC;
    float _beta;
    float _veb;
    bool  _saturated;
    static constexpr float VEB_ON = 0.7f;
    static constexpr float R_ON   = 10.0f;
    static constexpr float R_OFF  = 1e6f;
};

// ── N-Channel MOSFET (Simplified Switch Model) ─────────────────────────────

class NmosFet : public CircuitComponent {
public:
    NmosFet(int gridX, int gridY, float vThreshold = 2.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
    void updateFromSolution(MnaMatrix& mna) override;

    int nodeD() const { return _nodeD; }
    void setNodeD(int n) { _nodeD = n; }

    /** Nodes: _nodeA=Gate, _nodeB=Source, _nodeD=Drain */
private:
    int   _nodeD;        // drain node (3rd terminal)
    float _vThreshold;   // gate threshold voltage
    float _vgs;          // gate-source voltage
    bool  _conducting;
    static constexpr float R_ON  = 1.0f;    // 1Ω when conducting
    static constexpr float R_OFF = 1e6f;     // 1MΩ when off
};

// ── Op-Amp (uA741 — High-Gain Dependent Voltage Source) ─────────────────────

class OpAmp : public CircuitComponent {
public:
    OpAmp(int gridX, int gridY, float gain = 100000.0f);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
    void updateFromSolution(MnaMatrix& mna) override;

    int nodeInP() const { return _nodeA; }       // V+ (non-inverting)
    int nodeInN() const { return _nodeInN; }      // V- (inverting)
    int nodeOut() const { return _nodeB; }         // output
    void setNodeInN(int n) { _nodeInN = n; }

    int vsIndex() const { return _vsIndex; }
    void setVsIndex(int idx) { _vsIndex = idx; }

private:
    int   _nodeInN;      // inverting input node
    float _gain;         // open-loop gain
    float _vOut;         // output voltage (clamped)
    int   _vsIndex;
    static constexpr float V_SAT = 13.5f;  // saturation voltage (±)
};

// ════════════════════════════════════════════════════════════════════════════
// OUTPUTS — Visual/Audio Components
// ════════════════════════════════════════════════════════════════════════════

// ── Buzzer (Piezo) ──────────────────────────────────────────────────────────

class BuzzerComponent : public CircuitComponent {
public:
    BuzzerComponent(int gridX, int gridY);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
    void updateFromSolution(MnaMatrix& mna) override;

    bool isActive() const { return _active; }

private:
    bool  _active;
    float _current;
    static constexpr float IMPEDANCE = 42.0f;  // typical piezo impedance
    static constexpr float THRESHOLD_V = 1.5f;  // min voltage to sound
};

// ── 7-Segment Display ──────────────────────────────────────────────────────

class SevenSegment : public CircuitComponent {
public:
    SevenSegment(int gridX, int gridY);

    void stampMatrix(MnaMatrix& mna) override;
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
    void updateFromSolution(MnaMatrix& mna) override;

    uint8_t segments() const { return _segments; }
    void setSegments(uint8_t s) { _segments = s; }

private:
    uint8_t _segments;   // bit mask: a=0x01..g=0x40, dp=0x80
    float   _voltage;
    bool    _active;
    static constexpr float LED_VF = 2.0f;
    static constexpr float R_INTERNAL = 100.0f;
};
