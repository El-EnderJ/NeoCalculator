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

// ═════════════════════════════════════════════════
// Constructor
// ═════════════════════════════════════════════════
SystemApp::SystemApp(DisplayDriver &display, KeyMatrix &keypad)
    : _display(display),
      _keypad(keypad),
      _mainMenu(display),
      _calcApp(nullptr),
      _grapherApp(nullptr),
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

    // ── Boot Splash ──
    _display.tft().fillScreen(COLOR_BACKGROUND);
    _display.tft().setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    _display.tft().setTextSize(3);
    String title = "NumOS";
    int w = _display.tft().textWidth(title);
    _display.tft().drawString(title, (320 - w) / 2, 90);

    _display.tft().setTextSize(1);
    _display.tft().setTextColor(COLOR_TEXT_LIGHT, COLOR_BACKGROUND);
    String ver = "v1.0.0 Alpha";
    int wv = _display.tft().textWidth(ver);
    _display.tft().drawString(ver, (320 - wv) / 2, 130);
    delay(800);

    // ── Create CalculationApp ──
    _calcApp = new CalculationApp(_display, _vars);
    _calcApp->setAngleMode(_angleMode);
    _calcApp->begin();

    // ── Create GrapherApp ──
    _grapherApp = new GrapherApp(_display, _vars);
    _grapherApp->begin();

    // ── Load apps & go to menu ──
    initApps();
    _mode = Mode::MENU;
    _selectedAppIndex = 0;
    _redraw = true;
}

// ═════════════════════════════════════════════════
// initApps() — Populate the app grid (SINGLE definition)
// ═════════════════════════════════════════════════
void SystemApp::initApps() {
    _apps.clear();
    _apps.emplace_back(0, "Calculation",  icon_Calculation);
    _apps.emplace_back(1, "Grapher",      icon_Grapher);
    _apps.emplace_back(2, "Python",       icon_Python);
    _apps.emplace_back(3, "Statistics",   icon_Statistics);
    _apps.emplace_back(4, "Equations",    icon_Equations);
    _apps.emplace_back(5, "Settings",     icon_Settings);
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

    // CalculationApp manages its own _redraw internally
    if (_mode == Mode::APP_CALCULATION && _calcApp) {
        _calcApp->render();
    } else if (_mode == Mode::APP_GRAPHER && _grapherApp) {
        _grapherApp->render(); // GrapherApp handles its own redraw check
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
        case Mode::MENU:            renderMenu();       break;
        case Mode::APP_CALCULATION: break;  // Handled in update() via _calcApp
        case Mode::APP_GRAPHER:     renderGraphMode();  break;
        case Mode::STEP_VIEW:       renderSteps();      break;
        // All placeholder apps
        case Mode::APP_PYTHON:
        case Mode::APP_STATISTICS:
        case Mode::APP_EQUATIONS:
        case Mode::APP_SETTINGS:
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
    int startY = 34;

    // 5. Swap bytes for 16-bit bitmaps
    tft.setSwapBytes(true);

    // 6. Draw each app
    for (int i = 0; i < (int)_apps.size(); i++) {
        int col = i % cols;
        int row = i / cols;

        int x = startX + col * (iconW + gapX);
        int y = startY + row * cellH;

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

    tft.setSwapBytes(false);
}

// ═════════════════════════════════════════════════
// handleKey() — Global input dispatcher
// ═════════════════════════════════════════════════
void SystemApp::handleKey(const KeyEvent &ev) {
    // Only act on PRESS and REPEAT (no duplicate processing)
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (_mode) {
        case Mode::MENU:
            handleKeyMenu(ev);
            break;
        case Mode::APP_CALCULATION:
            // MODE key returns to menu, everything else goes to CalculationApp
            if (ev.code == KeyCode::MODE) {
                _mode = Mode::MENU;
                _redraw = true;
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
        // All placeholder apps share generic handler
        case Mode::APP_PYTHON:
        case Mode::APP_STATISTICS:
        case Mode::APP_EQUATIONS:
        case Mode::APP_SETTINGS:
            handleKeyApp(ev);
            break;
    }
}

// ═════════════════════════════════════════════════
// handleKeyMenu() — Grid navigation
// ═════════════════════════════════════════════════
void SystemApp::handleKeyMenu(const KeyEvent &ev) {
    const int cols = 3;
    int maxIdx = (int)_apps.size() - 1;

    switch (ev.code) {
        case KeyCode::RIGHT:
            _selectedAppIndex = (_selectedAppIndex < maxIdx) ? _selectedAppIndex + 1 : 0;
            _redraw = true;
            break;

        case KeyCode::LEFT:
            _selectedAppIndex = (_selectedAppIndex > 0) ? _selectedAppIndex - 1 : maxIdx;
            _redraw = true;
            break;

        case KeyCode::DOWN:
            if (_selectedAppIndex + cols <= maxIdx) {
                _selectedAppIndex += cols;
                _redraw = true;
            }
            break;

        case KeyCode::UP:
            if (_selectedAppIndex - cols >= 0) {
                _selectedAppIndex -= cols;
                _redraw = true;
            }
            break;

        case KeyCode::ENTER:    // OK / EXE
            switchApp(_apps[_selectedAppIndex].id);
            break;

        default:
            break;
    }
}

// ═════════════════════════════════════════════════
// switchApp() — Transition from menu to app
// ═════════════════════════════════════════════════
void SystemApp::switchApp(int id) {
    switch (id) {
        case 0: _mode = Mode::APP_CALCULATION; break;
        case 1: _mode = Mode::APP_GRAPHER;     break;
        case 2: _mode = Mode::APP_PYTHON;      break;
        case 3: _mode = Mode::APP_STATISTICS;   break;
        case 4: _mode = Mode::APP_EQUATIONS;    break;
        case 5: _mode = Mode::APP_SETTINGS;     break;
        default: _mode = Mode::MENU;            break;
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
        if ((_mode == Mode::APP_PYTHON     && _apps[i].id == 2) ||
            (_mode == Mode::APP_STATISTICS && _apps[i].id == 3) ||
            (_mode == Mode::APP_EQUATIONS  && _apps[i].id == 4) ||
            (_mode == Mode::APP_SETTINGS   && _apps[i].id == 5)) {
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
    String sub = "Proximamente";
    int sw = tft.textWidth(sub);
    tft.drawString(sub, (320 - sw) / 2, 120);

    // Hint at bottom
    tft.setTextColor(COLOR_PRIMARY, COLOR_BACKGROUND);
    String hint = "Pulsa MODE para volver";
    int hw = tft.textWidth(hint);
    tft.drawString(hint, (320 - hw) / 2, 220);
}

// ═════════════════════════════════════════════════
// handleKeyApp() — Generic handler for placeholder apps
// Pressing MODE returns to MENU
// ═════════════════════════════════════════════════
void SystemApp::handleKeyApp(const KeyEvent &ev) {
    if (ev.code == KeyCode::MODE || ev.code == KeyCode::AC) {
        _mode = Mode::MENU;
        _redraw = true;
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
        _mode = Mode::MENU;
        _redraw = true;
    } else if (_grapherApp) {
        _grapherApp->handleKey(ev);
    }
}

// ═════════════════════════════════════════════════
// renderGraphMode()
// ═════════════════════════════════════════════════
void SystemApp::renderGraphMode() {
    if (_grapherApp) _grapherApp->render();
}
