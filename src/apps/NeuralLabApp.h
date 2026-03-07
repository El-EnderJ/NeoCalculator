/**
 * NeuralLabApp.h
 * Visual Neural Network Playground for NumOS.
 * App ID 16 — LVGL-native, renders decision boundary via upscaled buffer.
 *
 * Design, train, and visualize Multi-Layer Perceptrons (MLP) from scratch.
 * Features: Decision boundary visualization, weight-colored network graph,
 *           real-time loss chart, three educational scenarios.
 */

#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "hal/ArduinoCompat.h"
#endif

#include <lvgl.h>
#include "ui/StatusBar.h"
#include "input/KeyCodes.h"
#include "NeuralEngine.h"

class NeuralLabApp {
public:
    NeuralLabApp();
    ~NeuralLabApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Screen layout ──
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 240;
    static constexpr int STATUS_H = 24;
    static constexpr int INFO_H   = 14;

    // ── Decision boundary buffer (low-res, upscaled) ──
    static constexpr int DB_W = 80;
    static constexpr int DB_H = 60;
    static constexpr int DB_SCALE = 4;  // 80*4 = 320, 60*4 = 240

    // ── Loss chart ──
    static constexpr int LOSS_HISTORY_SIZE = 100;

    // ── Classifier points ──
    static constexpr int MAX_POINTS = 64;

    // ── Colors ──
    static constexpr uint32_t COL_BG       = 0x0A0A0E;
    static constexpr uint32_t COL_TEXT      = 0xCCCCCC;
    static constexpr uint32_t COL_TEXT_DIM  = 0x888888;
    static constexpr uint32_t COL_POSITIVE  = 0x4488FF;  // Blue for positive weights
    static constexpr uint32_t COL_NEGATIVE  = 0xFF4444;  // Red for negative weights
    static constexpr uint32_t COL_NEURON    = 0xDDDDDD;
    static constexpr uint32_t COL_CURSOR    = 0xFFD700;
    static constexpr uint32_t COL_CLASS_A   = 0xFF3333;  // Red class
    static constexpr uint32_t COL_CLASS_B   = 0x3333FF;  // Blue class

    // ── LVGL objects ──
    lv_obj_t*       _screen;
    lv_obj_t*       _drawObj;       // Main visualization area
    lv_obj_t*       _infoLabel;     // Bottom info bar
    lv_obj_t*       _chartObj;      // Loss chart area
    lv_timer_t*     _trainTimer;    // Training loop timer
    ui::StatusBar   _statusBar;

    // ── Neural engine ──
    NeuralEngine    _engine;

    // ── Render buffer for decision boundary (RGB565) ──
    uint16_t*       _dbBuffer;      // 80x60 low-res
    uint16_t*       _renderBuf;     // 320x240 upscaled
    lv_image_dsc_t  _imgDsc;

    // ── Training state ──
    bool     _training;         // Is training running?
    uint32_t _epochCount;       // Total epochs trained
    float    _lastLoss;         // Most recent MSE
    float    _lossHistory[LOSS_HISTORY_SIZE];
    int      _lossHistoryIdx;
    int      _lossHistoryCount;

    // ── Scenario ──
    Scenario _scenario;

    // ── Scenario data ──
    TrainSample _samples[MAX_POINTS];
    int         _sampleCount;

    // ── Classifier cursor (for CLASSIFIER scenario) ──
    int  _cursorX;
    int  _cursorY;
    int  _cursorClass;  // 0 = Red, 1 = Blue

    // ── UI state ──
    char _infoBuf[128];
    int  _selectedHiddenLayer;  // which hidden layer F1 modifies (1-based)

    // ── UI methods ──
    void createUI();
    void updateInfoLabel();

    // ── Rendering ──
    void renderDecisionBoundary();
    void renderNetworkGraph();
    void renderLossChart();
    void renderClassifierPoints();
    void renderToBuffer();
    void upscaleBuffer();

    // ── Scenario management ──
    void loadScenario(Scenario s);
    void setupXOR();
    void setupClassifier();
    void setupSineRegression();
    void addClassifierPoint(int x, int y, int cls);

    // ── Training ──
    void trainStep(int epochs);

    // ── Color helpers ──
    static uint16_t rgb888to565(uint32_t rgb);
    static uint16_t blendColor(float value);  // 0..1 → blue..red gradient

    // ── LVGL callbacks ──
    static void onDraw(lv_event_t* e);
    static void onTrainTimer(lv_timer_t* timer);
};
