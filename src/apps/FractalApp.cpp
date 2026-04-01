#include "FractalApp.h"
#include "../input/KeyboardManager.h"
#include <cstring>
#include <new>

FractalApp::FractalApp() 
    : _screen(nullptr),
      _canvasArea(nullptr),
      _fractalImage(nullptr),
      _centerX(-0.5f),    // standard offset for full mandelbrot
      _centerY(0.0f),
      _zoom(1.0f),        // standard initial zoom
      _sliceZ(0.0f),
      _maxIter(64),       // low iter for fast initial render
      _mandelbulbPower(8),
      _mode(RenderMode::Mandelbrot)
{
    // Make sure descriptor is clean
    memset(&_imgDsc, 0, sizeof(lv_image_dsc_t));
}

FractalApp::~FractalApp() {
    end();
}

void FractalApp::begin() {
    if (_screen) return; // Already begun

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(0x000000), 0);

    createUI();
    initializeBuffer();

    _orbit = new (std::nothrow) FractalEngine::ReferenceOrbit();
    if (!_orbit) {
        // Handle failure if needed, although on ESP32-S3 with 16MB PSRAM this should rarely happen
    }

    _renderRequested = false;
    _renderComplete  = false;
    _abortRequested  = false;
    _isIdle          = true;
    _completedStrips = 0;
    _totalStrips     = 0;
    _rebaseRequired  = false;

#ifdef ARDUINO
    xTaskCreatePinnedToCore(
        renderTaskWrapper,     // Task function
        "fractalTask",         // Name
        32768,                 // Increased stack size to 32KB (ReferenceOrbit is 32KB!)
        this,                  // Parameters
        3,                     // Priority
        &_renderTaskHandle,    // Task handle
        1                      // Core 1
    );
#endif

    // Timer to poll _renderComplete at ~30 FPS (33 ms)
    _updateTimer = lv_timer_create(checkStatusTimer, 33, this);
}

void FractalApp::end() {
    if (_updateTimer) {
        lv_timer_delete(_updateTimer);
        _updateTimer = nullptr;
    }

#ifdef ARDUINO
    if (_renderTaskHandle) {
        // Signal task to exit and abort any running process
        _abortRequested = true;
        _taskShouldExit = true;

        // Wake task if it's currently waiting for a notification
        xTaskNotifyGive(_renderTaskHandle);

        // Wait cleanly for task to terminate itself
        while (!_taskExited.load(std::memory_order_acquire)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        _renderTaskHandle = nullptr; // Task deleted itself or exited successfully
    }
#endif

    if (_screen) {
        lv_obj_delete(_screen);
        _screen = nullptr;
    }
    _statusBar.resetPointers();
    _buffer.reset(); // Free PSRAM

    if (_orbit) {
        delete _orbit;
        _orbit = nullptr;
    }
}

void FractalApp::load() {
    if (!_screen) {
        begin();
    }
    
    // Load screen into active display
    lv_screen_load(_screen);
    _statusBar.update();

    // Do an initial render
    renderFractal();
}

void FractalApp::handleKey(const KeyEvent& ev) {
    handleInput(ev);
}

void FractalApp::handleInput(const KeyEvent& ev) {
    // Only care about pressing down or repeat
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) {
        return;
    }

    bool changed = false;
    const float panStep = 0.2f / _zoom;

    switch (ev.code) {
        case KeyCode::UP:
            if (_mode == RenderMode::MandelbulbSlice) { _sliceZ += 0.05f / _zoom; changed = true; }
            else { _centerY += panStep; shiftBuffer(0, 15); }
            break;
        case KeyCode::DOWN:
            if (_mode == RenderMode::MandelbulbSlice) { _sliceZ -= 0.05f / _zoom; changed = true; }
            else { _centerY -= panStep; shiftBuffer(0, -15); }
            break;
        case KeyCode::LEFT:
            _centerX -= panStep;
            if (_mode == RenderMode::Mandelbrot) shiftBuffer(15, 0);
            else changed = true;
            break;
        case KeyCode::RIGHT:
            _centerX += panStep;
            if (_mode == RenderMode::Mandelbrot) shiftBuffer(-15, 0);
            else changed = true;
            break;
        case KeyCode::ADD:
            _zoom *= 1.5f;
            if (_mode == RenderMode::Mandelbrot) scaleBuffer(1.5f);
            else changed = true;
            break;
        case KeyCode::SUB:
            _zoom /= 1.5f;
            if (_mode == RenderMode::Mandelbrot) scaleBuffer(1.0f / 1.5f);
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
        case KeyCode::MODE:
            _mode = (_mode == RenderMode::Mandelbrot) ? RenderMode::MandelbulbSlice : RenderMode::Mandelbrot;
            _rebaseRequired = false;
            changed = true;
            break;
        default:
            break;
    }

    if (changed) {
        renderFractal();
    }
}

