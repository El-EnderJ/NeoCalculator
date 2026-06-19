/*
 * NumOS - Glyph Assembly Engine Implementation
 * Copyright (C) 2026 Juan Ramon
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "MathGlyphAssembly.h"

#include <algorithm>

#include "../MathTypography.h"

namespace vpam {

int16_t DelimiterAssembler::duToPxCeil(int16_t du, int16_t emSizePx) {
    const int32_t num = static_cast<int32_t>(du) * emSizePx + kStixMathUPM - 1;
    return static_cast<int16_t>(num / kStixMathUPM);
}

int16_t DelimiterAssembler::duToPxRound(int16_t du, int16_t emSizePx) {
    const int32_t num = static_cast<int32_t>(du) * emSizePx + kStixMathUPM / 2;
    return static_cast<int16_t>(num / kStixMathUPM);
}

DelimiterAssembler::FsmResult
DelimiterAssembler::validateAndSplit(const GlyphPartRecord* parts,
                                     uint8_t count) {
    FsmResult result{};
    result.valid = false;
    result.extenderPart = nullptr;

    if (count == 0 || parts == nullptr) {
        result.valid = true;
        return result;
    }

    FsmState state = FsmState::START;
    uint8_t nonExtenderCount = 0;

    for (uint8_t i = 0; i < count; ++i) {
        const GlyphPartRecord& part = parts[i];

        if (part.isExtender) {
            switch (state) {
                case FsmState::START:
                case FsmState::BOTTOM_PIECES:
                    if (result.extenderPart != nullptr) return result;
                    result.extenderPart = &part;
                    state = FsmState::EXTENDER_SEEN;
                    break;
                case FsmState::EXTENDER_SEEN:
                case FsmState::TOP_PIECES:
                default:
                    return result;
            }
        } else {
            ++nonExtenderCount;
            if (nonExtenderCount > 3) return result;

            switch (state) {
                case FsmState::START:
                case FsmState::BOTTOM_PIECES:
                    result.bottomParts[result.bottomCount++] = &part;
                    state = FsmState::BOTTOM_PIECES;
                    break;
                case FsmState::EXTENDER_SEEN:
                case FsmState::TOP_PIECES:
                    result.topParts[result.topCount++] = &part;
                    state = FsmState::TOP_PIECES;
                    break;
                default:
                    return result;
            }
        }
    }

    state = FsmState::DONE;
    (void)state;
    result.valid = true;
    return result;
}

DelimiterAssembler::AssemblyResult
DelimiterAssembler::assemble(int16_t targetHeightPx,
                             const MathVariantTable& table,
                             int16_t emSizePx) {
    AssemblyResult result;

    int16_t bestH = duToPxCeil(table.baseHeight, emSizePx);
    uint32_t bestCp = table.baseCodepoint;
    int16_t bestW = duToPxRound(table.baseWidth, emSizePx);

    for (uint8_t i = 0; i < table.sizeVariantCount; ++i) {
        const auto& sv = table.sizeVariants[i];
        const int16_t svH = duToPxCeil(sv.height, emSizePx);
        const int16_t svW = duToPxRound(sv.glyphWidth, emSizePx);
        if (svH >= targetHeightPx) {
            bestH = svH;
            bestCp = sv.glyphCp;
            bestW = svW;
            break;
        }
        if (svH > bestH) {
            bestH = svH;
            bestCp = sv.glyphCp;
            bestW = svW;
        }
    }

    auto returnBestVariant = [&]() {
        result.pieces[0] = { bestCp, false, bestH, 0, 0, 0 };
        result.pieceCount = 1;
        result.widthPx = bestW;
        result.totalHeightPx = bestH;
        result.isAssembly = false;
        result.valid = true;
    };

    if (bestH >= targetHeightPx) {
        returnBestVariant();
        return result;
    }

    if (table.assemblyPartCount == 0 || table.assemblyParts == nullptr) {
        returnBestVariant();
        return result;
    }

    const FsmResult fsm = validateAndSplit(table.assemblyParts,
                                           table.assemblyPartCount);
    if (!fsm.valid || fsm.extenderPart == nullptr) {
        returnBestVariant();
        return result;
    }

    struct TempPiece {
        uint32_t glyphCp;
        bool isExtender;
        int16_t rawAdvancePx;
        uint8_t partIdx;
    };

    auto partConnectorStart = [&](uint8_t pi) -> int16_t {
        return duToPxCeil(table.assemblyParts[pi].startConnector, emSizePx);
    };
    auto partConnectorEnd = [&](uint8_t pi) -> int16_t {
        return duToPxCeil(table.assemblyParts[pi].endConnector, emSizePx);
    };

    const uint8_t extPartIdx = static_cast<uint8_t>(
        fsm.extenderPart - table.assemblyParts);
    const int16_t extAdvancePx = duToPxCeil(
        fsm.extenderPart->fullAdvance, emSizePx);

    auto buildTemp = [&](uint8_t extenderCopies, TempPiece* temp,
                         uint8_t& tempCount) {
        tempCount = 0;

        for (int8_t i = static_cast<int8_t>(fsm.topCount) - 1;
             i >= 0 && tempCount < kMaxParts; --i) {
            const auto* part = fsm.topParts[i];
            const uint8_t pi = static_cast<uint8_t>(part - table.assemblyParts);
            temp[tempCount++] = {
                part->glyphCp, false,
                duToPxCeil(part->fullAdvance, emSizePx), pi
            };
        }

        for (uint8_t i = 0; i < extenderCopies && tempCount < kMaxParts; ++i) {
            temp[tempCount++] = {
                fsm.extenderPart->glyphCp, true, extAdvancePx, extPartIdx
            };
        }

        for (uint8_t i = 0; i < fsm.bottomCount && tempCount < kMaxParts; ++i) {
            const auto* part = fsm.bottomParts[i];
            const uint8_t pi = static_cast<uint8_t>(part - table.assemblyParts);
            temp[tempCount++] = {
                part->glyphCp, false,
                duToPxCeil(part->fullAdvance, emSizePx), pi
            };
        }
    };

    auto rawHeight = [](const TempPiece* temp, uint8_t count) -> int16_t {
        int16_t total = 0;
        for (uint8_t i = 0; i < count; ++i) {
            total = static_cast<int16_t>(total + temp[i].rawAdvancePx);
        }
        return total;
    };

    auto maxOverlapAt = [&](const TempPiece* temp, uint8_t i) -> int16_t {
        return static_cast<int16_t>(std::min(
            partConnectorEnd(temp[i].partIdx),
            partConnectorStart(temp[i + 1].partIdx)));
    };

    auto minHeightWithMaxOverlap = [&](const TempPiece* temp,
                                       uint8_t count) -> int16_t {
        int16_t total = rawHeight(temp, count);
        for (uint8_t i = 0; i + 1 < count; ++i) {
            total = static_cast<int16_t>(total - maxOverlapAt(temp, i));
        }
        return total;
    };

    TempPiece temp[kMaxParts];
    uint8_t tempCount = 0;
    uint8_t extenderCopies = 0;
    buildTemp(extenderCopies, temp, tempCount);

    // WHY: OpenType assembly grows by reducing connector overlap before
    // inserting another extender. That avoids visibly oversized delimiters.
    while (tempCount < kMaxParts && rawHeight(temp, tempCount) < targetHeightPx) {
        ++extenderCopies;
        buildTemp(extenderCopies, temp, tempCount);
    }

    const int16_t rawH = rawHeight(temp, tempCount);
    const int16_t minH = minHeightWithMaxOverlap(temp, tempCount);
    const int16_t desiredH = static_cast<int16_t>(
        std::max<int16_t>(minH, std::min<int16_t>(targetHeightPx, rawH)));
    int16_t overlapBudget = static_cast<int16_t>(rawH - desiredH);

    int16_t chosenOverlap[kMaxParts] = {};
    // Step 2: extend all connections EQUALLY by reducing overlap (Spec §4.2).
    // First count seams with non-zero overlap capacity, then distribute the
    // overlapBudget evenly across all of them so no single seam is compressed
    // more than its neighbours.
    {
        uint8_t seamCount = 0;
        for (uint8_t i = 0; i + 1 < tempCount; ++i) {
            if (maxOverlapAt(temp, i) > 0) ++seamCount;
        }
        if (seamCount > 0) {
            const int16_t perSeam = static_cast<int16_t>(overlapBudget / seamCount);
            int16_t remainder = static_cast<int16_t>(overlapBudget % seamCount);
            for (uint8_t i = 0; i + 1 < tempCount; ++i) {
                const int16_t maxOv = maxOverlapAt(temp, i);
                if (maxOv > 0) {
                    int16_t share = perSeam;
                    if (remainder > 0) { ++share; --remainder; }
                    chosenOverlap[i] = std::min(maxOv, share);
                }
            }
        }
    }

    int16_t actualH = rawH;
    for (uint8_t i = 0; i + 1 < tempCount; ++i) {
        actualH = static_cast<int16_t>(actualH - chosenOverlap[i]);
    }

    const int16_t asmWidthPx = duToPxRound(table.assemblyParts[0].glyphWidth,
                                           emSizePx);
    uint8_t outIdx = 0;
    for (uint8_t i = 0; i < tempCount && outIdx < kMaxParts; ++i) {
        result.pieces[outIdx++] = {
            temp[i].glyphCp,
            temp[i].isExtender,
            temp[i].rawAdvancePx,
            partConnectorStart(temp[i].partIdx),
            partConnectorEnd(temp[i].partIdx),
            static_cast<int16_t>((i + 1 < tempCount) ? chosenOverlap[i] : 0)
        };
    }

    result.pieceCount = outIdx;
    result.widthPx = asmWidthPx;
    result.totalHeightPx = actualH;
    result.isAssembly = (outIdx > 1);
    result.valid = true;
    return result;
}

int16_t DelimiterAssembler::glyphWidthPx(uint32_t baseCp, int16_t emSizePx) {
    const MathVariantTable* table = lookupVariantTable(baseCp);
    if (!table) return 0;

    return duToPxRound(table->baseWidth, emSizePx);
}

DelimiterAssembler::GlyphMetrics
DelimiterAssembler::glyphMetricsForHeightPx(uint32_t baseCp,
                                            int16_t targetHeightPx,
                                            int16_t emSizePx) {
    const MathVariantTable* table = lookupVariantTable(baseCp);
    if (!table) {
        return { 0, 0, 0, false, false };
    }

    const AssemblyResult assembly = assemble(targetHeightPx, *table, emSizePx);
    if (!assembly.valid) {
        return { 0, 0, 0, false, false };
    }

    return {
        assembly.isAssembly ? 0 : assembly.pieces[0].glyphCp,
        assembly.widthPx,
        assembly.totalHeightPx,
        assembly.isAssembly,
        true
    };
}

}  // namespace vpam
