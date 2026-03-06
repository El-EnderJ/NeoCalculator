/**
 * SettingsApp.cpp — Settings configuration panel for NumOS.
 *
 * Clean NumWorks-inspired settings UI with toggle rows.
 *
 * Part of: NumOS — System Settings
 */

#include "SettingsApp.h"
#include "../Config.h"

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
        "Complex numbers",
        "Decimal precision",
    };

    for (int i = 0; i < NUM_ITEMS; ++i) {
        int y = 16 + i * (ROW_H + ROW_GAP);

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
        lv_obj_set_style_text_font(_labels[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(_labels[i], lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_align(_labels[i], LV_ALIGN_LEFT_MID, 12, 0);

        // Value (right side)
        _values[i] = lv_label_create(_rows[i]);
        lv_obj_set_style_text_font(_values[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(_values[i], LV_ALIGN_RIGHT_MID, -12, 0);
    }

    // Hint at bottom
    _hintLabel = lv_label_create(_container);
    lv_label_set_text(_hintLabel,
        LV_SYMBOL_UP LV_SYMBOL_DOWN " Navigate   ENTER Toggle   MODE Back");
    lv_obj_set_style_text_font(_hintLabel, &lv_font_montserrat_12, LV_PART_MAIN);
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
    // Complex toggle
    if (setting_complex_enabled) {
        lv_label_set_text(_values[0], "ON");
        lv_obj_set_style_text_color(_values[0], lv_color_hex(COL_VALUE_ON), LV_PART_MAIN);
    } else {
        lv_label_set_text(_values[0], "OFF");
        lv_obj_set_style_text_color(_values[0], lv_color_hex(COL_VALUE_OFF), LV_PART_MAIN);
    }

    // Decimal precision
    char buf[16];
    snprintf(buf, sizeof(buf), "%d digits", setting_decimal_precision);
    lv_label_set_text(_values[1], buf);
    lv_obj_set_style_text_color(_values[1], lv_color_hex(COL_VALUE), LV_PART_MAIN);
}

// ════════════════════════════════════════════════════════════════════════════
// toggleCurrent — Change the current setting's value
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::toggleCurrent() {
    switch (_focus) {
        case 0:  // Complex numbers toggle
            setting_complex_enabled = !setting_complex_enabled;
            break;

        case 1: {  // Decimal precision cycle: 6 → 8 → 10 → 12 → 6
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
    }

    updateValues();
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
            // For precision: cycle backward
            if (_focus == 1) {
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
            }
            break;

        case KeyCode::RIGHT:
            // For precision: cycle forward
            if (_focus == 1) {
                toggleCurrent();
            }
            break;

        default:
            break;
    }
}