void FractalApp::createUI() {
    // Status Bar
    _statusBar.create(_screen);
    _statusBar.setTitle("Fractals");

    // Canvas container
    _canvasArea = lv_obj_create(_screen);
    lv_obj_set_size(_canvasArea, SCREEN_W, CANVAS_H);
    lv_obj_align(_canvasArea, LV_ALIGN_TOP_LEFT, 0, BAR_H);
    
    lv_obj_set_scrollbar_mode(_canvasArea, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(_canvasArea, LV_OBJ_FLAG_SCROLLABLE);

    // Padding & border removal
    lv_obj_set_style_pad_all(_canvasArea, 0, 0);
    lv_obj_set_style_border_width(_canvasArea, 0, 0);
    lv_obj_set_style_bg_color(_canvasArea, lv_color_hex(0x000000), 0);

    // Image Object
    _fractalImage = lv_image_create(_canvasArea);
    lv_obj_align(_fractalImage, LV_ALIGN_TOP_LEFT, 0, 0);

    // Loading Label ("Rendering...")
    _loadingLabel = lv_label_create(_screen);
    lv_label_set_text(_loadingLabel, "Rendering...");
    lv_obj_set_style_text_color(_loadingLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(_loadingLabel, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    lv_obj_add_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN); // Hidden by default
}

void FractalApp::initializeBuffer() {
    // Allocate 320 x 215 buffer in PSRAM
    if (!_buffer.allocate(SCREEN_W * CANVAS_H)) {
        // Fallback or error
        return;
    }

    _imgDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    _imgDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    _imgDsc.header.flags = 0;
    _imgDsc.header.w = SCREEN_W;
    _imgDsc.header.h = CANVAS_H;
    _imgDsc.header.stride = SCREEN_W * 2; 
    _imgDsc.data_size = SCREEN_W * CANVAS_H * 2;
    _imgDsc.data = (const uint8_t*)_buffer.data();
}

void FractalApp::renderFractal() {
    if (!_buffer.data()) return;

    _abortRequested = true;
    while (!_isIdle) {
#ifdef ARDUINO
        vTaskDelay(1);
#else
        break;
#endif
    }
    
    lv_obj_remove_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN);

    _abortRequested = false;

#ifdef ARDUINO
    if (!_renderTaskHandle) return;

    _renderComplete = false;
    _completedStrips = 0;

    constexpr int STRIP_H = 16;
    _totalStrips = (CANVAS_H + STRIP_H - 1) / STRIP_H;
    _renderRequested = true;
    _isIdle = false;
    xTaskNotifyGive(_renderTaskHandle);
#else
    if (_mode == RenderMode::MandelbulbSlice) {
        FractalEngine::renderMandelbulbSliceStrip(
            _buffer.data(),
            SCREEN_W, CANVAS_H,
            _centerX, _centerY, _sliceZ,
            _zoom, _maxIter, _mandelbulbPower,
            0, CANVAS_H,
            nullptr,
            true
        );
    } else {
        FractalEngine::renderMandelbrot(
            _buffer.data(),
            SCREEN_W, CANVAS_H,
            _centerX, _centerY,
            _zoom, _maxIter,
            0, 0, SCREEN_W, CANVAS_H, nullptr, true
        );
    }

    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);
    lv_obj_add_flag(_loadingLabel, LV_OBJ_FLAG_HIDDEN);
#endif
}

void FractalApp::shiftBuffer(int dx, int dy) {
    uint16_t* p = _buffer.data();
    if (!p) return;

    _abortRequested = true;
    while (!_isIdle) {
#ifdef ARDUINO
        vTaskDelay(1);
#else
        break;
#endif
    }

    if (dy != 0) {
        int rowBytes = SCREEN_W * sizeof(uint16_t);
        if (dy > 0) memmove(p + dy * SCREEN_W, p, (CANVAS_H - dy) * rowBytes);
        else memmove(p, p - dy * SCREEN_W, (CANVAS_H + dy) * rowBytes);
    }
    
    if (dx != 0) {
        for (int y = 0; y < CANVAS_H; y++) {
            if (dx > 0) memmove(p + y * SCREEN_W + dx, p + y * SCREEN_W, (SCREEN_W - dx) * sizeof(uint16_t));
            else memmove(p + y * SCREEN_W, p + y * SCREEN_W - dx, (SCREEN_W + dx) * sizeof(uint16_t));
        }
    }

    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);

    // After visually shifting, request a full background render for deep zoom precision
    renderFractal();
}

