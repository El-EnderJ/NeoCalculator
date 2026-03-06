/**
 * SystemApp.cpp
 * NumOS System Application — Full implementation.
 * 
 * All rendering goes directly to _display.tft() because
 * DisplayDriver has _useSprite=false (no full-screen sprite).
 */

#include "SystemApp.h"
#include "ui/Theme.h"
#include "ui/Icons.h"
#include "Config.h"
#include "math/VariableManager.h"
#include "input/KeyboardManager.h"

#ifdef ARDUINO
#include <esp_sleep.h>
#include <LittleFS.h>
#endif

// Definido en main.cpp. true = LVGL activo (modo MENU).
// false = app heredada corriendo directamente en TFT.
extern bool g_lvglActive;

// ═════════════════════════════════════════════════
// Constructor
// ═════════════════════════════════════════════════
SystemApp::SystemApp(DisplayDriver &display, Keyboard &keypad)
    : _display(display),
      _keypad(keypad),
      _mainMenu(display),
      _calcApp(nullptr),
      _grapherApp(nullptr),
      _equationsApp(nullptr),
      _calculusApp(nullptr),
      _settingsApp(nullptr),
      _statisticsApp(nullptr),
      _probabilityApp(nullptr),
      _regressionApp(nullptr),
      _matricesApp(nullptr),
      _tokenizer(),
      _parser(),
      _evaluator(),
      _vars(),
      _equationSolver(),
      _stepLogger(),
      _graphView(display),
      _shiftActive(false),
      _alphaActive(false),
      _angleMode(AngleMode::DEG),
      _mode(Mode::MENU),
      _hasSteps(false),
      _stepScroll(0),
      _selectedAppIndex(0),
      _menuScrollOffset(0),
      _redraw(true)
{
}

// ═════════════════════════════════════════════════
// begin() — Boot sequence
// ═════════════════════════════════════════════════
void SystemApp::begin() {
    _vars.begin();
    _evaluator.setAngleMode(_angleMode);
    _equationSolver.setAngleMode(_angleMode);
    _graphView.setAngleMode(_angleMode);

    // ── LittleFS: cargar variables persistidas ──
    if (LittleFS.begin(true)) {   // true = formatOnFail
        if (vpam::VariableManager::instance().loadFromFlash()) {
            Serial.println("[SYSTEM] LittleFS OK, variables loaded");
        } else {
            Serial.println("[SYSTEM] LittleFS OK, vars.dat not found (first boot)");
        }
    } else {
        Serial.println("[SYSTEM] LittleFS FAIL (continuing without persistence)");
    }

    // ── Create CalculationApp (LVGL-native VPAM) ──
    _calcApp = new CalculationApp();
    _calcApp->begin();

    // ── Create GrapherApp (LVGL-native 2.0) ──
    // Lazy-init: begin() se llama en load() la primera vez que el
    // usuario abre la app.  Llamarlo aqui durante el boot bloquea
    // el sistema porque LVGL no esta bombeando todavia.
    _grapherApp = new GrapherApp();

    // ── Create EquationsApp (LVGL-native CAS-Lite) ──
    _equationsApp = new EquationsApp();
    _equationsApp->begin();

    // ── Create CalculusApp (LVGL-native Derivatives + Integrals) ──
    _calculusApp = new CalculusApp();
    _calculusApp->begin();

    // ── Create SettingsApp (LVGL-native Settings) ──
    _settingsApp = new SettingsApp();
    _settingsApp->begin();

    // ── Create StatisticsApp (LVGL-native Statistics) ──
    _statisticsApp = new StatisticsApp();
    _statisticsApp->begin();

    // ── Create ProbabilityApp (LVGL-native Distributions) ──
    _probabilityApp = new ProbabilityApp();
    _probabilityApp->begin();

    // ── Create RegressionApp (LVGL-native Regression) ──
    _regressionApp = new RegressionApp();
    _regressionApp->begin();

    // ── Create MatricesApp (LVGL-native Matrices) ──
    _matricesApp = new MatricesApp();
    _matricesApp->begin();

    // ── Load apps & go to menu ──
    initApps();
    _mode = Mode::MENU;
    _selectedAppIndex = 0;
    _redraw = false;

    // ── LVGL Launcher ──
    // Registra el callback que LVGL llamará cuando el usuario pulse ENTER
    // sobre una card del grid (evento LV_EVENT_CLICKED).
    _mainMenu.setLaunchCallback([this](int id) { launchApp(id); });

    // Construye los widgets LVGL del launcher (llamar una sola vez).
    _mainMenu.create();

    // Conecta el indev del teclado físico al grupo de foco del launcher.
    lv_indev_set_group(LvglKeypad::indev(), _mainMenu.group());

    // Activa la pantalla del launcher (la hace visible).
    _mainMenu.load();
    g_lvglActive = true;
}

