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
 * NativeHal.cpp — Punto de entrada y HAL para simulación nativa en PC
 *
 * Solo se compila cuando NATIVE_SIM está definido (platform = native).
 *
 * Flujo completo de la aplicación:
 *   1. SplashScreen  (fade-in animado real, ~2 s)
 *   2. MainMenu      (launcher con grid de apps, flechas + ENTER)
 *   3. CalculationApp (teclado directo → handleKey)
 *   4. MODE (Home/m) vuelve al launcher
 *
 * Responsabilidades:
 *   · Crear ventana SDL2 de 320×240 (escalada ×2)
 *   · Inicializar LVGL con flush callback a SDL texture
 *   · Mapear el teclado del PC a KeyCode de la calculadora
 *   · Gestionar el ciclo de vida: Splash → Menú → App → Menú
 *   · Enrutar las teclas según el modo activo:
 *       – MENU  → LvglKeypad (navegación LVGL por grupo/gridnav)
 *       – CALC  → CalculationApp.handleKey() directo
 *
 * Mapeo de teclado:
 *   Enter          → ENTER
 *   Flechas        → UP / DOWN / LEFT / RIGHT
 *   0-9            → NUM_0..NUM_9
 *   ESC            → AC
 *   Backspace      → DEL
 *   + - * /        → ADD SUB MUL DIV
 *   ( )            → LPAREN RPAREN
 *   ^ .            → POW DOT
 *   Tab            → ALPHA
 *   LShift/RShift  → SHIFT
 *   Insert         → STO  (Store)
 *   Home / m       → MODE (volver al launcher)
 *   s              → SIN
 *   c              → COS
 *   t              → TAN
 *   l              → LN
 *   g              → LOG
 *   r              → SQRT
 *   p              → CONST_PI
 *   e              → CONST_E
 *   x              → VAR_X
 *   y              → VAR_Y
 *   f / F5 / =     → FREE_EQ  (S⇔D)
 *   n              → NEGATE
 *
 * Notas Phase 3A (solo emulador, sin impacto en firmware):
 *   · SDL_KEYDOWN → PRESS/REPEAT, SDL_KEYUP → RELEASE (antes solo KEYDOWN).
 *   · Coordenadas logicas 320×240 + integer scale (ventana ×N nitida).
 *   · Auto-salida CLI: --frames N | --run-for-ms N | --headless | --scale N.
 *
 * Notas Phase 4A (solo emulador, sin impacto en firmware):
 *   · --script P reproduce un .numos: inyecta teclas por la MISMA ruta de
 *     despacho que SDL (dispatchKey) y captura PPM por frame (scriptStepBegin/
 *     scriptCaptureIfPending). Determinista: 1 comando por frame, usar con
 *     --deterministic --frames N. Ver docs/emulator-sdl2-quickstart.md.
 */

#ifdef NATIVE_SIM

#include <SDL2/SDL.h>
#include <lvgl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "../input/KeyCodes.h"
#include "../input/LvglKeypad.h"
#include "../input/KeyboardManager.h"
#include "../apps/CalculationApp.h"
#include "../apps/SettingsApp.h"          // Phase 5A: emulator-safe LVGL-native app
#include "../apps/StatisticsApp.h"        // Phase 6A: LVGL-only, pure-math (no CAS/HW)
#include "../apps/ProbabilityApp.h"       // Phase 6A: LVGL-only, pure-math (no CAS/HW)
#include "../apps/SequencesApp.h"         // Phase 7A: LVGL-only, pure-math (no CAS/HW)
#include "../apps/RegressionApp.h"        // Phase 7C: LVGL-only, pure-math (no CAS/HW)
#include "../display/DisplayDriver.h"
#include "../ui/SplashScreen.h"
#include "../ui/MainMenu.h"
#include "../ui/StatusBar.h"              // Phase 5A: Math Showcase title bar
#include "../ui/MathRenderer.h"          // Phase 5A: MathCanvas (reuse, no geometry change)
#include "../ui/MathTypography.h"        // Phase 5A: initMathTypography()
#include "../math/MathRenderVisualCases.h" // Phase 5A: curated accepted expressions

// ════════════════════════════════════════════════════════════════════════════
// Global CAS settings (native definitions)
//
// Declarados extern en Config.h y definidos en main.cpp, pero main.cpp es
// #ifdef ARDUINO. En la simulación nativa main.cpp no se compila, así que
// definimos aquí las tres variables con los mismos tipos (Config.h:77-79) y
// los mismos valores por defecto que el firmware (main.cpp:31-33).
// ════════════════════════════════════════════════════════════════════════════
bool setting_complex_enabled  = true;
int  setting_decimal_precision = 10;
bool setting_edu_steps         = false;

// ════════════════════════════════════════════════════════════════════════════
// DisplayDriver stubs (MainMenu almacena una referencia pero nunca la usa)
// ════════════════════════════════════════════════════════════════════════════
DisplayDriver::DisplayDriver() {}
DisplayDriver::~DisplayDriver() {}
void DisplayDriver::begin() {}
void DisplayDriver::initLvgl(void*, void*, uint32_t) {}
void DisplayDriver::lvglFlushCb(lv_display_t*, const lv_area_t*, uint8_t*) {}

// ════════════════════════════════════════════════════════════════════════════
// Constantes
//
// SCREEN_W/H son la resolucion LOGICA del dispositivo (ILI9341 320×240). NUNCA
// cambian: la textura LVGL y el "logical size" del renderer se fijan a este
// tamaño para que el emulador presente exactamente el mismo sistema de
// coordenadas que el firmware. El escalado a la ventana del PC lo gestiona SDL
// (logical size + integer scale), NO el codigo de dibujo de formulas.
// ════════════════════════════════════════════════════════════════════════════
static constexpr int SCREEN_W             = 320;  // ancho logico (NO tocar)
static constexpr int SCREEN_H             = 240;  // alto  logico (NO tocar)
static constexpr int DEFAULT_WINDOW_SCALE = 2;    // factor por defecto (×2)

// Politica de temporizacion del bucle nativo (ver bucle principal):
//  · El reloj de LVGL es lv_tick_set_cb(SDL_GetTicks) → tiempo de pared real.
//  · El bucle duerme FRAME_DELAY_MS por iteracion para ceder CPU (sin busy-spin).
//  · Con renderer VSYNC, SDL_RenderPresent tambien limita el ritmo.
static constexpr uint32_t FRAME_DELAY_MS = 5;     // ~200 fps techo

// ════════════════════════════════════════════════════════════════════════════
// Opciones de linea de comandos (modo auto-salida para smoke-tests / CI)
//
// SOLO afectan al emulador; no existen en el firmware. El uso interactivo
// normal (sin argumentos) no cambia en absoluto: maxFrames/maxMs < 0 = sin
// limite, por lo que el bucle corre hasta cerrar la ventana, igual que antes.
// ════════════════════════════════════════════════════════════════════════════
struct EmuOptions {
    long maxFrames   = -1;                    // <0 = sin limite de frames
    long maxMs       = -1;                    // <0 = sin limite de tiempo
    int  windowScale = DEFAULT_WINDOW_SCALE;  // factor de escala de ventana
    bool headless    = false;                 // SDL_VIDEODRIVER=dummy
    bool quiet       = false;                 // silenciar logs por-tecla/iter
    // ── Phase 3B (solo emulador) ────────────────────────────────────────────
    bool deterministic       = false;         // tick sintetico de paso fijo
    long stepMs              = 16;            // ms por frame en modo determinista
    const char* screenshotPath = nullptr;     // volcar PPM al salir si != null
    // ── Phase 4A (solo emulador) ────────────────────────────────────────────
    const char* scriptPath     = nullptr;     // reproducir script .numos si != null
};
static EmuOptions g_opts;

// ════════════════════════════════════════════════════════════════════════════
// Reloj sintetico para el modo determinista (Phase 3B)
//
// En modo --deterministic, LVGL NO lee el reloj de pared (SDL_GetTicks): lee
// este contador, que el bucle principal avanza un paso fijo (stepMs) por frame.
// Asi el estado de animaciones/timers pasa a ser funcion del INDICE de frame,
// reproducible entre ejecuciones y maquinas (sin jitter de SDL_Delay/VSYNC).
// La firma uint32_t(void) coincide con lv_tick_get_cb_t, igual que SDL_GetTicks,
// por lo que es un sustituto directo en lv_tick_set_cb. Inerte salvo --deterministic.
// ════════════════════════════════════════════════════════════════════════════
static uint32_t g_detTick = 0;                // ms virtuales acumulados
static uint32_t detTickCb() { return g_detTick; }

// ════════════════════════════════════════════════════════════════════════════
// Modos de la aplicación
// ════════════════════════════════════════════════════════════════════════════
enum class AppMode : uint8_t {
    SPLASH,         // Pantalla de carga con animación
    MENU,           // Launcher (grid de apps)
    CALCULATION,    // Calculadora científica
    SETTINGS,       // Ajustes (LVGL-native; Phase 5A, emulador)
    MATH_SHOWCASE,  // Vitrina de render matemático (Phase 5A, solo emulador)
    STATISTICS,     // Estadística (LVGL-native; Phase 6A, emulador)
    PROBABILITY,    // Probabilidad (LVGL-native; Phase 6A, emulador)
    SEQUENCES,      // Secuencias (LVGL-native; Phase 7A, emulador)
    REGRESSION      // Regresion (LVGL-native; Phase 7C, emulador)
};

// ════════════════════════════════════════════════════════════════════════════
// Estado global
// ════════════════════════════════════════════════════════════════════════════
static SDL_Window*   g_window     = nullptr;
static SDL_Renderer* g_renderer   = nullptr;
static SDL_Texture*  g_texture    = nullptr;
static bool          g_quit       = false;
static AppMode       g_mode       = AppMode::SPLASH;
static bool          g_splashDone = false;   // Flag para transición diferida