void FractalApp::scaleBuffer(float scaleFactor) {
    uint16_t* p = _buffer.data();
    if (!p) return;

    _abortRequested = true;
    while (!_isIdle) {
#ifdef ARDUINO
        vTaskDelay(1);
#else
        break;
#endif
    }

    // Allocate scratch PSRAM buffer
    utils::PSRAMBuffer<uint16_t> scratch;
    if (!scratch.allocate(SCREEN_W * CANVAS_H)) {
        renderFractal();
        return; 
    }

    uint16_t* s = scratch.data();
    
    float invScale = 1.0f / scaleFactor;
    float cx = SCREEN_W / 2.0f;
    float cy = CANVAS_H / 2.0f;

    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            int srcX = (int)(cx + (x - cx) * invScale);
            int srcY = (int)(cy + (y - cy) * invScale);

            if (srcX >= 0 && srcX < SCREEN_W && srcY >= 0 && srcY < CANVAS_H) {
                s[y * SCREEN_W + x] = p[srcY * SCREEN_W + srcX];
            } else {
                s[y * SCREEN_W + x] = 0x0000;
            }
        }
    }

    // Copy back
    memcpy(p, s, SCREEN_W * CANVAS_H * sizeof(uint16_t));

    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);
    
    renderFractal();
}

void FractalApp::renderTaskWrapper(void* param) {
#ifdef ARDUINO
    FractalApp* app = static_cast<FractalApp*>(param);
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Immediate exit check
        if (app->_taskShouldExit.load(std::memory_order_relaxed)) {
            break;
        }

        if (app->_renderRequested && app->_buffer.data()) {
            app->_renderRequested = false;
            app->_isIdle = false;

            if (!app->_orbit) return; // safety check

            constexpr int STRIP_H = 16;
            if (app->_mode == RenderMode::Mandelbrot) {
                FractalEngine::buildReferenceOrbit(*app->_orbit, (double)app->_centerX, (double)app->_centerY, app->_maxIter);
            }

            for (int y = 0; y < FractalApp::CANVAS_H; y += STRIP_H) {
                if (app->_abortRequested) break;
                int yEnd = y + STRIP_H;
                if (yEnd > FractalApp::CANVAS_H) yEnd = FractalApp::CANVAS_H;

                if (app->_mode == RenderMode::MandelbulbSlice) {
                    FractalEngine::renderMandelbulbSliceStrip(
                        app->_buffer.data(),
                        FractalApp::SCREEN_W, FractalApp::CANVAS_H,
                        app->_centerX, app->_centerY, app->_sliceZ,
                        app->_zoom, app->_maxIter, app->_mandelbulbPower,
                        y, yEnd,
                        &app->_abortRequested,
                        true
                    );
                } else {
                    FractalEngine::renderMandelbrotPerturbationStrip(
                        app->_buffer.data(),
                        FractalApp::SCREEN_W, FractalApp::CANVAS_H,
                        app->_centerX, app->_centerY,
                        app->_zoom, app->_maxIter,
                        y, yEnd,
                        *app->_orbit,
                        &app->_abortRequested,
                        &app->_rebaseRequired,
                        true
                    );
                    if (app->_rebaseRequired) {
                        app->_rebaseRequired = false;
                        FractalEngine::buildReferenceOrbit(
                            *app->_orbit,
                            (double)app->_centerX,
                            (double)app->_centerY,
                            app->_maxIter
                        );
                        y = -STRIP_H;
                        app->_completedStrips = 0;
                        continue;
                    }
                }
                app->_completedStrips++;
            }

            app->_isIdle = true;
            if (!app->_abortRequested) {
                app->_renderComplete = true; // Signal completion to LVGL timer
            }
        }
    }
#endif
}

void FractalApp::checkStatusTimer(lv_timer_t* timer) {
    FractalApp* app = static_cast<FractalApp*>(lv_timer_get_user_data(timer));
    if (!app) return;

    bool shouldRefresh = false;
    if (app->_completedStrips > 0) {
        shouldRefresh = true;
        app->_completedStrips = 0;
    }

    if (app->_renderComplete) {
        app->_renderComplete = false;
        shouldRefresh = true;
    }

    if (shouldRefresh) {
        lv_image_set_src(app->_fractalImage, &app->_imgDsc);
        lv_obj_invalidate(app->_fractalImage);
        lv_obj_add_flag(app->_loadingLabel, LV_OBJ_FLAG_HIDDEN);
    }
}
