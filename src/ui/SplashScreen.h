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
 * SplashScreen.h
 * Pantalla de arranque LVGL con animación fade-in profesional.
 *
 * Secuencia visual:
 *   1. Pantalla negra → fade-in del logo "NumOS" (Montserrat 20, naranja)
 *   2. Fade-in del texto "v1.0.0 | Powered by numOS" (Montserrat 12, gris)
 *   3. Tras completar la animación, llama al callback onComplete
 *
 * Uso desde main.cpp:
 *   SplashScreen splash;
 *   splash.create();
 *   splash.show([]() { g_app.begin(); });
 *   // luego bombear lv_timer_handler() hasta que isFinished() sea true
 */

#pragma once

#include <lvgl.h>
#include <functional>

class SplashScreen {
public:
    SplashScreen();

    /**
     * Construye la pantalla LVGL del splash (screen + labels).
     * Llamar DESPUÉS de lv_init() y del registro del display.
     */
    void create();

    /**
     * Activa el splash screen y arranca la animación fade-in.
     * @param onComplete  Callback ejecutado al terminar toda la animación.
     */
    void show(std::function<void()> onComplete);

    /**
     * Libera la pantalla del splash y sus labels (ticket MT-03).
     *
     * Orden de vida OBLIGATORIO (clase de bug: use-after-free de la pantalla
     * activa, ver SystemApp.cpp deferred teardown):
     *   1. lv_screen_load[_anim]() de la SIGUIENTE pantalla (el menú) ya fue
     *      llamado, Y su animación FADE_IN (200 ms) ya terminó — es decir,
     *      el splash ya no es la pantalla activa ni está siendo renderizado.
     *   2. Solo entonces llamar destroy().
     * Es seguro llamarla dos veces (no-op si ya se destruyó) y se rehúsa a
     * borrar si el splash sigue siendo la pantalla activa.
     * Recupera ~1 KB + 3 objetos del pool LVGL fijo de 64 KB.
     */
    void destroy();

    /** ¿Ha terminado la animación y se puede avanzar? */
    bool isFinished() const { return _finished; }

private:
    lv_obj_t* _screen;       // Screen LVGL del splash
    lv_obj_t* _lblLogo;      // Label "NumOS"
    lv_obj_t* _lblVersion;   // Label "v1.0.0 | Powered by numOS"
    bool      _finished;     // True cuando la animación ha terminado

    std::function<void()> _onComplete;

    /** Callback estático para lv_anim_set_completed_cb */
    static void onAnimDone(lv_anim_t* a);
};
