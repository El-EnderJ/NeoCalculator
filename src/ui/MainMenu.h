/**
 * MainMenu.h
 * Launcher NumOS — Pixel-Perfect NumWorks aesthetic (High-Fidelity).
 *
 * LVGL 9.x implementation:
 *  - 24 px orange status bar (#FF9900) with rad/deg, title, battery
 *  - 3-column grid of white cards with geometric vector-style icons
 *  - Focus: blue border (#0074D9) + 1.05× scale + overshoot animation
 *  - Shadows: diffuse 4 px normal, intensified on focus
 *  - Dot-grid background pattern (#D1D1D1 on #F5F5F5)
 *  - Anti-aliased Montserrat 14 throughout
 */

#pragma once

#ifdef ARDUINO
  #include <Arduino.h>
#else
  #include "hal/ArduinoCompat.h"
#endif
#include <lvgl.h>
#include <functional>

class DisplayDriver;

class MainMenu {
public:
    explicit MainMenu(DisplayDriver& display);
    ~MainMenu();

    // ── Lifecycle ────────────────────────────────────────────────────────
    void create();
    void load();

    // ── Input ────────────────────────────────────────────────────────────
    void setLaunchCallback(std::function<void(int)> cb);
    bool moveFocusByDelta(int dCol, int dRow);

    // ── Accessors ────────────────────────────────────────────────────────
    lv_group_t* group()  const { return _group; }
    lv_obj_t*   screen() const { return _screen; }

private:
    // ── App descriptor ───────────────────────────────────────────────────
    struct AppEntry {
        int         id;
        const char* name;
        uint32_t    color;       // Primary icon colour
        uint32_t    colorLight;  // Lighter accent for geometric decoration
    };

    static const AppEntry APPS[];
    static const int      APP_COUNT;

    // ── UI construction ──────────────────────────────────────────────────
    void buildStatusBar();
    void buildGrid();
    lv_obj_t* buildCard(lv_obj_t* parent, const AppEntry& app);
    lv_obj_t* cardById(int appId) const;
    int focusedCardId() const;

    /** Creates a geometric vector icon inside `parent` based on app id. */
    void createAppIcon(lv_obj_t* parent, const AppEntry& app);

    // Flex layout: col/row params no longer required (dynamic wrapping)

    // ── Styles ───────────────────────────────────────────────────────────
    void initStyles();

    lv_style_t _styleScreen;
    lv_style_t _styleCard;
    lv_style_t _styleCardFocused;
    lv_style_t _styleCardPressed;
    lv_style_t _styleIconBox;
    lv_style_t _styleAppName;

    // Focus transition (overshoot, 250 ms)
    lv_style_transition_dsc_t _transCard;

    // ── Event handlers ───────────────────────────────────────────────────
    static void onCardEvent(lv_event_t* e);
    static void onCardFocused(lv_event_t* e);   ///< Logs focus changes to Serial
    static void onGridDraw(lv_event_t* e);
    static void onIconDraw(lv_event_t* e);
    /** Disables shadow rendering on all cards+icons during scroll for FPS. */
    static void onScrollBegin(lv_event_t* e);
    /** Restores shadow rendering after scroll ends. */
    static void onScrollEnd(lv_event_t* e);

    // ── Members ──────────────────────────────────────────────────────────
    DisplayDriver& _display;

    lv_obj_t*   _screen    = nullptr;
    lv_obj_t*   _grid      = nullptr;
    lv_obj_t*   _firstCard = nullptr;   ///< First card — focused on create()
    lv_group_t* _group     = nullptr;

    std::function<void(int)> _launchCb;
};