// Instancias de aplicación
static DisplayDriver    g_displayStub;        // Stub para MainMenu
static SplashScreen*    g_splash  = nullptr;
static MainMenu*        g_menu    = nullptr;
static CalculationApp*  g_calcApp = nullptr;
static SettingsApp*     g_settingsApp = nullptr;   // Phase 5A (emulador)
static StatisticsApp*   g_statsApp = nullptr;      // Phase 6A (emulador)
static ProbabilityApp*  g_probApp  = nullptr;      // Phase 6A (emulador)
static SequencesApp*    g_seqApp   = nullptr;      // Phase 7A (emulador)
static RegressionApp*   g_regApp   = nullptr;      // Phase 7C (emulador)

// ── Math Showcase (Phase 5A, solo emulador) ─────────────────────────────────
// Identificador de app fuera del rango de tarjetas del launcher (0..21) para que
// nunca colisione con una tarjeta real: la vitrina NO es una tarjeta, se abre con
// `open_app MathShowcase`. Su estado se crea perezosamente en showcaseLoad().
static constexpr int     APPID_MATH_SHOWCASE = 100;
static lv_obj_t*         g_showcaseScreen  = nullptr;
static ui::StatusBar*    g_showcaseBar     = nullptr;
static vpam::MathCanvas* g_showcaseCanvas  = nullptr;
static lv_obj_t*         g_showcaseCaption = nullptr;
static vpam::NodePtr     g_showcaseRoot;            // AST de la expresión activa
static int               g_showcaseIndex   = 0;

// Buffer de LVGL (pantalla completa, RGB565)
// 320×240 × 2 bytes = 153 600 bytes → trivial en PC
static uint8_t g_lvBuf[SCREEN_W * SCREEN_H * sizeof(uint16_t)];

// Forward declarations
static void launchApp(int appId);
static void returnToMenu();
static void onSplashDone();
// Phase 5A: Math Showcase (definidas tras returnToMenu).
static void showcaseLoad();
static void showcaseShow(int slot);
static void showcaseEnd();

// ════════════════════════════════════════════════════════════════════════════
// sdl_flush_cb — Transfiere pixels LVGL a la SDL texture
//
// ¡IMPORTANTE! Esta función se ejecuta DENTRO de lv_timer_handler().
// NO debe llamar a SDL_RenderPresent() aquí porque bloquea (VSYNC)
// e impide que el bucle principal procese eventos SDL → "No responde".
//
// Solo actualizamos la textura (rápido). La presentación real se hace
// en el bucle principal, DESPUÉS de que lv_timer_handler() retorna.
// ════════════════════════════════════════════════════════════════════════════
static bool g_needsPresent = false;  // Flag: hay pixeles nuevos que mostrar

static void sdl_flush_cb(lv_display_t* disp,
                          const lv_area_t* area,
                          uint8_t* px_map)
{
    const int w = lv_area_get_width(area);
    const int h = lv_area_get_height(area);

    SDL_Rect rect;
    rect.x = area->x1;
    rect.y = area->y1;
    rect.w = w;
    rect.h = h;

    // Solo copiar pixels a la textura (no bloquea)
    SDL_UpdateTexture(g_texture, &rect, px_map, w * 2);
    g_needsPresent = true;

    lv_display_flush_ready(disp);
}