// ═════════════════════════════════════════════════
// initApps() — Populate the app grid (SINGLE definition)
// ═════════════════════════════════════════════════
void SystemApp::initApps() {
    _apps.clear();
    _apps.emplace_back(0, "Calculation",  icon_Calculation);
    _apps.emplace_back(1, "Grapher",      icon_Grapher);
    _apps.emplace_back(2, "Equations",    icon_Equations);
    _apps.emplace_back(3, "Calculus",     icon_Elements);
    _apps.emplace_back(4, "Statistics",   icon_Distributions);
    _apps.emplace_back(5, "Probability",  icon_Distributions);
    _apps.emplace_back(6, "Regression",   icon_Regression);
    _apps.emplace_back(7, "Sequence",     icon_Sequences);
    _apps.emplace_back(8, "Python",       icon_Python);
    _apps.emplace_back(9, "Settings",     icon_Settings);
}


// ═════════════════════════════════════════════════
// update() — Main loop tick (called every frame)
// ═════════════════════════════════════════════════
void SystemApp::update() {
    _keypad.update();

    KeyEvent ev;
    while (_keypad.pollEvent(ev)) {
        handleKey(ev);
    }

    // CalculationApp es ahora LVGL-native: LVGL maneja su renderizado
    // via lv_timer_handler() en main.cpp. No se llama render().
    if (_mode == Mode::APP_CALCULATION) {
        // LVGL handles CalculationApp rendering
    } else if (_mode == Mode::APP_CALCULUS) {
        // LVGL handles CalculusApp rendering
    } else if (_mode == Mode::APP_EQUATIONS) {
        // LVGL handles EquationsApp rendering
    } else if (_mode == Mode::APP_GRAPHER) {
        // LVGL handles GrapherApp rendering
    } else if (_mode == Mode::APP_SETTINGS) {
        // LVGL handles SettingsApp rendering
    } else if (_mode == Mode::APP_STATISTICS) {
        // LVGL handles StatisticsApp rendering
    } else if (_mode == Mode::APP_PROBABILITY) {
        // LVGL handles ProbabilityApp rendering
    } else if (_mode == Mode::MENU) {
        // LVGL maneja el renderizado del menú via lv_timer_handler() en main.cpp
        _redraw = false;
    } else if (_redraw) {
        render();
        _redraw = false;
    }
}

// ═════════════════════════════════════════════════
// injectKey() — External event injection (SerialBridge)
// ═════════════════════════════════════════════════
void SystemApp::injectKey(const KeyEvent &ev) {
    handleKey(ev);
}

// ═════════════════════════════════════════════════
// render() — Dispatch to active mode
// ═════════════════════════════════════════════════
void SystemApp::render() {
    switch (_mode) {
        case Mode::MENU:            /* LVGL maneja el menú — no-op */   break;
        case Mode::APP_CALCULATION: break;  // Handled in update() via _calcApp
        case Mode::APP_CALCULUS:    break;  // LVGL-native — no-op
        case Mode::APP_EQUATIONS:   break;  // LVGL-native — no-op
        case Mode::APP_SETTINGS:    break;  // LVGL-native — no-op
        case Mode::APP_STATISTICS:  break;  // LVGL-native — no-op
        case Mode::APP_PROBABILITY: break;  // LVGL-native — no-op
        case Mode::APP_REGRESSION:  break;  // LVGL-native — no-op
        case Mode::APP_MATRICES:    break;  // LVGL-native — no-op
        case Mode::APP_GRAPHER:     renderGraphMode();  break;
        case Mode::STEP_VIEW:       renderSteps();      break;
        // All placeholder apps
        case Mode::APP_PYTHON:
            renderAppView();
            break;
    }
}


// ═════════════════════════════════════════════════
// drawStatusBar() — Shared yellow top bar
// ═════════════════════════════════════════════════
void SystemApp::drawStatusBar() {
    drawStatusBar("NumOS");
}

