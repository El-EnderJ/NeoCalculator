/**
 * PeriodicTableApp.h — Interactive Periodic Table for NumOS.
 *
 * LVGL-native app with 3 tabs:
 *   1. Table   — Custom-drawn 18×10 periodic table grid with detail panel
 *   2. Molar   — Molar mass calculator (formula parser)
 *   3. Balance — Chemical equation balancer
 *
 * Uses Flash-resident ChemDatabase (zero RAM for static data).
 * Custom draw engine via LV_EVENT_DRAW_MAIN (no lv_canvas).
 *
 * Part of: NumOS — Chemistry Module
 */

#pragma once

#include <lvgl.h>
#include "../ui/StatusBar.h"
#include "../input/KeyCodes.h"
#include "../input/KeyboardManager.h"
#include "ChemDatabase.h"
#include "ChemExtraData.h"

class PeriodicTableApp {
public:
    PeriodicTableApp();
    ~PeriodicTableApp();

    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Tabs ─────────────────────────────────────────────────────────────
    enum class Tab : uint8_t { TABLE, MOLAR, BALANCE };

    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 240;

    // ── Grid layout constants ────────────────────────────────────────────
    static constexpr int GRID_COLS   = 18;
    static constexpr int GRID_ROWS   = 10;
    static constexpr int CELL_W      = 17;
    static constexpr int CELL_H      = 14;
    static constexpr int GRID_X0     = 2;
    static constexpr int GRID_Y0     = 2;   // relative to grid obj
    static constexpr int GRID_TOTAL_W = GRID_COLS * CELL_W + 1;
    static constexpr int GRID_TOTAL_H = GRID_ROWS * CELL_H + 1;

    // Detail panel height
    static constexpr int DETAIL_H    = 80;

    // ── LVGL objects ─────────────────────────────────────────────────────
    lv_obj_t*       _screen;
    ui::StatusBar   _statusBar;

    // Tab bar
    lv_obj_t*       _tabBtns[3];
    lv_obj_t*       _tabLabels[3];

    // Table tab
    lv_obj_t*       _gridObj;        // custom-drawn periodic table
    lv_obj_t*       _detailPanel;    // element detail display
    lv_obj_t*       _detSymbol;      // big symbol label
    lv_obj_t*       _detName;        // full name
    lv_obj_t*       _detNumber;      // atomic number
    lv_obj_t*       _detMass;        // atomic mass
    lv_obj_t*       _detConfig;      // electron configuration
    lv_obj_t*       _detEN;          // electronegativity
    lv_obj_t*       _detCategory;    // category label (colored)

    // Molar mass tab
    lv_obj_t*       _molarContainer;
    lv_obj_t*       _molarInput;     // formula display
    lv_obj_t*       _molarResult;    // calculated mass
    lv_obj_t*       _molarHint;

    // Balancer tab
    lv_obj_t*       _balContainer;
    lv_obj_t*       _balInput;       // equation display
    lv_obj_t*       _balResult;      // balanced result
    lv_obj_t*       _balHint;

    // Deep Dive modal
    lv_obj_t*       _modalOverlay;   // fullscreen dark overlay
    lv_obj_t*       _modalBox;       // content container (scrollable)
    bool            _modalOpen;
    int             _modalScrollY;   // scroll offset for modal content

    // ── State ────────────────────────────────────────────────────────────
    Tab             _currentTab;
    uint8_t         _cursorAtomicNumber;   // 1-118
    bool            _tabFocused;           // true = focus on tab bar

    // Text input buffers
    char            _molarBuf[64];
    int             _molarLen;
    char            _balBuf[128];
    int             _balLen;

    // ── UI construction ──────────────────────────────────────────────────
    void createUI();
    void createTabBar();
    void createTableTab();
    void createMolarTab();
    void createBalanceTab();

    // ── Tab switching ────────────────────────────────────────────────────
    void switchTab(Tab tab);
    void updateTabHighlight();

    // ── Table tab logic ──────────────────────────────────────────────────
    void updateFocusedElement();
    void navigateTable(KeyCode dir);

    // ── Input handling per tab ───────────────────────────────────────────
    void handleKeyTable(const KeyEvent& ev);
    void handleKeyMolar(const KeyEvent& ev);
    void handleKeyBalance(const KeyEvent& ev);

    // ── Deep Dive modal ──────────────────────────────────────────────────
    void openDeepDive();
    void closeDeepDive();

    // ── Text input helpers ───────────────────────────────────────────────
    void appendChar(char* buf, int& len, int maxLen, char c);
    void deleteChar(char* buf, int& len);
    char keyToChar(const KeyEvent& ev);

    // ── Custom draw callback ─────────────────────────────────────────────
    static void onGridDraw(lv_event_t* e);

    // ── Category color mapping ───────────────────────────────────────────
    static uint32_t categoryColor(chem::ChemCategory cat);
};