// ════════════════════════════════════════════════════════════════════════════
// mapSdlToKeyCode — Traduce SDL_Keycode a KeyCode de la calculadora
// ════════════════════════════════════════════════════════════════════════════
static KeyCode mapSdlToKeyCode(SDL_Keycode sym)
{
    switch (sym) {
        // Navegación
        case SDLK_RETURN:
        case SDLK_KP_ENTER:     return KeyCode::ENTER;
        case SDLK_UP:           return KeyCode::UP;
        case SDLK_DOWN:         return KeyCode::DOWN;
        case SDLK_LEFT:         return KeyCode::LEFT;
        case SDLK_RIGHT:        return KeyCode::RIGHT;

        // Control
        case SDLK_ESCAPE:       return KeyCode::AC;
        case SDLK_BACKSPACE:
        case SDLK_DELETE:       return KeyCode::DEL;
        case SDLK_HOME:         return KeyCode::MODE;

        // Modificadores
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:       return KeyCode::SHIFT;
        case SDLK_TAB:          return KeyCode::ALPHA;
        case SDLK_INSERT:       return KeyCode::STO;    // Store (Phase 3A)

        // Dígitos (fila superior del teclado)
        case SDLK_0:
        case SDLK_KP_0:         return KeyCode::NUM_0;
        case SDLK_1:
        case SDLK_KP_1:         return KeyCode::NUM_1;
        case SDLK_2:
        case SDLK_KP_2:         return KeyCode::NUM_2;
        case SDLK_3:
        case SDLK_KP_3:         return KeyCode::NUM_3;
        case SDLK_4:
        case SDLK_KP_4:         return KeyCode::NUM_4;
        case SDLK_5:
        case SDLK_KP_5:         return KeyCode::NUM_5;
        case SDLK_6:
        case SDLK_KP_6:         return KeyCode::NUM_6;
        case SDLK_7:
        case SDLK_KP_7:         return KeyCode::NUM_7;
        case SDLK_8:
        case SDLK_KP_8:         return KeyCode::NUM_8;
        case SDLK_9:
        case SDLK_KP_9:         return KeyCode::NUM_9;

        // Operadores
        case SDLK_KP_PLUS:      return KeyCode::ADD;
        case SDLK_KP_MINUS:     return KeyCode::SUB;
        case SDLK_KP_MULTIPLY:
        case SDLK_ASTERISK:     return KeyCode::MUL;
        case SDLK_KP_DIVIDE:
        case SDLK_SLASH:        return KeyCode::DIV;
        case SDLK_KP_PERIOD:    return KeyCode::DOT;
        case SDLK_CARET:        return KeyCode::POW;

        // Paréntesis (necesitan SHIFT en teclado US)
        case SDLK_LEFTPAREN:    return KeyCode::LPAREN;
        case SDLK_RIGHTPAREN:   return KeyCode::RPAREN;

        // Funciones y constantes (teclas de letras)
        case SDLK_s:            return KeyCode::SIN;
        case SDLK_c:            return KeyCode::COS;
        case SDLK_t:            return KeyCode::TAN;
        case SDLK_l:            return KeyCode::LN;
        case SDLK_g:            return KeyCode::LOG;
        case SDLK_r:            return KeyCode::SQRT;
        case SDLK_p:            return KeyCode::CONST_PI;
        case SDLK_e:            return KeyCode::CONST_E;

        // Variables
        case SDLK_x:            return KeyCode::VAR_X;
        case SDLK_y:            return KeyCode::VAR_Y;

        // S⇔D y especiales
        case SDLK_f:
        case SDLK_F5:
        case SDLK_EQUALS:       return KeyCode::FREE_EQ;

        // Periodo: punto normal
        case SDLK_PERIOD:       return KeyCode::DOT;

        // +/- en fila principal
        case SDLK_PLUS:         return KeyCode::ADD;
        case SDLK_MINUS:        return KeyCode::SUB;

        // m = MODE (alternativo a Home)
        case SDLK_m:            return KeyCode::MODE;

        // n = NEGATE
        case SDLK_n:            return KeyCode::NEGATE;

        default: return KeyCode::NONE;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// scriptNameToKeyCode — Traduce un NOMBRE de tecla de script a KeyCode (Phase 4A)
//
// Vocabulario propio del replay de scripts. NO inventa KeyCodes: cada nombre se
// asigna a un KeyCode existente (los mismos que produce mapSdlToKeyCode). Nombres
// alfabeticos case-insensitive; los simbolos (+ - * / ^ . ( ) =) y los digitos se
// comparan tal cual. Nota NumOS: la tecla de division ES la de fraccion
// (KeyCode::DIV -> insertFraction, CalculationApp.cpp:344), por eso FRAC == "/".
// Devuelve KeyCode::NONE si el nombre es desconocido.
// ════════════════════════════════════════════════════════════════════════════
static KeyCode scriptNameToKeyCode(const std::string& raw)
{
    // Simbolos directos.
    if (raw == "+") return KeyCode::ADD;
    if (raw == "-") return KeyCode::SUB;
    if (raw == "*") return KeyCode::MUL;
    if (raw == "/") return KeyCode::DIV;
    if (raw == "^") return KeyCode::POW;
    if (raw == ".") return KeyCode::DOT;
    if (raw == "(") return KeyCode::LPAREN;
    if (raw == ")") return KeyCode::RPAREN;
    if (raw == "=") return KeyCode::FREE_EQ;

    // Digitos sueltos. OJO: NUM_0..NUM_9 NO son contiguos en el enum
    // (KeyCodes.h:68-77), asi que NO se puede hacer aritmetica de enum.
    if (raw.size() == 1 && raw[0] >= '0' && raw[0] <= '9') {
        switch (raw[0]) {
            case '0': return KeyCode::NUM_0;
            case '1': return KeyCode::NUM_1;
            case '2': return KeyCode::NUM_2;
            case '3': return KeyCode::NUM_3;
            case '4': return KeyCode::NUM_4;
            case '5': return KeyCode::NUM_5;
            case '6': return KeyCode::NUM_6;
            case '7': return KeyCode::NUM_7;
            case '8': return KeyCode::NUM_8;
            case '9': return KeyCode::NUM_9;
        }
    }

    // Nombres alfabeticos (case-insensitive).
    std::string n = raw;
    for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (n == "enter")                         return KeyCode::ENTER;
    if (n == "left")                          return KeyCode::LEFT;
    if (n == "right")                         return KeyCode::RIGHT;
    if (n == "up")                            return KeyCode::UP;
    if (n == "down")                          return KeyCode::DOWN;
    if (n == "backspace" || n == "del" ||
        n == "delete")                        return KeyCode::DEL;
    if (n == "ac" || n == "esc" ||
        n == "escape")                        return KeyCode::AC;
    if (n == "home" || n == "mode")           return KeyCode::MODE;
    // GRAPH (KeyCodes.h:61): app-specific function key. StatisticsApp uses it to
    // cycle Data->Stats->Graph tabs (StatisticsApp.cpp:532); on the Data tab
    // LEFT/RIGHT are column nav, so GRAPH is the only key that reaches a computed
    // tab. Needed by statistics_data_smoke.numos (Phase 6C). Emulator-only.
    if (n == "graph")                         return KeyCode::GRAPH;
    if (n == "x" || n == "varx")              return KeyCode::VAR_X;
    if (n == "y" || n == "vary")              return KeyCode::VAR_Y;
    if (n == "pow")                           return KeyCode::POW;
    if (n == "frac" || n == "fraction" ||
        n == "div")                           return KeyCode::DIV;   // DIV == fraccion
    if (n == "dot")                           return KeyCode::DOT;
    if (n == "add")                           return KeyCode::ADD;
    if (n == "sub")                           return KeyCode::SUB;
    if (n == "mul")                           return KeyCode::MUL;
    if (n == "lparen")                        return KeyCode::LPAREN;
    if (n == "rparen")                        return KeyCode::RPAREN;
    if (n == "sqrt")                          return KeyCode::SQRT;
    if (n == "sin")                           return KeyCode::SIN;
    if (n == "cos")                           return KeyCode::COS;
    if (n == "tan")                           return KeyCode::TAN;
    if (n == "ln")                            return KeyCode::LN;
    if (n == "log")                           return KeyCode::LOG;
    if (n == "pi")                            return KeyCode::CONST_PI;
    if (n == "e")                             return KeyCode::CONST_E;
    if (n == "negate" || n == "neg")          return KeyCode::NEGATE;
    if (n == "ans")                           return KeyCode::ANS;
    if (n == "shift")                         return KeyCode::SHIFT;
    if (n == "alpha")                         return KeyCode::ALPHA;
    if (n == "sto")                           return KeyCode::STO;
    if (n == "freeeq" || n == "sd")           return KeyCode::FREE_EQ;

    return KeyCode::NONE;
}

// ════════════════════════════════════════════════════════════════════════════
// dispatchKey — Enruta UNA tecla (ya mapeada) segun el modo activo
//
// Extraido del switch(g_mode) de processSdlEvents para que TANTO la entrada SDL
// en vivo COMO el replay de scripts (Phase 4A) usen exactamente la misma ruta de
// despacho: MENU sintetiza PRESS+RELEASE para gridnav (solo en isDown), CALC
// reenvia a CalculationApp::handleKey con el caso especial MODE -> returnToMenu.
// ════════════════════════════════════════════════════════════════════════════
static void dispatchKey(KeyCode kc, KeyAction action, bool isDown)
{
    switch (g_mode) {
        case AppMode::SPLASH:
            // Ignorar teclas durante la animación del splash
            break;

        case AppMode::MENU:
            // El indev LVGL (gridnav) necesita un par PRESS+RELEASE para
            // disparar CLICKED; lo sintetizamos en el down-edge. El up-edge no
            // añade nada aqui (por eso solo actuamos en isDown).
            if (isDown) {
                LvglKeypad::pushKey(kc, true);    // PRESS
                LvglKeypad::pushKey(kc, false);   // RELEASE (dispara CLICKED)
            }
            break;

        case AppMode::CALCULATION:
            // MODE → volver al launcher (solo en la pulsacion).
            if (isDown && kc == KeyCode::MODE) {
                returnToMenu();
                break;
            }
            // El resto de teclas → CalculationApp directamente, incluyendo
            // RELEASE. handleKey() lo ignora igual que en el firmware
            // (CalculationApp.cpp filtra todo lo que no sea PRESS/REPEAT).
            {
                KeyEvent ke;
                ke.code   = kc;
                ke.action = action;
                ke.row    = -1;
                ke.col    = -1;
                if (!g_opts.quiet) {
                    std::printf("[CALC] handleKey(code=%d action=%d)\n",
                                static_cast<int>(kc),
                                static_cast<int>(action));
                }
                g_calcApp->handleKey(ke);
            }
            break;

        case AppMode::SETTINGS:
            // Mismo contrato que CALCULATION: MODE vuelve al launcher; el resto
            // (incluido RELEASE) se reenvia a SettingsApp::handleKey, que filtra
            // todo lo que no sea PRESS/REPEAT igual que en el firmware.
            if (isDown && kc == KeyCode::MODE) {
                returnToMenu();
                break;
            }
            if (g_settingsApp) {
                KeyEvent ke;
                ke.code   = kc;
                ke.action = action;
                ke.row    = -1;
                ke.col    = -1;
                g_settingsApp->handleKey(ke);
            }
            break;

        case AppMode::STATISTICS:
            // Phase 6A: mismo contrato que SETTINGS — MODE vuelve al launcher; el
            // resto (incluido RELEASE) se reenvia a StatisticsApp::handleKey.
            if (isDown && kc == KeyCode::MODE) {
                returnToMenu();
                break;
            }
            if (g_statsApp) {
                KeyEvent ke;
                ke.code   = kc;
                ke.action = action;
                ke.row    = -1;
                ke.col    = -1;
                g_statsApp->handleKey(ke);
            }
            break;

        case AppMode::PROBABILITY:
            // Phase 6A: mismo contrato que SETTINGS — MODE vuelve al launcher; el
            // resto (incluido RELEASE) se reenvia a ProbabilityApp::handleKey.
            if (isDown && kc == KeyCode::MODE) {
                returnToMenu();
                break;
            }
            if (g_probApp) {
                KeyEvent ke;
                ke.code   = kc;
                ke.action = action;
                ke.row    = -1;
                ke.col    = -1;
                g_probApp->handleKey(ke);
            }
            break;

        case AppMode::SEQUENCES:
            // Phase 7A: mismo contrato que SETTINGS — MODE vuelve al launcher; el
            // resto (incluido RELEASE) se reenvia a SequencesApp::handleKey.
            if (isDown && kc == KeyCode::MODE) {
                returnToMenu();
                break;
            }
            if (g_seqApp) {
                KeyEvent ke;
                ke.code   = kc;
                ke.action = action;
                ke.row    = -1;
                ke.col    = -1;
                g_seqApp->handleKey(ke);
            }
            break;

        case AppMode::REGRESSION:
            // Phase 7C: mismo contrato que SETTINGS — MODE vuelve al launcher; el
            // resto (incluido RELEASE) se reenvia a RegressionApp::handleKey.
            if (isDown && kc == KeyCode::MODE) {
                returnToMenu();
                break;
            }
            if (g_regApp) {
                KeyEvent ke;
                ke.code   = kc;
                ke.action = action;
                ke.row    = -1;
                ke.col    = -1;
                g_regApp->handleKey(ke);
            }
            break;

        case AppMode::MATH_SHOWCASE:
            // MODE vuelve al launcher; IZQ/ARRIBA y DCHA/ABAJO recorren los casos
            // curados. Solo en el down-edge (una pulsacion = un paso), sin cursor
            // ni timers → captura determinista.
            if (isDown && kc == KeyCode::MODE) {
                returnToMenu();
                break;
            }
            if (isDown) {
                if (kc == KeyCode::LEFT || kc == KeyCode::UP) {
                    showcaseShow(g_showcaseIndex - 1);
                } else if (kc == KeyCode::RIGHT || kc == KeyCode::DOWN ||
                           kc == KeyCode::ENTER) {
                    showcaseShow(g_showcaseIndex + 1);
                }
            }
            break;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// processSdlEvents — Procesa eventos SDL y los enruta según el modo activo
// ════════════════════════════════════════════════════════════════════════════
static void processSdlEvents()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            g_quit = true;
            return;
        }

        // Procesar pulsacion (KEYDOWN) Y liberacion (KEYUP). Antes solo KEYDOWN.
        if (ev.type != SDL_KEYDOWN && ev.type != SDL_KEYUP) continue;

        const bool        isDown   = (ev.type == SDL_KEYDOWN);
        const bool        isRepeat = isDown && (ev.key.repeat != 0);
        const SDL_Keycode sym      = ev.key.keysym.sym;

        KeyCode kc = mapSdlToKeyCode(sym);
        if (kc == KeyCode::NONE) {
            // Log de teclas sin mapear (modo debug nativo, silenciable).
            if (!g_opts.quiet) {
                std::printf("[KEY] sin-mapear SDL=%s (%s)\n",
                            SDL_GetKeyName(sym), isDown ? "down" : "up");
            }
            continue;
        }

        // KeyAction fiel al firmware:
        //   KEYDOWN nuevo     → PRESS
        //   KEYDOWN repetido  → REPEAT  (auto-repeticion del SO; INTENCIONADO,
        //                       igual que el driver Keyboard de hardware. No es
        //                       un "doble PRESS": CalculationApp distingue ambos)
        //   KEYUP             → RELEASE
        const KeyAction action = !isDown  ? KeyAction::RELEASE
                               : isRepeat ? KeyAction::REPEAT
                                          : KeyAction::PRESS;

        if (!g_opts.quiet) {
            const char* modeStr = (g_mode == AppMode::SPLASH) ? "SPLASH"
                                : (g_mode == AppMode::MENU)   ? "MENU"
                                                              : "CALC";
            const char* actStr  = (action == KeyAction::PRESS)  ? "PRESS"
                                : (action == KeyAction::REPEAT) ? "REPEAT"
                                                                : "RELEASE";
            std::printf("[KEY] SDL=%s code=%d action=%s mode=%s\n",
                        SDL_GetKeyName(sym), static_cast<int>(kc),
                        actStr, modeStr);
        }

        // Despacho compartido con el replay de scripts (Phase 4A).
        dispatchKey(kc, action, isDown);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Gestión del ciclo de vida de las aplicaciones
// ════════════════════════════════════════════════════════════════════════════

/**
 * Callback del SplashScreen: se ejecuta cuando la animación termina.
 *
 * ¡IMPORTANTE! Este callback se ejecuta DENTRO de lv_timer_handler()
 * (vía lv_anim completed_cb). Crear objetos LVGL pesados aquí corrompe
 * el pipeline de renderizado y congela la ventana.
 *
 * Solución: solo levantamos un flag. El bucle principal hace la
 * transición real FUERA del contexto de LVGL.
 */
static void onSplashDone()
{
    std::printf("[SPLASH] Animacion completada -> flag diferido\n");
    g_splashDone = true;
}

/**
 * Crea las apps y carga el MainMenu.
 * Se llama desde el bucle principal cuando g_splashDone == true.
 */
static void transitionToMenu()
{
    std::printf("[TRANSITION] Creando apps y launcher...\n");

    // Crear la calculadora (pre-crear pantalla para lanzamiento rápido)
    g_calcApp = new CalculationApp();
    g_calcApp->begin();

    // Phase 5A: SettingsApp (LVGL-native). begin() perezoso: su pantalla se crea
    // en el primer load() (SettingsApp::load llama a begin() si hace falta).
    g_settingsApp = new SettingsApp();

    // Phase 6A: Statistics & Probability (LVGL-native). Mismo patron perezoso que
    // SettingsApp: solo se construye el objeto; su pantalla se crea en el primer
    // load() (cada *App::load() llama a begin() si hace falta).
    g_statsApp = new StatisticsApp();
    g_probApp  = new ProbabilityApp();

    // Phase 7A: SequencesApp (LVGL-native). Mismo patron perezoso: solo se construye
    // el objeto; su pantalla se crea en el primer load() (load() llama a begin()).
    g_seqApp   = new SequencesApp();

    // Phase 7C: RegressionApp (LVGL-native). Mismo patron perezoso: solo se construye
    // el objeto; su pantalla se crea en el primer load() (load() llama a begin()).
    g_regApp   = new RegressionApp();

    // Crear y mostrar el MainMenu
    g_menu = new MainMenu(g_displayStub);
    g_menu->create();
    g_menu->setLaunchCallback(launchApp);
    g_menu->load();

    // Conectar el teclado LVGL al grupo del menú
    lv_indev_set_group(LvglKeypad::indev(), g_menu->group());
    g_mode = AppMode::MENU;
    std::printf("[MENU] Launcher cargado — flechas + ENTER para navegar\n");
}

/**
 * Callback del MainMenu: lanza la app seleccionada.
 * @param appId  ID de la app (0 = Calculation, 1 = Grapher, etc.)
 */
static void launchApp(int appId)
{
    std::printf("[APP] Lanzando app %d\n", appId);

    switch (appId) {
        case 0: // Calculation
            g_calcApp->load();
            g_mode = AppMode::CALCULATION;
            std::printf("[APP] CalculationApp activa — escribe con el teclado\n");
            break;

        case 4: // Statistics (LVGL-native; Phase 6A, mismo id que la tarjeta del launcher)
            if (g_statsApp) {
                g_statsApp->load();
                g_mode = AppMode::STATISTICS;
                std::printf("[APP] StatisticsApp activa\n");
            }
            break;

        case 5: // Probability (LVGL-native; Phase 6A, mismo id que la tarjeta del launcher)
            if (g_probApp) {
                g_probApp->load();
                g_mode = AppMode::PROBABILITY;
                std::printf("[APP] ProbabilityApp activa\n");
            }
            break;

        case 6: // Regression (LVGL-native; Phase 7C, mismo id que la tarjeta del launcher)
            if (g_regApp) {
                g_regApp->load();
                g_mode = AppMode::REGRESSION;
                std::printf("[APP] RegressionApp activa\n");
            }
            break;

        case 7: // Sequences (LVGL-native; Phase 7A, mismo id que la tarjeta del launcher)
            if (g_seqApp) {
                g_seqApp->load();
                g_mode = AppMode::SEQUENCES;
                std::printf("[APP] SequencesApp activa\n");
            }
            break;

        case 10: // Settings (LVGL-native; mismo id que la tarjeta del launcher)
            if (g_settingsApp) {
                g_settingsApp->load();
                g_mode = AppMode::SETTINGS;
                std::printf("[APP] SettingsApp activa\n");
            }
            break;

        case APPID_MATH_SHOWCASE: // Math Showcase (solo emulador)
            showcaseLoad();
            g_mode = AppMode::MATH_SHOWCASE;
            std::printf("[APP] Math Showcase activa — IZQ/DCHA cambia de caso\n");
            break;

        default:
            std::printf("[APP] App %d no implementada en simulador\n", appId);
            break;
    }
}

/**
 * Vuelve al launcher desde cualquier app.
 * Destruye y re-crea la pantalla de la app (listo para el próximo lanzamiento).
 */
static void returnToMenu()
{
    std::printf("[APP] Volviendo al launcher\n");

    // Teardown segun la app activa ANTES de cambiar g_mode. Llamado fuera del
    // contexto de lv_timer_handler (desde dispatchKey/scriptStepBegin), por lo
    // que el destruir/re-crear sincrono es seguro.
    switch (g_mode) {
        case AppMode::CALCULATION:
            if (g_calcApp) {
                g_calcApp->end();
                g_calcApp->begin();   // Pre-crear para la próxima vez
            }
            break;
        case AppMode::SETTINGS:
            // SettingsApp::load() vuelve a llamar begin() perezosamente.
            if (g_settingsApp) g_settingsApp->end();
            break;
        case AppMode::STATISTICS:
            // Phase 6A: StatisticsApp::load() vuelve a llamar begin() perezosamente.
            if (g_statsApp) g_statsApp->end();
            break;
        case AppMode::PROBABILITY:
            // Phase 6A: ProbabilityApp::load() vuelve a llamar begin() perezosamente.
            if (g_probApp) g_probApp->end();
            break;
        case AppMode::SEQUENCES:
            // Phase 7A: SequencesApp::load() vuelve a llamar begin() perezosamente.
            if (g_seqApp) g_seqApp->end();
            break;
        case AppMode::REGRESSION:
            // Phase 7C: RegressionApp::load() vuelve a llamar begin() perezosamente.
            if (g_regApp) g_regApp->end();
            break;
        case AppMode::MATH_SHOWCASE:
            showcaseEnd();
            break;
        default:
            break;
    }

    // Resetear modificadores de teclado (SHIFT/ALPHA/STO)
    vpam::KeyboardManager::instance().reset();

    g_menu->load();
    lv_indev_set_group(LvglKeypad::indev(), g_menu->group());
    g_mode = AppMode::MENU;
    std::printf("[MENU] Launcher restaurado\n");
}

// ════════════════════════════════════════════════════════════════════════════
// Math Showcase (Phase 5A) — SOLO emulador
//
// Muestra un subconjunto CURADO de los MathRenderVisualCases ACEPTADOS, dibujados
// con el MISMO MathCanvas que usa el firmware, con el cursor APAGADO (sin
// parpadeo → captura determinista), sin overlays de diagnostico y sin logs por
// caso. NO construye geometria nueva: reutiliza tal cual los builders aceptados de
// src/math/MathRenderVisualCases.cpp. Es independiente del MathRenderVisualTestApp
// de validacion (que vuelca metricas por serial y vive tras NUMOS_MATH_VISUAL_VERIFY).
// ════════════════════════════════════════════════════════════════════════════

// Casos curados (en el orden de presentacion pedido). Cada id existe en
// mathRenderVisualCases() (MathRenderVisualCases.cpp:158-178); se resuelven por id
// para reutilizar EXACTAMENTE la geometria aceptada.
static const char* const kShowcaseIds[] = {
    "mixed_row_fraction_power",     // 1 + 2/3 + x^2
    "photo_2_plus_2_over_2",        // 2 + 2/2
    "power_2_squared",              // 2^2
    "power_x_ten",                  // x^10
    "power_fraction_base_squared",  // (2/3)^2
    "power_two_half",               // 2^(1/2)
    "nested_fraction",              // (1 + 1/2) / (x + 3)  (fraccion anidada)
};
static constexpr int kShowcaseCount =
    static_cast<int>(sizeof(kShowcaseIds) / sizeof(kShowcaseIds[0]));

static const vpam::MathRenderVisualCase* showcaseCaseAt(int slot)
{
    const vpam::MathRenderVisualCase* cases = vpam::mathRenderVisualCases();
    const std::size_t n = vpam::mathRenderVisualCaseCount();
    for (std::size_t i = 0; i < n; ++i) {
        if (std::strcmp(cases[i].id, kShowcaseIds[slot]) == 0) return &cases[i];
    }
    return nullptr;
}

static void showcaseShow(int slot)
{
    if (kShowcaseCount <= 0 || !g_showcaseCanvas) return;
    if (slot < 0)               slot = kShowcaseCount - 1;
    if (slot >= kShowcaseCount) slot = 0;
    g_showcaseIndex = slot;

    const vpam::MathRenderVisualCase* vc = showcaseCaseAt(slot);
    if (!vc) return;

    // Orden de llamadas IDENTICO al consumidor probado y aceptado
    // (MathRenderVisualTestApp::showCase, CursorMode::Off): desconectar antes de
    // que el unique_ptr libere el AST anterior, reconstruir, fijar expresion con
    // cursor nullptr (sin parpadeo), estilo y reset de scroll.
    g_showcaseCanvas->setExpression(nullptr, nullptr);
    g_showcaseRoot = vc->build();
    vpam::NodeRow* rootRow = static_cast<vpam::NodeRow*>(g_showcaseRoot.get());

    g_showcaseCanvas->stopCursorBlink();
    g_showcaseCanvas->setExpression(rootRow, nullptr);   // cursor OFF → determinista
    g_showcaseCanvas->setMathStyle(vc->style);
    g_showcaseCanvas->resetScroll();
    g_showcaseCanvas->invalidate();

    if (g_showcaseCaption) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%d/%d   %s",
                      slot + 1, kShowcaseCount, vc->label);
        lv_label_set_text(g_showcaseCaption, buf);
    }
}

static void showcaseBuild()
{
    if (g_showcaseScreen) return;
    ui::initMathTypography();   // idempotente; CalculationApp ya lo hizo

    g_showcaseScreen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_showcaseScreen, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_showcaseScreen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(g_showcaseScreen, LV_OBJ_FLAG_SCROLLABLE);

    g_showcaseBar = new ui::StatusBar();
    g_showcaseBar->create(g_showcaseScreen);
    g_showcaseBar->setTitle("Math Showcase");
    g_showcaseBar->setBatteryLevel(100);

    const int barH = ui::StatusBar::HEIGHT + 8;

    g_showcaseCanvas = new vpam::MathCanvas();
    g_showcaseCanvas->create(g_showcaseScreen);
    g_showcaseCanvas->setAutoHeightEnabled(false);
    lv_obj_set_pos(g_showcaseCanvas->obj(), 0, barH);
    lv_obj_set_size(g_showcaseCanvas->obj(), SCREEN_W, SCREEN_H - barH - 40);
    lv_obj_set_style_bg_opa(g_showcaseCanvas->obj(), LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_showcaseCanvas->obj(), 0, LV_PART_MAIN);

    // Pie de pagina limpio (texto, no overlay) con el caso actual.
    g_showcaseCaption = lv_label_create(g_showcaseScreen);
    lv_obj_set_style_text_font(g_showcaseCaption, &stix_math_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_showcaseCaption, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_width(g_showcaseCaption, SCREEN_W - 24);
    lv_label_set_long_mode(g_showcaseCaption, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(g_showcaseCaption, 12, SCREEN_H - 30);
}

static void showcaseLoad()
{
    showcaseBuild();
    g_showcaseIndex = 0;
    showcaseShow(0);
    lv_screen_load_anim(g_showcaseScreen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
}

static void showcaseEnd()
{
    if (g_showcaseCanvas) {
        g_showcaseCanvas->setExpression(nullptr, nullptr);
        g_showcaseCanvas->destroy();
        delete g_showcaseCanvas;
        g_showcaseCanvas = nullptr;
    }
    g_showcaseRoot.reset();
    if (g_showcaseBar) {
        g_showcaseBar->destroy();     // nulifica antes de borrar la pantalla padre
        delete g_showcaseBar;
        g_showcaseBar = nullptr;
    }
    if (g_showcaseScreen) {
        lv_obj_delete(g_showcaseScreen);
        g_showcaseScreen  = nullptr;
        g_showcaseCaption = nullptr;
    }
}

// ════════════════════════════════════════════════════════════════════════════
//   parseArgs / printUsage — opciones de auto-salida (smoke-tests / CI)
// ════════════════════════════════════════════════════════════════════════════
static void printUsage(const char* prog)
{
    std::printf(
        "Uso: %s [opciones]\n"
        "  --frames N       ejecuta N iteraciones del bucle y sale limpiamente\n"
        "  --run-for-ms N   ejecuta ~N ms (reloj real) y sale limpiamente\n"
        "  --scale N        escala de ventana entera 1..8 (def. %d)\n"
        "  --headless       sin ventana (SDL_VIDEODRIVER=dummy), util en CI\n"
        "  --quiet          silencia el log por-tecla/por-iteracion\n"
        "  --deterministic  tick sintetico de paso fijo (reproducible); usar con --frames\n"
        "  --step-ms N      ms virtuales por frame en --deterministic 1..1000 (def. %d)\n"
        "  --screenshot P   vuelca el frame final 320x240 a un PPM (P6) en la ruta P\n"
        "  --dump-frame P   alias de --screenshot\n"
        "  --script P       reproduce un script de entrada determinista (.numos) desde P\n"
        "  --help, -h       muestra esta ayuda y sale\n",
        prog, DEFAULT_WINDOW_SCALE, 16);
}

static bool parseArgs(int argc, char** argv, EmuOptions& opt)
{
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto needVal = [&](long& dst) {
            if (i + 1 < argc) dst = std::atol(argv[++i]);
            else std::fprintf(stderr, "[ARGS] %s requiere un valor\n", a);
        };
        auto needStr = [&](const char*& dst) {
            if (i + 1 < argc) dst = argv[++i];
            else std::fprintf(stderr, "[ARGS] %s requiere un valor\n", a);
        };
        if      (std::strcmp(a, "--frames") == 0)     needVal(opt.maxFrames);
        else if (std::strcmp(a, "--run-for-ms") == 0) needVal(opt.maxMs);
        else if (std::strcmp(a, "--scale") == 0) {
            long s = opt.windowScale; needVal(s);
            if (s >= 1 && s <= 8) opt.windowScale = static_cast<int>(s);
            else std::fprintf(stderr, "[ARGS] --scale fuera de rango (1..8)\n");
        }
        else if (std::strcmp(a, "--headless") == 0)   opt.headless = true;
        else if (std::strcmp(a, "--quiet") == 0)      opt.quiet = true;
        else if (std::strcmp(a, "--deterministic") == 0) opt.deterministic = true;
        else if (std::strcmp(a, "--step-ms") == 0) {
            long s = opt.stepMs; needVal(s);
            if (s >= 1 && s <= 1000) opt.stepMs = s;
            else std::fprintf(stderr, "[ARGS] --step-ms fuera de rango (1..1000)\n");
        }
        else if (std::strcmp(a, "--screenshot") == 0 ||
                 std::strcmp(a, "--dump-frame") == 0) needStr(opt.screenshotPath);
        else if (std::strcmp(a, "--script") == 0)     needStr(opt.scriptPath);
        else if (std::strcmp(a, "--help") == 0 ||
                 std::strcmp(a, "-h") == 0) { printUsage(argv[0]); return false; }
        else std::fprintf(stderr, "[ARGS] opcion desconocida: %s\n", a);
    }
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
//   saveScreenshotPPM — vuelca el framebuffer logico (g_lvBuf) a PPM (P6)
//
// Fuente: g_lvBuf, el buffer CPU de pantalla completa que LVGL compone en modo
// LV_DISPLAY_RENDER_MODE_FULL (siempre contiene el frame 320x240 actual). NO se
// lee la textura ni el renderer, por lo que funciona identico en --headless y
// con cualquier --scale (la captura es SIEMPRE la geometria logica 320x240, no
// la ventana escalada). Formato PPM P6: sin dependencias (cabecera ASCII + RGB
// crudo). Conversion RGB565 (little-endian host) -> RGB888 por pixel.
// ════════════════════════════════════════════════════════════════════════════
static bool saveScreenshotPPM(const char* path)
{
    std::FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "[SHOT] no se pudo abrir '%s' para escribir\n", path);
        return false;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    const uint16_t* px = reinterpret_cast<const uint16_t*>(g_lvBuf);
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) {
        const uint16_t p = px[i];
        uint8_t r = static_cast<uint8_t>((p >> 11) & 0x1F);  // 5 bits
        uint8_t g = static_cast<uint8_t>((p >>  5) & 0x3F);  // 6 bits
        uint8_t b = static_cast<uint8_t>( p        & 0x1F);  // 5 bits
        // Expansion a 8 bits replicando los bits altos (no perder rango).
        r = static_cast<uint8_t>((r << 3) | (r >> 2));
        g = static_cast<uint8_t>((g << 2) | (g >> 4));
        b = static_cast<uint8_t>((b << 3) | (b >> 2));
        const uint8_t rgb[3] = { r, g, b };
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
//   Replay de scripts de entrada determinista (Phase 4A) — SOLO emulador
//
// Formato linea-a-linea (.numos), trozeado por espacios. Comandos:
//   # comentario        · lineas en blanco se ignoran
//   wait N              · espera N frames del emulador (N >= 0)
//   key NOMBRE          · inyecta PRESS+RELEASE por la MISMA ruta que SDL
//   keydown NOMBRE      · solo PRESS
//   keyup NOMBRE        · solo RELEASE
//   screenshot RUTA     · vuelca un PPM tras el render de ESTE frame
//   log "mensaje"       · imprime un mensaje a stdout
//   open_app NOMBRE     · (Phase 5A) lanza una app por nombre via launchApp()
//                         (Calculation|Settings|MathShowcase) — sin navegar el grid
//
// Modelo de planificacion (determinista): UN comando por frame, ejecutado ANTES
// de processSdlEvents() para que la tecla sea visible al avance de tick y a
// lv_timer_handler() de ESE frame; `wait N` consume N frames; `screenshot`
// marca una captura diferida que se realiza DESPUES del render (g_lvBuf ya
// contiene el frame compuesto). El script se valida ENTERO al cargar (fail-fast,
// exit != 0) para no ejecutar a medias y capturar un estado erroneo.
// ════════════════════════════════════════════════════════════════════════════
enum class ScriptCmdType : uint8_t {
    Wait, Key, KeyDown, KeyUp, Screenshot, Log,
    OpenApp,               // open_app NAME  (Phase 5A: lanza app por nombre)
    // Phase 4B-C: aserciones semanticas (sin OCR, sin pixeles). Comparan el
    // estado de la app activa / el resultado calculado por CalculationApp.
    AssertApp,             // assert_app NAME   (NAME canonico en strArg)
    AssertResult,          // assert_result TEXT          (igualdad exacta)
    AssertResultContains,  // assert_result_contains TEXT (subcadena)
    AssertNoError          // assert_no_error             (resultado sin error)
};

struct ScriptCmd {
    ScriptCmdType type;
    KeyCode       key   = KeyCode::NONE;  // Key/KeyDown/KeyUp
    long          waitN = 0;              // Wait (frames) / OpenApp (id resuelto)
    std::string   strArg;                 // Screenshot (ruta) / Log (mensaje) /
                                          // Assert* (texto esperado / app canonica) /
                                          // OpenApp (nombre canonico, para el log)
    int           line  = 0;              // linea de origen (diagnostico)
};

static std::vector<ScriptCmd> g_script;
static size_t      g_scriptPC      = 0;
static long        g_scriptWait    = 0;      // frames restantes de espera
static bool        g_scriptActive  = false;
static bool        g_scriptDone    = false;
static bool        g_pendingShot   = false;  // hay screenshot diferido este frame
static std::string g_pendingShotPath;
static int         g_exitCode      = 0;      // codigo de salida del proceso

static bool scriptErr(const char* path, int line, const char* msg)
{
    std::fprintf(stderr, "[SCRIPT] %s:%d: %s\n", path, line, msg);
    return false;
}

// Acepta solo enteros decimales >= 0 (rechaza '-', vacio y no-digitos).
static bool parseNonNegLong(const std::string& s, long& out)
{
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    out = std::strtol(s.c_str(), nullptr, 10);
    return out >= 0;
}

// Phase 4B-C: traduce el NOMBRE de app de un `assert_app` a su forma canonica.
// Case-insensitive; acepta alias amigables. Devuelve nullptr si no se reconoce
// (error de parseo -> el script falla al cargar con exit 2).
static const char* canonicalAppName(const std::string& name)
{
    std::string lc = name;
    for (char& c : lc) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lc == "calculation" || lc == "calc")     return "Calculation";
    if (lc == "menu"        || lc == "launcher") return "Menu";
    if (lc == "splash")                           return "Splash";
    if (lc == "settings")                         return "Settings";        // Phase 5A
    if (lc == "mathshowcase" || lc == "math_showcase" ||
        lc == "showcase")                         return "MathShowcase";    // Phase 5A
    if (lc == "statistics" || lc == "stats")      return "Statistics";      // Phase 6A
    if (lc == "probability" || lc == "prob")      return "Probability";     // Phase 6A
    if (lc == "sequences" || lc == "seq")         return "Sequences";       // Phase 7A
    if (lc == "regression" || lc == "reg")        return "Regression";      // Phase 7C
    return nullptr;
}

// Phase 5A: traduce el NOMBRE de app de `open_app` a su id de launchApp(). Acepta
// los mismos alias amigables que canonicalAppName(). Devuelve -1 si no se reconoce
// (error de parseo -> el script falla al cargar con exit 2). Solo apps cableadas
// en el emulador son lanzables.
static int scriptAppNameToId(const std::string& name)
{
    std::string lc = name;
    for (char& c : lc) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lc == "calculation" || lc == "calc")                          return 0;
    if (lc == "statistics" || lc == "stats")                          return 4;   // Phase 6A
    if (lc == "probability" || lc == "prob")                          return 5;   // Phase 6A
    if (lc == "sequences" || lc == "seq")                             return 7;   // Phase 7A
    if (lc == "regression" || lc == "reg")                            return 6;   // Phase 7C
    if (lc == "settings")                                             return 10;
    if (lc == "mathshowcase" || lc == "math_showcase" || lc == "showcase")
                                                                      return APPID_MATH_SHOWCASE;
    return -1;
}

// Carga y VALIDA el script entero. Devuelve false (sin ejecutar nada) ante el
// primer error, para que main() salga con codigo != 0.
static bool loadScript(const char* path)
{
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "[SCRIPT] no se pudo abrir el script '%s'\n", path);
        return false;
    }

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        // CRLF (scripts de Windows en runners Linux): quitar el CR final.
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd))      continue;   // linea en blanco
        if (cmd[0] == '#')      continue;   // comentario

        std::string lc = cmd;
        for (char& c : lc) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        ScriptCmd sc;
        sc.line = lineNo;

        if (lc == "wait") {
            std::string arg, extra;
            if (!(iss >> arg))  return scriptErr(path, lineNo, "wait requiere un numero de frames");
            if (iss >> extra)   return scriptErr(path, lineNo, "wait: demasiados argumentos");
            long n;
            if (!parseNonNegLong(arg, n)) return scriptErr(path, lineNo, "wait espera un entero >= 0");
            sc.type  = ScriptCmdType::Wait;
            sc.waitN = n;
        }
        else if (lc == "key" || lc == "keydown" || lc == "keyup") {
            std::string name, extra;
            if (!(iss >> name)) return scriptErr(path, lineNo, "key/keydown/keyup requieren un NOMBRE de tecla");
            if (iss >> extra)   return scriptErr(path, lineNo, "demasiados argumentos para una tecla");
            KeyCode kc = scriptNameToKeyCode(name);
            if (kc == KeyCode::NONE) return scriptErr(path, lineNo, "nombre de tecla desconocido");
            sc.type = (lc == "key")     ? ScriptCmdType::Key
                    : (lc == "keydown") ? ScriptCmdType::KeyDown
                                        : ScriptCmdType::KeyUp;
            sc.key  = kc;
        }
        else if (lc == "screenshot") {
            std::string p, extra;
            if (!(iss >> p))    return scriptErr(path, lineNo, "screenshot requiere una ruta");
            if (iss >> extra)   return scriptErr(path, lineNo, "screenshot: la ruta no puede contener espacios");
            sc.type   = ScriptCmdType::Screenshot;
            sc.strArg = p;
        }
        else if (lc == "log") {
            // Resto de la linea, recortando espacios y comillas envolventes.
            std::string rest;
            std::getline(iss, rest);
            size_t b = rest.find_first_not_of(" \t");
            rest = (b == std::string::npos) ? std::string() : rest.substr(b);
            if (rest.size() >= 2 && rest.front() == '"' && rest.back() == '"')
                rest = rest.substr(1, rest.size() - 2);
            sc.type   = ScriptCmdType::Log;
            sc.strArg = rest;
        }
        else if (lc == "open_app") {
            std::string name, extra;
            if (!(iss >> name)) return scriptErr(path, lineNo, "open_app requiere un NOMBRE de app");
            if (iss >> extra)   return scriptErr(path, lineNo, "open_app: demasiados argumentos");
            int id = scriptAppNameToId(name);
            if (id < 0) return scriptErr(path, lineNo,
                                         "open_app: app no lanzable (Calculation|Statistics|Probability|Sequences|Regression|Settings|MathShowcase)");
            sc.type   = ScriptCmdType::OpenApp;
            sc.waitN  = id;
            const char* canon = canonicalAppName(name);
            sc.strArg = canon ? canon : name;
        }
        else if (lc == "assert_app") {
            std::string name, extra;
            if (!(iss >> name)) return scriptErr(path, lineNo, "assert_app requiere un NOMBRE de app");
            if (iss >> extra)   return scriptErr(path, lineNo, "assert_app: demasiados argumentos");
            const char* canon = canonicalAppName(name);
            if (!canon) return scriptErr(path, lineNo,
                                         "assert_app: app desconocida (Calculation|Menu|Splash|Statistics|Probability|Sequences|Regression|Settings|MathShowcase)");
            sc.type   = ScriptCmdType::AssertApp;
            sc.strArg = canon;
        }
        else if (lc == "assert_result" || lc == "assert_result_contains") {
            // Resto de la linea (texto esperado): recorta espacios y comillas.
            std::string rest;
            std::getline(iss, rest);
            size_t b = rest.find_first_not_of(" \t");
            rest = (b == std::string::npos) ? std::string() : rest.substr(b);
            size_t e = rest.find_last_not_of(" \t");
            if (e != std::string::npos) rest = rest.substr(0, e + 1);
            if (rest.size() >= 2 && rest.front() == '"' && rest.back() == '"')
                rest = rest.substr(1, rest.size() - 2);
            if (rest.empty()) return scriptErr(path, lineNo, "assert_result requiere el texto esperado");
            sc.type   = (lc == "assert_result") ? ScriptCmdType::AssertResult
                                                : ScriptCmdType::AssertResultContains;
            sc.strArg = rest;
        }
        else if (lc == "assert_no_error") {
            std::string extra;
            if (iss >> extra) return scriptErr(path, lineNo, "assert_no_error no acepta argumentos");
            sc.type = ScriptCmdType::AssertNoError;
        }
        else {
            return scriptErr(path, lineNo, "comando desconocido");
        }

        g_script.push_back(std::move(sc));
    }

    g_scriptActive = !g_script.empty();
    std::printf("[SCRIPT] cargado '%s': %zu comandos\n", path, g_script.size());
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// Phase 4B-C: aserciones semanticas (sin OCR, sin pixeles).
// ════════════════════════════════════════════════════════════════════════════

