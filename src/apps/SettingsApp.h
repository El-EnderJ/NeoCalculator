/**
 * SettingsApp.h — Settings configuration panel for NumOS.
 *
 * LVGL-native app with clean NumWorks-inspired UI:
 *   - Complex roots toggle (ON/OFF)
 *   - Decimal precision selector (6/8/10/12)
 *   - Angle mode display (informational)
 *
 * Part of: NumOS — System Settings
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

class SettingsApp {
public:
    SettingsApp();
    ~SettingsApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    static constexpr int NUM_ITEMS = 2;
    static constexpr int SCREEN_W  = 320;
    static constexpr int SCREEN_H  = 240;
    static constexpr int PAD       = 12;
    static constexpr int ROW_H     = 44;
    static constexpr int ROW_GAP   = 4;

    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // UI elements
    lv_obj_t*       _container;
    lv_obj_t*       _rows[NUM_ITEMS];
    lv_obj_t*       _labels[NUM_ITEMS];
    lv_obj_t*       _values[NUM_ITEMS];
    lv_obj_t*       _hintLabel;

    int             _focus;

    void createUI();
    void updateFocus();
    void updateValues();
    void toggleCurrent();
};
