/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "FractalApp.h"

#include <cstring>
#include <new>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#define FRACTAL_DIAG 0

#if FRACTAL_DIAG
#define FLOG(...) Serial.printf(__VA_ARGS__)
#define FLOGN(msg) Serial.println(msg)
#else
#define FLOG(...)
#define FLOGN(msg)
#endif

namespace {
constexpr int kStripHeight = 16;
constexpr int kPassBlockSizes[] = {8, 4, 1};
constexpr int kNumRenderPasses = static_cast<int>(sizeof(kPassBlockSizes) / sizeof(kPassBlockSizes[0]));
}

const FractalApp::ModuleCardConfig FractalApp::kModules[FractalApp::MODULE_COUNT] = {
    {"Mandelbrot",    "z^2 + c",          FractalEngine::FractalType::Mandelbrot,   -0.50f,  0.00f, 1.00f,  96, 0.00f, 8, -0.80f, 0.156f},
    {"Julia",         "k=-0.8 + 0.156i", FractalEngine::FractalType::Julia,         0.00f,   0.00f, 1.20f, 128, 0.00f, 8, -0.80f, 0.156f},
    {"Burning Ship",  "Abs Fold",         FractalEngine::FractalType::BurningShip,  -1.76f, -0.03f, 1.70f, 128, 0.00f, 8, -0.80f, 0.156f},
    {"Mandelbulb 3D", "Slice z=0",        FractalEngine::FractalType::Mandelbulb3D,  0.00f,   0.00f, 1.00f,  56, 0.00f, 8, -0.80f, 0.156f},
};

FractalApp::FractalApp()
    : _screen(nullptr),
      _moduleRoot(nullptr),
      _canvasArea(nullptr),
      _fractalImage(nullptr),
      _loadingLabel(nullptr),
      _centerX(-0.5f),
      _centerY(0.0f),
      _zoom(1.0f),
      _sliceZ(0.0f),
      _maxIter(96),
      _mandelbulbPower(8),
      _juliaCRe(-0.8f),
      _juliaCIm(0.156f),
      _activeType(FractalEngine::FractalType::Mandelbrot) {
    std::memset(&_imgDsc, 0, sizeof(lv_image_dsc_t));
    for (int i = 0; i < MODULE_COUNT; ++i) {
        std::memset(&_atlasPreviewDsc[i], 0, sizeof(lv_image_dsc_t));
    }
}

FractalApp::~FractalApp() {
    end();
}

bool FractalApp::consumeExitRequest() {
    bool requested = _exitRequested;
    _exitRequested = false;
    return requested;
}

void FractalApp::begin() {
    if (_screen) return;

    FLOGN("[Fractal] begin() start");

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);

    _statusBar.create(_screen);
    _statusBar.setTitle("Fractal Atlas");

    generateAtlasPreviewsOnce();
    FLOGN("[Fractal] previews ready");
    buildAtlasLauncher();
    FLOGN("[Fractal] atlas launcher built");

    _orbit = new (std::nothrow) FractalEngine::ReferenceOrbit();
    FLOG("[Fractal] orbit alloc: %s\n", _orbit ? "OK" : "FAIL");

    _renderRequested = false;
    _renderComplete = false;
    _abortRequested = false;
    _isIdle = true;
    _completedStrips = 0;
    _totalStrips = 0;
    _rebaseRequired = true;
    _renderPass = 0;

    FLOGN("[Fractal] begin() done");
}

void FractalApp::end() {
    stopRenderTask();

    destroyModuleUI();
    destroyAtlasLauncher();

    _statusBar.destroy();

    if (_screen) {
        lv_obj_delete(_screen);
        _screen = nullptr;
    }

    _buffer.reset();

    if (_orbit) {
        delete _orbit;
        _orbit = nullptr;
    }
}

void FractalApp::load() {
    FLOGN("[Fractal] load() enter");
    if (!_screen) begin();

    _exitRequested = false;
    _pendingTransition = TransitionRequest::None;
    _pendingModuleIndex = -1;
    _transitionBusy = false;

    if (_state == AppState::STATE_FRACTAL_MODULE) {
        performLeaveModuleTransition();
    } else {
        stopRenderTask();
    }

    if (!_atlasRoot) {
        buildAtlasLauncher();
    }

    _state = AppState::STATE_ATLAS_LAUNCHER;
    _statusBar.setTitle("Fractal Atlas");
    lv_screen_load(_screen);
    _statusBar.update();
    FLOGN("[Fractal] load() exit");
}