// Forma textual canonica del resultado de CalculationApp. Lee el ExactVal ya
// calculado (no toca el render) y lo formatea. Es EXACTA para los casos que
// asertamos —entero y fraccion pura, p.ej. "3" y "5/6"— y best-effort (forma
// lineal determinista) para radicales/π/e, cuya asercion queda fuera del
// alcance de esta fase.
static std::string formatExactVal(const vpam::ExactVal& v)
{
    if (!v.ok)         return v.error.empty() ? std::string("ERROR") : v.error;
    if (v.approximate) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.10g", v.approxVal);
        return std::string(buf);
    }

    auto frac = [](long long n, long long d) -> std::string {
        char buf[48];
        if (d == 1) std::snprintf(buf, sizeof(buf), "%lld", n);
        else        std::snprintf(buf, sizeof(buf), "%lld/%lld", n, d);
        return std::string(buf);
    };

    // Entero o fraccion pura (den>0): el caso comun y el unico que asertamos.
    if (v.inner == 1 && v.piMul == 0 && v.eMul == 0)
        return frac(static_cast<long long>(v.num), static_cast<long long>(v.den));

    // Radicales / π / e: forma lineal best-effort.
    std::string s = frac(static_cast<long long>(v.num), static_cast<long long>(v.den));
    if (v.inner > 1) {
        s += "*";
        if (v.outer != 1) { s += std::to_string(static_cast<long long>(v.outer)); s += "*"; }
        s += "sqrt(" + std::to_string(static_cast<long long>(v.inner)) + ")";
    }
    if (v.piMul != 0) { s += "*pi"; if (v.piMul != 1) s += "^" + std::to_string(static_cast<int>(v.piMul)); }
    if (v.eMul  != 0) { s += "*e";  if (v.eMul  != 1) s += "^" + std::to_string(static_cast<int>(v.eMul));  }
    return s;
}

