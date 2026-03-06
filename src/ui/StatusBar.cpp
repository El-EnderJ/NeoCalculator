/**
 * StatusBar.cpp — Barra de estado global LVGL (24 px)
 *
 * Implementación:
 *   · Reloj: <time.h> + oscilador interno.  En ESP32 se arranca con
 *     una hora arbitraria (00:00) a menos que haya NTP; el usuario
 *     puede ajustarla desde Settings.
 *   · Modificadores: lee KeyboardManager::instance().indicatorText()
 *   · Ángulo: lee vpam::g_angleMode
 *   · Batería: dibuja un mini icono proporcional al % cargado
 */

#include "StatusBar.h"
#include "../input/KeyboardManager.h"
#include "../math/MathEvaluator.h"   // g_angleMode, AngleMode

#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef ARDUINO
#include <sys/time.h>   // settimeofday (ESP-IDF)
#endif

namespace ui {

// ════════════════════════════════════════════════════════════════════════════
// create() — Construye la jerarquía de widgets LVGL
// ════════════════════════════════════════════════════════════════════════════

void StatusBar::create(lv_obj_t* parent) {
    if (_bar) return;  // ya creada

    // ── Contenedor principal ──────────────────────────────────────────
    _bar = lv_obj_create(parent);
    lv_obj_set_size(_bar, SCREEN_W, HEIGHT);
    lv_obj_set_pos(_bar, 0, 0);
    lv_obj_set_style_bg_color(_bar, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_bar, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_bar, LV_OBJ_FLAG_SCROLLABLE);

    // ── Reloj (izquierda) ─────────────────────────────────────────────
    _clockLabel = lv_label_create(_bar);
    lv_label_set_text(_clockLabel, "00:00");
    lv_obj_set_style_text_font(_clockLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_clockLabel, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_align(_clockLabel, LV_ALIGN_LEFT_MID, 6, 0);

    // ── Título de la app (centrado) ───────────────────────────────────
    _titleLabel = lv_label_create(_bar);
    lv_label_set_text(_titleLabel, "");
    lv_obj_set_style_text_font(_titleLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_titleLabel, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(_titleLabel, LV_ALIGN_CENTER, 0, 0);

    // ── Indicador de modificador (a la derecha del título) ────────────
    _modLabel = lv_label_create(_bar);
    lv_label_set_text(_modLabel, "");
    lv_obj_set_style_text_font(_modLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_modLabel, lv_color_hex(0xFFD700), LV_PART_MAIN);  // Amarillo dorado
    lv_obj_align(_modLabel, LV_ALIGN_RIGHT_MID, -80, 0);

    // ── Modo angular (antes de batería) ───────────────────────────────
    _angleLabel = lv_label_create(_bar);
    lv_label_set_text(_angleLabel, "RAD");
    lv_obj_set_style_text_font(_angleLabel, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(_angleLabel, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_align(_angleLabel, LV_ALIGN_RIGHT_MID, -38, 0);

    // ── Icono de batería (esquina derecha) — 3-bar style ────────────
    _batIcon = lv_obj_create(_bar);
    lv_obj_set_size(_batIcon, 22, 11);
    lv_obj_align(_batIcon, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(_batIcon, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_batIcon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_batIcon, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_style_border_width(_batIcon, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_batIcon, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_batIcon, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_batIcon, LV_OBJ_FLAG_SCROLLABLE);

    // Create 3 individual bars inside the battery icon
    for (int i = 0; i < 3; ++i) {
        lv_obj_t* bar = lv_obj_create(_batIcon);
        lv_obj_set_size(bar, 4, 7);
        lv_obj_set_pos(bar, 2 + i * 6, 1);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x66CC66), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
        lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    }

    // ── Línea separadora inferior (1 px) ──────────────────────────────
    _separator = lv_obj_create(parent);
    lv_obj_set_size(_separator, SCREEN_W, 1);
    lv_obj_set_pos(_separator, 0, HEIGHT);
    lv_obj_set_style_bg_color(_separator, lv_color_hex(COL_SEP), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_separator, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(_separator, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(_separator, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_separator, 0, LV_PART_MAIN);
    lv_obj_remove_flag(_separator, LV_OBJ_FLAG_SCROLLABLE);

    // ── Inicialización del reloj ──────────────────────────────────────
#ifdef ARDUINO
    // Si no se ha establecido la hora aún, arrancar en 00:00
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    if (tv.tv_sec < 1000) {
        // Hora no configurada → establecer a 00:00 del 1-ene-2026
        struct tm t = {};
        t.tm_year = 126;  // 2026 - 1900
        t.tm_mon  = 0;
        t.tm_mday = 1;
        t.tm_hour = 0;
        t.tm_min  = 0;
        t.tm_sec  = 0;
        time_t epoch = mktime(&t);
        struct timeval newTv = { .tv_sec = epoch, .tv_usec = 0 };
        settimeofday(&newTv, nullptr);
    }
#endif

    // Refresco inicial
    update();
}

// ════════════════════════════════════════════════════════════════════════════
// destroy()
// ════════════════════════════════════════════════════════════════════════════

void StatusBar::destroy() {
    // Los widgets LVGL son hijos de la pantalla padre;
    // se destruyen con ella. Sólo limpiamos punteros.
    _bar        = nullptr;
    _clockLabel = nullptr;
    _titleLabel = nullptr;
    _modLabel   = nullptr;
    _angleLabel = nullptr;
    _batIcon    = nullptr;
    _separator  = nullptr;
}

void StatusBar::resetPointers() {
    destroy();  // idéntico: sólo nulifica punteros, no llama lv_obj_delete
}

// ════════════════════════════════════════════════════════════════════════════
// setTitle()
// ════════════════════════════════════════════════════════════════════════════

void StatusBar::setTitle(const char* title) {
    if (_titleLabel) {
        lv_label_set_text(_titleLabel, title);
        lv_obj_align(_titleLabel, LV_ALIGN_CENTER, 0, 0);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// setBatteryLevel()
// ════════════════════════════════════════════════════════════════════════════

void StatusBar::setBatteryLevel(uint8_t pct) {
    if (pct > 100) pct = 100;
    _batLevel = pct;
    updateBatteryIcon();
}

// ════════════════════════════════════════════════════════════════════════════
// update() — Actualización global de todos los indicadores
// ════════════════════════════════════════════════════════════════════════════

void StatusBar::update() {
    if (!_bar) return;
    updateClock();
    updateModifier();
    updateAngleMode();
    updateBatteryIcon();
}

// ════════════════════════════════════════════════════════════════════════════
// updateClock() — Lee <time.h> y actualiza el label
// ════════════════════════════════════════════════════════════════════════════

void StatusBar::updateClock() {
    if (!_clockLabel) return;

    time_t now = time(nullptr);
    struct tm tmBuf;

#ifdef ARDUINO
    localtime_r(&now, &tmBuf);
#else
    // Desktop/test fallback
    #ifdef _WIN32
    localtime_s(&tmBuf, &now);
    #else
    localtime_r(&now, &tmBuf);
    #endif
#endif

    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", tmBuf.tm_hour, tmBuf.tm_min);
    lv_label_set_text(_clockLabel, buf);
}

// ════════════════════════════════════════════════════════════════════════════
// updateModifier() — Lee KeyboardManager y actualiza el label
// ════════════════════════════════════════════════════════════════════════════

void StatusBar::updateModifier() {
    if (!_modLabel) return;

    auto& km = vpam::KeyboardManager::instance();
    const char* txt = km.indicatorText();
    lv_label_set_text(_modLabel, txt);
}

// ════════════════════════════════════════════════════════════════════════════
// updateAngleMode() — Lee g_angleMode y actualiza el label
// ════════════════════════════════════════════════════════════════════════════

void StatusBar::updateAngleMode() {
    if (!_angleLabel) return;

    const char* txt = (vpam::g_angleMode == vpam::AngleMode::DEG) ? "DEG" : "RAD";
    lv_label_set_text(_angleLabel, txt);
}

// ════════════════════════════════════════════════════════════════════════════
// updateBatteryIcon() — Ajusta el relleno del icono de batería
// ════════════════════════════════════════════════════════════════════════════

void StatusBar::updateBatteryIcon() {
    if (!_batIcon) return;

    // 3 bars: bar0 (0-33%), bar1 (34-66%), bar2 (67-100%)
    // Color per level: green >50%, yellow >20%, red ≤20%
    uint32_t col;
    if (_batLevel > 50) {
        col = 0x66CC66;  // Verde
    } else if (_batLevel > 20) {
        col = 0xFFCC00;  // Amarillo
    } else {
        col = 0xFF4444;  // Rojo
    }

    int numBars = 0;
    if (_batLevel > 66) numBars = 3;
    else if (_batLevel > 33) numBars = 2;
    else if (_batLevel > 5)  numBars = 1;

    for (int i = 0; i < 3; ++i) {
        lv_obj_t* bar = lv_obj_get_child(_batIcon, i);
        if (!bar) continue;
        if (i < numBars) {
            lv_obj_set_style_bg_color(bar, lv_color_hex(col), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
        }
    }
}

} // namespace ui
