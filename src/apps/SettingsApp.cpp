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
 * SettingsApp.cpp — Settings configuration panel for NumOS.
 *
 * Clean NumWorks-inspired settings UI with toggle rows.
 *
 * Part of: NumOS — System Settings
 */

#include "SettingsApp.h"
#include "../Config.h"
#include "../math/AngleModeRuntime.h"

#ifdef __EMSCRIPTEN__
#include "../hal/FileSystem.h"
#include <cstdint>
#include <cstring>
#endif

using namespace vpam;

// ══ Color palette (matches system theme) ═════════════════════════════
static constexpr uint32_t COL_BG         = 0xFFFFFF;
static constexpr uint32_t COL_ROW_BG     = 0xF5F5F5;
static constexpr uint32_t COL_ROW_FOCUS  = 0xE3F2FD;
static constexpr uint32_t COL_BORDER     = 0xD0D0D0;
static constexpr uint32_t COL_FOCUS_BD   = 0x4A90D9;
static constexpr uint32_t COL_TEXT       = 0x1A1A1A;
static constexpr uint32_t COL_VALUE_ON   = 0x2E7D32;
static constexpr uint32_t COL_VALUE_OFF  = 0xB71C1C;
static constexpr uint32_t COL_VALUE      = 0x1565C0;
static constexpr uint32_t COL_HINT       = 0x808080;

// ══ Precision options ════════════════════════════════════════════════
static const int PRECISIONS[] = {6, 8, 10, 12};
static constexpr int NUM_PREC  = 4;

#ifdef __EMSCRIPTEN__
namespace {
constexpr const char* SETTINGS_PATH = "/settings.dat";
constexpr uint32_t SETTINGS_MAGIC = 0x53543031;  // "ST01"
constexpr uint8_t SETTINGS_FORMAT_VERSION = 1;
constexpr size_t SETTINGS_RECORD_SIZE = 10;
}

bool SettingsApp::savePersistentState() {
    uint8_t record[SETTINGS_RECORD_SIZE] = {};
    std::memcpy(record, &SETTINGS_MAGIC, sizeof(SETTINGS_MAGIC));
    record[4] = SETTINGS_FORMAT_VERSION;
    record[5] = numos::angleModeIsDeg() ? 1 : 0;
    record[6] = setting_complex_enabled ? 1 : 0;
    record[7] = setting_edu_steps ? 1 : 0;
    record[8] = static_cast<uint8_t>(setting_decimal_precision);
    record[9] = 0;

    File file = LittleFS.open(SETTINGS_PATH, "w");
    if (!file) return false;
    const bool complete = file.write(record, sizeof(record)) == sizeof(record);
    file.close();
    return complete;
}

bool SettingsApp::loadPersistentState() {
    File file = LittleFS.open(SETTINGS_PATH, "r");
    if (!file) return false;  // normal first run

    uint8_t record[SETTINGS_RECORD_SIZE] = {};
    const bool complete = file.read(record, sizeof(record)) == sizeof(record);
    file.close();
    if (!complete) return false;

    uint32_t magic = 0;
    std::memcpy(&magic, record, sizeof(magic));
    if (magic != SETTINGS_MAGIC || record[4] != SETTINGS_FORMAT_VERSION)
        return false;

    const int precision = static_cast<int>(record[8]);
    if (precision != 6 && precision != 8 &&
        precision != 10 && precision != 12) {
        return false;
    }
    numos::setAngleMode(record[5] ? vpam::AngleMode::DEG
                                  : vpam::AngleMode::RAD);
    setting_complex_enabled = record[6] != 0;
    setting_edu_steps = record[7] != 0;
    setting_decimal_precision = precision;
    return true;
}
#endif

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

SettingsApp::SettingsApp()
    : _screen(nullptr)
    , _container(nullptr)
    , _hintLabel(nullptr)
    , _focus(0)
{
    for (int i = 0; i < NUM_ITEMS; ++i) {
        _rows[i]   = nullptr;
        _labels[i] = nullptr;
        _values[i] = nullptr;
    }
}

SettingsApp::~SettingsApp() {
    end();
}

// ════════════════════════════════════════════════════════════════════════════
// begin — Create LVGL screen and widgets (called once at startup)
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::begin() {
    if (_screen) return;

    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Settings");

    createUI();
}

