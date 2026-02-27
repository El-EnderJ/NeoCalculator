/**
 * SplashScreen.cpp
 * Implementación de la pantalla de arranque con fade-in LVGL.
 *
 * Diseño visual:
 *   · Fondo negro (#000000)
 *   · Logo "NumOS" centrado, Montserrat 20, naranja NumWorks (#FF9500)
 *   · Versión "v1.0.0 | Powered by numOS" en parte inferior, Montserrat 12, gris claro
 *
 * Animación:
 *   · Logo: fade-in 0→255 en 1200 ms, ease_in_out, delay 200 ms
 *   · Versión: fade-in 0→255 en 800 ms, ease_in_out, delay 600 ms
 *   · Al completar la animación del logo, espera 800 ms y marca _finished = true
 *   · Total visible: ~2.2 s (200 delay + 1200 fade + 800 hold)
 */

#include "SplashScreen.h"

// ══════════════════════════════════════════════════════════════════════════
// Constructor
// ══════════════════════════════════════════════════════════════════════════
SplashScreen::SplashScreen()
    : _screen(nullptr),
      _lblLogo(nullptr),
      _lblVersion(nullptr),
      _finished(false),
      _onComplete(nullptr)
{
}

// ══════════════════════════════════════════════════════════════════════════
// create() — Construye la pantalla LVGL del splash
// ══════════════════════════════════════════════════════════════════════════
void SplashScreen::create() {
    // ── Screen negro ──
    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── Logo "NumOS" ──
    _lblLogo = lv_label_create(_screen);
    lv_label_set_text(_lblLogo, "NumOS");
    lv_obj_set_style_text_font(_lblLogo, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_lblLogo, lv_color_make(0xFF, 0x95, 0x00), 0);
    lv_obj_set_style_text_align(_lblLogo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblLogo, LV_ALIGN_CENTER, 0, -12);
    lv_obj_set_style_opa(_lblLogo, LV_OPA_TRANSP, 0);  // Empieza invisible

    // ── Version "v1.0.0 | Powered by numOS" ──
    _lblVersion = lv_label_create(_screen);
    lv_label_set_text(_lblVersion, "v1.0.0 | Powered by numOS");
    lv_obj_set_style_text_font(_lblVersion, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblVersion, lv_color_make(0x90, 0x90, 0x90), 0);
    lv_obj_set_style_text_align(_lblVersion, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblVersion, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_opa(_lblVersion, LV_OPA_TRANSP, 0);  // Empieza invisible
}

// ══════════════════════════════════════════════════════════════════════════
// show() — Activa el screen y arranca las animaciones
// ══════════════════════════════════════════════════════════════════════════
void SplashScreen::show(std::function<void()> onComplete) {
    _onComplete = onComplete;
    _finished   = false;

    // Cargar el screen del splash
    lv_screen_load(_screen);

    // ── Animacion 1: Logo fade-in ──
    lv_anim_t aLogo;
    lv_anim_init(&aLogo);
    lv_anim_set_var(&aLogo, _lblLogo);
    lv_anim_set_exec_cb(&aLogo, [](void* obj, int32_t val) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj),
                             static_cast<lv_opa_t>(val), 0);
    });
    lv_anim_set_values(&aLogo, 0, 255);
    lv_anim_set_duration(&aLogo, 1200);
    lv_anim_set_delay(&aLogo, 200);
    lv_anim_set_path_cb(&aLogo, lv_anim_path_ease_in_out);
    aLogo.user_data = this;
    lv_anim_set_completed_cb(&aLogo, onAnimDone);
    lv_anim_start(&aLogo);

    // ── Animacion 2: Version fade-in ──
    lv_anim_t aVer;
    lv_anim_init(&aVer);
    lv_anim_set_var(&aVer, _lblVersion);
    lv_anim_set_exec_cb(&aVer, [](void* obj, int32_t val) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj),
                             static_cast<lv_opa_t>(val), 0);
    });
    lv_anim_set_values(&aVer, 0, 255);
    lv_anim_set_duration(&aVer, 800);
    lv_anim_set_delay(&aVer, 600);
    lv_anim_set_path_cb(&aVer, lv_anim_path_ease_in_out);
    lv_anim_start(&aVer);
}

// ══════════════════════════════════════════════════════════════════════════
// onAnimDone() — Callback estático: animación del logo completada
// ══════════════════════════════════════════════════════════════════════════
void SplashScreen::onAnimDone(lv_anim_t* a) {
    auto* self = static_cast<SplashScreen*>(a->user_data);
    if (!self) return;

    self->_finished = true;
    if (self->_onComplete) {
        self->_onComplete();
    }
}