void FractalApp::update() {
    if (!_screen) return;

    processPendingTransition();

    if (_state == AppState::STATE_FRACTAL_MODULE) {
        refreshModuleFrame();
    }

    _statusBar.update();
}

void FractalApp::handleKey(const KeyEvent& ev) {
    handleInput(ev);
}

void FractalApp::handleInput(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) {
        return;
    }

    if (_transitionBusy) {
        return;
    }

    if (_state == AppState::STATE_ATLAS_LAUNCHER) {
        int next = _selectedCard;
        switch (ev.code) {
            case KeyCode::UP:
                if (next >= 2) next -= 2;
                break;
            case KeyCode::DOWN:
                if (next + 2 < MODULE_COUNT) next += 2;
                break;
            case KeyCode::LEFT:
                if ((next & 1) == 1) next -= 1;
                break;
            case KeyCode::RIGHT:
                if ((next & 1) == 0 && (next + 1) < MODULE_COUNT) next += 1;
                break;
            case KeyCode::ENTER:
            case KeyCode::EXE:
                _pendingModuleIndex = _selectedCard;
                _pendingTransition = TransitionRequest::EnterModule;
                return;
            case KeyCode::MODE:
                _exitRequested = true;
                return;
            default:
                return;
        }

        if (next != _selectedCard) {
            _selectedCard = next;
            updateAtlasSelection();
        }
        return;
    }

    if (_state != AppState::STATE_FRACTAL_MODULE) {
        return;
    }

    if (ev.code == KeyCode::MODE) {
        _pendingTransition = TransitionRequest::LeaveModule;
        return;
    }

    const bool isMandelbulb = (_activeType == FractalEngine::FractalType::Mandelbulb3D);
    const float panStep = 0.2f / _zoom;
    bool changed = false;

    switch (ev.code) {
        case KeyCode::UP:
            if (isMandelbulb) {
                _sliceZ += 0.05f / _zoom;
                changed = true;
            } else {
                _centerY += panStep;
                shiftBuffer(0, 15);
            }
            break;

        case KeyCode::DOWN:
            if (isMandelbulb) {
                _sliceZ -= 0.05f / _zoom;
                changed = true;
            } else {
                _centerY -= panStep;
                shiftBuffer(0, -15);
            }
            break;

        case KeyCode::LEFT:
            _centerX -= panStep;
            if (!isMandelbulb) shiftBuffer(15, 0);
            else changed = true;
            break;

        case KeyCode::RIGHT:
            _centerX += panStep;
            if (!isMandelbulb) shiftBuffer(-15, 0);
            else changed = true;
            break;

        case KeyCode::ADD:
            _zoom *= 1.5f;
            if (!isMandelbulb) scaleBuffer(1.5f);
            else changed = true;
            break;

        case KeyCode::SUB:
            _zoom /= 1.5f;
            if (!isMandelbulb) scaleBuffer(1.0f / 1.5f);
            else changed = true;
            break;

        case KeyCode::MUL:
            _maxIter += 32;
            changed = true;
            break;

        case KeyCode::DIV:
            if (_maxIter > 32) _maxIter -= 32;
            changed = true;
            break;

        default:
            break;
    }

    if (changed) {
        renderFractal();
    }
}

