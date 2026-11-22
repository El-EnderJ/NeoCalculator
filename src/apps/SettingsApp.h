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

/**
 * SettingsApp.h — Settings configuration panel for NumOS.
 *
 * LVGL-native app with clean NumWorks-inspired UI:
 *   - Angle mode toggle (Radians/Degrees) — writes the runtime source of
 *     truth (AngleModeRuntime.h)
 *   - Complex roots toggle (ON/OFF)
 *   - Decimal precision selector (6/8/10/12)
 *   - Step-by-step educational mode toggle (ON/OFF)
 *
 * Part of: NumOS — System Settings
 */

#pragma once

#include <lvgl.h>
#include "../Config.h"
#include "BrightnessSettingPolicy.h"
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"

class DisplayDriver;

class SettingsApp {
public:
    explicit SettingsApp(DisplayDriver* display = nullptr);
    ~SettingsApp();

    void begin();
    void end();
    /** Commit the final visible brightness before a screen change. */
    void prepareToLeave();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }
    bool navigateBack() { return false; }

#if defined(NATIVE_SIM) || defined(__EMSCRIPTEN__) || NUMOS_BOARD_PROD_WROOM1U_N16R8
    /** Persist the compact settings record (LittleFS on hardware/IDBFS on web). */
    static bool loadPersistentState();
    static bool savePersistentState();
#endif

private:
    static constexpr int NUM_ITEMS =
        NUMOS_BOARD_PROD_WROOM1U_N16R8 ? 6 : 5;
    static constexpr int LANGUAGE_ITEM = NUM_ITEMS - 1;
    static constexpr int SCREEN_W  = 320;
    static constexpr int SCREEN_H  = 240;
    static constexpr int PAD       = 12;
    static constexpr int ROW_H     =
        NUMOS_BOARD_PROD_WROOM1U_N16R8 ? 24 : 30;
    static constexpr int ROW_GAP   = 2;
    static constexpr int ABOUT_H   = 30;   ///< static two-line build identity row

    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // UI elements
    lv_obj_t*       _container;
    lv_obj_t*       _rows[NUM_ITEMS];
    lv_obj_t*       _labels[NUM_ITEMS];
    lv_obj_t*       _values[NUM_ITEMS];
    lv_obj_t*       _hintLabel;
    lv_obj_t*       _brightnessSlider;
    lv_obj_t*       _aboutLabel = nullptr;  ///< "About" / "Acerca de" (not focusable)

    int             _focus;
    DisplayDriver*  _display;
    numos::settings::BrightnessSettingSession _brightnessSession;

    void createUI();
    void updateFocus();
    void updateValues();
    void toggleCurrent();
    void adjustBrightness(int delta);
    void adjustLanguage(bool forward);
    void updateHint();
};
