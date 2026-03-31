#include "FractalApp.h"
#include "../input/KeyboardManager.h"
#include <cstring>

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
        8192,                  // Stack size
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
        vTaskDelete(_renderTaskHandle);
        _renderTaskHandle = nullptr;
    }
#endif

    if (_screen) {
        lv_obj_delete(_screen);
        _screen = nullptr;
    }
    _buffer.reset(); // Free PSRAM
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
            if (_mode == RenderMode::MandelbulbSlice) _sliceZ += 0.05f / _zoom;
            else _centerY += panStep;
            changed = true;
            break;
        case KeyCode::DOWN:
            if (_mode == RenderMode::MandelbulbSlice) _sliceZ -= 0.05f / _zoom;
            else _centerY -= panStep;
            changed = true;
            break;
        case KeyCode::LEFT:
            _centerX -= panStep;
            changed = true;
            break;
        case KeyCode::RIGHT:
            _centerX += panStep;
            changed = true;
            break;
        case KeyCode::ADD:
            _zoom *= 1.5f;
            changed = true;
            break;
        case KeyCode::SUB:
            _zoom /= 1.5f;
            changed = true;
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

#ifdef ARDUINO
    if (!_renderTaskHandle) return;

    _abortRequested = true;
    while (!_isIdle) {
        vTaskDelay(1);
    }
    _abortRequested = false;
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
            true
        );
    }

    lv_image_set_src(_fractalImage, &_imgDsc);
    lv_obj_invalidate(_fractalImage);
#endif
}

void FractalApp::renderTaskWrapper(void* param) {
#ifdef ARDUINO
    FractalApp* app = static_cast<FractalApp*>(param);
    while (true) {
        // Wait for notification from renderFractal()
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (app->_renderRequested && app->_buffer.data()) {
            app->_renderRequested = false;
            app->_isIdle = false;

            constexpr int STRIP_H = 16;
            FractalEngine::ReferenceOrbit orbit;
            if (app->_mode == RenderMode::Mandelbrot) {
                FractalEngine::buildReferenceOrbit(orbit, (double)app->_centerX, (double)app->_centerY, app->_maxIter);
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
                        orbit,
                        &app->_abortRequested,
                        &app->_rebaseRequired,
                        true
                    );
                    if (app->_rebaseRequired) {
                        app->_rebaseRequired = false;
                        FractalEngine::buildReferenceOrbit(
                            orbit,
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
    }
}