void SystemApp::drawStatusBar(const String &title) {
    TFT_eSPI &tft = _display.tft();

    tft.fillRect(0, 0, 320, 24, COLOR_HEADER_BG);
    tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER_BG);
    tft.setTextSize(2);
    tft.drawString(title, 8, 4);

    // Battery icon
    tft.drawRect(280, 7, 20, 10, COLOR_HEADER_TEXT);
    tft.fillRect(282, 9, 16, 6,  COLOR_HEADER_TEXT);
    tft.fillRect(300, 10, 2, 4,  COLOR_HEADER_TEXT);

    // Angle mode indicator
    tft.setTextSize(1);
    String indicator = (_angleMode == AngleMode::DEG) ? "DEG" : "RAD";
    int iw = tft.textWidth(indicator);
    tft.drawString(indicator, 270 - iw, 9);
}

// ═══════════════════════════════════════════════════════
// renderMenu() — Icon grid, draws DIRECTLY to TFT
// ═══════════════════════════════════════════════════════
void SystemApp::renderMenu() {
    TFT_eSPI &tft = _display.tft();

    // 1. White background
    tft.fillScreen(COLOR_BACKGROUND);

    // 2. Status bar
    drawStatusBar();

    // 3. Safety check
    if (_apps.empty()) {
        tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
        tft.setTextSize(2);
        tft.drawString("NO APPS LOADED", 50, 120);
        return;
    }

    // 4. Grid constants
    const int cols   = 3;
    const int iconW  = 64;
    const int iconH  = 64;
    const int gapX   = 25;
    const int labelH = 16;
    const int gapY   = 12;
    const int cellH  = iconH + labelH + gapY;

    int totalW = cols * iconW + (cols - 1) * gapX;
    int startX = (320 - totalW) / 2;
    // baseY is 34 minus scroll offset
    int baseY  = 34 - _menuScrollOffset;


    // 5. Swap bytes for 16-bit bitmaps
    tft.setSwapBytes(true);

    // 6. Draw each app
    for (int i = 0; i < (int)_apps.size(); i++) {
        int col = i % cols;
        int row = i / cols;

        int x = startX + col * (iconW + gapX);
        int y = baseY  + row * cellH;

        // Clipping: only draw if icon/label is visible (status bar is at Y=0-23)
        if (y + iconH + labelH < 24 || y > 240) continue;


        // ── Selection cursor (orange, 2-line thick rounded rect) ──
        if (i == _selectedAppIndex) {
            int pad = 5;
            tft.drawRoundRect(x - pad, y - pad,
                              iconW + pad * 2, iconH + pad * 2,
                              8, COLOR_PRIMARY);
            tft.drawRoundRect(x - pad + 1, y - pad + 1,
                              iconW + (pad - 1) * 2, iconH + (pad - 1) * 2,
                              7, COLOR_PRIMARY);
        }

        // ── Bitmap icon ──
        tft.pushImage(x, y, iconW, iconH, _apps[i].iconData);

        // ── Label (full name, centered) ──
        tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
        tft.setTextSize(1);
        String name = _apps[i].name;   // Full name, no abbreviations
        int tw = tft.textWidth(name);
        tft.drawString(name, x + (iconW - tw) / 2, y + iconH + 4);
    }

    // Ensure status bar stays on top of any partially visible icons
    drawStatusBar();

    tft.setSwapBytes(false);
}


