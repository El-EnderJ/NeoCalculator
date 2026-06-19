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

#pragma once

#include <cstdint>
#include <lvgl.h>

namespace ui {

// Global math style used by equation-oriented widgets only.
extern lv_style_t style_math_primary;

struct MathFontFace {
    const lv_font_t* font;
    int16_t mathEmSizePx;
};

// Initialize math typography styles once (call after lv_init).
void initMathTypography();

// Canonical math font profiles. mathEmSizePx is the OpenType MATH design size,
// not the LVGL line_height.
MathFontFace mathPrimaryFontFace();
MathFontFace mathScriptFontFace();
MathFontFace mathScriptScriptFontFace();

// Canonical font for VPAM/MathRenderer equation drawing.
const lv_font_t* mathPrimaryFont();

// Script-level font (e.g., superscripts/subscripts).
const lv_font_t* mathScriptFont();

// ScriptScript-level font (nested superscripts/subscripts).
const lv_font_t* mathScriptScriptFont();

// Returns the nominal MATH em for a generated STIX font, falling back to the
// LVGL line height only for unknown external fonts.
int16_t nominalMathEmSizeForFont(const lv_font_t* font);

} // namespace ui
