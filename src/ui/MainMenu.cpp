/**
 * MainMenu.cpp
 */

#include "MainMenu.h"

MainMenu::MainMenu(DisplayDriver &display) 
    : _display(display), _active(false), _currentSelection(AppMode::SCIENTIFIC), _selectedIndex(0) 
{}

void MainMenu::show() {
    _active = true;
    draw();
}

AppMode MainMenu::indexToMode(int index) {
    switch(index) {
        case 0: return AppMode::SCIENTIFIC;
        case 1: return AppMode::GRAPHING;
        case 2: return AppMode::EQUATION;
        case 3: return AppMode::SYSTEM;
        case 4: return AppMode::SETTINGS;
        default: return AppMode::SCIENTIFIC;
    }
}

void MainMenu::handleKey(const KeyEvent &ev) {
    if (!_active) return;
    
    if (ev.code == KeyCode::ENTER) {
        _currentSelection = indexToMode(_selectedIndex);
        _active = false; // Menu closes, SystemApp should read selection
        return;
    }
    
    // Grid 2 columns, 3 rows? 
    // Let's do 2 columns x 3 rows.
    // 0 1
    // 2 3
    // 4 5
    
    int cols = 2;
    int total = 5; 

    switch(ev.code) {
        case KeyCode::RIGHT_F5: // or RIGHT
            if ((_selectedIndex % cols) < cols - 1) _selectedIndex++;
            break;
        case KeyCode::LEFT:
            if ((_selectedIndex % cols) > 0) _selectedIndex--;
            break;
        case KeyCode::DOWN:
            if (_selectedIndex + cols < total) _selectedIndex += cols;
            break;
        case KeyCode::UP:
            if (_selectedIndex - cols >= 0) _selectedIndex -= cols;
            break;
        default:
            return;
    }
    
    draw();
}

void MainMenu::draw() {
    _display.clear(TFT_BLACK);
    _display.setTextColor(TFT_WHITE, TFT_BLACK);
    _display.setTextSize(2);
    _display.drawText(60, 10, "MAIN MENU");
    
    for (int i=0; i<5; ++i) {
        drawIcon(i, i == _selectedIndex);
    }
}

void MainMenu::drawIcon(int index, bool selected) {
    int col = index % 2;
    int row = index / 2;
    
    int w = 100;
    int h = 40; // reduced height to text list for now, or box
    int gap = 10;
    
    int startX = 15;
    int startY = 40;
    
    int x = startX + col * (w + gap);
    int y = startY + row * (h + gap);
    
    uint16_t color = selected ? TFT_YELLOW : TFT_BLUE;
    uint16_t txtColor = selected ? TFT_BLACK : TFT_WHITE;
    
    // Draw Box
    if (selected) {
        _display.fillRect(x, y, w, h, color);
    } else {
        _display.tft().drawRect(x, y, w, h, color); // Outline
    }
    
    // Label
    _display.setTextSize(1);
    _display.setTextColor(txtColor, selected ? color : TFT_BLACK);
    
    String label;
    switch(index) {
        case 0: label = "SCIENTIFIC"; break;
        case 1: label = "GRAPHING"; break;
        case 2: label = "EQUATION"; break;
        case 3: label = "SYSTEM 2x2/3x3"; break;
        case 4: label = "SETTINGS"; break;
    }
    
    _display.drawText(x + 5, y + 12, label);
}