// ═════════════════════════════════════════════════
// handleKey() — Global input dispatcher
// ═════════════════════════════════════════════════
void SystemApp::handleKey(const KeyEvent &ev) {
    // Only act on PRESS and REPEAT (no duplicate processing)
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    // Debug: log key events de teclado físico (row>=0) para detectar ghosts.
    // Eventos de SerialBridge (row=-1) ya se logean en SerialBridge.cpp.
    if (ev.row >= 0) {
        Serial.printf("[KEY-HW] code=%d action=%d row=%d col=%d\n",
                      (int)ev.code, (int)ev.action, ev.row, ev.col);
    }

    auto& km = vpam::KeyboardManager::instance();

    // ── SHIFT + AC → Apagado del sistema (DESACTIVADO mientras USB conectado) ──
    // Deep sleep deshabilitado temporalmente: con cable USB conectado
    // el sistema debe permanecer encendido al 100%.
    if (km.isShift() && ev.code == KeyCode::AC) {
        Serial.println("[SYSTEM] SHIFT+AC detected — deep sleep DISABLED (USB mode)");
        km.reset();
        _shiftActive = false;
        // powerOff();  // ← deshabilitado
        return;
    }

    // ── SHIFT/ALPHA gestión global (CalculationApp y CalculusApp tienen su propia) ──
    if (ev.code == KeyCode::SHIFT && _mode != Mode::APP_CALCULATION && _mode != Mode::APP_CALCULUS) {
        _shiftActive = !_shiftActive;
        km.pressShift();
        return;
    }
    if (ev.code == KeyCode::ALPHA && _mode != Mode::APP_CALCULATION && _mode != Mode::APP_CALCULUS) {
        _alphaActive = !_alphaActive;
        km.pressAlpha();
        return;
    }

    switch (_mode) {
        case Mode::MENU:
            handleKeyMenu(ev);
            break;
        case Mode::APP_CALCULATION:
            // MODE key returns to menu, everything else goes to CalculationApp
            if (ev.code == KeyCode::MODE) {
                returnToMenu();
            } else if (_calcApp) {
                _calcApp->handleKey(ev);
            }
            break;
        case Mode::APP_GRAPHER:
            handleKeyGraph(ev);
            break;
        case Mode::STEP_VIEW:
            handleKeySteps(ev);
            break;
        case Mode::APP_EQUATIONS:
            if (ev.code == KeyCode::MODE) {
                returnToMenu();
            } else if (_equationsApp) {
                _equationsApp->handleKey(ev);
            }
            break;
        case Mode::APP_CALCULUS:
            if (ev.code == KeyCode::MODE) {
                returnToMenu();
            } else if (_calculusApp) {
                _calculusApp->handleKey(ev);
            }
            break;
        case Mode::APP_SETTINGS:
            if (ev.code == KeyCode::MODE) {
                returnToMenu();
            } else if (_settingsApp) {
                _settingsApp->handleKey(ev);
            }
            break;
        case Mode::APP_STATISTICS:
            if (ev.code == KeyCode::MODE) {
                returnToMenu();
            } else if (_statisticsApp) {
                _statisticsApp->handleKey(ev);
            }
            break;
        case Mode::APP_PROBABILITY:
            if (ev.code == KeyCode::MODE) {
                returnToMenu();
            } else if (_probabilityApp) {
                _probabilityApp->handleKey(ev);
            }
            break;
        case Mode::APP_REGRESSION:
            if (ev.code == KeyCode::MODE) {
                returnToMenu();
            } else if (_regressionApp) {
                _regressionApp->handleKey(ev);
            }
            break;
        case Mode::APP_MATRICES:
            if (ev.code == KeyCode::MODE) {
                returnToMenu();
            } else if (_matricesApp) {
                _matricesApp->handleKey(ev);
            }
            break;
        // All placeholder apps share generic handler
        case Mode::APP_PYTHON:
        case Mode::APP_TABLE:
            handleKeyApp(ev);
            break;
    }
}

// ═════════════════════════════════════════════════
// handleKeyMenu() — Reenvía las teclas en modo MENU al indev de LVGL.
// LVGL + gridnav manejan la navegación 2D del grid internamente.
// Al pulsar ENTER, la card enfocada emite LV_EVENT_CLICKED → launchApp().
// ═════════════════════════════════════════════════
void SystemApp::handleKeyMenu(const KeyEvent &ev) {
    // Mantener siempre el grupo de foco conectado al indev en modo menú.
    if (LvglKeypad::indev() && _mainMenu.group()) {
        lv_indev_set_group(LvglKeypad::indev(), _mainMenu.group());
    }
    g_lvglActive = true;

    // Navegación 2D explícita del launcher (sin gridnav).
    switch (ev.code) {
        case KeyCode::UP:
            if (_mainMenu.moveFocusByDelta(0, -1)) {
                Serial.printf("[GUI] Focus move: UP\n");
            }
            break;
        case KeyCode::DOWN:
            if (_mainMenu.moveFocusByDelta(0, 1)) {
                Serial.printf("[GUI] Focus move: DOWN\n");
            }
            break;
        case KeyCode::LEFT:
            if (_mainMenu.moveFocusByDelta(-1, 0)) {
                Serial.printf("[GUI] Focus move: LEFT\n");
            }
            break;
        case KeyCode::RIGHT:
            if (_mainMenu.moveFocusByDelta(1, 0)) {
                Serial.printf("[GUI] Focus move: RIGHT\n");
            }
            break;
        case KeyCode::ENTER:
        case KeyCode::AC:
        case KeyCode::DEL:
        case KeyCode::F1:
        case KeyCode::F2:
            Serial.printf("[GUI] Evento enviado a LVGL: Code %d (mode=MENU)\n", (int)ev.code);
            LvglKeypad::pushKey(ev.code, true);
            LvglKeypad::pushKey(ev.code, false);
            break;
        default:
            // Ignorar en launcher teclas no navegacionales.
            break;
    }
}


