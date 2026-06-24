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

#include "StixGlyphGallery.h"

#include <Arduino.h>
#include <algorithm>
#include <array>

#include "MathSymbols.h"
#include "../fonts/StixMathFont.h"

namespace ui {
namespace {

struct GlyphProbe {
    uint32_t codepoint;
    const char* name;
};

bool probeGlyph(const lv_font_t* font, uint32_t codepoint, const char* name, lv_font_glyph_dsc_t* out = nullptr) {
    lv_font_glyph_dsc_t glyph;
    const bool ok = lv_font_get_glyph_dsc(font, &glyph, codepoint, 0);
    if (ok) {
        Serial.printf("[STIX] %-9s U+%04lX OK  adv=%d ofs_y=%d box=%dx%d\n",
                      name,
                      static_cast<unsigned long>(codepoint),
                      static_cast<int>(glyph.adv_w),
                      static_cast<int>(glyph.ofs_y),
                      static_cast<int>(glyph.box_w),
                      static_cast<int>(glyph.box_h));
        if (out) {
            *out = glyph;
        }
    } else {
        Serial.printf("[STIX] %-9s U+%04lX MISSING\n",
                      name,
                      static_cast<unsigned long>(codepoint));
    }
    return ok;
}

} // namespace

bool runStixGlyphAlignmentDiagnostics(const lv_font_t* font) {
    const lv_font_t* target = font ? font : &stix_math_18;

    const std::array<GlyphProbe, 14> probes = {{
        {0x222B, "SYMB_INT"},
        {0x2211, "SYMB_SUM"},
        {0x221A, "SYMB_SQRT"},
        {0x221E, "SYMB_INF"},
        {0x211D, "SYMB_REAL"},
        {0x2102, "SYMB_CPLX"},
        {0x03B1, "SYMB_ALPHA"},
        {0x03B2, "SYMB_BETA"},
        {0x03B3, "SYMB_GAMMA"},
        {0x2282, "SYMB_SUB"},
        {0x1D49C, "SMP_A"},
        {0x1D4D1, "SMP_B"},
        {0x1D7D8, "SMP_0"},
        {0x1D7D9, "SMP_1"}
    }};

    bool allFound = true;
    for (const auto& probe : probes) {
        if (!probeGlyph(target, probe.codepoint, probe.name)) {
            allFound = false;
        }
    }

    lv_font_glyph_dsc_t intGlyph{};
    lv_font_glyph_dsc_t sumGlyph{};
    lv_font_glyph_dsc_t sqrtGlyph{};

    bool alignDataOk = probeGlyph(target, 0x222B, "ALIGN_INT", &intGlyph)
                    && probeGlyph(target, 0x2211, "ALIGN_SUM", &sumGlyph)
                    && probeGlyph(target, 0x221A, "ALIGN_SQRT", &sqrtGlyph);

    bool baselineOk = false;
    if (alignDataOk) {
        const int minOfsY = std::min({static_cast<int>(intGlyph.ofs_y),
                                      static_cast<int>(sumGlyph.ofs_y),
                                      static_cast<int>(sqrtGlyph.ofs_y)});
        const int maxOfsY = std::max({static_cast<int>(intGlyph.ofs_y),
                                      static_cast<int>(sumGlyph.ofs_y),
                                      static_cast<int>(sqrtGlyph.ofs_y)});
        const int delta = maxOfsY - minOfsY;

        // Delta <= 8 keeps visible baseline drift under control on 240px-high UI rows.
        baselineOk = delta <= 8;

        Serial.printf("[STIX] Baseline delta(int,sum,sqrt)=%d -> %s\n",
                      delta,
                      baselineOk ? "OK" : "WARN");
    }

    const bool passed = allFound && alignDataOk && baselineOk;
    Serial.printf("[STIX] Diagnostics: %s\n", passed ? "PASS" : "FAIL");
    return passed;
}

void showStixGlyphGallery(uint32_t holdMs) {
    if (holdMs == 0) {
        return;
    }

    lv_obj_t* activeScreen = lv_screen_active();
    if (!activeScreen) {
        return;
    }

    lv_obj_t* panel = lv_obj_create(activeScreen);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 320, 240);
    lv_obj_set_style_bg_color(panel, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(panel);
    lv_label_set_text(title, "STIX TWO MATH - GLYPH GALLERY");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_pos(title, 8, 8);

    static const char* kSamples[] = {
        "\xE2\x88\xAB_a^b f(x)dx",
        "\xE2\x88\x91_{k=1}^{\xE2\x88\x9E}",
        "\xE2\x88\x9A[n]{x}",
        "\xE2\x84\x9D \xE2\x8A\x82 \xE2\x84\x82",
        "\xCE\xB1 \xCE\xB2 \xCE\xB3",
        "\xF0\x9D\x92\x9C \xF0\x9D\x93\x91 \xF0\x9D\x9F\x98 \xF0\x9D\x9F\x99"
    };

    int y = 40;
    for (const char* sample : kSamples) {
        lv_obj_t* label = lv_label_create(panel);
        lv_label_set_text(label, sample);
        lv_obj_set_style_text_font(label, &stix_math_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(0x202020), LV_PART_MAIN);
        lv_obj_set_pos(label, 12, y);
        y += 30;
    }

    lv_obj_t* legend = lv_label_create(panel);
    lv_label_set_text(legend,
                      "SYMB_INT  SYMB_SUM  SYMB_SQRT  SYMB_REAL  SYMB_COMPLEX");
    lv_obj_set_style_text_font(legend, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(legend, lv_color_hex(0x3A3A3A), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(legend, 1, LV_PART_MAIN);
    lv_obj_set_pos(legend, 8, 218);

    const uint32_t endAt = millis() + holdMs;
    while (millis() < endAt) {
        lv_timer_handler();
        delay(5);
    }

    lv_obj_delete(panel);
}

} // namespace ui
