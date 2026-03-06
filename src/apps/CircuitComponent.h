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
    MCU
};

// ── Base class ──────────────────────────────────────────────────────────────

class CircuitComponent {
public:
    CircuitComponent(CompType type, int gridX, int gridY)
        : _type(type), _gridX(gridX), _gridY(gridY)
        , _nodeA(0), _nodeB(0), _rotation(0) {}
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

    // ── Accessors ────────────────────────────────────────────────────────
    CompType type()   const { return _type; }
    int gridX()       const { return _gridX; }
    int gridY()       const { return _gridY; }
    int nodeA()       const { return _nodeA; }
    int nodeB()       const { return _nodeB; }
    int rotation()    const { return _rotation; }

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
