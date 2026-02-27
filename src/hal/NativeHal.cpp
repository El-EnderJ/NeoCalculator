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
 */

#ifdef NATIVE_SIM

#include <SDL2/SDL.h>
#include <lvgl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../input/KeyCodes.h"
#include "../input/LvglKeypad.h"
#include "../input/KeyboardManager.h"
#include "../apps/CalculationApp.h"
#include "../display/DisplayDriver.h"
#include "../ui/SplashScreen.h"
#include "../ui/MainMenu.h"

// ════════════════════════════════════════════════════════════════════════════
// DisplayDriver stubs (MainMenu almacena una referencia pero nunca la usa)
// ════════════════════════════════════════════════════════════════════════════
DisplayDriver::DisplayDriver() {}
void DisplayDriver::begin() {}
void DisplayDriver::initLvgl(void*, void*, uint32_t) {}
void DisplayDriver::lvglFlushCb(lv_display_t*, const lv_area_t*, uint8_t*) {}

// ════════════════════════════════════════════════════════════════════════════
// Constantes
// ════════════════════════════════════════════════════════════════════════════
static constexpr int SCREEN_W     = 320;
static constexpr int SCREEN_H     = 240;
static constexpr int WINDOW_SCALE = 2;     // ×2 para visibilidad en PC

// ════════════════════════════════════════════════════════════════════════════
// Modos de la aplicación
// ════════════════════════════════════════════════════════════════════════════
enum class AppMode : uint8_t {
    SPLASH,         // Pantalla de carga con animación
    MENU,           // Launcher (grid de apps)
    CALCULATION     // Calculadora científica
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

// Buffer de LVGL (pantalla completa, RGB565)
// 320×240 × 2 bytes = 153 600 bytes → trivial en PC
static uint8_t g_lvBuf[SCREEN_W * SCREEN_H * sizeof(uint16_t)];

// Forward declarations
static void launchApp(int appId);
static void returnToMenu();
static void onSplashDone();

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

        // Solo procesar teclas (KEYDOWN)
        if (ev.type != SDL_KEYDOWN) continue;

        KeyCode kc = mapSdlToKeyCode(ev.key.keysym.sym);
        if (kc == KeyCode::NONE) continue;

        KeyAction action = (ev.key.repeat == 0) ? KeyAction::PRESS
                                                 : KeyAction::REPEAT;

        // ── Debug: mostrar cada tecla detectada ──
        const char* modeStr = (g_mode == AppMode::SPLASH) ? "SPLASH"
                            : (g_mode == AppMode::MENU)   ? "MENU"
                                                          : "CALC";
        std::printf("[KEY] SDL=%s code=%d action=%s mode=%s\n",
                    SDL_GetKeyName(ev.key.keysym.sym),
                    static_cast<int>(kc),
                    (action == KeyAction::PRESS) ? "PRESS" : "REPEAT",
                    modeStr);

        switch (g_mode) {
            case AppMode::SPLASH:
                // Ignorar teclas durante la animación del splash
                break;

            case AppMode::MENU:
                // Todas las teclas van a LvglKeypad para que LVGL gestione
                // la navegación del grid (flechas + ENTER → click en tarjeta)
                LvglKeypad::pushKey(kc, true);    // PRESS
                LvglKeypad::pushKey(kc, false);   // RELEASE (dispara CLICKED)
                break;

            case AppMode::CALCULATION:
                // MODE → volver al launcher
                if (kc == KeyCode::MODE) {
                    returnToMenu();
                    break;
                }
                // Todas las demás teclas → CalculationApp directamente
                {
                    KeyEvent ke;
                    ke.code   = kc;
                    ke.action = action;
                    ke.row    = -1;
                    ke.col    = -1;
                    std::printf("[CALC] handleKey(code=%d)\n",
                                static_cast<int>(kc));
                    g_calcApp->handleKey(ke);
                }
                break;
        }
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

    if (g_calcApp) {
        g_calcApp->end();
        g_calcApp->begin();   // Pre-crear para la próxima vez
    }

    // Resetear modificadores de teclado (SHIFT/ALPHA/STO)
    vpam::KeyboardManager::instance().reset();

    g_menu->load();
    lv_indev_set_group(LvglKeypad::indev(), g_menu->group());
    g_mode = AppMode::MENU;
    std::printf("[MENU] Launcher restaurado\n");
}

// ════════════════════════════════════════════════════════════════════════════
//   main() — Punto de entrada para la simulación nativa en PC
// ════════════════════════════════════════════════════════════════════════════
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::printf("╔═══════════════════════════════════════╗\n");
    std::printf("║   NumOS Simulator  (PC / SDL2)        ║\n");
    std::printf("║   320×240  RGB565  —  LVGL 9.x        ║\n");
    std::printf("╚═══════════════════════════════════════╝\n\n");

    // ── 1. Inicializar SDL2 ─────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        std::fprintf(stderr, "[ERROR] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    g_window = SDL_CreateWindow(
        "NumOS Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W * WINDOW_SCALE, SCREEN_H * WINDOW_SCALE,
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

    std::printf("[SDL2] Ventana %dx%d (escala x%d) creada OK\n",
                SCREEN_W, SCREEN_H, WINDOW_SCALE);

    // ── 2. Inicializar LVGL ─────────────────────────────────────────────
    lv_init();

    // Registrar SDL_GetTicks como fuente de reloj para LVGL.
    // Esto es IMPRESCINDIBLE: sin tick, las animaciones y timers de LVGL
    // no avanzan (la pantalla se congela).
    lv_tick_set_cb(SDL_GetTicks);
    std::printf("[LVGL] lv_init() OK — tick = SDL_GetTicks()\n");

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
    std::printf("[SIM]          m/Home = volver al launcher\n\n");

    // ── 6. Bucle principal ──────────────────────────────────────────────
    uint32_t loopCount = 0;
    while (!g_quit) {
        processSdlEvents();

        // Transición diferida Splash → Menú (fuera del contexto LVGL)
        if (g_splashDone && g_mode == AppMode::SPLASH) {
            g_splashDone = false;
            transitionToMenu();
        }

        lv_timer_handler();

        // Presentar el frame en pantalla (fuera de lv_timer_handler)
        if (g_needsPresent) {
            g_needsPresent = false;
            SDL_RenderClear(g_renderer);
            SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
            SDL_RenderPresent(g_renderer);
        }

        SDL_Delay(5);   // ~200 fps máximo, suficiente para la UI

        // Debug: confirmar que el bucle sigue vivo
        if (++loopCount % 200 == 0) {
            const char* modeStr = (g_mode == AppMode::SPLASH) ? "SPLASH"
                                : (g_mode == AppMode::MENU)   ? "MENU"
                                                              : "CALC";
            std::printf("[LOOP] iter=%u mode=%s\n", loopCount, modeStr);
        }
    }

    // ── 7. Cleanup ──────────────────────────────────────────────────────
    std::printf("\n[SIM] Cerrando...\n");
    if (g_calcApp) { g_calcApp->end(); delete g_calcApp; }
    delete g_menu;
    delete g_splash;

    SDL_DestroyTexture(g_texture);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    std::printf("[SIM] Bye!\n");
    return 0;
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
