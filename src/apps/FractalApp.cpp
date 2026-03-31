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
      _maxIter(64)        // low iter for fast initial render
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

#ifdef ARDUINO
    xTaskCreatePinnedToCore(
        renderTaskWrapper,     // Task function
        "fractalTask",         // Name
        8192,                  // Stack size
        this,                  // Parameters
        1,                     // Priority
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
    // Only care about pressing down or repeat
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) {
        return;
    }

    const float panStep = 0.2f / _zoom;

    switch (ev.code) {
        case KeyCode::UP:
            _centerY += panStep;
            renderFractal();
            break;
        case KeyCode::DOWN:
            _centerY -= panStep;
            renderFractal();
            break;
        case KeyCode::LEFT:
            _centerX -= panStep;
            renderFractal();
            break;
        case KeyCode::RIGHT:
            _centerX += panStep;
            renderFractal();
            break;
        case KeyCode::ADD:
            _zoom *= 1.5f;
            renderFractal();
            break;
        case KeyCode::SUB:
            _zoom /= 1.5f;
            renderFractal();
            break;
        case KeyCode::MUL:
            _maxIter += 32;
            renderFractal();
            break;
        case KeyCode::DIV:
            if (_maxIter > 32) _maxIter -= 32;
            renderFractal();
            break;
        default:
            break;
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
    // Signal task to start
    _renderRequested = true;
    if (_renderTaskHandle) {
        xTaskNotifyGive(_renderTaskHandle);
    }
#else
    // Fallback for PC emulator
    FractalEngine::renderMandelbrot(
        _buffer.data(),
        SCREEN_W, CANVAS_H,
        _centerX, _centerY,
        _zoom, _maxIter,
        true 
    );

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

            FractalEngine::renderMandelbrot(
                app->_buffer.data(),
                FractalApp::SCREEN_W, FractalApp::CANVAS_H,
                app->_centerX, app->_centerY,
                app->_zoom, app->_maxIter,
                true
            );

            app->_renderComplete = true; // Signal completion to LVGL timer
        }
    }
#endif
}

void FractalApp::checkStatusTimer(lv_timer_t* timer) {
    FractalApp* app = static_cast<FractalApp*>(lv_timer_get_user_data(timer));
    if (app && app->_renderComplete) {
        app->_renderComplete = false;
        
        lv_image_set_src(app->_fractalImage, &app->_imgDsc);
        lv_obj_invalidate(app->_fractalImage);
    }
}