// Nombre canonico de la app activa (coherente con canonicalAppName()).
static const char* activeAppName()
{
    return (g_mode == AppMode::SPLASH)        ? "Splash"
         : (g_mode == AppMode::MENU)          ? "Menu"
         : (g_mode == AppMode::SETTINGS)      ? "Settings"
         : (g_mode == AppMode::MATH_SHOWCASE) ? "MathShowcase"
         : (g_mode == AppMode::STATISTICS)    ? "Statistics"
         : (g_mode == AppMode::PROBABILITY)   ? "Probability"
         : (g_mode == AppMode::SEQUENCES)     ? "Sequences"
         : (g_mode == AppMode::REGRESSION)    ? "Regression"
                                              : "Calculation";
}

// Diagnostico de asercion: SIEMPRE se imprime (independiente de --quiet, que
// solo silencia ruido por-frame). Un FAIL marca exit 4 y detiene el replay.
static void assertFail(int line, const std::string& msg)
{
    std::fprintf(stderr, "[ASSERT] %s:%d: FAIL - %s\n",
                 g_opts.scriptPath ? g_opts.scriptPath : "<script>", line, msg.c_str());
    g_exitCode = 4;
    g_quit     = true;
}
static void assertPass(int line, const std::string& msg)
{
    std::printf("[ASSERT] %s:%d: PASS - %s\n",
                g_opts.scriptPath ? g_opts.scriptPath : "<script>", line, msg.c_str());
}