// ═════════════════════════════════════════════════
// launchApp() — Lanza una app por ID desde el launcher LVGL
// ═════════════════════════════════════════════════
void SystemApp::launchApp(int id) {
    // Ensure LVGL layout is finalized before switching screens.
    // Prevents crashes when launching from a card that was scrolled off-screen.
    lv_obj_update_layout(lv_scr_act());

    if (id == 0) {
        // CalculationApp es LVGL-native: LVGL sigue activo
        g_lvglActive = true;
        switchApp(id);
        if (_calcApp) _calcApp->load();
    } else if (id == 2) {
        // EquationsApp es LVGL-native: ecuaciones y sistemas
        g_lvglActive = true;
        switchApp(id);
        if (_equationsApp) _equationsApp->load();
    } else if (id == 3) {
        // CalculusApp es LVGL-native: derivadas + integrales
        g_lvglActive = true;
        switchApp(id);
        if (_calculusApp) _calculusApp->load();
    } else if (id == 1) {
        // GrapherApp es LVGL-native 2.0
        g_lvglActive = true;
        switchApp(id);
        if (_grapherApp) _grapherApp->load();
    } else if (id == 4) {
        // StatisticsApp es LVGL-native
        g_lvglActive = true;
        switchApp(id);
        if (_statisticsApp) _statisticsApp->load();
    } else if (id == 5) {
        // ProbabilityApp es LVGL-native
        g_lvglActive = true;
        switchApp(id);
        if (_probabilityApp) _probabilityApp->load();
    } else if (id == 6) {
        // RegressionApp es LVGL-native
        g_lvglActive = true;
        switchApp(id);
        if (_regressionApp) _regressionApp->load();
    } else if (id == 7) {
        // MatricesApp es LVGL-native
        g_lvglActive = true;
        switchApp(id);
        if (_matricesApp) _matricesApp->load();
    } else if (id == 9) {
        // Settings es LVGL-native
        g_lvglActive = true;
        switchApp(id);
        if (_settingsApp) _settingsApp->load();
    } else {
        g_lvglActive = false;   // Pausa LVGL: la app escribe directo al TFT
        switchApp(id);           // Actualiza _mode y fuerza _redraw
    }
}

// ═════════════════════════════════════════════════
// returnToMenu() — Reanuda LVGL y recarga el launcher
// ═════════════════════════════════════════════════
void SystemApp::returnToMenu() {
    // Si venimos de la CalculationApp (LVGL-native), detener su cursor
    if (_mode == Mode::APP_CALCULATION && _calcApp) {
        _calcApp->end();
        _calcApp->begin();   // Recrear para la próxima vez
    }
    // Si venimos de la EquationsApp (LVGL-native)
    if (_mode == Mode::APP_EQUATIONS && _equationsApp) {
        _equationsApp->end();
        _equationsApp->begin();
    }
    // Si venimos de la CalculusApp (LVGL-native)
    if (_mode == Mode::APP_CALCULUS && _calculusApp) {
        _calculusApp->end();
        _calculusApp->begin();
    }
    // Si venimos de la GrapherApp (LVGL-native 2.0)
    // Solo end(); load()->begin() reconstruye en la proxima apertura.
    if (_mode == Mode::APP_GRAPHER && _grapherApp) {
        _grapherApp->end();
    }
    // Si venimos de la SettingsApp (LVGL-native)
    if (_mode == Mode::APP_SETTINGS && _settingsApp) {
        _settingsApp->end();
        _settingsApp->begin();
    }
    // Si venimos de la StatisticsApp (LVGL-native)
    if (_mode == Mode::APP_STATISTICS && _statisticsApp) {
        _statisticsApp->end();
        _statisticsApp->begin();
    }
    // Si venimos de la ProbabilityApp (LVGL-native)
    if (_mode == Mode::APP_PROBABILITY && _probabilityApp) {
        _probabilityApp->end();
        _probabilityApp->begin();
    }
    // Si venimos de la RegressionApp (LVGL-native)
    if (_mode == Mode::APP_REGRESSION && _regressionApp) {
        _regressionApp->end();
        _regressionApp->begin();
    }
    // Si venimos de la MatricesApp (LVGL-native)
    if (_mode == Mode::APP_MATRICES && _matricesApp) {
        _matricesApp->end();
        _matricesApp->begin();
    }

    _mode    = Mode::MENU;
    _redraw  = false;
    g_lvglActive = true;
    _mainMenu.load();                     // Activa el screen del launcher
    lv_obj_invalidate(lv_scr_act());      // Fuerza redibujado completo
}

