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
#include "../display/DisplayDriver.h"
#include "../math/AngleModeRuntime.h"
#include "../i18n/Locale.h"
#include "../ui/TutorFonts.h"

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
#if NUMOS_PRODUCTION_DEMO_PROFILE
#include "../demo/DemoBootHealth.h"
#endif
#include "../demo/DemoSettingsRecord.h"
#include <FS.h>
#include <LittleFS.h>
#include <cstdint>
#include <cstring>
#elif defined(NATIVE_SIM) || defined(__EMSCRIPTEN__)
#include "../hal/FileSystem.h"
#include "CompactSettingsRecord.h"
#include <cstdint>
#include <cstring>
#endif
#include "../BuildInfo.h"

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

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
namespace {
constexpr const char* SETTINGS_PATH = "/settings.dat";
constexpr const char* SETTINGS_TEMP_PATH = "/settings.tmp";
uint8_t g_persistedBrightness =
    numos::display::kSafeDisplayProfile.initialBacklight;
}

bool SettingsApp::savePersistentState() {
#if NUMOS_PRODUCTION_DEMO_PROFILE
    if (numos::demo::safeModeActive()) return false;
#endif
    const auto record = numos::demo::encodeSettingsRecord(
        numos::angleModeIsDeg(), setting_complex_enabled,
        setting_edu_steps, static_cast<uint8_t>(setting_decimal_precision),
        g_persistedBrightness, numos::i18n::localeStorageValue());

    LittleFS.remove(SETTINGS_TEMP_PATH);
    fs::File file = LittleFS.open(SETTINGS_TEMP_PATH, "w");
    if (!file) return false;
    const bool complete =
        file.write(record.data(), record.size()) == record.size();
    file.close();
    if (!complete) {
        LittleFS.remove(SETTINGS_TEMP_PATH);
        return false;
    }
    LittleFS.remove(SETTINGS_PATH);
    const bool saved = LittleFS.rename(SETTINGS_TEMP_PATH, SETTINGS_PATH);
    if (saved) {
        Serial.printf("[SETTINGS] persist brightness=%u\n",
                      static_cast<unsigned>(g_persistedBrightness));
    }
    return saved;
}

bool SettingsApp::loadPersistentState() {
    fs::File file = LittleFS.open(SETTINGS_PATH, "r");
    if (!file) return false;  // normal first run

    std::array<uint8_t, numos::demo::kSettingsRecordSize> record{};
    if (file.size() != record.size()) {
        file.close();
        return false;
    }
    const bool complete =
        file.read(record.data(), record.size()) == record.size();
    file.close();
    if (!complete) return false;

    numos::demo::DecodedSettings decoded{};
    if (!numos::demo::decodeSettingsRecord(
            record.data(), record.size(), decoded)) return false;

    numos::i18n::productLocale = numos::i18n::storedLocale(decoded.tutorLanguage);
    if (decoded.angleValid) {
        numos::setAngleMode(decoded.angleDeg ? vpam::AngleMode::DEG
                                             : vpam::AngleMode::RAD);
    }
    if (decoded.complexValid)
        setting_complex_enabled = decoded.complexEnabled;
    if (decoded.educationValid)
        setting_edu_steps = decoded.educationEnabled;
    if (decoded.precisionValid)
        setting_decimal_precision = decoded.precision;
    if (decoded.brightnessValid) {
        setting_brightness = decoded.brightness;
        g_persistedBrightness = decoded.brightness;
    }
    if (decoded.brightnessMigrated) {
        Serial.printf("[SETTINGS] brightness migration raw=%u visible=%u\n",
                      static_cast<unsigned>(record[9]),
                      static_cast<unsigned>(g_persistedBrightness));
        (void)savePersistentState();
    }
    return true;
}
#elif defined(NATIVE_SIM) || defined(__EMSCRIPTEN__)
namespace {
constexpr const char* SETTINGS_PATH = "/settings.dat";
constexpr const char* SETTINGS_TEMP_PATH = "/settings.tmp";
}

bool SettingsApp::savePersistentState() {
    const auto record = numos::settings::encodeCompactSettings({
        numos::angleModeIsDeg(), setting_complex_enabled, setting_edu_steps,
        static_cast<uint8_t>(setting_decimal_precision), numos::i18n::productLocale});
    File file = LittleFS.open(SETTINGS_TEMP_PATH, "w");
    if (!file) return false;
    const bool complete = file.write(record.data(), record.size()) == record.size();
    file.close();
    if (!complete) {
        LittleFS.remove(SETTINGS_TEMP_PATH);
        return false;
    }
    return LittleFS.rename(SETTINGS_TEMP_PATH, SETTINGS_PATH);
}