// Procesa el comando de script de ESTE frame. Se llama AL INICIO del bucle,
// antes de processSdlEvents(), avance de tick y lv_timer_handler().
static void scriptStepBegin()
{
    if (!g_scriptActive || g_scriptDone)   return;
    if (g_scriptWait > 0) { --g_scriptWait; return; }
    if (g_scriptPC >= g_script.size()) { g_scriptDone = true; return; }

    const ScriptCmd& sc = g_script[g_scriptPC++];
    switch (sc.type) {
        case ScriptCmdType::Wait:
            g_scriptWait = sc.waitN;     // 0 => el siguiente comando corre el proximo frame
            break;
        case ScriptCmdType::Key:
            dispatchKey(sc.key, KeyAction::PRESS,   true);
            dispatchKey(sc.key, KeyAction::RELEASE, false);
            break;
        case ScriptCmdType::KeyDown:
            dispatchKey(sc.key, KeyAction::PRESS, true);
            break;
        case ScriptCmdType::KeyUp:
            dispatchKey(sc.key, KeyAction::RELEASE, false);
            break;
        case ScriptCmdType::Screenshot:
            g_pendingShot     = true;
            g_pendingShotPath = sc.strArg;   // captura tras el render de este frame
            break;
        case ScriptCmdType::Log:
            std::printf("[SCRIPT] %s\n", sc.strArg.c_str());
            break;

        // ── Phase 5A: lanzar app por nombre (misma ruta que el launcher) ──
        case ScriptCmdType::OpenApp:
            launchApp(static_cast<int>(sc.waitN));
            break;

        // ── Phase 4B-C: aserciones semanticas ───────────────────────────
        case ScriptCmdType::AssertApp: {
            const char* cur = activeAppName();
            if (sc.strArg == cur)
                assertPass(sc.line, "assert_app " + sc.strArg);
            else
                assertFail(sc.line, "assert_app esperaba '" + sc.strArg +
                                    "' pero la app activa es '" + cur + "'");
            break;
        }
        case ScriptCmdType::AssertResult:
        case ScriptCmdType::AssertResultContains: {
            if (g_mode != AppMode::CALCULATION || !g_calcApp) {
                assertFail(sc.line, "assert_result requiere CalculationApp activa (app actual: '" +
                                    std::string(activeAppName()) + "')");
                break;
            }
            if (!g_calcApp->debugHasResult()) {
                assertFail(sc.line, "assert_result: no hay resultado evaluado "
                                    "(pulsa ENTER y deja asentar con `wait` antes de asertar)");
                break;
            }
            const std::string actual = formatExactVal(g_calcApp->debugLastResult());
            const bool ok = (sc.type == ScriptCmdType::AssertResult)
                          ? (actual == sc.strArg)
                          : (actual.find(sc.strArg) != std::string::npos);
            const char* verb = (sc.type == ScriptCmdType::AssertResult) ? "==" : "contiene";
            if (ok)
                assertPass(sc.line, "assert_result " + std::string(verb) + " '" +
                                    sc.strArg + "' (actual='" + actual + "')");
            else
                assertFail(sc.line, "assert_result esperaba (" + std::string(verb) + ") '" +
                                    sc.strArg + "' pero actual='" + actual + "'");
            break;
        }
        case ScriptCmdType::AssertNoError: {
            if (g_mode == AppMode::CALCULATION && g_calcApp && g_calcApp->debugHasResult() &&
                !g_calcApp->debugLastResult().ok) {
                assertFail(sc.line, "assert_no_error: el resultado tiene error '" +
                                    g_calcApp->debugLastResult().error + "'");
            } else {
                assertPass(sc.line, "assert_no_error");
            }
            break;
        }
    }
}

