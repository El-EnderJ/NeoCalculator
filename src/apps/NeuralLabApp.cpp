/**
 * NeuralLabApp.cpp
 * Visual Neural Network Playground for NumOS — "Neural Lab Elite"
 * App ID 16 — LVGL-native, decision boundary visualization.
 *
 * Cyberpunk-themed UI with bilinear-interpolated decision boundary,
 * animated network graph, loss chart, accuracy gauge, topology HUD.
 */

#include "NeuralLabApp.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

#ifdef ARDUINO
#include <esp_heap_caps.h>
#include <LittleFS.h>
#else
#include "hal/FileSystem.h"
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Portable memory helpers
// ═══════════════════════════════════════════════════════════════════════════════

static uint16_t* allocBuf16(size_t count) {
#ifdef ARDUINO
    return (uint16_t*)heap_caps_malloc(count * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
#else
    return new (std::nothrow) uint16_t[count];
#endif
}

static void freeBuf16(uint16_t* p) {
#ifdef ARDUINO
    if (p) heap_caps_free(p);
#else
    delete[] p;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
// Color Helpers
// ═══════════════════════════════════════════════════════════════════════════════

uint16_t NeuralLabApp::rgb888to565(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8)  & 0xFF;
    uint8_t b =  rgb        & 0xFF;
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

uint16_t NeuralLabApp::blendColor(float value) {
    // value 0..1: 0 = blue (class B), 1 = red (class A)
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    // Cyberpunk-style gradient: deep blue → dark purple → magenta → red
    uint8_t r, g, b;
    if (value < 0.5f) {
        float t = value * 2.0f;
        r = (uint8_t)(t * 140);
        g = (uint8_t)(20 + 30 * (1.0f - t));
        b = (uint8_t)(180 - t * 80);
    } else {
        float t = (value - 0.5f) * 2.0f;
        r = (uint8_t)(140 + t * 60);
        g = (uint8_t)(20 + 20 * (1.0f - t));
        b = (uint8_t)(100 - t * 80);
    }

    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════════

NeuralLabApp::NeuralLabApp()
    : _screen(nullptr)
    , _drawObj(nullptr)
    , _infoLabel(nullptr)
    , _hudLabel(nullptr)
    , _chartObj(nullptr)
    , _trainTimer(nullptr)
    , _dbBuffer(nullptr)
    , _renderBuf(nullptr)
    , _training(false)
    , _epochCount(0)
    , _lastLoss(1.0f)
    , _lastAccuracy(0.0f)
    , _lossHistoryIdx(0)
    , _lossHistoryCount(0)
    , _autoStop(true)
    , _scenario(Scenario::XOR)
    , _sampleCount(0)
    , _cursorX(DB_W / 2)
    , _cursorY(DB_H / 2)
    , _selectedHiddenLayer(1)
    , _animFrame(0)
    , _enterHandled(false)
{
    _infoBuf[0] = '\0';
    _hudBuf[0] = '\0';
    memset(&_imgDsc, 0, sizeof(_imgDsc));
    memset(_lossHistory, 0, sizeof(_lossHistory));
}

NeuralLabApp::~NeuralLabApp() {
    end();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::begin() {
    if (_screen) return;

    // ── Allocate buffers ──
    _dbBuffer  = allocBuf16(DB_W * DB_H);
    _renderBuf = allocBuf16(SCREEN_W * SCREEN_H);
    if (!_dbBuffer || !_renderBuf) {
        Serial.println("[NeuralLabApp] PSRAM alloc failed!");
        return;
    }
    memset(_dbBuffer,  0, DB_W * DB_H * sizeof(uint16_t));
    memset(_renderBuf, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t));

    // ── Setup image descriptor ──
    _imgDsc.header.w  = SCREEN_W;
    _imgDsc.header.h  = SCREEN_H;
    _imgDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    _imgDsc.data_size = SCREEN_W * SCREEN_H * 2;
    _imgDsc.data      = (const uint8_t*)_renderBuf;

    // ── Create LVGL screen ──
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), 0);

    // ── Status bar ──
    _statusBar.create(_screen);

    // ── Create UI ──
    createUI();

    // ── Load default scenario ──
    loadScenario(Scenario::XOR);
}

void NeuralLabApp::end() {
    if (!_screen) return;

    _training = false;

    if (_trainTimer) {
        lv_timer_delete(_trainTimer);
        _trainTimer = nullptr;
    }

    _statusBar.resetPointers();
    lv_obj_delete(_screen);
    _screen    = nullptr;
    _drawObj   = nullptr;
    _infoLabel = nullptr;
    _hudLabel  = nullptr;
    _chartObj  = nullptr;

    _engine.freeAll();

    if (_dbBuffer)  { freeBuf16(_dbBuffer);  _dbBuffer  = nullptr; }
    if (_renderBuf) { freeBuf16(_renderBuf); _renderBuf = nullptr; }
}

void NeuralLabApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Neural Lab Elite");
    _statusBar.update();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ═══════════════════════════════════════════════════════════════════════════════
// UI Creation
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::createUI() {
    // ── Main draw object (full screen image) ──
    _drawObj = lv_image_create(_screen);
    lv_obj_set_pos(_drawObj, 0, STATUS_H);
    lv_obj_set_size(_drawObj, SCREEN_W, SCREEN_H - STATUS_H);
    lv_image_set_src(_drawObj, &_imgDsc);
    lv_obj_add_event_cb(_drawObj, onDraw, LV_EVENT_DRAW_MAIN_END, this);

    // ── Info label at bottom ──
    _infoLabel = lv_label_create(_screen);
    lv_obj_set_style_text_color(_infoLabel, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(_infoLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(_infoLabel, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    lv_label_set_text(_infoLabel, "Neural Lab Elite | F2:Train F4:Scenario");

    // ── Topology HUD label (top-left overlay) ──
    _hudLabel = lv_label_create(_screen);
    lv_obj_set_style_text_color(_hudLabel, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(_hudLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_bg_color(_hudLabel, lv_color_hex(COL_HUD_BG), 0);
    lv_obj_set_style_bg_opa(_hudLabel, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(_hudLabel, 3, 0);
    lv_obj_set_style_border_color(_hudLabel, lv_color_hex(COL_HUD_BORDER), 0);
    lv_obj_set_style_border_width(_hudLabel, 1, 0);
    lv_obj_set_style_border_opa(_hudLabel, LV_OPA_40, 0);
    lv_obj_set_pos(_hudLabel, 4, STATUS_H + 2);
    lv_label_set_text(_hudLabel, "");

    // ── Training timer (runs at ~30 FPS for UI updates) ──
    _trainTimer = lv_timer_create(onTrainTimer, 33, this);
}

void NeuralLabApp::updateInfoLabel() {
    const char* scenarioNames[] = {"XOR", "Classifier", "Sine", "Circular", "Spiral"};
    int si = (int)_scenario;
    const char* scenarioName = (si < 5) ? scenarioNames[si] : "?";

    const char* actName = "Sigmoid";
    if (_engine.getActivation() == Activation::RELU) actName = "ReLU";
    else if (_engine.getActivation() == Activation::TANH) actName = "Tanh";

    snprintf(_infoBuf, sizeof(_infoBuf),
             "Ep:%lu Loss:%.4f LR:%.3f %s|%s%s",
             (unsigned long)_epochCount, (double)_lastLoss,
             (double)_engine.getLearningRate(),
             scenarioName, actName,
             _training ? " \xE2\x96\xB6" : " \xE2\x8F\xB8");
    lv_label_set_text(_infoLabel, _infoBuf);
}

void NeuralLabApp::updateHUD() {
    // Build topology string: [2-8-8-1]
    char topo[48];
    topo[0] = '[';
    int pos = 1;
    for (int l = 0; l < _engine.getNumLayers(); l++) {
        if (l > 0) topo[pos++] = '-';
        pos += snprintf(topo + pos, sizeof(topo) - pos, "%d", _engine.getLayerSize(l));
    }
    topo[pos++] = ']';
    topo[pos] = '\0';

    const char* actName = "Sig";
    if (_engine.getActivation() == Activation::RELU) actName = "ReLU";
    else if (_engine.getActivation() == Activation::TANH) actName = "Tanh";

    snprintf(_hudBuf, sizeof(_hudBuf),
             "%s P:%d %s Mom%.1f L2:%.0e",
             topo, _engine.getParamCount(), actName,
             (double)_engine.getMomentum(),
             (double)_engine.getL2Lambda());
    lv_label_set_text(_hudLabel, _hudBuf);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Scenario Setup
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::loadScenario(Scenario s) {
    _scenario    = s;
    _training    = false;
    _epochCount  = 0;
    _lastLoss    = 1.0f;
    _lastAccuracy = 0.0f;
    _lossHistoryIdx   = 0;
    _lossHistoryCount = 0;
    memset(_lossHistory, 0, sizeof(_lossHistory));

    switch (s) {
        case Scenario::XOR:             setupXOR();            break;
        case Scenario::CLASSIFIER:      setupClassifier();     break;
        case Scenario::SINE_REGRESSION: setupSineRegression(); break;
        case Scenario::CIRCULAR:        setupCircular();       break;
        case Scenario::SPIRAL:          setupSpiral();         break;
        default: setupXOR(); break;
    }

    _engine.initWeights();
    updateInfoLabel();
    updateHUD();
}

void NeuralLabApp::setupXOR() {
    const int topo[] = {2, 4, 1};
    _engine.setTopology(topo, 3);
    _engine.setActivation(Activation::SIGMOID);
    _engine.setLearningRate(0.5f);
    _engine.setMomentum(0.9f);
    _engine.setL2Lambda(0.0f);

    _sampleCount = 4;
    const float xorData[4][2] = {{0,0}, {0,1}, {1,0}, {1,1}};
    const float xorTarget[4]  = {0, 1, 1, 0};

    for (int i = 0; i < 4; i++) {
        _samples[i].inputs[0]  = xorData[i][0];
        _samples[i].inputs[1]  = xorData[i][1];
        _samples[i].targets[0] = xorTarget[i];
    }
}

void NeuralLabApp::setupClassifier() {
    const int topo[] = {2, 6, 1};
    _engine.setTopology(topo, 3);
    _engine.setActivation(Activation::SIGMOID);
    _engine.setLearningRate(0.3f);
    _engine.setMomentum(0.9f);
    _engine.setL2Lambda(0.0f);

    _sampleCount = 0;
    addClassifierPoint(20, 15, 0);  // Red
    addClassifierPoint(25, 20, 0);
    addClassifierPoint(15, 25, 0);
    addClassifierPoint(60, 45, 1);  // Blue
    addClassifierPoint(55, 40, 1);
    addClassifierPoint(65, 35, 1);

    _cursorX = DB_W / 2;
    _cursorY = DB_H / 2;
}

void NeuralLabApp::setupSineRegression() {
    const int topo[] = {1, 8, 1};
    _engine.setTopology(topo, 3);
    _engine.setActivation(Activation::TANH);
    _engine.setLearningRate(0.01f);
    _engine.setMomentum(0.9f);
    _engine.setL2Lambda(0.0f);

    _sampleCount = 20;
    for (int i = 0; i < _sampleCount; i++) {
        float x = (float)i / (_sampleCount - 1);
        _samples[i].inputs[0]  = x;
        _samples[i].targets[0] = (sinf(2.0f * (float)M_PI * x) + 1.0f) / 2.0f;
    }
}

void NeuralLabApp::setupCircular() {
    const int topo[] = {2, 8, 8, 1};
    _engine.setTopology(topo, 4);
    _engine.setActivation(Activation::RELU);
    _engine.setLearningRate(0.1f);
    _engine.setMomentum(0.9f);
    _engine.setL2Lambda(1e-4f);

    _sampleCount = 0;
    // Inner ring (class 0 = Red)
    for (int i = 0; i < 16; i++) {
        float angle = 2.0f * (float)M_PI * i / 16.0f;
        float r = 0.15f + 0.05f * ((float)rand() / RAND_MAX);
        int px = (int)((0.5f + r * cosf(angle)) * DB_W);
        int py = (int)((0.5f + r * sinf(angle)) * DB_H);
        addClassifierPoint(px, py, 0);
    }
    // Outer ring (class 1 = Blue)
    for (int i = 0; i < 16; i++) {
        float angle = 2.0f * (float)M_PI * i / 16.0f;
        float r = 0.35f + 0.05f * ((float)rand() / RAND_MAX);
        int px = (int)((0.5f + r * cosf(angle)) * DB_W);
        int py = (int)((0.5f + r * sinf(angle)) * DB_H);
        addClassifierPoint(px, py, 1);
    }
}

void NeuralLabApp::setupSpiral() {
    const int topo[] = {2, 8, 8, 1};
    _engine.setTopology(topo, 4);
    _engine.setActivation(Activation::RELU);
    _engine.setLearningRate(0.05f);
    _engine.setMomentum(0.9f);
    _engine.setL2Lambda(1e-4f);

    _sampleCount = 0;
    // Two interleaved spirals
    for (int i = 0; i < 16; i++) {
        float t = (float)i / 16.0f;
        float angle = t * 3.0f * (float)M_PI;
        float r = 0.1f + 0.3f * t;
        // Spiral arm A (class 0)
        int px = (int)((0.5f + r * cosf(angle)) * DB_W);
        int py = (int)((0.5f + r * sinf(angle)) * DB_H);
        if (px >= 0 && px < DB_W && py >= 0 && py < DB_H)
            addClassifierPoint(px, py, 0);
        // Spiral arm B (class 1, rotated 180°)
        px = (int)((0.5f - r * cosf(angle)) * DB_W);
        py = (int)((0.5f - r * sinf(angle)) * DB_H);
        if (px >= 0 && px < DB_W && py >= 0 && py < DB_H)
            addClassifierPoint(px, py, 1);
    }
}

void NeuralLabApp::addClassifierPoint(int x, int y, int cls) {
    if (_sampleCount >= MAX_POINTS) return;
    _samples[_sampleCount].inputs[0]  = (float)x / DB_W;
    _samples[_sampleCount].inputs[1]  = (float)y / DB_H;
    _samples[_sampleCount].targets[0] = (float)cls;
    _sampleCount++;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Training
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::trainStep(int epochs) {
    for (int e = 0; e < epochs; e++) {
        _lastLoss = _engine.trainEpoch(_samples, _sampleCount);
        _epochCount++;

        // Record loss history
        _lossHistory[_lossHistoryIdx] = _lastLoss;
        _lossHistoryIdx = (_lossHistoryIdx + 1) % LOSS_HISTORY_SIZE;
        if (_lossHistoryCount < LOSS_HISTORY_SIZE) _lossHistoryCount++;

        // Auto-stop on convergence
        if (_autoStop && _lastLoss < 0.001f) {
            _training = false;
            break;
        }
    }

    // Update accuracy for classification scenarios
    if (_scenario != Scenario::SINE_REGRESSION) {
        _lastAccuracy = _engine.computeAccuracy(_samples, _sampleCount);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Rendering
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::renderDecisionBoundary() {
    if (!_dbBuffer) return;

    for (int py = 0; py < DB_H; py++) {
        for (int px = 0; px < DB_W; px++) {
            float input[2];
            if (_scenario == Scenario::SINE_REGRESSION) {
                input[0] = (float)px / DB_W;
                input[1] = 0.0f;
            } else {
                input[0] = (float)px / DB_W;
                input[1] = (float)py / DB_H;
            }

            _engine.forward(input);
            float out = _engine.getOutputAt(0);

            if (_scenario == Scenario::SINE_REGRESSION) {
                float targetY = 1.0f - (float)py / DB_H;
                float diff = fabsf(out - targetY);
                if (diff < 0.03f) {
                    _dbBuffer[py * DB_W + px] = rgb888to565(0x00FFCC);
                } else {
                    uint8_t g = (uint8_t)(15 + targetY * 25);
                    _dbBuffer[py * DB_W + px] = rgb888to565(
                        ((uint32_t)(g/3) << 16) | ((uint32_t)g << 8) | (uint32_t)(g/2));
                }
            } else {
                _dbBuffer[py * DB_W + px] = blendColor(out);
            }
        }
    }
}

void NeuralLabApp::blurDecisionBuffer() {
    if (!_dbBuffer) return;

    // 2x2 box blur on the low-res buffer for smooth "decision glow"
    // Process in-place with a temporary row buffer
    uint16_t rowBuf[DB_W];

    for (int py = 0; py < DB_H - 1; py++) {
        memcpy(rowBuf, &_dbBuffer[py * DB_W], DB_W * sizeof(uint16_t));
        for (int px = 0; px < DB_W - 1; px++) {
            // Average 2x2 block: current, right, below, below-right
            uint16_t c00 = rowBuf[px];
            uint16_t c10 = rowBuf[px + 1];
            uint16_t c01 = _dbBuffer[(py + 1) * DB_W + px];
            uint16_t c11 = _dbBuffer[(py + 1) * DB_W + px + 1];

            // Extract and average RGB565 components
            int r = (((c00 >> 11) & 0x1F) + ((c10 >> 11) & 0x1F) +
                     ((c01 >> 11) & 0x1F) + ((c11 >> 11) & 0x1F)) >> 2;
            int g = (((c00 >> 5) & 0x3F) + ((c10 >> 5) & 0x3F) +
                     ((c01 >> 5) & 0x3F) + ((c11 >> 5) & 0x3F)) >> 2;
            int b = ((c00 & 0x1F) + (c10 & 0x1F) +
                     (c01 & 0x1F) + (c11 & 0x1F)) >> 2;

            _dbBuffer[py * DB_W + px] = ((uint16_t)r << 11) | ((uint16_t)g << 5) | (uint16_t)b;
        }
    }
}

void NeuralLabApp::upscaleBufferBilinear() {
    if (!_dbBuffer || !_renderBuf) return;

    // Bilinear interpolation: 80x60 → 320x240
    for (int destY = 0; destY < SCREEN_H; destY++) {
        float srcYf = (float)destY / DB_SCALE;
        int srcY0 = (int)srcYf;
        int srcY1 = srcY0 + 1;
        float fy = srcYf - srcY0;
        if (srcY0 >= DB_H - 1) { srcY0 = DB_H - 1; srcY1 = DB_H - 1; fy = 0.0f; }

        for (int destX = 0; destX < SCREEN_W; destX++) {
            float srcXf = (float)destX / DB_SCALE;
            int srcX0 = (int)srcXf;
            int srcX1 = srcX0 + 1;
            float fx = srcXf - srcX0;
            if (srcX0 >= DB_W - 1) { srcX0 = DB_W - 1; srcX1 = DB_W - 1; fx = 0.0f; }

            uint16_t c00 = _dbBuffer[srcY0 * DB_W + srcX0];
            uint16_t c10 = _dbBuffer[srcY0 * DB_W + srcX1];
            uint16_t c01 = _dbBuffer[srcY1 * DB_W + srcX0];
            uint16_t c11 = _dbBuffer[srcY1 * DB_W + srcX1];

            // Bilinear interpolation in RGB565
            float r = ((c00 >> 11) & 0x1F) * (1-fx)*(1-fy) + ((c10 >> 11) & 0x1F) * fx*(1-fy) +
                      ((c01 >> 11) & 0x1F) * (1-fx)*fy     + ((c11 >> 11) & 0x1F) * fx*fy;
            float g = ((c00 >> 5) & 0x3F) * (1-fx)*(1-fy)  + ((c10 >> 5) & 0x3F) * fx*(1-fy) +
                      ((c01 >> 5) & 0x3F) * (1-fx)*fy      + ((c11 >> 5) & 0x3F) * fx*fy;
            float b = (c00 & 0x1F) * (1-fx)*(1-fy)         + (c10 & 0x1F) * fx*(1-fy) +
                      (c01 & 0x1F) * (1-fx)*fy             + (c11 & 0x1F) * fx*fy;

            _renderBuf[destY * SCREEN_W + destX] =
                ((uint16_t)(r + 0.5f) << 11) | ((uint16_t)(g + 0.5f) << 5) | (uint16_t)(b + 0.5f);
        }
    }
}

void NeuralLabApp::renderClassifierPoints() {
    if (!_renderBuf) return;

    // Draw training points as 5x5 colored dots with white outline
    for (int i = 0; i < _sampleCount; i++) {
        int px = (int)(_samples[i].inputs[0] * DB_W) * DB_SCALE;
        int py = (int)(_samples[i].inputs[1] * DB_H) * DB_SCALE;
        uint16_t color = (_samples[i].targets[0] > 0.5f)
                         ? rgb888to565(COL_CLASS_B)
                         : rgb888to565(COL_CLASS_A);
        uint16_t outline = rgb888to565(0xFFFFFF);

        // Outline
        for (int dy = -3; dy <= 3; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                if (dx*dx + dy*dy <= 12 && dx*dx + dy*dy > 4) {
                    int rx = px + dx, ry = py + dy;
                    if (rx >= 0 && rx < SCREEN_W && ry >= 0 && ry < SCREEN_H)
                        _renderBuf[ry * SCREEN_W + rx] = outline;
                }
            }
        }
        // Fill
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                if (dx*dx + dy*dy <= 4) {
                    int rx = px + dx, ry = py + dy;
                    if (rx >= 0 && rx < SCREEN_W && ry >= 0 && ry < SCREEN_H)
                        _renderBuf[ry * SCREEN_W + rx] = color;
                }
            }
        }
    }

    // Draw cursor crosshair for classifier mode
    if (_scenario == Scenario::CLASSIFIER || _scenario == Scenario::CIRCULAR ||
        _scenario == Scenario::SPIRAL) {
        int cx = _cursorX * DB_SCALE;
        int cy = _cursorY * DB_SCALE;
        uint16_t curCol = rgb888to565(COL_CURSOR);

        // Crosshair with gap in center
        for (int d = -6; d <= 6; d++) {
            if (abs(d) < 2) continue;  // gap in center
            int rx = cx + d, ry = cy;
            if (rx >= 0 && rx < SCREEN_W && ry >= 0 && ry < SCREEN_H)
                _renderBuf[ry * SCREEN_W + rx] = curCol;
            rx = cx; ry = cy + d;
            if (rx >= 0 && rx < SCREEN_W && ry >= 0 && ry < SCREEN_H)
                _renderBuf[ry * SCREEN_W + rx] = curCol;
        }
    }
}

void NeuralLabApp::renderNetworkGraph() {
    if (!_renderBuf) return;

    int numLayers = _engine.getNumLayers();
    if (numLayers < 2) return;

    // Network graph drawn in bottom-right corner
    int graphX = SCREEN_W - 130;
    int graphY = 30;
    int graphW = 120;
    int graphH = 80;

    // Semi-transparent dark background with cyberpunk border
    for (int y = graphY; y < graphY + graphH && y < SCREEN_H; y++) {
        for (int x = graphX; x < graphX + graphW && x < SCREEN_W; x++) {
            if (x >= 0) {
                uint16_t c = _renderBuf[y * SCREEN_W + x];
                uint8_t r = ((c >> 11) & 0x1F) >> 1;
                uint8_t g = ((c >> 5) & 0x3F) >> 1;
                uint8_t b = (c & 0x1F) >> 1;
                _renderBuf[y * SCREEN_W + x] = (r << 11) | (g << 5) | b;
            }
        }
    }

    // Cyan border
    uint16_t borderCol = rgb888to565(0x006666);
    for (int x = graphX; x < graphX + graphW && x < SCREEN_W; x++) {
        if (x >= 0 && graphY < SCREEN_H) _renderBuf[graphY * SCREEN_W + x] = borderCol;
        if (x >= 0 && graphY + graphH - 1 < SCREEN_H)
            _renderBuf[(graphY + graphH - 1) * SCREEN_W + x] = borderCol;
    }
    for (int y = graphY; y < graphY + graphH && y < SCREEN_H; y++) {
        if (graphX >= 0) _renderBuf[y * SCREEN_W + graphX] = borderCol;
        if (graphX + graphW - 1 < SCREEN_W)
            _renderBuf[y * SCREEN_W + graphX + graphW - 1] = borderCol;
    }

    // Calculate neuron positions
    struct NPos { int x, y; };
    NPos positions[NE_MAX_LAYERS][NE_MAX_NEURONS];

    for (int l = 0; l < numLayers; l++) {
        int n = _engine.getLayerSize(l);
        int lx = graphX + 10 + l * (graphW - 20) / (numLayers - 1);
        for (int i = 0; i < n; i++) {
            int ly = graphY + 10 + (i + 1) * (graphH - 20) / (n + 1);
            positions[l][i] = {lx, ly};
        }
    }

    // Draw connections (weight-colored lines) with glow for active weights
    _animFrame++;
    for (int l = 0; l < numLayers - 1; l++) {
        int fromN = _engine.getLayerSize(l);
        int toN   = _engine.getLayerSize(l + 1);
        for (int j = 0; j < toN; j++) {
            for (int i = 0; i < fromN; i++) {
                float w = _engine.getWeight(l, j, i);
                float absW = fabsf(w);
                if (absW < 0.01f) continue;

                // Glow intensity: stronger weights glow brighter
                float glow = (absW > 2.0f) ? 1.0f : absW / 2.0f;
                uint8_t intensity = (uint8_t)(40 + 180 * glow);

                uint16_t lineColor;
                if (w > 0) {
                    lineColor = rgb888to565(
                        ((uint32_t)(intensity/4) << 16) |
                        ((uint32_t)(intensity/2) << 8) |
                        (uint32_t)intensity);
                } else {
                    lineColor = rgb888to565(
                        ((uint32_t)intensity << 16) |
                        ((uint32_t)(intensity/4) << 8) |
                        (uint32_t)(intensity/6));
                }

                // Bresenham line
                int x0 = positions[l][i].x, y0 = positions[l][i].y;
                int x1 = positions[l+1][j].x, y1 = positions[l+1][j].y;
                int dx = abs(x1 - x0), dy = abs(y1 - y0);
                int sx = (x0 < x1) ? 1 : -1;
                int sy = (y0 < y1) ? 1 : -1;
                int err = dx - dy;
                int lineLen = dx + dy;
                int pixIdx = 0;

                while (true) {
                    if (x0 >= 0 && x0 < SCREEN_W && y0 >= 0 && y0 < SCREEN_H) {
                        _renderBuf[y0 * SCREEN_W + x0] = lineColor;
                    }

                    // Forward pass animation: small bright pulse traveling along connections
                    if (_training && lineLen > 0) {
                        float pulsePos = (float)((_animFrame * 3 + l * 7 + i * 3 + j * 5) % 40) / 40.0f;
                        float pixFrac = (float)pixIdx / (float)(lineLen > 0 ? lineLen : 1);
                        if (fabsf(pixFrac - pulsePos) < 0.1f) {
                            if (x0 >= 0 && x0 < SCREEN_W && y0 >= 0 && y0 < SCREEN_H)
                                _renderBuf[y0 * SCREEN_W + x0] = rgb888to565(0xFFFFFF);
                        }
                    }

                    if (x0 == x1 && y0 == y1) break;
                    int e2 = 2 * err;
                    if (e2 > -dy) { err -= dy; x0 += sx; }
                    if (e2 <  dx) { err += dx; y0 += sy; }
                    pixIdx++;
                }
            }
        }
    }

    // Draw neurons as circles with activation-based color
    for (int l = 0; l < numLayers; l++) {
        int n = _engine.getLayerSize(l);
        for (int i = 0; i < n; i++) {
            int cx = positions[l][i].x;
            int cy = positions[l][i].y;
            float nOut = _engine.getNeuronOutput(l, i);
            float nAbs = fabsf(nOut);
            if (nAbs > 1.0f) nAbs = 1.0f;

            // Color based on activation: cyan-white spectrum
            uint8_t r = (uint8_t)(40 + 215 * nAbs);
            uint8_t g = (uint8_t)(80 + 175 * nAbs);
            uint8_t b = (uint8_t)(100 + 155 * nAbs);
            uint16_t nCol = rgb888to565(((uint32_t)r << 16) | ((uint32_t)g << 8) | b);

            // Filled circle radius 3
            for (int dy = -3; dy <= 3; dy++) {
                for (int dx = -3; dx <= 3; dx++) {
                    if (dx*dx + dy*dy <= 9) {
                        int px = cx + dx, py = cy + dy;
                        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                            _renderBuf[py * SCREEN_W + px] = nCol;
                        }
                    }
                }
            }

            // Bright edge ring for "glow" effect on high activation
            if (nAbs > 0.5f) {
                uint16_t glowCol = rgb888to565(COL_ACCENT);
                for (int dy = -4; dy <= 4; dy++) {
                    for (int dx = -4; dx <= 4; dx++) {
                        int dist2 = dx*dx + dy*dy;
                        if (dist2 > 9 && dist2 <= 16) {
                            int px = cx + dx, py = cy + dy;
                            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H)
                                _renderBuf[py * SCREEN_W + px] = glowCol;
                        }
                    }
                }
            }
        }
    }
}

void NeuralLabApp::renderLossChart() {
    if (!_renderBuf || _lossHistoryCount < 2) return;

    int chartX = SCREEN_W - 110;
    int chartY = SCREEN_H - 55;
    int chartW = 100;
    int chartH = 40;

    // Dark background
    for (int y = chartY; y < chartY + chartH && y < SCREEN_H; y++) {
        for (int x = chartX; x < chartX + chartW && x < SCREEN_W; x++) {
            if (x >= 0) {
                _renderBuf[y * SCREEN_W + x] = rgb888to565(0x0A0A14);
            }
        }
    }

    // Cyan border
    uint16_t borderCol = rgb888to565(0x006666);
    for (int x = chartX; x < chartX + chartW && x < SCREEN_W; x++) {
        if (x >= 0) {
            _renderBuf[chartY * SCREEN_W + x] = borderCol;
            if (chartY + chartH - 1 < SCREEN_H)
                _renderBuf[(chartY + chartH - 1) * SCREEN_W + x] = borderCol;
        }
    }
    for (int y = chartY; y < chartY + chartH && y < SCREEN_H; y++) {
        if (chartX >= 0)
            _renderBuf[y * SCREEN_W + chartX] = borderCol;
        if (chartX + chartW - 1 < SCREEN_W)
            _renderBuf[y * SCREEN_W + chartX + chartW - 1] = borderCol;
    }

    // Find max loss for scaling (use log scale)
    float maxLoss = 0.001f;
    float minLoss = 1e6f;
    for (int i = 0; i < _lossHistoryCount; i++) {
        int idx = (_lossHistoryIdx - _lossHistoryCount + i + LOSS_HISTORY_SIZE) % LOSS_HISTORY_SIZE;
        float v = _lossHistory[idx];
        if (v > maxLoss) maxLoss = v;
        if (v > 0 && v < minLoss) minLoss = v;
    }

    // Logarithmic scale
    float logMax = log10f(maxLoss > 0 ? maxLoss : 1.0f);
    float logMin = log10f(minLoss > 0 ? minLoss : 1e-6f);
    if (logMax - logMin < 0.1f) logMin = logMax - 1.0f;

    // Plot loss curve
    uint16_t lineCol = rgb888to565(0x00FFCC);
    int prevPx = -1, prevPy = -1;
    for (int i = 0; i < _lossHistoryCount; i++) {
        int idx = (_lossHistoryIdx - _lossHistoryCount + i + LOSS_HISTORY_SIZE) % LOSS_HISTORY_SIZE;
        int px = chartX + 1 + i * (chartW - 2) / _lossHistoryCount;
        float logVal = log10f(_lossHistory[idx] > 0 ? _lossHistory[idx] : 1e-6f);
        float norm = (logVal - logMin) / (logMax - logMin);
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        int py = chartY + chartH - 2 - (int)(norm * (chartH - 4));

        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
            _renderBuf[py * SCREEN_W + px] = lineCol;
        }

        if (prevPx >= 0) {
            int dx = abs(px - prevPx), dy = abs(py - prevPy);
            int sx = (prevPx < px) ? 1 : -1;
            int sy = (prevPy < py) ? 1 : -1;
            int err = dx - dy;
            int lx = prevPx, ly = prevPy;
            while (true) {
                if (lx >= 0 && lx < SCREEN_W && ly >= 0 && ly < SCREEN_H)
                    _renderBuf[ly * SCREEN_W + lx] = lineCol;
                if (lx == px && ly == py) break;
                int e2 = 2 * err;
                if (e2 > -dy) { err -= dy; lx += sx; }
                if (e2 <  dx) { err += dx; ly += sy; }
            }
        }
        prevPx = px;
        prevPy = py;
    }

    // "Loss" label
    uint16_t labelCol = rgb888to565(0x888888);
    // Draw a small "L" indicator at top-left of chart
    for (int d = 0; d < 6; d++) {
        int lx = chartX + 3, ly = chartY + 3 + d;
        if (lx >= 0 && lx < SCREEN_W && ly >= 0 && ly < SCREEN_H)
            _renderBuf[ly * SCREEN_W + lx] = labelCol;
    }
    for (int d = 0; d < 4; d++) {
        int lx = chartX + 3 + d, ly = chartY + 8;
        if (lx >= 0 && lx < SCREEN_W && ly >= 0 && ly < SCREEN_H)
            _renderBuf[ly * SCREEN_W + lx] = labelCol;
    }
}

void NeuralLabApp::renderAccuracyGauge() {
    if (!_renderBuf || _scenario == Scenario::SINE_REGRESSION) return;

    // Small accuracy gauge in bottom-left area
    int gaugeX = 4;
    int gaugeY = SCREEN_H - 55;
    int gaugeW = 50;
    int gaugeH = 12;

    // Background
    for (int y = gaugeY; y < gaugeY + gaugeH && y < SCREEN_H; y++) {
        for (int x = gaugeX; x < gaugeX + gaugeW && x < SCREEN_W; x++) {
            _renderBuf[y * SCREEN_W + x] = rgb888to565(0x0A0A14);
        }
    }

    // Fill bar based on accuracy
    float frac = _lastAccuracy / 100.0f;
    if (frac > 1.0f) frac = 1.0f;
    int fillW = (int)(frac * (gaugeW - 2));
    uint16_t fillCol = (frac > 0.9f) ? rgb888to565(0x00FF88) :
                       (frac > 0.5f) ? rgb888to565(0xFFCC00) :
                                       rgb888to565(0xFF4444);
    for (int y = gaugeY + 1; y < gaugeY + gaugeH - 1 && y < SCREEN_H; y++) {
        for (int x = gaugeX + 1; x < gaugeX + 1 + fillW && x < SCREEN_W; x++) {
            _renderBuf[y * SCREEN_W + x] = fillCol;
        }
    }

    // Border
    uint16_t bCol = rgb888to565(0x006666);
    for (int x = gaugeX; x < gaugeX + gaugeW && x < SCREEN_W; x++) {
        _renderBuf[gaugeY * SCREEN_W + x] = bCol;
        if (gaugeY + gaugeH - 1 < SCREEN_H)
            _renderBuf[(gaugeY + gaugeH - 1) * SCREEN_W + x] = bCol;
    }
    for (int y = gaugeY; y < gaugeY + gaugeH && y < SCREEN_H; y++) {
        _renderBuf[y * SCREEN_W + gaugeX] = bCol;
        if (gaugeX + gaugeW - 1 < SCREEN_W)
            _renderBuf[y * SCREEN_W + gaugeX + gaugeW - 1] = bCol;
    }
}

void NeuralLabApp::renderToBuffer() {
    // 1. Render decision boundary to low-res buffer
    renderDecisionBoundary();

    // 2. Apply box blur for smooth "decision glow"
    blurDecisionBuffer();

    // 3. Bilinear upscale to full resolution
    upscaleBufferBilinear();

    // 4. Overlay training points
    if (_scenario != Scenario::SINE_REGRESSION) {
        renderClassifierPoints();
    }

    // 5. Overlay network graph with animations
    renderNetworkGraph();

    // 6. Overlay loss chart (log scale)
    renderLossChart();

    // 7. Accuracy gauge
    renderAccuracyGauge();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Model Persistence
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::saveModel() {
    // Allocate serialization buffer
    int maxBytes = 4096;
    uint8_t* buf;
#ifdef ARDUINO
    buf = (uint8_t*)heap_caps_malloc(maxBytes, MALLOC_CAP_SPIRAM);
#else
    buf = new (std::nothrow) uint8_t[maxBytes];
#endif
    if (!buf) return;

    int written = _engine.serialize(buf, maxBytes);
    if (written > 0) {
        File f = LittleFS.open("/neural.nn", "w");
        if (f) {
            f.write(buf, written);
            f.close();
        }
    }

#ifdef ARDUINO
    heap_caps_free(buf);
#else
    delete[] buf;
#endif
}

void NeuralLabApp::loadModel() {
    if (!LittleFS.exists("/neural.nn")) return;

    File f = LittleFS.open("/neural.nn", "r");
    if (!f) return;

    int maxBytes = 4096;
    uint8_t* buf;
#ifdef ARDUINO
    buf = (uint8_t*)heap_caps_malloc(maxBytes, MALLOC_CAP_SPIRAM);
#else
    buf = new (std::nothrow) uint8_t[maxBytes];
#endif
    if (!buf) { f.close(); return; }

    int bytesRead = (int)f.read(buf, maxBytes);
    f.close();

    if (bytesRead > 0) {
        if (_engine.deserialize(buf, bytesRead)) {
            _epochCount = 0;
            _lastLoss = 1.0f;
            _training = false;
            updateHUD();
        }
    }

#ifdef ARDUINO
    heap_caps_free(buf);
#else
    delete[] buf;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
// LVGL Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::onDraw(lv_event_t* e) {
    (void)e;  // Drawing handled via image source update
}

void NeuralLabApp::onTrainTimer(lv_timer_t* timer) {
    NeuralLabApp* app = (NeuralLabApp*)lv_timer_get_user_data(timer);
    if (!app || !app->_screen) return;

    // Train multiple epochs per frame if training is active
    if (app->_training && app->_sampleCount > 0) {
        app->trainStep(50);  // 50 epochs per frame tick → ~1500 epochs/sec at 30fps
    }

    // Render visualization
    app->renderToBuffer();

    // Update info label and HUD
    app->updateInfoLabel();
    app->updateHUD();

    // Invalidate draw area to trigger redraw
    if (app->_drawObj) {
        lv_image_set_src(app->_drawObj, &app->_imgDsc);
        lv_obj_invalidate(app->_drawObj);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Key Handling
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::handleKey(const KeyEvent& ev) {
    if (!_screen) return;
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (ev.code) {
        // ── F1: Topology Menu — Add/Remove neurons ──
        case KeyCode::F1: {
            if (_engine.getNumLayers() > 2) {
                _engine.addNeuronToLayer(_selectedHiddenLayer);
                _epochCount = 0;
                _lastLoss = 1.0f;
                updateHUD();
            }
            break;
        }

        case KeyCode::DEL: {
            if (_engine.getNumLayers() > 2) {
                _engine.removeNeuronFromLayer(_selectedHiddenLayer);
                _epochCount = 0;
                _lastLoss = 1.0f;
                updateHUD();
            }
            break;
        }

        // ── F2: Toggle Training (Play/Pause) ──
        case KeyCode::F2:
            _training = !_training;
            break;

        // ── F3: Optimizer Settings — Cycle activation, adjust momentum ──
        case KeyCode::F3: {
            int next = ((int)_engine.getActivation() + 1) % (int)Activation::COUNT;
            _engine.setActivation((Activation)next);
            _engine.initWeights();
            _epochCount = 0;
            _lastLoss = 1.0f;
            _lossHistoryIdx = 0;
            _lossHistoryCount = 0;
            memset(_lossHistory, 0, sizeof(_lossHistory));
            updateHUD();
            break;
        }

        // ── F4: Cycle Scenarios (XOR, Circular, Spiral, Sine, Classifier) ──
        case KeyCode::F4: {
            int next = ((int)_scenario + 1) % (int)Scenario::COUNT;
            loadScenario((Scenario)next);
            break;
        }

        // ── F5: Save/Load Model ──
        case KeyCode::F5: {
            // Quick press = save, with SHIFT or REPEAT = load
            if (ev.action == KeyAction::REPEAT) {
                loadModel();
            } else {
                saveModel();
            }
            break;
        }

        // ── EXE (<): Step Mode — 1 backprop pass ──
        case KeyCode::EXE:
            if (_sampleCount > 0) {
                trainStep(1);
            }
            break;

        // ── Arrow keys ──
        case KeyCode::UP:
            if (_scenario == Scenario::CLASSIFIER || _scenario == Scenario::CIRCULAR ||
                _scenario == Scenario::SPIRAL) {
                _cursorY = (_cursorY - 1 + DB_H) % DB_H;
            } else {
                // Increase learning rate
                float lr = _engine.getLearningRate();
                lr += 0.005f;
                if (lr > 1.0f) lr = 1.0f;
                _engine.setLearningRate(lr);
            }
            break;
        case KeyCode::DOWN:
            if (_scenario == Scenario::CLASSIFIER || _scenario == Scenario::CIRCULAR ||
                _scenario == Scenario::SPIRAL) {
                _cursorY = (_cursorY + 1) % DB_H;
            } else {
                // Decrease learning rate
                float lr = _engine.getLearningRate();
                lr -= 0.005f;
                if (lr < 0.001f) lr = 0.001f;
                _engine.setLearningRate(lr);
            }
            break;
        case KeyCode::LEFT:
            if (_scenario == Scenario::CLASSIFIER || _scenario == Scenario::CIRCULAR ||
                _scenario == Scenario::SPIRAL) {
                _cursorX = (_cursorX - 1 + DB_W) % DB_W;
            }
            break;
        case KeyCode::RIGHT:
            if (_scenario == Scenario::CLASSIFIER || _scenario == Scenario::CIRCULAR ||
                _scenario == Scenario::SPIRAL) {
                _cursorX = (_cursorX + 1) % DB_W;
            }
            break;

        // ── ENTER: Place point (Blue=short, Red=long-press) ──
        case KeyCode::ENTER:
            if (_scenario == Scenario::CLASSIFIER || _scenario == Scenario::CIRCULAR ||
                _scenario == Scenario::SPIRAL) {
                if (!_enterHandled) {
                    _enterHandled = true;
                    // Place Blue point (class 1) on short press
                    addClassifierPoint(_cursorX, _cursorY, 1);
                } else if (ev.action == KeyAction::REPEAT) {
                    // Long press: change last point to Red (class 0), reset training
                    if (_sampleCount > 0) {
                        _samples[_sampleCount - 1].targets[0] = 0.0f;
                        _epochCount = 0;
                        _lastLoss = 1.0f;
                    }
                }
            }
            break;

        // ── NUM_1/NUM_2: Select hidden layer for F1 modification ──
        case KeyCode::NUM_1:
            if (_selectedHiddenLayer > 1) _selectedHiddenLayer--;
            break;
        case KeyCode::NUM_2:
            if (_selectedHiddenLayer < _engine.getNumLayers() - 2)
                _selectedHiddenLayer++;
            break;

        // ── NUM_3: Toggle L2 regularization ──
        case KeyCode::NUM_3: {
            float l2 = _engine.getL2Lambda();
            if (l2 < 1e-6f) {
                _engine.setL2Lambda(1e-4f);
            } else if (l2 < 5e-4f) {
                _engine.setL2Lambda(1e-3f);
            } else {
                _engine.setL2Lambda(0.0f);
            }
            updateHUD();
            break;
        }

        // ── NUM_4: Toggle auto-stop ──
        case KeyCode::NUM_4:
            _autoStop = !_autoStop;
            break;

        default:
            break;
    }

    // Reset ENTER long-press tracking on non-ENTER key
    if (ev.code != KeyCode::ENTER) {
        _enterHandled = false;
    }
}
