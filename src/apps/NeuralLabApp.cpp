/**
 * NeuralLabApp.cpp
 * Visual Neural Network Playground for NumOS.
 * App ID 16 — LVGL-native, decision boundary visualization.
 */

#include "NeuralLabApp.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

#ifdef ARDUINO
#include <esp_heap_caps.h>
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

    uint8_t r = (uint8_t)(value * 200);
    uint8_t b = (uint8_t)((1.0f - value) * 200);
    uint8_t g = (uint8_t)(40 + 30 * (1.0f - fabsf(value - 0.5f) * 2.0f));

    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════════

NeuralLabApp::NeuralLabApp()
    : _screen(nullptr)
    , _drawObj(nullptr)
    , _infoLabel(nullptr)
    , _chartObj(nullptr)
    , _trainTimer(nullptr)
    , _dbBuffer(nullptr)
    , _renderBuf(nullptr)
    , _training(false)
    , _epochCount(0)
    , _lastLoss(1.0f)
    , _lossHistoryIdx(0)
    , _lossHistoryCount(0)
    , _scenario(Scenario::XOR)
    , _sampleCount(0)
    , _cursorX(DB_W / 2)
    , _cursorY(DB_H / 2)
    , _cursorClass(0)
    , _selectedHiddenLayer(1)
{
    _infoBuf[0] = '\0';
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
    _screen   = nullptr;
    _drawObj  = nullptr;
    _infoLabel = nullptr;
    _chartObj = nullptr;

    _engine.freeAll();

    if (_dbBuffer)  { freeBuf16(_dbBuffer);  _dbBuffer  = nullptr; }
    if (_renderBuf) { freeBuf16(_renderBuf); _renderBuf = nullptr; }
}

void NeuralLabApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Neural Lab");
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
    lv_obj_set_style_text_color(_infoLabel, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(_infoLabel, &lv_font_montserrat_10, 0);
    lv_obj_align(_infoLabel, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    lv_label_set_text(_infoLabel, "Neural Lab | F2:Train F4:Scenario");

    // ── Training timer (runs at ~30 FPS for UI updates) ──
    _trainTimer = lv_timer_create(onTrainTimer, 33, this);
}

void NeuralLabApp::updateInfoLabel() {
    const char* scenarioName = "XOR";
    if (_scenario == Scenario::CLASSIFIER) scenarioName = "Classifier";
    else if (_scenario == Scenario::SINE_REGRESSION) scenarioName = "Sine";

    const char* actName = "Sigmoid";
    if (_engine.getActivation() == Activation::RELU) actName = "ReLU";
    else if (_engine.getActivation() == Activation::TANH) actName = "Tanh";

    snprintf(_infoBuf, sizeof(_infoBuf),
             "Epoch:%lu | Loss:%.4f | LR:%.2f | %s | %s",
             (unsigned long)_epochCount, (double)_lastLoss,
             (double)_engine.getLearningRate(),
             scenarioName, actName);
    lv_label_set_text(_infoLabel, _infoBuf);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Scenario Setup
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::loadScenario(Scenario s) {
    _scenario    = s;
    _training    = false;
    _epochCount  = 0;
    _lastLoss    = 1.0f;
    _lossHistoryIdx   = 0;
    _lossHistoryCount = 0;
    memset(_lossHistory, 0, sizeof(_lossHistory));

    switch (s) {
        case Scenario::XOR:            setupXOR();            break;
        case Scenario::CLASSIFIER:     setupClassifier();     break;
        case Scenario::SINE_REGRESSION: setupSineRegression(); break;
        default: setupXOR(); break;
    }

    _engine.initWeights();
    updateInfoLabel();
}

void NeuralLabApp::setupXOR() {
    const int topo[] = {2, 4, 1};
    _engine.setTopology(topo, 3);
    _engine.setLearningRate(0.5f);

    _sampleCount = 4;
    // XOR truth table
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
    _engine.setLearningRate(0.3f);

    // Pre-place some default points
    _sampleCount = 0;
    addClassifierPoint(20, 15, 0);  // Red
    addClassifierPoint(25, 20, 0);
    addClassifierPoint(15, 25, 0);
    addClassifierPoint(60, 45, 1);  // Blue
    addClassifierPoint(55, 40, 1);
    addClassifierPoint(65, 35, 1);

    _cursorX = DB_W / 2;
    _cursorY = DB_H / 2;
    _cursorClass = 0;
}

void NeuralLabApp::setupSineRegression() {
    const int topo[] = {1, 8, 1};
    _engine.setTopology(topo, 3);
    _engine.setActivation(Activation::TANH);
    _engine.setLearningRate(0.01f);

    // Generate sine samples: x in [0,1], y = (sin(2*pi*x) + 1) / 2
    _sampleCount = 20;
    for (int i = 0; i < _sampleCount; i++) {
        float x = (float)i / (_sampleCount - 1);
        _samples[i].inputs[0]  = x;
        _samples[i].targets[0] = (sinf(2.0f * (float)M_PI * x) + 1.0f) / 2.0f;
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
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Rendering
// ═══════════════════════════════════════════════════════════════════════════════

void NeuralLabApp::renderDecisionBoundary() {
    if (!_dbBuffer) return;

    // For each pixel in 80x60, pass normalized (x,y) through network
    for (int py = 0; py < DB_H; py++) {
        for (int px = 0; px < DB_W; px++) {
            float input[2];
            if (_scenario == Scenario::SINE_REGRESSION) {
                // For sine: x-axis is input, show prediction vs target
                input[0] = (float)px / DB_W;
                input[1] = 0.0f;
            } else {
                input[0] = (float)px / DB_W;
                input[1] = (float)py / DB_H;
            }

            _engine.forward(input);
            float out = _engine.getOutputAt(0);

            if (_scenario == Scenario::SINE_REGRESSION) {
                // For sine regression, color based on y-position matching output
                float targetY = 1.0f - (float)py / DB_H;  // flip Y
                float diff = fabsf(out - targetY);
                if (diff < 0.03f) {
                    // Predicted curve: bright green
                    _dbBuffer[py * DB_W + px] = rgb888to565(0x00FF00);
                } else {
                    // Background gradient
                    uint8_t g = (uint8_t)(20 + targetY * 30);
                    _dbBuffer[py * DB_W + px] = rgb888to565((g/2) << 16 | g << 8 | (g/2));
                }
            } else {
                // Classification: blend red/blue based on output
                _dbBuffer[py * DB_W + px] = blendColor(out);
            }
        }
    }
}

void NeuralLabApp::upscaleBuffer() {
    if (!_dbBuffer || !_renderBuf) return;

    // Upscale 80x60 → 320x240 (4x nearest neighbor)
    // But only fill the area below the status bar
    for (int py = 0; py < DB_H; py++) {
        for (int px = 0; px < DB_W; px++) {
            uint16_t c = _dbBuffer[py * DB_W + px];
            for (int sy = 0; sy < DB_SCALE; sy++) {
                int destY = py * DB_SCALE + sy;
                if (destY >= SCREEN_H) break;
                for (int sx = 0; sx < DB_SCALE; sx++) {
                    int destX = px * DB_SCALE + sx;
                    _renderBuf[destY * SCREEN_W + destX] = c;
                }
            }
        }
    }
}

void NeuralLabApp::renderClassifierPoints() {
    if (!_renderBuf) return;

    // Draw training points as 3x3 colored squares on the render buffer
    for (int i = 0; i < _sampleCount; i++) {
        int px = (int)(_samples[i].inputs[0] * DB_W) * DB_SCALE;
        int py = (int)(_samples[i].inputs[1] * DB_H) * DB_SCALE;
        uint16_t color = (_samples[i].targets[0] > 0.5f)
                         ? rgb888to565(COL_CLASS_B)
                         : rgb888to565(COL_CLASS_A);

        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                int rx = px + dx;
                int ry = py + dy;
                if (rx >= 0 && rx < SCREEN_W && ry >= 0 && ry < SCREEN_H) {
                    _renderBuf[ry * SCREEN_W + rx] = color;
                }
            }
        }
    }

    // Draw cursor for classifier mode
    if (_scenario == Scenario::CLASSIFIER) {
        int cx = _cursorX * DB_SCALE;
        int cy = _cursorY * DB_SCALE;
        uint16_t curCol = rgb888to565(COL_CURSOR);

        // Crosshair
        for (int d = -4; d <= 4; d++) {
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

    // Network graph drawn in bottom-right corner (120x80 area)
    int graphX = SCREEN_W - 130;
    int graphY = 30;
    int graphW = 120;
    int graphH = 80;

    // Semi-transparent dark background for graph
    for (int y = graphY; y < graphY + graphH && y < SCREEN_H; y++) {
        for (int x = graphX; x < graphX + graphW && x < SCREEN_W; x++) {
            if (x >= 0) {
                // Darken existing pixel
                uint16_t c = _renderBuf[y * SCREEN_W + x];
                uint8_t r = ((c >> 11) & 0x1F) >> 1;
                uint8_t g = ((c >> 5) & 0x3F) >> 1;
                uint8_t b = (c & 0x1F) >> 1;
                _renderBuf[y * SCREEN_W + x] = (r << 11) | (g << 5) | b;
            }
        }
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

    // Draw connections (weight-colored lines)
    for (int l = 0; l < numLayers - 1; l++) {
        int fromN = _engine.getLayerSize(l);
        int toN   = _engine.getLayerSize(l + 1);
        for (int j = 0; j < toN; j++) {
            for (int i = 0; i < fromN; i++) {
                float w = _engine.getWeight(l, j, i);
                // Color: blue for positive, red for negative
                uint16_t lineColor;
                float absW = fabsf(w);
                if (absW < 0.01f) continue;  // skip near-zero weights
                if (w > 0) {
                    uint8_t intensity = (uint8_t)(60 + 140 * (absW > 2.0f ? 1.0f : absW / 2.0f));
                    lineColor = rgb888to565((uint32_t)intensity << 0 |
                                            (uint32_t)(intensity/3) << 8);
                } else {
                    uint8_t intensity = (uint8_t)(60 + 140 * (absW > 2.0f ? 1.0f : absW / 2.0f));
                    lineColor = rgb888to565((uint32_t)intensity << 16 |
                                            (uint32_t)(intensity/3) << 8);
                }

                // Bresenham line from positions[l][i] to positions[l+1][j]
                int x0 = positions[l][i].x, y0 = positions[l][i].y;
                int x1 = positions[l+1][j].x, y1 = positions[l+1][j].y;
                int dx = abs(x1 - x0), dy = abs(y1 - y0);
                int sx = (x0 < x1) ? 1 : -1;
                int sy = (y0 < y1) ? 1 : -1;
                int err = dx - dy;

                while (true) {
                    if (x0 >= 0 && x0 < SCREEN_W && y0 >= 0 && y0 < SCREEN_H) {
                        _renderBuf[y0 * SCREEN_W + x0] = lineColor;
                    }
                    if (x0 == x1 && y0 == y1) break;
                    int e2 = 2 * err;
                    if (e2 > -dy) { err -= dy; x0 += sx; }
                    if (e2 <  dx) { err += dx; y0 += sy; }
                }
            }
        }
    }

    // Draw neurons as circles
    for (int l = 0; l < numLayers; l++) {
        int n = _engine.getLayerSize(l);
        for (int i = 0; i < n; i++) {
            int cx = positions[l][i].x;
            int cy = positions[l][i].y;
            float nOut = _engine.getNeuronOutput(l, i);
            uint8_t brightness = (uint8_t)(80 + 175 * (nOut > 1.0f ? 1.0f : (nOut < 0.0f ? 0.0f : nOut)));
            uint16_t nCol = rgb888to565(((uint32_t)brightness << 16) |
                                         ((uint32_t)brightness << 8) |
                                         brightness);

            // Draw filled circle radius 3
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
        }
    }
}

void NeuralLabApp::renderLossChart() {
    if (!_renderBuf || _lossHistoryCount < 2) return;

    // Draw loss chart in bottom-right corner (100x40 area)
    int chartX = SCREEN_W - 110;
    int chartY = SCREEN_H - 55;
    int chartW = 100;
    int chartH = 40;

    // Dark background
    for (int y = chartY; y < chartY + chartH && y < SCREEN_H; y++) {
        for (int x = chartX; x < chartX + chartW && x < SCREEN_W; x++) {
            if (x >= 0) {
                _renderBuf[y * SCREEN_W + x] = rgb888to565(0x111118);
            }
        }
    }

    // Border
    uint16_t borderCol = rgb888to565(0x333344);
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

    // Find max loss for scaling
    float maxLoss = 0.001f;
    for (int i = 0; i < _lossHistoryCount; i++) {
        int idx = (_lossHistoryIdx - _lossHistoryCount + i + LOSS_HISTORY_SIZE) % LOSS_HISTORY_SIZE;
        if (_lossHistory[idx] > maxLoss) maxLoss = _lossHistory[idx];
    }

    // Plot loss curve
    uint16_t lineCol = rgb888to565(0x00FF88);
    int prevPx = -1, prevPy = -1;
    for (int i = 0; i < _lossHistoryCount; i++) {
        int idx = (_lossHistoryIdx - _lossHistoryCount + i + LOSS_HISTORY_SIZE) % LOSS_HISTORY_SIZE;
        int px = chartX + 1 + i * (chartW - 2) / _lossHistoryCount;
        float norm = _lossHistory[idx] / maxLoss;
        if (norm > 1.0f) norm = 1.0f;
        int py = chartY + chartH - 2 - (int)(norm * (chartH - 4));

        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
            _renderBuf[py * SCREEN_W + px] = lineCol;
        }

        // Connect with line to previous point
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
}

void NeuralLabApp::renderToBuffer() {
    // 1. Render decision boundary to low-res buffer
    renderDecisionBoundary();

    // 2. Upscale to full resolution
    upscaleBuffer();

    // 3. Overlay training points (for classifier)
    if (_scenario != Scenario::SINE_REGRESSION) {
        renderClassifierPoints();
    }

    // 4. Overlay network graph
    renderNetworkGraph();

    // 5. Overlay loss chart
    renderLossChart();
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

    // Update info label
    app->updateInfoLabel();

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
        // ── F1: Add/Remove neurons in hidden layer ──
        case KeyCode::F1: {
            // Add neuron to selected hidden layer
            if (_engine.getNumLayers() > 2) {
                _engine.addNeuronToLayer(_selectedHiddenLayer);
                _epochCount = 0;
                _lastLoss = 1.0f;
            }
            break;
        }

        // ── SHIFT+F1 or DEL: Remove neuron ──
        case KeyCode::DEL: {
            if (_engine.getNumLayers() > 2) {
                _engine.removeNeuronFromLayer(_selectedHiddenLayer);
                _epochCount = 0;
                _lastLoss = 1.0f;
            }
            break;
        }

        // ── F2: Start/Pause Training ──
        case KeyCode::F2:
            _training = !_training;
            break;

        // ── F3: Reset weights (Xavier/He re-initialization) ──
        case KeyCode::F3:
            _engine.initWeights();
            _epochCount = 0;
            _lastLoss = 1.0f;
            _lossHistoryIdx = 0;
            _lossHistoryCount = 0;
            memset(_lossHistory, 0, sizeof(_lossHistory));
            break;

        // ── F4: Cycle scenarios ──
        case KeyCode::F4: {
            int next = ((int)_scenario + 1) % (int)Scenario::COUNT;
            loadScenario((Scenario)next);
            break;
        }

        // ── F5: Cycle activation functions ──
        case KeyCode::F5: {
            int next = ((int)_engine.getActivation() + 1) % (int)Activation::COUNT;
            _engine.setActivation((Activation)next);
            _engine.initWeights();
            _epochCount = 0;
            _lastLoss = 1.0f;
            break;
        }

        // ── EXE: Step-by-step training (1 epoch) ──
        case KeyCode::EXE:
            if (_sampleCount > 0) {
                trainStep(1);
            }
            break;

        // ── Arrow keys: Move cursor (Classifier mode) ──
        case KeyCode::UP:
            if (_scenario == Scenario::CLASSIFIER) {
                _cursorY = (_cursorY - 1 + DB_H) % DB_H;
            }
            break;
        case KeyCode::DOWN:
            if (_scenario == Scenario::CLASSIFIER) {
                _cursorY = (_cursorY + 1) % DB_H;
            }
            break;
        case KeyCode::LEFT:
            if (_scenario == Scenario::CLASSIFIER) {
                _cursorX = (_cursorX - 1 + DB_W) % DB_W;
            }
            break;
        case KeyCode::RIGHT:
            if (_scenario == Scenario::CLASSIFIER) {
                _cursorX = (_cursorX + 1) % DB_W;
            }
            break;

        // ── ENTER: Place point (Classifier mode) ──
        case KeyCode::ENTER:
            if (_scenario == Scenario::CLASSIFIER) {
                addClassifierPoint(_cursorX, _cursorY, _cursorClass);
            }
            break;

        // ── NUM_1/NUM_2: Select point class (Red=0 / Blue=1) ──
        case KeyCode::NUM_1:
            _cursorClass = 0;
            break;
        case KeyCode::NUM_2:
            _cursorClass = 1;
            break;

        // ── NUM_3/NUM_4: Select hidden layer for F1 modification ──
        case KeyCode::NUM_3:
            if (_selectedHiddenLayer > 1) _selectedHiddenLayer--;
            break;
        case KeyCode::NUM_4:
            if (_selectedHiddenLayer < _engine.getNumLayers() - 2)
                _selectedHiddenLayer++;
            break;

        default:
            break;
    }
}