void SystemApp::switchApp(int id) {
    switch (id) {
        case 0: _mode = Mode::APP_CALCULATION; break;
        case 1: _mode = Mode::APP_GRAPHER;     break;
        case 2: _mode = Mode::APP_EQUATIONS;   break;
        case 3: _mode = Mode::APP_CALCULUS;    break;
        case 4: _mode = Mode::APP_STATISTICS;  break;
        case 5: _mode = Mode::APP_PROBABILITY; break;
        case 6: _mode = Mode::APP_REGRESSION;  break;
        case 7: _mode = Mode::APP_MATRICES;    break;
        case 8: _mode = Mode::APP_PYTHON;      break;
        case 9: _mode = Mode::APP_SETTINGS;    break;
        default: _mode = Mode::MENU;           break;
    }
    _redraw = true;
}


// ═════════════════════════════════════════════════
// renderAppView() — Placeholder screen for unfinished apps
// ═════════════════════════════════════════════════
void SystemApp::renderAppView() {
    TFT_eSPI &tft = _display.tft();

    // White screen
    tft.fillScreen(COLOR_BACKGROUND);

    // Status bar with app name
    String appName = "App";
    for (int i = 0; i < (int)_apps.size(); i++) {
        // Find the matching app to get its name
        if ((_mode == Mode::APP_STATISTICS  && _apps[i].id == 3) ||
            (_mode == Mode::APP_PROBABILITY && _apps[i].id == 4) ||
            (_mode == Mode::APP_MATRICES    && _apps[i].id == 5) ||
            (_mode == Mode::APP_REGRESSION  && _apps[i].id == 6) ||
            (_mode == Mode::APP_PYTHON      && _apps[i].id == 7) ||
            (_mode == Mode::APP_SETTINGS    && _apps[i].id == 8)) {
            appName = _apps[i].name;
            break;
        }
    }

    drawStatusBar(appName);

    // Placeholder content
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(2);
    String msg = "App: " + appName;
    int mw = tft.textWidth(msg);
    tft.drawString(msg, (320 - mw) / 2, 90);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_TEXT_LIGHT, COLOR_BACKGROUND);
    String sub = "Coming Soon";
    int sw = tft.textWidth(sub);
    tft.drawString(sub, (320 - sw) / 2, 120);

    // Hint at bottom
    tft.setTextColor(COLOR_PRIMARY, COLOR_BACKGROUND);
    String hint = "Press MODE to go back";
    int hw = tft.textWidth(hint);
    tft.drawString(hint, (320 - hw) / 2, 220);
}

// ═════════════════════════════════════════════════
// handleKeyApp() — Generic handler for placeholder apps
// Pressing MODE returns to MENU
// ═════════════════════════════════════════════════
void SystemApp::handleKeyApp(const KeyEvent &ev) {
    if (ev.code == KeyCode::MODE || ev.code == KeyCode::AC) {
        returnToMenu();
    }
}

// ═════════════════════════════════════════════════
// handleKeySteps()
// ═════════════════════════════════════════════════
void SystemApp::handleKeySteps(const KeyEvent &ev) {
    if (ev.code == KeyCode::AC || ev.code == KeyCode::ENTER || ev.code == KeyCode::MODE) {
        _mode = Mode::APP_CALCULATION;
    } else if (ev.code == KeyCode::DOWN) {
        _stepScroll++;
    } else if (ev.code == KeyCode::UP) {
        if (_stepScroll > 0) _stepScroll--;
    }
    _redraw = true;
}

