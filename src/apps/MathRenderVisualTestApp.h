#pragma once

#include <cstdint>

#include <lvgl.h>

#include "../input/KeyCodes.h"
#include "../math/CursorController.h"
#include "../math/MathAST.h"
#include "../ui/MathRenderer.h"
#include "../ui/StatusBar.h"

class MathRenderVisualTestApp {
public:
    MathRenderVisualTestApp();
    ~MathRenderVisualTestApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    static constexpr int16_t SCREEN_W = 320;
    static constexpr int16_t SCREEN_H = 240;
    static constexpr int16_t PAD = 8;
    static constexpr int16_t CANVAS_W = SCREEN_W - 2 * PAD;
    static constexpr int16_t CANVAS_Y = 54;
    static constexpr int16_t CANVAS_H = SCREEN_H - CANVAS_Y - PAD;
    static constexpr int16_t REPORT_X_PAD = 16;
    static constexpr int16_t SCROLL_STEP = 40;

    enum class CursorMode : uint8_t {
        Off,
        Start,
        End
    };

    lv_obj_t* _screen;
    ui::StatusBar _statusBar;
    vpam::MathCanvas _canvas;
    lv_obj_t* _indexLabel;
    lv_obj_t* _caseLabel;
    lv_obj_t* _metricsLabel;
    lv_obj_t* _hintLabel;

    vpam::NodePtr _root;
    vpam::NodeRow* _rootRow;
    vpam::CursorController _cursor;
    int _caseIndex;
    CursorMode _cursorMode;
    int16_t _scrollOffset;

    void createUI();
    void showCase(int index);
    void updateLabels();
    void printSerialReport() const;
    void applyCursorMode();
    void setCursorAtStart();
    void setCursorAtEnd();
    int caseCount() const;
};
