/*
 * NumOS — Glyph Assembly Engine
 * Copyright (C) 2026 Juan Ramon
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 4: DelimiterAssembler — Canonical OpenType 1.9.1 glyph assembly
 * for scalable math delimiters and large operators.
 *
 * Implements Spec §4.2 (MathVariants), §4.3 (GlyphAssembly constraints),
 * and §4.4 (size variant selection).
 *
 * The 6-state FSM translates OpenType's native bottom-to-top vertical
 * piece ordering into top-to-bottom for execution in the renderer,
 * enforcing Gecko's production constraints:
 *   - Maximum 3 non-extender pieces
 *   - Exactly one distinct extender glyph per assembly table
 *   - Size variants iteration starts at index 0 (the base glyph)
 */

#pragma once

#include <cstdint>
#include "stix_math_variants.h"

namespace vpam {

// ════════════════════════════════════════════════════════════════════════════
// DelimiterAssembler — Pure-static glyph assembly engine
//
// Designed for zero heap allocation: all results fit in stack-allocated
// structs with fixed-size arrays. No STL containers, no dynamic memory.
//
// Usage:
//   auto result = DelimiterAssembler::assemble(targetHeightPx, *table, emSize);
//   // result.parts[] now holds top-to-bottom ordered glyphs
//   // result.widthPx is the horizontal advance for layout
// ════════════════════════════════════════════════════════════════════════════
class DelimiterAssembler {
public:
    /// Maximum parts in any assembly result (stack allocation bound).
    static constexpr uint8_t kMaxParts = 8;

    /// A single piece in the assembled output (top-to-bottom order).
    struct Piece {
        uint32_t glyphCp;           ///< Unicode codepoint to render
        bool     isExtender;        ///< true ⇒ repeat this piece to fill the gap
        int16_t  heightPx;          ///< Glyph full advance height in pixels (raw, before overlap)
        int16_t  startConnectorPx;  ///< Connector length at glyph top (DU→px, for overlap calc)
        int16_t  endConnectorPx;    ///< Connector length at glyph bottom (DU→px, for overlap calc)
        int16_t  overlapAfterPx;    ///< Chosen connector overlap to next piece.
    };

    /// Complete assembly result — stack-allocated, no heap.
    struct AssemblyResult {
        Piece   pieces[kMaxParts];
        uint8_t pieceCount;      ///< Number of valid entries in pieces[]
        int16_t widthPx;         ///< Horizontal advance of the assembled glyph
        int16_t totalHeightPx;    ///< Total height of the assembled glyph
        bool    isAssembly;      ///< true = multi-part, false = single variant glyph
        bool    valid;           ///< false on error (fallback to base glyph)

        AssemblyResult()
            : pieceCount(0), widthPx(0), totalHeightPx(0),
              isAssembly(false), valid(false) {}
    };

    /// Selected glyph metrics for a target height.
    /// For assembled glyphs, glyphCp is 0 because multiple pieces are used.
    struct GlyphMetrics {
        uint32_t glyphCp;       ///< Selected single glyph, or 0 for assembly.
        int16_t  widthPx;       ///< Horizontal advance in pixels.
        int16_t  heightPx;      ///< Selected/assembled height in pixels.
        bool     isAssembly;    ///< true when multiple pieces are required.
        bool     valid;         ///< false when no table exists.
    };

    /// Assemble (or select a size variant for) a delimiter/operator glyph.
    ///
    /// Algorithm (OpenType 1.9.1 §4.2, §4.4):
    ///   1. Search size variants from index 0 (base glyph) upward.
    ///      If a variant's height ≥ targetHeight, use it as a single glyph.
    ///   2. If no size variant covers the target height, run the assembly FSM:
    ///      a. Parse assembly parts bottom-to-top (OpenType storage order).
    ///      b. Validate: ≤3 non-extender pieces, exactly 1 extender.
    ///      c. Reorder to top-to-bottom for renderer execution.
    ///   3. Reduce connector overlap between adjacent pieces (Step 2).
    ///   4. Inject additional extenders until target height is met (Step 3).
    ///
    /// @param targetHeightPx  Required height in pixels.
    /// @param table           Variant/assembly data for this glyph.
    /// @param emSizePx        Font em-size in pixels (for DU→px conversion).
    /// @return AssemblyResult with top-to-bottom ordered pieces.
    static AssemblyResult assemble(int16_t targetHeightPx,
                                   const MathVariantTable& table,
                                   int16_t emSizePx);

    /// Compute the pixel width of a base glyph from the variant table.
    /// Returns 0 if no table is found for the given codepoint.
    static int16_t glyphWidthPx(uint32_t baseCp, int16_t emSizePx);

    /// Compute selected glyph metrics for the requested target height.
    /// This mirrors assemble() so layout, cursor, and drawing agree.
    static GlyphMetrics glyphMetricsForHeightPx(uint32_t baseCp,
                                                int16_t targetHeightPx,
                                                int16_t emSizePx);

private:
    DelimiterAssembler() = delete;  // Pure static

    /// 6-state Gecko-style FSM: validate and split assembly parts.
    enum class FsmState : uint8_t {
        START = 0,
        BOTTOM_PIECES,    // Reading non-extender pieces at start (OpenType bottom)
        EXTENDER_SEEN,     // Extender piece found (the repeatable one)
        TOP_PIECES,        // Reading non-extender pieces after extender (OpenType top)
        DONE,              // Terminal valid state after all pieces are consumed
        ERROR
    };

    /// Internal FSM result: bottom/extender/top split with validation.
    struct FsmResult {
        const GlyphPartRecord* bottomParts[3];  // Non-extender at start
        uint8_t bottomCount;
        const GlyphPartRecord* extenderPart;    // The one extender (nullptr if none)
        const GlyphPartRecord* topParts[3];     // Non-extender after extender
        uint8_t topCount;
        bool valid;
    };

    /// Run the validation FSM on raw assembly parts (bottom-to-top order).
    static FsmResult validateAndSplit(const GlyphPartRecord* parts,
                                      uint8_t count);

    /// Convert design units to pixels (ceiling for heights, round for widths).
    static int16_t duToPxCeil(int16_t du, int16_t emSizePx);
    static int16_t duToPxRound(int16_t du, int16_t emSizePx);
};

}  // namespace vpam