bool SettingsApp::loadPersistentState() {
    File file = LittleFS.open(SETTINGS_PATH, "r");
    if (!file) return false;
    std::array<uint8_t, numos::settings::kCompactSettingsSize> record{};
    if (file.size() != record.size()) return false;
    const bool complete = file.read(record.data(), record.size()) == record.size();
    file.close();
    numos::settings::CompactSettings decoded;
    if (!complete || !numos::settings::decodeCompactSettings(record.data(), record.size(), decoded)) return false;
    numos::setAngleMode(decoded.angleDeg ? vpam::AngleMode::DEG : vpam::AngleMode::RAD);
    setting_complex_enabled = decoded.complexEnabled;
    setting_edu_steps = decoded.educationEnabled;
    setting_decimal_precision = decoded.precision;
    numos::i18n::productLocale = decoded.locale;
    return true;
}
#endif

// ════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ════════════════════════════════════════════════════════════════════════════

SettingsApp::SettingsApp(DisplayDriver* display)
    : _screen(nullptr)
    , _container(nullptr)
    , _hintLabel(nullptr)
    , _brightnessSlider(nullptr)
    , _focus(0)
    , _display(display)
    , _brightnessSession()
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
    prepareToLeave();
    if (_screen) {
        _statusBar.destroy();   // nullify dangling pointers before parent screen is freed
        lv_obj_delete(_screen);
        _screen    = nullptr;
        _container = nullptr;
        _hintLabel = nullptr;
        _brightnessSlider = nullptr;
        _aboutLabel = nullptr;
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
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    _brightnessSession.begin(setting_brightness);
    setting_brightness = _brightnessSession.runtimeBrightness();
    if (_display) _display->setBacklightLevel(setting_brightness);
#endif
    _statusBar.setTitle("Settings");
    _statusBar.update();
    _focus = 0;
    updateValues();
    updateFocus();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void SettingsApp::prepareToLeave() {
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    if (!_brightnessSession.active()) return;

    const uint8_t before = setting_brightness;
    const auto decision = _brightnessSession.prepareToLeave();
    setting_brightness = decision.runtimeBrightness;
    if (_display && before != setting_brightness) {
        _display->setBacklightLevel(setting_brightness);
    }

    bool saved = false;
    if (decision.persist) {
        g_persistedBrightness =
            numos::settings::normalizePersistedBrightness(
                setting_brightness);
        saved = savePersistentState();
    }
    Serial.printf(
        "[SETTINGS] brightness-exit before=%u restored=%u persist=%u saved=%u\n",
        static_cast<unsigned>(before),
        static_cast<unsigned>(setting_brightness),
        decision.persist ? 1U : 0U, saved ? 1U : 0U);
#endif
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
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
        "Brightness",
#endif
        "Language",
    };

    for (int i = 0; i < NUM_ITEMS; ++i) {
        int y = 6 + i * (ROW_H + ROW_GAP);

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
        lv_obj_set_style_text_font(_labels[i], ui::tutorFont14(), LV_PART_MAIN);
        lv_obj_set_style_text_color(_labels[i], lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_align(_labels[i], LV_ALIGN_LEFT_MID, 12, 0);

        // Value (right side)
        // Phase 7I: plain UI text → lv_font_montserrat_14 (the value "%d digits"
        // contains a space that stix_math_18 cannot render — see _labels above).
        _values[i] = lv_label_create(_rows[i]);
        lv_obj_set_style_text_font(_values[i], ui::tutorFont14(), LV_PART_MAIN);
        lv_obj_align(_values[i], LV_ALIGN_RIGHT_MID, -12, 0);
    }

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    _brightnessSlider = lv_slider_create(_rows[4]);
    lv_obj_set_size(_brightnessSlider, 112, 8);
    lv_obj_align(_brightnessSlider, LV_ALIGN_LEFT_MID, 96, 0);
    lv_slider_set_range(_brightnessSlider,
                        numos::display::kMinimumPersistedBacklight,
                        numos::display::kMaximumBacklight);
    lv_obj_remove_flag(_brightnessSlider, LV_OBJ_FLAG_CLICKABLE);
#endif

    // About (INTEGRATION-01): static build identity, not part of the focus
    // cycle. Strings are literals from BuildInfo, set with _static: no heap.
    {
        const int aboutY = 6 + NUM_ITEMS * (ROW_H + ROW_GAP);
        lv_obj_t* about = lv_obj_create(_container);
        lv_obj_set_size(about, SCREEN_W - 2 * PAD, ABOUT_H);
        lv_obj_set_pos(about, PAD, aboutY);
        lv_obj_set_style_bg_opa(about, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_color(about, lv_color_hex(COL_BORDER), LV_PART_MAIN);
        lv_obj_set_style_border_width(about, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(about, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(about, 0, LV_PART_MAIN);
        lv_obj_remove_flag(about, LV_OBJ_FLAG_SCROLLABLE);

        _aboutLabel = lv_label_create(about);
        lv_obj_set_style_text_font(_aboutLabel, ui::tutorFont12(), LV_PART_MAIN);
        lv_obj_set_style_text_color(_aboutLabel, lv_color_hex(COL_TEXT), LV_PART_MAIN);
        lv_obj_align(_aboutLabel, LV_ALIGN_LEFT_MID, 12, 0);

        lv_obj_t* id = lv_label_create(about);
        lv_label_set_text_static(id, numos::build::id());
        lv_obj_set_style_text_font(id, ui::tutorFont12(), LV_PART_MAIN);
        lv_obj_set_style_text_color(id, lv_color_hex(COL_VALUE), LV_PART_MAIN);
        lv_obj_align(id, LV_ALIGN_TOP_RIGHT, -12, 0);

        // One fixed line: "numos-esp32-s3-wroom-1u-n16r8" is wider than the
        // old 196 px, so with auto height the label wrapped to two lines and
        // its top (bottom-aligned) overlapped the id (measured on device:
        // env y=185 above id y=189). Longer envs end in "..." instead.
        lv_obj_t* env = lv_label_create(about);
        lv_label_set_text_static(env, numos::build::environment());
        lv_obj_set_size(env, SCREEN_W - 2 * PAD - 80,
                        lv_font_get_line_height(ui::tutorFont12()));
        lv_label_set_long_mode(env, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(env, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_obj_set_style_text_font(env, ui::tutorFont12(), LV_PART_MAIN);
        lv_obj_set_style_text_color(env, lv_color_hex(COL_HINT), LV_PART_MAIN);
        lv_obj_align(env, LV_ALIGN_BOTTOM_RIGHT, -12, 0);
    }

    // Hint at bottom
    // Phase 7I: plain UI hint → lv_font_montserrat_14 (stix_math_18 has no U+0020
    // space glyph → tofu at every space). The LV_SYMBOL_UP/DOWN arrows
    // (U+F077/U+F078) are absent from BOTH stix_math_18 and lv_font_montserrat_14
    // in this build, so they already rendered as tofu — dropped here (the words
    // convey navigation just as the re-blessed RegressionApp hint does).
    _hintLabel = lv_label_create(_container);
    lv_obj_set_style_text_font(_hintLabel, ui::tutorFont12(), LV_PART_MAIN);
    lv_obj_set_style_text_color(_hintLabel, lv_color_hex(COL_HINT), LV_PART_MAIN);
    lv_obj_set_pos(_hintLabel, PAD, SCREEN_H - barH - 17);

    lv_obj_set_style_text_font(_labels[LANGUAGE_ITEM],ui::tutorFont12(),0);
    lv_obj_set_style_text_font(_values[LANGUAGE_ITEM],ui::tutorFont12(),0);
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
    updateHint();
    lv_obj_invalidate(_screen);
}

// ════════════════════════════════════════════════════════════════════════════
// updateValues — Refresh displayed values from global settings
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::updateValues() {
    using namespace numos::i18n;
    const bool spanish = isSpanish(productLocale);
    _statusBar.setTitle(spanish ? "Ajustes" : "Settings");
    static const char* labelsEN[] = {"Angle mode", "Complex numbers", "Decimal precision", "Step-by-step mode"};
    static const char* labelsES[] = {"Unidad angular", "Números complejos", "Precisión decimal", "Modo paso a paso"};
    for (int i = 0; i < 4; ++i) lv_label_set_text(_labels[i], spanish ? labelsES[i] : labelsEN[i]);
    lv_label_set_text(_labels[LANGUAGE_ITEM], spanish ? "Idioma" : "Language");
    if (_aboutLabel) lv_label_set_text_static(_aboutLabel, spanish ? "Acerca de" : "About");
    lv_label_set_text(_values[LANGUAGE_ITEM], localeDisplayName(productLocale));
    lv_obj_set_style_text_color(_values[LANGUAGE_ITEM], lv_color_hex(COL_VALUE), LV_PART_MAIN);
    updateHint();
    // Angle mode (runtime source of truth — same value the StatusBar badge shows)
    lv_label_set_text(_values[0], numos::angleModeIsDeg() ? (spanish ? "Grados" : "Degrees") : (spanish ? "Radianes" : "Radians"));
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
    snprintf(buf, sizeof(buf), spanish ? "%d dígitos" : "%d digits", setting_decimal_precision);
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

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    lv_label_set_text(_labels[4], spanish ? "Brillo" : "Brightness");
    const unsigned percent =
        (static_cast<unsigned>(setting_brightness) * 100U +
         numos::display::kMaximumBacklight / 2U) /
        numos::display::kMaximumBacklight;
    char brightnessText[8];
    snprintf(brightnessText, sizeof(brightnessText), "%u%%", percent);
    lv_label_set_text(_values[4], brightnessText);
    lv_obj_set_style_text_color(_values[4], lv_color_hex(COL_VALUE), LV_PART_MAIN);
    lv_slider_set_value(_brightnessSlider, setting_brightness, LV_ANIM_OFF);
#endif
}

void SettingsApp::updateHint() {
    using namespace numos::i18n;
    if (!_hintLabel) return;
    if (_focus == LANGUAGE_ITEM) {
        char hint[64];
        snprintf(hint, sizeof(hint), isSpanish(productLocale)
            ? "</> Idioma %u/4   EXE Cambia   BACK Vuelve"
            : "</> Language %u/4   EXE Change   BACK Exit",
            unsigned(productLocaleIndex(productLocale)) + 1);
        lv_label_set_text(_hintLabel, hint);
    } else {
        lv_label_set_text(_hintLabel, isSpanish(productLocale)
            ? "^v Elige  </> Cambia  BACK Vuelve"
            : "^v Select  </> Adjust  BACK Exit");
    }
}

void SettingsApp::adjustLanguage(bool forward) {
    numos::i18n::productLocale = numos::i18n::adjacentLocale(numos::i18n::productLocale, forward);
    updateValues();
#if defined(NATIVE_SIM) || defined(__EMSCRIPTEN__) || NUMOS_BOARD_PROD_WROOM1U_N16R8
    savePersistentState();
#endif
}

void SettingsApp::adjustBrightness(const int delta) {
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
    const int maximum = numos::display::kMaximumBacklight;
    int next = static_cast<int>(setting_brightness) + delta;
    if (next < numos::display::kMinimumPersistedBacklight) {
        next = numos::display::kMinimumPersistedBacklight;
    }
    if (next > maximum) next = maximum;
    if (next == setting_brightness) return;

    setting_brightness = _brightnessSession.setRuntime(next);
    if (_display) _display->setBacklightLevel(setting_brightness);
    updateValues();
#else
    (void)delta;
#endif
}

// ════════════════════════════════════════════════════════════════════════════
// toggleCurrent — Change the current setting's value
// ════════════════════════════════════════════════════════════════════════════

void SettingsApp::toggleCurrent() {
    if(_focus==LANGUAGE_ITEM) {
        adjustLanguage(true);
        return;
    }
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

#if NUMOS_BOARD_PROD_WROOM1U_N16R8
        case 4:  // Minimum -> normal -> maximum -> minimum.
            if (setting_brightness ==
                numos::display::kMinimumPersistedBacklight) {
                adjustBrightness(
                    numos::display::kSafeDisplayProfile.initialBacklight);
            } else if (setting_brightness < numos::display::kMaximumBacklight) {
                adjustBrightness(numos::display::kMaximumBacklight -
                                 setting_brightness);
            } else {
                adjustBrightness(-numos::display::kMaximumBacklight);
            }
            break;
#endif
    }

    updateValues();
#if defined(NATIVE_SIM) || defined(__EMSCRIPTEN__)
    // One compact record per completed user action. FileSystem marks the
    // successful close dirty; JavaScript coalesces repeated actions.
    savePersistentState();
#elif NUMOS_BOARD_PROD_WROOM1U_N16R8
    // Brightness writes are deferred until prepareToLeave(). Other settings
    // retain immediate persistence, using the last committed brightness.
    if (_focus != 4) savePersistentState();
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
            if(_focus==LANGUAGE_ITEM && ev.action==KeyAction::REPEAT)break;
            toggleCurrent();
            break;

        case KeyCode::LEFT:
            if(_focus==LANGUAGE_ITEM){if(ev.action==KeyAction::PRESS)adjustLanguage(false);break;}
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
#if defined(NATIVE_SIM) || defined(__EMSCRIPTEN__) || NUMOS_BOARD_PROD_WROOM1U_N16R8
                savePersistentState();
#endif
            }
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
            else if (_focus == 4) {
                adjustBrightness(-8);
            }
#endif
            break;

        case KeyCode::RIGHT:
            if(_focus==LANGUAGE_ITEM){if(ev.action==KeyAction::PRESS)adjustLanguage(true);break;}
            // For precision (row 2): cycle forward
            if (_focus == 2) {
                toggleCurrent();
            }
#if NUMOS_BOARD_PROD_WROOM1U_N16R8
            else if (_focus == 4) {
                adjustBrightness(8);
            }
#endif
            break;

        default:
            break;
    }
}
