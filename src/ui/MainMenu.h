/**
 * MainMenu.h
 * Grid-based icon menu for choosing calculator modes.
 */

#pragma once

#include <Arduino.h>
#include "../display/DisplayDriver.h"
#include "../input/KeyCodes.h"
#include "../input/KeyMatrix.h"

enum class AppMode {
    SCIENTIFIC,
    GRAPHING,
    EQUATION,
    SYSTEM, // Linear systems
    SETTINGS
};

class MainMenu {
public:
    MainMenu(DisplayDriver &display);

    void show();
    void handleKey(const KeyEvent &ev);
    
    AppMode getSelectedMode() const { return _currentSelection; }
    bool isActive() const { return _active; }
    void setActive(bool active) { _active = active; if(active) draw(); }

private:
    DisplayDriver &_display;
    bool _active;
    AppMode _currentSelection; // which icon is selected
    
    // Grid layout: 2 rows of 3 cols (or similar)
    // 0: SCIENTIFIC  1: GRAPHING
    // 2: EQUATION    3: SYSTEM
    // 4: SETTINGS    5: (Empty) or HardwareTest?
    
    int _selectedIndex; // 0..4
    
    void draw();
    void drawIcon(int index, bool selected);
    AppMode indexToMode(int index);
};
