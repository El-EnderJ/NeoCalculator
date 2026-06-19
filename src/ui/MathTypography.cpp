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

#include "MathTypography.h"

#include "../fonts/StixMathFont.h"

namespace ui {

lv_style_t style_math_primary;

static bool g_mathTypographyInited = false;

static constexpr int16_t kStixPrimaryMathEmPx = 18;
static constexpr int16_t kStixScriptMathEmPx = 12;
static constexpr int16_t kStixScriptScriptMathEmPx = 8;

void initMathTypography() {
    if (g_mathTypographyInited) {
        return;
    }

    lv_style_init(&style_math_primary);
    lv_style_set_text_font(&style_math_primary, &stix_math_18);
    lv_style_set_text_color(&style_math_primary, lv_color_black());
    lv_style_set_text_opa(&style_math_primary, LV_OPA_COVER);

    g_mathTypographyInited = true;
}

MathFontFace mathPrimaryFontFace() {
    return { &stix_math_18, kStixPrimaryMathEmPx };
}

MathFontFace mathScriptFontFace() {
    return { &stix_math_12, kStixScriptMathEmPx };
}

MathFontFace mathScriptScriptFontFace() {
    return { &stix_math_8, kStixScriptScriptMathEmPx };
}

const lv_font_t* mathPrimaryFont() {
    return mathPrimaryFontFace().font;
}

const lv_font_t* mathScriptFont() {
    return mathScriptFontFace().font;
}

const lv_font_t* mathScriptScriptFont() {
    return mathScriptScriptFontFace().font;
}

int16_t nominalMathEmSizeForFont(const lv_font_t* font) {
    if (font == &stix_math_18) return kStixPrimaryMathEmPx;
    if (font == &stix_math_12) return kStixScriptMathEmPx;
    if (font == &stix_math_8) return kStixScriptScriptMathEmPx;
    return font ? static_cast<int16_t>(font->line_height) : kStixPrimaryMathEmPx;
}

} // namespace ui