void FractalApp::generateAtlasPreviewsOnce() {
    if (_atlasPreviewReady) return;

    FLOGN("[Fractal] preview generation start");

    for (int i = 0; i < MODULE_COUNT; ++i) {
        const ModuleCardConfig& cfg = kModules[i];
        FLOG("[Fractal] preview module %d (%s)\n", i, cfg.title);

        int previewIter = cfg.defaultMaxIter;
        int previewStep = 2;
        if (cfg.type == FractalEngine::FractalType::Mandelbulb3D) {
            previewIter = 20;
            previewStep = 4;
        } else if (previewIter > 48) {
            previewIter = 48;
        }

        FractalEngine::renderFractalStrip(
            _atlasPreviewPixels[i],
            PREVIEW_W,
            PREVIEW_H,
            cfg.type,
            cfg.defaultCenterX,
            cfg.defaultCenterY,
            cfg.defaultZoom,
            previewIter,
            0,
            PREVIEW_H,
            nullptr,
            previewStep,
            true,
            cfg.defaultJuliaCRe,
            cfg.defaultJuliaCIm,
            cfg.defaultSliceZ,
            cfg.defaultPower
        );

        _atlasPreviewDsc[i].header.magic = LV_IMAGE_HEADER_MAGIC;
        _atlasPreviewDsc[i].header.cf = LV_COLOR_FORMAT_RGB565;
        _atlasPreviewDsc[i].header.flags = 0;
        _atlasPreviewDsc[i].header.w = PREVIEW_W;
        _atlasPreviewDsc[i].header.h = PREVIEW_H;
        _atlasPreviewDsc[i].header.stride = PREVIEW_W * 2;
        _atlasPreviewDsc[i].data_size = PREVIEW_W * PREVIEW_H * 2;
        _atlasPreviewDsc[i].data = reinterpret_cast<const uint8_t*>(_atlasPreviewPixels[i]);

#ifdef ARDUINO
        vTaskDelay(1);
#endif
    }

    _atlasPreviewReady = true;
    FLOGN("[Fractal] preview generation done");
}

