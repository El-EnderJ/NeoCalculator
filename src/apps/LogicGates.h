/**
 * LogicGates.h — Digital Logic Sublayer for CircuitCoreApp.
 *
 * Logic gates (74HCxx series) use a Digital Sublayer:
 *   - Inputs treated as High (>2.5V) or Low (<0.8V)
 *   - Output stamps an ideal Voltage Source (5V or 0V)
 *   - NO MNA matrix solve for gate logic (instantaneous)
 *
 * This is vastly more efficient than simulating transistor-level gates.
 *
 * Part of: NumOS — Circuit Core Module
 */

#pragma once

#include "CircuitComponent.h"

// ── Logic thresholds (CMOS 5V logic) ────────────────────────────────────────
static constexpr float LOGIC_HIGH_THRESHOLD = 2.5f;   // V_IH
static constexpr float LOGIC_LOW_THRESHOLD  = 0.8f;   // V_IL
static constexpr float LOGIC_HIGH_V         = 5.0f;   // V_OH
static constexpr float LOGIC_LOW_V          = 0.0f;   // V_OL

// ── Base class for 2-input logic gates ──────────────────────────────────────

class LogicGate : public CircuitComponent {
public:
    LogicGate(CompType type, int gridX, int gridY);

    void stampMatrix(MnaMatrix& mna) override;
    void updateFromSolution(MnaMatrix& mna) override;

    /** Node assignments: _nodeA = input A, _nodeB = input B, _nodeOut = output */
    int nodeOut() const { return _nodeOut; }
    void setNodeOut(int n) { _nodeOut = n; }

    int vsIndex() const { return _vsIndex; }
    void setVsIndex(int idx) { _vsIndex = idx; }

    bool outputHigh() const { return _outputHigh; }

protected:
    /** Subclass implements logic function: given two input booleans, return output. */
    virtual bool evaluate(bool inA, bool inB) const = 0;

    int  _nodeOut;     // output node
    int  _vsIndex;     // voltage source index for output
    bool _outputHigh;  // current output state

    /** Shared drawing helper for gate body */
    void drawGateBody(lv_layer_t* layer, int cx, int cy, const char* label);
};

// ── AND Gate (74HC08) ───────────────────────────────────────────────────────

class AndGate : public LogicGate {
public:
    AndGate(int gridX, int gridY);
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
protected:
    bool evaluate(bool inA, bool inB) const override { return inA && inB; }
};

// ── OR Gate (74HC32) ────────────────────────────────────────────────────────

class OrGate : public LogicGate {
public:
    OrGate(int gridX, int gridY);
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
protected:
    bool evaluate(bool inA, bool inB) const override { return inA || inB; }
};

// ── NOT Gate (74HC04) — Inverter (single input, _nodeB unused) ──────────────

class NotGate : public LogicGate {
public:
    NotGate(int gridX, int gridY);
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
protected:
    bool evaluate(bool inA, bool /*inB*/) const override { return !inA; }
};

// ── NAND Gate (74HC00) ──────────────────────────────────────────────────────

class NandGate : public LogicGate {
public:
    NandGate(int gridX, int gridY);
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
protected:
    bool evaluate(bool inA, bool inB) const override { return !(inA && inB); }
};

// ── XOR Gate (74HC86) ───────────────────────────────────────────────────────

class XorGate : public LogicGate {
public:
    XorGate(int gridX, int gridY);
    void draw(lv_layer_t* layer, int offsetX, int offsetY) override;
protected:
    bool evaluate(bool inA, bool inB) const override { return inA != inB; }
};