// ════════════════════════════════════════════════════════════════════════════
// end — Destroy the LVGL screen
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::end() {
    if (_screen) {
        _statusBar.destroy();   // nullify dangling pointers before parent screen is freed
        lv_obj_delete(_screen);
        _screen    = nullptr;
        _container = nullptr;
        _hintLabel = nullptr;
        for (int i = 0; i < NUM_ITEMS; ++i) {
            _rows[i] = nullptr;
            _labels[i] = nullptr;
            _values[i] = nullptr;
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// load — Activate the settings screen
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::load() {
    if (!_screen) begin();
    _statusBar.setTitle("Settings");
    _statusBar.update();
    _focus = 0;
    updateValues();
    updateFocus();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

// ════════════════════════════════════════════════════════════════════════════
// createUI — Build the settings rows
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::createUI() {
    int barH = ui::StatusBar::HEIGHT + 1;

    _container = lv_obj_create(_screen);
    lv_obj_set_size(_container, SCREEN_W, SCREEN_H - barH);
    lv_obj_set_pos(_container, 0, barH);
    lv_obj_set_style_bg_opa(_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_container, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_container, LV_OBJ_FLAG_SCROLLABLE);

    const char* labels[NUM_ITEMS] = {
        "Angle mode",          // row 0 per SET spec §E.3.4 (highest-priority row)
        "Complex numbers",
        "Decimal precision",
        "Step-by-step mode",
    };

    for (int i = 0; i < NUM_ITEMS; ++i) {
        int y = 10 + i * (ROW_H + ROW_GAP);   // 4 rows + bottom hint fit in 215 px

        // Row background
        _rows[i] = lv_obj_create(_container);
        lv_obj_set_size(_rows[i], SCREEN_W - 2 * PAD, ROW_H);
        lv_obj_set_pos(_rows[i], PAD, y);
        lv_obj_set_style_bg_color(_rows[i], lv_color_hex(COL_ROW_BG), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_rows[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(_rows[i], lv_color_hex(COL_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_width(_rows[i], 1, LV_PART_MAIN);
        lv_obj_set_style_radius(_rows[i], 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_rows[i], 0, LV_PART_MAIN);
        lv_obj_remove_flag(_rows[i], LV_OBJ_FLAG_SCROLLABLE);

        // Label (left side)
        _labels[i] = lv_label_create(_rows[i]);
        lv_label_set_text(_labels[i], labels[i]);
        // Phase 7I: plain UI text → lv_font_montserrat_14. stix_math_18's cmap
        // starts at U+0021, so it has no U+0020 (space) glyph; with
        // LV_USE_FONT_PLACEHOLDER the spaced names ("Complex numbers", etc.)
        // painted a tofu box at every space.
        lv_obj_set_style_text_font(_labels[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(_labels[i], lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_align(_labels[i], LV_ALIGN_LEFT_MID, 12, 0);

        // Value (right side)
        // Phase 7I: plain UI text → lv_font_montserrat_14 (the value "%d digits"
        // contains a space that stix_math_18 cannot render — see _labels above).
        _values[i] = lv_label_create(_rows[i]);
        lv_obj_set_style_text_font(_values[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(_values[i], LV_ALIGN_RIGHT_MID, -12, 0);
    }

    // Hint at bottom
    // Phase 7I: plain UI hint → lv_font_montserrat_14 (stix_math_18 has no U+0020
    // space glyph → tofu at every space). The LV_SYMBOL_UP/DOWN arrows
    // (U+F077/U+F078) are absent from BOTH stix_math_18 and lv_font_montserrat_14
    // in this build, so they already rendered as tofu — dropped here (the words
    // convey navigation just as the re-blessed RegressionApp hint does).
    _hintLabel = lv_label_create(_container);
    lv_label_set_text(_hintLabel, "Navigate   ENTER Toggle   MODE Back");
    lv_obj_set_style_text_font(_hintLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_hintLabel, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_hintLabel, PAD, SCREEN_H - barH - 22);

    updateValues();
    updateFocus();
}

// ════════════════════════════════════════════════════════════════════════════
// updateFocus — Highlight the focused row
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::updateFocus() {
    for (int i = 0; i < NUM_ITEMS; ++i) {
        if (i == _focus) {
            lv_obj_set_style_bg_color(_rows[i], lv_color_hex(COL_ROW_FOCUS), LV_PART_MAIN);
            lv_obj_set_style_border_color(_rows[i], lv_color_hex(COL_FOCUS_BD), LV_PART_MAIN);
            lv_obj_set_style_border_width(_rows[i], 2, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_color(_rows[i], lv_color_hex(COL_ROW_BG), LV_PART_MAIN);
            lv_obj_set_style_border_color(_rows[i], lv_color_hex(COL_BORDER), LV_PART_MAIN);
            lv_obj_set_style_border_width(_rows[i], 1, LV_PART_MAIN);
        }
    }
    lv_obj_invalidate(_screen);
}

// ════════════════════════════════════════════════════════════════════════════
// updateValues — Refresh displayed values from global settings
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::updateValues() {
    // Angle mode (runtime source of truth — same value the StatusBar badge shows)
    lv_label_set_text(_values[0], numos::angleModeIsDeg() ? "Degrees" : "Radians");
    lv_obj_set_style_text_color(_values[0], lv_color_hex(COL_VALUE), LV_PART_MAIN);

    // Complex toggle
    if (setting_complex_enabled) {
        lv_label_set_text(_values[1], "ON");
        lv_obj_set_style_text_color(_values[1], lv_color_hex(COL_VALUE_ON), LV_PART_MAIN);
    } else {
        lv_label_set_text(_values[1], "OFF");
        lv_obj_set_style_text_color(_values[1], lv_color_hex(COL_VALUE_OFF), LV_PART_MAIN);
    }

    // Decimal precision
    char buf[16];
    snprintf(buf, sizeof(buf), "%d digits", setting_decimal_precision);
    lv_label_set_text(_values[2], buf);
    lv_obj_set_style_text_color(_values[2], lv_color_hex(COL_VALUE), LV_PART_MAIN);

    // Step-by-step educational mode
    if (setting_edu_steps) {
        lv_label_set_text(_values[3], "ON");
        lv_obj_set_style_text_color(_values[3], lv_color_hex(COL_VALUE_ON), LV_PART_MAIN);
    } else {
        lv_label_set_text(_values[3], "OFF");
        lv_obj_set_style_text_color(_values[3], lv_color_hex(COL_VALUE_OFF), LV_PART_MAIN);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// toggleCurrent — Change the current setting's value
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::toggleCurrent() {
    switch (_focus) {
        case 0:  // Angle mode toggle: writes the runtime truth, badge follows
            numos::setAngleMode(numos::angleModeIsDeg() ? vpam::AngleMode::RAD
                                                        : vpam::AngleMode::DEG);
            _statusBar.update();   // repaint this screen's DEG/RAD badge now (§E.3.4)
            break;

        case 1:  // Complex numbers toggle
            setting_complex_enabled = !setting_complex_enabled;
            break;

        case 2: {  // Decimal precision cycle: 6 → 8 → 10 → 12 → 6
            int idx = 0;
            for (int j = 0; j < NUM_PREC; ++j) {
                if (PRECISIONS[j] == setting_decimal_precision) {
                    idx = j;
                    break;
                }
            }
            idx = (idx + 1) % NUM_PREC;
            setting_decimal_precision = PRECISIONS[idx];
            break;
        }

        case 3:  // Step-by-step educational mode toggle
            setting_edu_steps = !setting_edu_steps;
            break;
    }

    updateValues();
#ifdef __EMSCRIPTEN__
    // One compact record per completed user action. FileSystem marks the
    // successful close dirty; JavaScript coalesces repeated actions.
    savePersistentState();
#endif
}

// ════════════════════════════════════════════════════════════════════════════
// handleKey — Process key events in settings
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (ev.code) {
        case KeyCode::UP:
            if (_focus > 0) {
                --_focus;
                updateFocus();
            }
            break;

        case KeyCode::DOWN:
            if (_focus < NUM_ITEMS - 1) {
                ++_focus;
                updateFocus();
            }
            break;

        case KeyCode::ENTER:
            toggleCurrent();
            break;

        case KeyCode::LEFT:
            // For precision (row 2): cycle backward
            if (_focus == 2) {
                int idx = 0;
                for (int j = 0; j < NUM_PREC; ++j) {
                    if (PRECISIONS[j] == setting_decimal_precision) {
                        idx = j;
                        break;
                    }
                }
                idx = (idx - 1 + NUM_PREC) % NUM_PREC;
                setting_decimal_precision = PRECISIONS[idx];
                updateValues();
#ifdef __EMSCRIPTEN__
                savePersistentState();
#endif
            }
            break;

        case KeyCode::RIGHT:
            // For precision (row 2): cycle forward
            if (_focus == 2) {
                toggleCurrent();
            }
            break;

        default:
            break;
    }
}