void FractalApp::buildAtlasLauncher() {
    if (_atlasRoot) return;
    if (!_atlasPreviewReady) generateAtlasPreviewsOnce();

    _statusBar.setTitle("Fractal Atlas");

    _atlasRoot = lv_obj_create(_screen);
    lv_obj_set_size(_atlasRoot, SCREEN_W, CANVAS_H);
    lv_obj_align(_atlasRoot, LV_ALIGN_TOP_LEFT, 0, BAR_H);
    lv_obj_set_style_bg_color(_atlasRoot, lv_color_hex(0x0D1117), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_atlasRoot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_atlasRoot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_atlasRoot, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(_atlasRoot, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_column(_atlasRoot, 8, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(_atlasRoot, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(_atlasRoot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(_atlasRoot, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(_atlasRoot, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < MODULE_COUNT; ++i) {
        lv_obj_t* card = lv_obj_create(_atlasRoot);
        _atlasCards[i] = card;

        lv_obj_set_size(card, 148, 100);
        lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x161B22), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(card, lv_color_hex(0x2A3340), LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_row(card, 3, LV_PART_MAIN);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* previewFrame = lv_obj_create(card);
        lv_obj_set_size(previewFrame, PREVIEW_W + 6, PREVIEW_H + 6);
        lv_obj_set_style_radius(previewFrame, 6, LV_PART_MAIN);
        lv_obj_set_style_bg_color(previewFrame, lv_color_hex(0x05070B), LV_PART_MAIN);
        lv_obj_set_style_border_color(previewFrame, lv_color_hex(0x2A3340), LV_PART_MAIN);
        lv_obj_set_style_border_width(previewFrame, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_all(previewFrame, 0, LV_PART_MAIN);
        lv_obj_remove_flag(previewFrame, LV_OBJ_FLAG_SCROLLABLE);

        _atlasPreviewImages[i] = lv_image_create(previewFrame);
        lv_image_set_src(_atlasPreviewImages[i], &_atlasPreviewDsc[i]);
        lv_obj_center(_atlasPreviewImages[i]);

        lv_obj_t* title = lv_label_create(card);
        lv_label_set_text(title, kModules[i].title);
        lv_obj_set_style_text_color(title, lv_color_hex(0xE6EDF3), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);

        lv_obj_t* badge = lv_label_create(card);
        lv_label_set_text(badge, kModules[i].badge);
        lv_obj_set_style_text_color(badge, lv_color_hex(0x7D8590), LV_PART_MAIN);
        lv_obj_set_style_text_font(badge, &lv_font_montserrat_14, LV_PART_MAIN);
    }

    updateAtlasSelection();
}

void FractalApp::destroyAtlasLauncher() {
    if (_atlasRoot) {
        lv_obj_delete(_atlasRoot);
        _atlasRoot = nullptr;
    }
    for (int i = 0; i < MODULE_COUNT; ++i) {
        _atlasCards[i] = nullptr;
        _atlasPreviewImages[i] = nullptr;
    }
}

void FractalApp::updateAtlasSelection() {
    for (int i = 0; i < MODULE_COUNT; ++i) {
        if (!_atlasCards[i]) continue;
        const bool selected = (i == _selectedCard);

        lv_obj_set_style_outline_width(_atlasCards[i], selected ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_outline_color(_atlasCards[i], lv_color_hex(0x58A6FF), LV_PART_MAIN);
        lv_obj_set_style_border_color(
            _atlasCards[i],
            selected ? lv_color_hex(0x58A6FF) : lv_color_hex(0x2A3340),
            LV_PART_MAIN
        );
        // Avoid per-card transform effects on MCU to keep LVGL draw path deterministic.
        lv_obj_set_style_transform_zoom(_atlasCards[i], 256, LV_PART_MAIN);
        lv_obj_set_style_translate_y(_atlasCards[i], 0, LV_PART_MAIN);
    }
}

void FractalApp::applyModuleDefaults(int moduleIndex) {
    if (moduleIndex < 0 || moduleIndex >= MODULE_COUNT) {
        moduleIndex = 0;
    }

    const ModuleCardConfig& cfg = kModules[moduleIndex];
    _selectedCard = moduleIndex;
    _activeType = cfg.type;
    _centerX = cfg.defaultCenterX;
    _centerY = cfg.defaultCenterY;
    _zoom = cfg.defaultZoom;
    _sliceZ = cfg.defaultSliceZ;
    _maxIter = cfg.defaultMaxIter;
    _mandelbulbPower = cfg.defaultPower;
    _juliaCRe = cfg.defaultJuliaCRe;
    _juliaCIm = cfg.defaultJuliaCIm;

    _prevZoom = 0.0f;
    _prevMaxIter = 0;
    _rebaseRequired = (_activeType == FractalEngine::FractalType::Mandelbrot);
}

bool FractalApp::initializeBuffer() {
    if (_buffer.data()) return true;
    if (!_buffer.allocate(SCREEN_W * CANVAS_H)) return false;

    _imgDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    _imgDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    _imgDsc.header.flags = 0;
    _imgDsc.header.w = SCREEN_W;
    _imgDsc.header.h = CANVAS_H;
    _imgDsc.header.stride = SCREEN_W * 2;
    _imgDsc.data_size = SCREEN_W * CANVAS_H * 2;
    _imgDsc.data = reinterpret_cast<const uint8_t*>(_buffer.data());
    return true;
}

bool FractalApp::startRenderTask() {
#ifdef ARDUINO
    if (_renderTaskHandle) return true;

    FLOGN("[Fractal] startRenderTask()");

    _taskShouldExit.store(false, std::memory_order_relaxed);
    _taskExited.store(false, std::memory_order_relaxed);

    BaseType_t rc = xTaskCreatePinnedToCore(
        renderTaskWrapper,
        "fractalTask",
        32768,
        this,
        1,
        &_renderTaskHandle,
        1
    );

    if (rc != pdPASS || !_renderTaskHandle) {
        _renderTaskHandle = nullptr;
        FLOGN("[Fractal] startRenderTask FAILED");
        return false;
    }

    FLOGN("[Fractal] startRenderTask OK");

    vTaskDelay(1);
#endif
    return true;
}

void FractalApp::stopRenderTask() {
#ifdef ARDUINO
    if (_renderTaskHandle) {
    FLOGN("[Fractal] stopRenderTask()");
        _abortRequested = true;
        _renderRequested = false;
        _taskShouldExit.store(true, std::memory_order_relaxed);
        xTaskNotifyGive(_renderTaskHandle);

        const uint32_t t0 = millis();
        while (!_taskExited.load(std::memory_order_acquire) && (millis() - t0) < 1500) {
            vTaskDelay(1);
        }

        if (!_taskExited.load(std::memory_order_acquire)) {
            FLOGN("[Fractal] stopRenderTask timeout -> force delete");
            vTaskDelete(_renderTaskHandle);
            _taskExited.store(true, std::memory_order_release);
        }

        _renderTaskHandle = nullptr;
    }
#endif
    _renderRequested = false;
    _renderComplete = false;
    _abortRequested = false;
    _isIdle = true;
}

void FractalApp::createModuleUI() {
    _statusBar.setTitle("<- Fractals");

    _moduleRoot = lv_obj_create(_screen);
    lv_obj_set_size(_moduleRoot, SCREEN_W, CANVAS_H);
    lv_obj_align(_moduleRoot, LV_ALIGN_TOP_LEFT, 0, BAR_H);
    lv_obj_set_style_bg_color(_moduleRoot, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_moduleRoot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_moduleRoot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_moduleRoot, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(_moduleRoot, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(_moduleRoot, LV_OBJ_FLAG_SCROLLABLE);

    _canvasArea = lv_obj_create(_moduleRoot);
    lv_obj_set_size(_canvasArea, SCREEN_W, CANVAS_H);
    lv_obj_align(_canvasArea, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_canvasArea, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_canvasArea, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_canvasArea, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_canvasArea, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(_canvasArea, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(_canvasArea, LV_OBJ_FLAG_SCROLLABLE);

    _fractalImage = lv_image_create(_canvasArea);
    lv_obj_align(_fractalImage, LV_ALIGN_TOP_LEFT, 0, 0);

    _loadingLabel = lv_label_create(_moduleRoot);
    lv_label_set_text(_loadingLabel, "Rendering...");
    lv_obj_set_style_text_color(_loadingLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(_loadingLabel, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
    lv_obj_add_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN);
}

void FractalApp::destroyModuleUI() {
    if (_moduleRoot) {
        lv_obj_delete(_moduleRoot);
    }

    _moduleRoot = nullptr;
    _canvasArea = nullptr;
    _fractalImage = nullptr;
    _loadingLabel = nullptr;

    _buffer.reset();
    std::memset(&_imgDsc, 0, sizeof(lv_image_dsc_t));
}

bool FractalApp::performEnterModuleTransition(int moduleIndex) {
    if (moduleIndex < 0 || moduleIndex >= MODULE_COUNT) {
        return false;
    }

    FLOG("[Fractal] enter transition module=%d\n", moduleIndex);

    _transitionBusy = true;

    // Safe transition order: stop task -> delete launcher -> allocate canvas -> start task.
    stopRenderTask();
    applyModuleDefaults(moduleIndex);
    destroyAtlasLauncher();
    createModuleUI();

    if (!initializeBuffer()) {
        FLOGN("[Fractal] initializeBuffer FAILED");
        destroyModuleUI();
        buildAtlasLauncher();
        _state = AppState::STATE_ATLAS_LAUNCHER;
        _transitionBusy = false;
        return false;
    }

    _state = AppState::STATE_FRACTAL_MODULE;

    if (!startRenderTask()) {
        // Fallback to synchronous render path if task creation fails.
        _renderTaskHandle = nullptr;
    }

    _renderRequested = false;
    _renderComplete = false;
    _completedStrips = 0;
    _renderPass = 0;
    _totalStrips = (CANVAS_H + kStripHeight - 1) / kStripHeight;

    _pendingTransition = TransitionRequest::None;
    _pendingModuleIndex = -1;
    _transitionBusy = false;

    renderFractal();
    return true;
}

void FractalApp::performLeaveModuleTransition() {
    _transitionBusy = true;

    stopRenderTask();
    destroyModuleUI();
    _state = AppState::STATE_ATLAS_LAUNCHER;
    buildAtlasLauncher();
    _statusBar.setTitle("Fractal Atlas");

    _pendingTransition = TransitionRequest::None;
    _pendingModuleIndex = -1;
    _transitionBusy = false;
}

void FractalApp::processPendingTransition() {
    if (_transitionBusy) {
        return;
    }

    switch (_pendingTransition) {
        case TransitionRequest::EnterModule:
            if (_pendingModuleIndex >= 0) {
                performEnterModuleTransition(_pendingModuleIndex);
            } else {
                _pendingTransition = TransitionRequest::None;
            }
            break;

        case TransitionRequest::LeaveModule:
            performLeaveModuleTransition();
            break;

        case TransitionRequest::None:
        default:
            break;
    }
}

bool FractalApp::enterFractalModule(int moduleIndex) {
    _pendingModuleIndex = moduleIndex;
    _pendingTransition = TransitionRequest::EnterModule;
    return true;
}

void FractalApp::leaveFractalModule() {
    _pendingTransition = TransitionRequest::LeaveModule;
}

void FractalApp::refreshModuleFrame() {
    if (_state != AppState::STATE_FRACTAL_MODULE) return;
    if (!_fractalImage || !_imgDsc.data) return;

    bool shouldRefresh = false;

    if (_completedStrips > 0) {
        _completedStrips = 0;
        shouldRefresh = true;
    }

    if (_renderComplete) {
        _renderComplete = false;
        shouldRefresh = true;
    }

    if (shouldRefresh) {
        lv_image_set_src(_fractalImage, &_imgDsc);
        lv_obj_invalidate(_fractalImage);
        if (_loadingLabel && _renderPass >= kNumRenderPasses) {
            lv_obj_add_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void FractalApp::renderFractal() {
    if (_state != AppState::STATE_FRACTAL_MODULE || !_buffer.data() || !_imgDsc.data || !_fractalImage) {
        return;
    }

    _abortRequested = true;
#ifdef ARDUINO
    const uint32_t t0 = millis();
    while (!_isIdle && (millis() - t0) < 800) {
        vTaskDelay(1);
    }
#endif

    if (_loadingLabel) {
        lv_obj_remove_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN);
    }

    _abortRequested = false;
    _renderComplete = false;
    _completedStrips = 0;
    _renderPass = 0;
    _totalStrips = (CANVAS_H + kStripHeight - 1) / kStripHeight;

#ifdef ARDUINO
    if (_renderTaskHandle) {
        _renderRequested = true;
        _isIdle = false;
        xTaskNotifyGive(_renderTaskHandle);
        return;
    }
#endif

    // Fallback path if the render task is unavailable.
    FractalEngine::renderFractalStrip(
        _buffer.data(),
        SCREEN_W,
        CANVAS_H,
        _activeType,
        _centerX,
        _centerY,
        _zoom,
        _maxIter,
        0,
        CANVAS_H,
        nullptr,
        2,
        true,
        _juliaCRe,
        _juliaCIm,
        _sliceZ,
        _mandelbulbPower
    );

    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);
    if (_loadingLabel) lv_obj_add_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN);
}

void FractalApp::shiftBuffer(int dx, int dy) {
    if (_state != AppState::STATE_FRACTAL_MODULE) return;

    uint16_t* p = _buffer.data();
    if (!p) return;

    _abortRequested = true;
#ifdef ARDUINO
    const uint32_t t0 = millis();
    while (!_isIdle && (millis() - t0) < 800) {
        vTaskDelay(1);
    }
#endif

    if (dy != 0) {
        int rowBytes = SCREEN_W * static_cast<int>(sizeof(uint16_t));
        if (dy > 0) {
            std::memmove(p + dy * SCREEN_W, p, (CANVAS_H - dy) * rowBytes);
        } else {
            std::memmove(p, p - dy * SCREEN_W, (CANVAS_H + dy) * rowBytes);
        }
    }

    if (dx != 0) {
        for (int y = 0; y < CANVAS_H; ++y) {
            if (dx > 0) {
                std::memmove(p + y * SCREEN_W + dx, p + y * SCREEN_W, (SCREEN_W - dx) * sizeof(uint16_t));
            } else {
                std::memmove(p + y * SCREEN_W, p + y * SCREEN_W - dx, (SCREEN_W + dx) * sizeof(uint16_t));
            }
        }
    }

    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);

    renderFractal();
}

void FractalApp::scaleBuffer(float scaleFactor) {
    if (_state != AppState::STATE_FRACTAL_MODULE) return;

    uint16_t* p = _buffer.data();
    if (!p) return;

    _abortRequested = true;
#ifdef ARDUINO
    const uint32_t t0 = millis();
    while (!_isIdle && (millis() - t0) < 800) {
        vTaskDelay(1);
    }
#endif

    utils::PSRAMBuffer<uint16_t> scratch;
    if (!scratch.allocate(SCREEN_W * CANVAS_H)) {
        renderFractal();
        return;
    }

    uint16_t* s = scratch.data();
    float invScale = 1.0f / scaleFactor;
    float cx = SCREEN_W / 2.0f;
    float cy = CANVAS_H / 2.0f;

    for (int y = 0; y < CANVAS_H; ++y) {
        for (int x = 0; x < SCREEN_W; ++x) {
            int srcX = static_cast<int>(cx + (x - cx) * invScale);
            int srcY = static_cast<int>(cy + (y - cy) * invScale);

            if (srcX >= 0 && srcX < SCREEN_W && srcY >= 0 && srcY < CANVAS_H) {
                s[y * SCREEN_W + x] = p[srcY * SCREEN_W + srcX];
            } else {
                s[y * SCREEN_W + x] = 0x0000;
            }
        }
    }

    std::memcpy(p, s, SCREEN_W * CANVAS_H * sizeof(uint16_t));

    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);

    renderFractal();
}

void FractalApp::renderTaskWrapper(void* param) {
#ifdef ARDUINO
    FractalApp* app = static_cast<FractalApp*>(param);
    FLOGN("[Fractal] renderTask started");

    while (true) {
        app->_isIdle = true;
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        app->_isIdle = false;

        if (app->_taskShouldExit.load(std::memory_order_relaxed)) {
            break;
        }

        if (!app->_renderRequested || app->_state != AppState::STATE_FRACTAL_MODULE || !app->_buffer.data()) {
            app->_isIdle = true;
            continue;
        }

        app->_renderRequested = false;

        if (app->_activeType == FractalEngine::FractalType::Mandelbrot) {
            if (!app->_orbit) {
                app->_isIdle = true;
                continue;
            }

            if (app->_zoom != app->_prevZoom || app->_maxIter != app->_prevMaxIter || app->_rebaseRequired) {
                FractalEngine::buildReferenceOrbit(*app->_orbit, (double)app->_centerX, (double)app->_centerY, app->_maxIter);
                app->_prevZoom = app->_zoom;
                app->_prevMaxIter = app->_maxIter;
                app->_rebaseRequired = false;
            }
        }

        while (app->_renderPass < kNumRenderPasses && !app->_abortRequested && app->_buffer.data()) {
            int stepSize = kPassBlockSizes[app->_renderPass];

            for (int y = 0; y < FractalApp::CANVAS_H; y += kStripHeight) {
                if (app->_abortRequested || !app->_buffer.data()) {
                    break;
                }

                int yEnd = y + kStripHeight;
                if (yEnd > FractalApp::CANVAS_H) yEnd = FractalApp::CANVAS_H;

                if (app->_activeType == FractalEngine::FractalType::Mandelbrot && app->_orbit) {
                    FractalEngine::renderMandelbrotPerturbationStrip(
                        app->_buffer.data(),
                        FractalApp::SCREEN_W,
                        FractalApp::CANVAS_H,
                        app->_centerX,
                        app->_centerY,
                        app->_zoom,
                        app->_maxIter,
                        y,
                        yEnd,
                        *app->_orbit,
                        &app->_abortRequested,
                        &app->_rebaseRequired,
                        stepSize,
                        true
                    );

                    if (app->_rebaseRequired && !app->_abortRequested) {
                        app->_rebaseRequired = false;
                        FractalEngine::buildReferenceOrbit(*app->_orbit, (double)app->_centerX, (double)app->_centerY, app->_maxIter);
                        y = -kStripHeight;
                        app->_completedStrips = 0;
                        continue;
                    }
                } else {
                    FractalEngine::renderFractalStrip(
                        app->_buffer.data(),
                        FractalApp::SCREEN_W,
                        FractalApp::CANVAS_H,
                        app->_activeType,
                        app->_centerX,
                        app->_centerY,
                        app->_zoom,
                        app->_maxIter,
                        y,
                        yEnd,
                        &app->_abortRequested,
                        stepSize,
                        true,
                        app->_juliaCRe,
                        app->_juliaCIm,
                        app->_sliceZ,
                        app->_mandelbulbPower
                    );
                }

                app->_completedStrips++;
                vTaskDelay(1);
            }

            if (!app->_abortRequested) {
                app->_renderPass++;
                app->_renderComplete = true;
            }
        }

        app->_isIdle = true;
    }

    app->_taskExited.store(true, std::memory_order_release);
    FLOGN("[Fractal] renderTask exited");
    vTaskDelete(nullptr);
#endif
}