// ═════════════════════════════════════════════════
// renderSteps()
// ═════════════════════════════════════════════════
void SystemApp::renderSteps() {
    TFT_eSPI &tft = _display.tft();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Steps", 8, 4);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    int y = 28;
    for (size_t i = (size_t)_stepScroll; i < _stepLogger.size() && y < 220; i++) {
        String line = String(i + 1) + ": " + _stepLogger.get(i).description;
        tft.drawString(line, 5, y);
        y += 14;
    }

    // Scroll hints
    tft.setTextColor(COLOR_TEXT_LIGHT, TFT_BLACK);
    if (_stepScroll > 0)
        tft.drawString("^ UP", 280, 28);
    if ((size_t)_stepScroll + 13 < _stepLogger.size())
        tft.drawString("v DOWN", 270, 228);
}

// ═════════════════════════════════════════════════
// handleKeyGraph()
// ═════════════════════════════════════════════════
void SystemApp::handleKeyGraph(const KeyEvent &ev) {
    if (ev.code == KeyCode::MODE) {
        returnToMenu();
        return;
    }
    if (_grapherApp) {
        // AC at tab level on Expressions tab = return to Home
        if (ev.code == KeyCode::AC && _grapherApp->atTabLevel()
            && _grapherApp->isOnExpressions()) {
            returnToMenu();
            return;
        }
        _grapherApp->handleKey(ev);
    }
}

// ═════════════════════════════════════════════════
// renderGraphMode()
// ═════════════════════════════════════════════════
void SystemApp::renderGraphMode() {
    // GrapherApp 2.0 is LVGL-native — no manual rendering needed
}

// ═════════════════════════════════════════════════
// powerOff() — Apagado con fade-out LVGL + deep sleep
//
// Secuencia:
//   1. Crea un screen negro con opacity 0
//   2. Fade-in del negro (= fade-out visual) en 500 ms
//   3. Apaga backlight vía PWM
//   4. Configura ext0 wakeup en PIN_KEY_R1 (tecla ON, GPIO 2)
//   5. Entra en deep sleep
// ═════════════════════════════════════════════════
void SystemApp::powerOff() {
    // ── Deep sleep completamente DESACTIVADO en modo USB/Debug ──
    // Mientras el cable USB esté conectado, el sistema debe permanecer 100% operativo.
    // Para reactivar: descomentar el bloque de abajo.
    Serial.println("[SYSTEM] powerOff() called — IGNORED (USB mode active)");
    return;

    /*  // === DEEP SLEEP ORIGINAL (desactivado) ===

    // Asegurar que LVGL está activo para renderizar el fade
    g_lvglActive = true;

    // ── Screen negro para fade-out ──
    lv_obj_t* scrOff = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scrOff, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scrOff, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scrOff, LV_OBJ_FLAG_SCROLLABLE);

    // Label "Apagando..." sutil, centrado
    lv_obj_t* lblBye = lv_label_create(scrOff);
    lv_label_set_text(lblBye, LV_SYMBOL_POWER " Apagando...");
    lv_obj_set_style_text_font(lblBye, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblBye, lv_color_make(0x80, 0x80, 0x80), 0);
    lv_obj_set_style_text_align(lblBye, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblBye, LV_ALIGN_CENTER, 0, 0);

    // Transición: fade desde pantalla actual → negro en 500 ms
    lv_screen_load_anim(scrOff, LV_SCREEN_LOAD_ANIM_FADE_IN, 500, 0, true);

    // Bombear LVGL hasta que el fade complete (500 ms + margen)
    uint32_t t0 = millis();
    while (millis() - t0 < 650) {
        lv_timer_handler();
        delay(5);
    }

    // ── Apagar backlight via PWM ──
    analogWrite(PIN_TFT_BL, 0);

    Serial.println("[SYSTEM] Deep sleep...");
    Serial.flush();

    // ── Configurar wakeup ──
    // PIN_KEY_R1 = GPIO 2 (fila de la tecla ON = R1C0).
    // La fila tiene INPUT_PULLUP, al presionar ON la columna C0 tira a LOW.
    // ext0 despierta al detectar nivel bajo.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_KEY_R1, 0);

    // ── Entrar en deep sleep ──
    esp_deep_sleep_start();

    // No se ejecuta nada después de aquí
    */   // === FIN DEEP SLEEP ORIGINAL ===
}
