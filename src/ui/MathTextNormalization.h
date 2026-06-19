/*
 * NeoCalculator - NumOS
 * Copyright (C) 2026 Juan Ramon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstddef>
#include <cstring>

#include "MathSymbols.h"

namespace numos::mathsym {

inline constexpr std::size_t kNormalizedTextBufferBytes = 96;

enum class NormalizeTextStatus : unsigned char {
    Unchanged,
    Buffered,
    InsufficientBuffer,
};

struct NormalizedMathText {
    const char* source;
    const char* text;
    NormalizeTextStatus status;
};

struct TextAlias {
    const char* token;
    const char* utf8;
};

// Additional ASCII token -> UTF-8 fallback map for broad math symbol input.
inline constexpr TextAlias kExtendedTextAliases[] = {
    {"\\alpha",  SYMB_ALPHA}, {"\\beta",   SYMB_BETA}, {"\\Gamma", "\xCE\x93"},
    {"\\gamma",  SYMB_GAMMA}, {"\\Delta",  "\xCE\x94"}, {"\\delta", "\xCE\xB4"},
    {"\\epsilon","\xCE\xB5"}, {"\\zeta",   "\xCE\xB6"}, {"\\eta",   "\xCE\xB7"},
    {"\\Theta",  "\xCE\x98"}, {"\\theta",  "\xCE\xB8"}, {"\\iota",  "\xCE\xB9"},
    {"\\kappa",  "\xCE\xBA"}, {"\\Lambda", "\xCE\x9B"}, {"\\lambda","\xCE\xBB"},
    {"\\mu",     "\xCE\xBC"}, {"\\nu",     "\xCE\xBD"}, {"\\Xi",    "\xCE\x9E"},
    {"\\xi",     "\xCE\xBE"}, {"\\Pi",     "\xCE\xA0"}, {"\\pi",    "\xCF\x80"},
    {"\\rho",    "\xCF\x81"}, {"\\Sigma",  "\xCE\xA3"}, {"\\sigma", "\xCF\x83"},
    {"\\tau",    "\xCF\x84"}, {"\\Upsilon","\xCE\xA5"}, {"\\upsilon","\xCF\x85"},
    {"\\Phi",    "\xCE\xA6"}, {"\\phi",    "\xCF\x86"}, {"\\chi",   "\xCF\x87"},
    {"\\Psi",    "\xCE\xA8"}, {"\\psi",    "\xCF\x88"}, {"\\Omega", "\xCE\xA9"},
    {"\\omega",  "\xCF\x89"},
    {"\\infty",  SYMB_INFINITY}, {"\\partial",SYMB_PARTIAL}, {"\\nabla",SYMB_NABLA},
    {"\\int",    SYMB_INT}, {"\\iint",  "\xE2\x88\xAC"}, {"\\iiint","\xE2\x88\xAD"},
    {"\\oint",   "\xE2\x88\xAE"}, {"\\oiint", "\xE2\x88\xAF"},
    {"\\forall", "\xE2\x88\x80"}, {"\\exists","\xE2\x88\x83"}, {"\\nexists","\xE2\x88\x84"},
    {"\\therefore","\xE2\x88\xB4"}, {"\\because","\xE2\x88\xB5"}, {"\\implies","\xE2\x87\x92"},
    {"\\iff",    "\xE2\x87\x94"}, {"\\neg",   "\xC2\xAC"}, {"\\land", "\xE2\x88\xA7"},
    {"\\lor",    "\xE2\x88\xA8"}, {"\\in",    SYMB_IN}, {"\\notin",SYMB_NOT_IN},
    {"\\subset", SYMB_SUBSET}, {"\\subseteq",SYMB_SUBSETEQ}, {"\\cup",SYMB_UNION},
    {"\\cap",    SYMB_INTERSECTION}, {"\\setminus","\xE2\x88\x96"}, {"\\emptyset",SYMB_EMPTY_SET},
    {"\\equiv",  "\xE2\x89\xA1"}, {"\\notequiv","\xE2\x89\xA2"}, {"\\to",SYMB_ARROW_R},
    {"\\leftarrow",SYMB_ARROW_L}, {"\\leftrightarrow",SYMB_ARROW_LR}, {"\\oplus","\xE2\x8A\x95"},
    {"\\propto", "\xE2\x88\x9D"}, {"\\otimes","\xE2\x8A\x97"}, {"\\perp","\xE2\x8A\xA5"},
    {"\\parallel","\xE2\x88\xA5"}, {"\\angle","\xE2\x88\xA0"}, {"\\cong","\xE2\x89\x85"},
    {"\\sim",    "\xE2\x88\xBC"}, {"\\approx","\xE2\x89\x88"}, {"\\degree","\xC2\xB0"},
    {"\\triangle","\xE2\x96\xB3"}, {"\\leq",SYMB_LEQ}, {"\\geq",SYMB_GEQ},
    {"\\neq",    SYMB_NEQ}, {"\\mp","\xE2\x88\x93"}, {"\\times",SYMB_TIMES},
    {"\\ll",     "\xE2\x89\xAA"}, {"\\gg","\xE2\x89\xAB"}, {"\\circ","\xE2\x88\x98"},
    {"\\square", "\xE2\x96\xA1"}, {"\\aleph_0","\xE2\x84\xB5\xE2\x82\x80"},
    {"\\lfloor", "\xE2\x8C\x8A"}, {"\\rfloor","\xE2\x8C\x8B"}, {"\\lceil","\xE2\x8C\x88"},
    {"\\rceil",  "\xE2\x8C\x89"}, {"\\dagger","\xE2\x80\xA0"}, {"\\ast","\xE2\x88\x97"},
    {"\\hbar",   "\xE2\x84\x8F"}, {"\\mathbbN",SYMB_N}, {"\\mathbbZ",SYMB_Z},
    {"\\mathbbQ",SYMB_Q}, {"\\mathbbR",SYMB_REAL}, {"\\mathbbC",SYMB_COMPLEX},
    {"\\mathbbH","\xE2\x84\x8D"}
};

struct TextReplacement {
    const char* token;
    const char* utf8;
    std::size_t tokenLen;
    std::size_t utf8Len;
};

inline bool startsWithToken(const char* text, const char* token) {
    return std::strncmp(text, token, std::strlen(token)) == 0;
}

inline TextReplacement findTextReplacementAt(const char* text) {
    for (const auto& entry : kVpamSymbolMap) {
        if (startsWithToken(text, entry.token)) {
            return {entry.token, entry.glyph,
                    std::strlen(entry.token), std::strlen(entry.glyph)};
        }
    }

    for (const auto& entry : kExtendedTextAliases) {
        if (startsWithToken(text, entry.token)) {
            return {entry.token, entry.utf8,
                    std::strlen(entry.token), std::strlen(entry.utf8)};
        }
    }

    return {nullptr, nullptr, 0, 0};
}

inline NormalizedMathText normalizeMathTextNoAlloc(const char* input,
                                                   char* buffer,
                                                   std::size_t bufferSize) {
    if (input == nullptr) {
        return {nullptr, nullptr, NormalizeTextStatus::Unchanged};
    }

    bool changed = false;
    std::size_t outputLen = 0;
    for (const char* p = input; *p != '\0';) {
        const TextReplacement replacement = findTextReplacementAt(p);
        if (replacement.token != nullptr) {
            changed = true;
            outputLen += replacement.utf8Len;
            p += replacement.tokenLen;
        } else {
            ++outputLen;
            ++p;
        }
    }

    if (!changed) {
        return {input, input, NormalizeTextStatus::Unchanged};
    }

    if (buffer == nullptr || bufferSize == 0 || outputLen + 1 > bufferSize) {
        return {input, input, NormalizeTextStatus::InsufficientBuffer};
    }

    char* out = buffer;
    for (const char* p = input; *p != '\0';) {
        const TextReplacement replacement = findTextReplacementAt(p);
        if (replacement.token != nullptr) {
            std::memcpy(out, replacement.utf8, replacement.utf8Len);
            out += replacement.utf8Len;
            p += replacement.tokenLen;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';

    return {input, buffer, NormalizeTextStatus::Buffered};
}

} // namespace numos::mathsym