// Realiza la captura diferida por `screenshot`. Se llama DESPUES del render.
// Un fallo de escritura es fatal (exit != 0): un screenshot perdido invalida
// el proposito del replay determinista.
static void scriptCaptureIfPending()
{
    if (!g_pendingShot) return;
    g_pendingShot = false;

    if (!saveScreenshotPPM(g_pendingShotPath.c_str())) {
        std::fprintf(stderr, "[SCRIPT] fallo al escribir screenshot '%s'\n",
                     g_pendingShotPath.c_str());
        g_exitCode = 3;
        g_quit     = true;
        return;
    }
    if (!g_opts.quiet) {
        std::printf("[SHOT] PPM %dx%d escrito (script): %s\n",
                    SCREEN_W, SCREEN_H, g_pendingShotPath.c_str());
    }
}

// ════════════════════════════════════════════════════════════════════════════
//   main() — Punto de entrada para la simulación nativa en PC
// ════════════════════════════════════════════════════════════════════════════
int main(int argc, char** argv)
{
    if (!parseArgs(argc, argv, g_opts)) return 0;   // --help → salida limpia

    // Phase 4A: cargar y validar el script de entrada ANTES de inicializar SDL.
    // Un script malformado falla rapido (exit 2) sin tocar SDL/LVGL.
    if (g_opts.scriptPath) {
        if (!loadScript(g_opts.scriptPath)) return 2;
    }

    // Headless: fijar el driver "dummy" ANTES de SDL_Init para que el video y
    // la ventana funcionen sin display (CI/Linux sin X/Wayland). El usuario
    // tambien puede exportar SDL_VIDEODRIVER=dummy; SDL lo respeta de serie.
    if (g_opts.headless) {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        std::printf("[SIM] headless: SDL_VIDEODRIVER=dummy\n");
    }

    std::printf("╔═══════════════════════════════════════╗\n");
    std::printf("║   NumOS Simulator  (PC / SDL2)        ║\n");
    std::printf("║   320×240  RGB565  —  LVGL 9.x        ║\n");
    std::printf("╚═══════════════════════════════════════╝\n\n");

    // ── 1. Inicializar SDL2 ─────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        std::fprintf(stderr, "[ERROR] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Calidad de escalado: 0 = nearest-neighbor (pixeles nitidos al escalar ×N).
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    g_window = SDL_CreateWindow(
        "NumOS Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W * g_opts.windowScale, SCREEN_H * g_opts.windowScale,
        SDL_WINDOW_SHOWN
    );
    if (!g_window) {
        std::fprintf(stderr, "[ERROR] SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1,
                                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        // Fallback a software renderer
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        std::fprintf(stderr, "[ERROR] SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

    // Texture RGB565 para el framebuffer de LVGL
    g_texture = SDL_CreateTexture(
        g_renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H
    );
    if (!g_texture) {
        std::fprintf(stderr, "[ERROR] SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(g_renderer);
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

    // ── Coordenadas logicas 320×240 + escalado entero nitido ────────────────
    // logical size fija el sistema de coordenadas del renderer a 320×240
    // (identico al firmware); SDL escala a la ventana. integer scale evita
    // medias muestras → pixeles nitidos en ×2/×3/×4. Esto es puro escalado de
    // SALIDA: la geometria del renderizador de formulas NO se toca.
    SDL_RenderSetLogicalSize(g_renderer, SCREEN_W, SCREEN_H);
    SDL_RenderSetIntegerScale(g_renderer, SDL_TRUE);

    std::printf("[SDL2] Ventana %dx%d (escala x%d) creada OK\n",
                SCREEN_W, SCREEN_H, g_opts.windowScale);

    // ── Log determinista de geometria / escala / backend (diagnostico) ──────
    {
        int   winW = 0, winH = 0, outW = 0, outH = 0, logW = 0, logH = 0;
        float sx = 1.0f, sy = 1.0f;
        SDL_GetWindowSize(g_window, &winW, &winH);
        SDL_GetRendererOutputSize(g_renderer, &outW, &outH);
        SDL_RenderGetLogicalSize(g_renderer, &logW, &logH);
        SDL_RenderGetScale(g_renderer, &sx, &sy);
        SDL_RendererInfo info;
        const bool haveInfo = (SDL_GetRendererInfo(g_renderer, &info) == 0);
        std::printf("[SCALE] logical=%dx%d window=%dx%d output=%dx%d "
                    "scale=%.2fx%.2f integer=%s backend=%s(%s)\n",
                    logW, logH, winW, winH, outW, outH, sx, sy,
                    SDL_RenderGetIntegerScale(g_renderer) ? "on" : "off",
                    haveInfo ? info.name : "?",
                    (haveInfo && (info.flags & SDL_RENDERER_ACCELERATED))
                        ? "accel" : "software");
    }

    // ── 2. Inicializar LVGL ─────────────────────────────────────────────
    lv_init();

    // UNICA fuente de reloj de LVGL en el build nativo: SDL_GetTicks (ms de
    // pared). En LVGL 9.x el tick se fija EXCLUSIVAMENTE por lv_tick_set_cb;
    // el `LV_TICK_CUSTOM` de lv_conf.h es un mecanismo de LVGL 8.x y queda
    // INERTE en 9.x (verificado: no aparece en el core lv_tick.c instalado),
    // por lo que NO hay doble-tick. NO eliminar esta llamada: sin ella las
    // animaciones/timers se congelan. (lv_conf.h es compartido con el firmware
    // y no se toca; el macro muerto es inofensivo.)
    // Phase 3B: en --deterministic el tick es un contador sintetico de paso fijo
    // (detTickCb) que el bucle avanza por frame; por defecto sigue siendo el
    // reloj de pared (SDL_GetTicks), de modo que el uso interactivo no cambia.
    lv_tick_set_cb(g_opts.deterministic ? detTickCb : SDL_GetTicks);
    std::printf("[LVGL] lv_init() OK — tick = %s\n",
                g_opts.deterministic
                    ? "determinista (paso fijo por frame)"
                    : "SDL_GetTicks() (reloj de pared)");
    if (g_opts.deterministic) {
        std::printf("[LVGL] modo determinista: %ld ms virtuales por frame\n",
                    g_opts.stepMs);
    }

    // Crear el display LVGL con flush a SDL
    lv_display_t* disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_buffers(disp, g_lvBuf, nullptr, sizeof(g_lvBuf),
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, sdl_flush_cb);
    std::printf("[LVGL] Display driver registrado (%dx%d)\n", SCREEN_W, SCREEN_H);

    // ── 3. Driver de teclado LVGL ───────────────────────────────────────
    LvglKeypad::init();
    std::printf("[LVGL] Keypad indev registrado\n");

    // ── 4. Inicializar filesystem emulado ───────────────────────────────
    {
        extern bool nativeFS_init();
        nativeFS_init();
    }

    // ── 5. SplashScreen (animación fade-in real) ────────────────────────
    g_splash = new SplashScreen();
    g_splash->create();
    g_splash->show(onSplashDone);
    g_mode = AppMode::SPLASH;
    std::printf("[SPLASH] Animacion iniciada (~2 s)\n");

    std::printf("\n[SIM] Simulador corriendo. Cierra la ventana o Ctrl+C para salir.\n");
    std::printf("[SIM] Teclado: 0-9, +-*/, Enter, ESC=AC, Backspace=DEL, Flechas\n");
    std::printf("[SIM]          s=SIN c=COS t=TAN l=LN g=LOG r=SQRT p=PI e=E\n");
    std::printf("[SIM]          Tab=ALPHA  Shift=SHIFT  Insert=STO\n");
    std::printf("[SIM]          m/Home = volver al launcher  ·  --help para flags\n\n");

    // ── 6. Bucle principal ──────────────────────────────────────────────
    // Politica de temporizacion: UNA sola fuente de tick (SDL_GetTicks via
    // lv_tick_set_cb), UN solo punto de sleep (FRAME_DELAY_MS) y presentacion
    // diferida tras lv_timer_handler(). Sin busy-spin. El modo auto-salida
    // (--frames/--run-for-ms) solo añade una comprobacion de fin al final.
    uint32_t       loopCount  = 0;
    const uint32_t startTicks = SDL_GetTicks();   // referencia para --run-for-ms
    while (!g_quit) {
        // Phase 4A: inyecta el comando de script de este frame ANTES de la
        // entrada SDL, para que la tecla sea visible al tick y a
        // lv_timer_handler() de ESTE mismo frame. Inerte sin --script.
        scriptStepBegin();

        processSdlEvents();

        // Transición diferida Splash → Menú (fuera del contexto LVGL)
        if (g_splashDone && g_mode == AppMode::SPLASH) {
            g_splashDone = false;
            transitionToMenu();
        }

        // Phase 3B: en modo determinista avanzamos el reloj sintetico un paso
        // fijo ANTES de lv_timer_handler(), de modo que animaciones/timers son
        // funcion del indice de frame (reproducible). Inerte en modo normal.
        if (g_opts.deterministic) {
            g_detTick += static_cast<uint32_t>(g_opts.stepMs);
        }

        lv_timer_handler();

        // Presentar el frame en pantalla (fuera de lv_timer_handler)
        if (g_needsPresent) {
            g_needsPresent = false;
            SDL_RenderClear(g_renderer);
            SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
            SDL_RenderPresent(g_renderer);
        }

        // Phase 4A: captura diferida pedida por el script, DESPUES del render
        // (g_lvBuf ya contiene el frame compuesto por lv_timer_handler).
        scriptCaptureIfPending();

        // En modo determinista NO dormimos: el tick es sintetico, así que el
        // ritmo de pared es irrelevante y conviene terminar rapido (CI). En uso
        // interactivo cedemos CPU como siempre (~200 fps techo).
        if (!g_opts.deterministic) {
            SDL_Delay(FRAME_DELAY_MS);   // cede CPU (ver politica de temporizacion)
        }

        ++loopCount;

        // Heartbeat de depuracion (silenciable con --quiet).
        if (!g_opts.quiet && loopCount % 200 == 0) {
            const char* modeStr = (g_mode == AppMode::SPLASH) ? "SPLASH"
                                : (g_mode == AppMode::MENU)   ? "MENU"
                                                              : "CALC";
            std::printf("[LOOP] iter=%u mode=%s\n", loopCount, modeStr);
        }

        // ── Auto-salida (smoke-tests / CI) — inerte en uso interactivo ──────
        if (g_opts.maxFrames >= 0 && static_cast<long>(loopCount) >= g_opts.maxFrames) {
            std::printf("[SIM] auto-exit: %ld frames alcanzados\n", g_opts.maxFrames);
            g_quit = true;
        }
        if (g_opts.maxMs >= 0 &&
            static_cast<long>(SDL_GetTicks() - startTicks) >= g_opts.maxMs) {
            std::printf("[SIM] auto-exit: %ld ms alcanzados\n", g_opts.maxMs);
            g_quit = true;
        }
    }

    // ── Screenshot opcional (Phase 3B) ──────────────────────────────────
    // Tras el ultimo frame y ANTES del teardown: g_lvBuf aun contiene la imagen
    // logica 320x240 final. Solo se activa con --screenshot/--dump-frame.
    if (g_opts.screenshotPath) {
        if (saveScreenshotPPM(g_opts.screenshotPath)) {
            std::printf("[SHOT] PPM %dx%d escrito: %s\n",
                        SCREEN_W, SCREEN_H, g_opts.screenshotPath);
        }
    }

    // ── 7. Cleanup ──────────────────────────────────────────────────────
    std::printf("\n[SIM] Cerrando...\n");
    if (g_calcApp) { g_calcApp->end(); delete g_calcApp; }
    if (g_settingsApp) { g_settingsApp->end(); delete g_settingsApp; }  // Phase 5A
    if (g_statsApp) { g_statsApp->end(); delete g_statsApp; }           // Phase 6A
    if (g_probApp)  { g_probApp->end();  delete g_probApp;  }           // Phase 6A
    if (g_seqApp)   { g_seqApp->end();   delete g_seqApp;   }           // Phase 7A
    if (g_regApp)   { g_regApp->end();   delete g_regApp;   }           // Phase 7C
    showcaseEnd();                                                      // Phase 5A (no-op si inactiva)
    delete g_menu;
    delete g_splash;

    // Liberar LVGL por completo (objetos, displays, indev). Importante para
    // ejecuciones headless repetidas (CI) y para no dejar fugas al salir.
    lv_deinit();

    SDL_DestroyTexture(g_texture);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    std::printf("[SIM] Bye!\n");
    return g_exitCode;
}

// ── Helper para FileSystem init (llamado arriba) ────────────────────────────
#include "FileSystem.h"
#include "../math/VariableManager.h"

bool nativeFS_init() {
    if (LittleFS.begin(true)) {
        vpam::VariableManager::instance().loadFromFlash();
        std::printf("[FS] emulator_data/ OK, variables cargadas\n");
        return true;
    }
    std::printf("[FS] emulator_data/ FAIL\n");
    return false;
}

#endif // NATIVE_SIM
