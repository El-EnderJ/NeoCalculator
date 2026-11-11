// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "MathTypography.h"

namespace vpam {

constexpr bool isMathItalicGlyph(uint32_t cp) {
    return (cp >= 0x1D434 && cp <= 0x1D467) || cp == 0x210E ||
           (cp >= 0x1D6E2 && cp <= 0x1D71B);
}

// Presentation only. Variable identity and serialization never see these
// mathematical-alphabet codepoints. The same glyph exists at 18/12/8 px;
// coverage is checked against the source font and all compiled subsets.
constexpr uint32_t identifierGlyphCodepoint(uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return 0x1D434 + cp - 'A';
    if (cp >= 'a' && cp <= 'z') return cp == 'h' ? 0x210E : 0x1D44E + cp - 'a';
    // WHY: the accepted broad mathematical Delta is a product invariant.
    if (cp == 0x0394) return cp;
    if (cp >= 0x0391 && cp <= 0x03A9 && cp != 0x03A2) return 0x1D6E2 + cp - 0x0391;
    if (cp >= 0x03B1 && cp <= 0x03C9) return 0x1D6FC + cp - 0x03B1;
    switch (cp) {
        case 0x03D1: return 0x1D717;
        case 0x03D5: return 0x1D719;
        case 0x03D6: return 0x1D71B;
        case 0x03F5: return 0x1D716;
        default: return cp;
    }
}

inline const char* identifierDisplayText(const char* text, char (&buffer)[5]) {
    uint32_t cp = 0;
    const auto length = utf8Decode(reinterpret_cast<const uint8_t*>(text), cp);
    // Multi-letter engine identifiers and Ans/PreAns remain upright names.
    if (!length || text[length]) return text;
    const uint32_t mapped = identifierGlyphCodepoint(cp);
    if (mapped == cp) return text;
    if (mapped < 0x10000) {
        buffer[0] = static_cast<char>(0xE0 | (mapped >> 12));
        buffer[1] = static_cast<char>(0x80 | ((mapped >> 6) & 63));
        buffer[2] = static_cast<char>(0x80 | (mapped & 63));
        buffer[3] = 0;
    } else {
        buffer[0] = static_cast<char>(0xF0 | (mapped >> 18));
        buffer[1] = static_cast<char>(0x80 | ((mapped >> 12) & 63));
        buffer[2] = static_cast<char>(0x80 | ((mapped >> 6) & 63));
        buffer[3] = static_cast<char>(0x80 | (mapped & 63));
        buffer[4] = 0;
    }
    return buffer;
}

} // namespace vpam
