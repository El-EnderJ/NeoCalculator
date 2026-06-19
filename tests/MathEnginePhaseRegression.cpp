#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "src/math/MathAST.h"
#include "src/math/MathRenderVisualCases.h"
#include "src/math/MathStressExpressions.h"
#include "src/math/font/MathGlyphAssembly.h"
#include "src/ui/MathTextNormalization.h"

using namespace vpam;

static int failures = 0;

static void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "FAIL: " << name << "\n";
        ++failures;
    }
}

static std::string readTextFile(const char* path) {
    std::ifstream in(path);
    if (!in) return {};

    std::string out;
    std::string line;
    while (std::getline(in, line)) {
        out += line;
        out += '\n';
    }
    return out;
}

static std::string platformioEnvSection(const std::string& text,
                                        const char* envName) {
    const std::string header = std::string("[env:") + envName + "]";
    std::size_t start = text.find(header);
    while (start != std::string::npos && start != 0 && text[start - 1] != '\n') {
        start = text.find(header, start + header.size());
    }
    if (start == std::string::npos) return {};

    std::size_t end = text.find("\n[", start + header.size());
    if (end == std::string::npos) end = text.size();
    return text.substr(start, end - start);
}

static std::string platformioEffectiveEnvSection(const std::string& text,
                                                 const char* envName,
                                                 int depth = 0) {
    if (depth > 4) return {};
    const std::string section = platformioEnvSection(text, envName);
    if (section.empty()) return {};

    std::string effective = section;
    const std::string marker = "extends = env:";
    const std::size_t extendsPos = section.find(marker);
    if (extendsPos != std::string::npos) {
        const std::size_t nameStart = extendsPos + marker.size();
        std::size_t nameEnd = section.find_first_of(" \t\r\n", nameStart);
        if (nameEnd == std::string::npos) nameEnd = section.size();
        const std::string parent = section.substr(nameStart, nameEnd - nameStart);
        effective += '\n';
        effective += platformioEffectiveEnvSection(text, parent.c_str(), depth + 1);
    }
    return effective;
}

static bool containsFlag(const std::string& text, const char* flag) {
    return text.find(flag) != std::string::npos;
}

static void testPlatformioValidationFlagMatrix() {
    const std::string ini = readTextFile("platformio.ini");
    check(!ini.empty(), "platformio.ini is readable for build flag checks");

    const std::string production =
        platformioEffectiveEnvSection(ini, "esp32s3_n16r8");
    const std::string validation =
        platformioEffectiveEnvSection(ini, "esp32s3_n16r8_validate");
    const std::string overlay =
        platformioEffectiveEnvSection(ini, "esp32s3_n16r8_validate_overlay");
    const std::string sup1 =
        platformioEffectiveEnvSection(ini, "esp32s3_n16r8_validate_sup1");
    const std::string usbcdc =
        platformioEffectiveEnvSection(ini, "esp32s3_n16r8_validate_usbcdc");

    check(!production.empty(), "production PlatformIO environment exists");
    check(!validation.empty(), "clean validation PlatformIO environment exists");
    check(!overlay.empty(), "overlay validation PlatformIO environment exists");
    check(!sup1.empty(), "superscript +1 validation PlatformIO environment exists");
    check(!usbcdc.empty(), "USB CDC validation PlatformIO environment exists");

    check(!containsFlag(production, "NUMOS_MATH_VISUAL_VERIFY"),
          "production does not define NUMOS_MATH_VISUAL_VERIFY");
    check(!containsFlag(production, "NUMOS_MATH_INK_OVERLAY"),
          "production does not define NUMOS_MATH_INK_OVERLAY");
    check(!containsFlag(production, "NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX"),
          "production does not define superscript VPAM adjustment");
    check(containsFlag(production, "-DNUMOS_SERIAL_BACKEND_UART0=1"),
          "production selects UART0/CH343 serial backend");
    check(!containsFlag(production, "-DNUMOS_SERIAL_BACKEND_USB_CDC=1"),
          "production does not select native USB CDC serial backend");
    check(containsFlag(production, "-DARDUINO_USB_MODE=0"),
          "production keeps native USB mode disabled for CH343 UART");
    check(containsFlag(production, "-DARDUINO_USB_CDC_ON_BOOT=0"),
          "production keeps USB CDC on boot disabled for CH343 UART");
    check(!containsFlag(production, "-DARDUINO_USB_MODE=1"),
          "production does not route Serial to native USB mode");
    check(!containsFlag(production, "-DARDUINO_USB_CDC_ON_BOOT=1"),
          "production does not route SerialBridge to native USB CDC");

    check(containsFlag(ini, "monitor_rts     = 0"),
          "PlatformIO monitor leaves RTS deasserted");
    check(containsFlag(ini, "monitor_dtr     = 0"),
          "PlatformIO monitor leaves DTR deasserted");

    check(containsFlag(validation, "NUMOS_MATH_VISUAL_VERIFY"),
          "clean validation defines NUMOS_MATH_VISUAL_VERIFY");
    check(containsFlag(validation, "-DNUMOS_SERIAL_BACKEND_UART0=1"),
          "clean validation uses UART0/CH343 serial backend");
    check(!containsFlag(validation, "-DNUMOS_SERIAL_BACKEND_USB_CDC=1"),
          "clean validation does not select native USB CDC serial backend");
    check(!containsFlag(validation, "NUMOS_MATH_INK_OVERLAY"),
          "clean validation does not define NUMOS_MATH_INK_OVERLAY");
    check(!containsFlag(validation, "NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX"),
          "clean validation keeps default superscript adjustment");

    check(containsFlag(overlay, "NUMOS_MATH_VISUAL_VERIFY"),
          "overlay validation inherits NUMOS_MATH_VISUAL_VERIFY");
    check(containsFlag(overlay, "NUMOS_MATH_INK_OVERLAY"),
          "overlay validation defines NUMOS_MATH_INK_OVERLAY");

    check(containsFlag(sup1, "NUMOS_MATH_VISUAL_VERIFY"),
          "sup1 validation inherits NUMOS_MATH_VISUAL_VERIFY");
    check(!containsFlag(sup1, "NUMOS_MATH_INK_OVERLAY"),
          "sup1 validation keeps overlays disabled");
    check(containsFlag(sup1, "NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX=1"),
          "sup1 validation defines the opt-in +1 superscript adjustment");

    check(containsFlag(usbcdc, "NUMOS_MATH_VISUAL_VERIFY"),
          "USB CDC validation keeps Math Visual enabled");
    check(containsFlag(usbcdc, "-DNUMOS_SERIAL_BACKEND_USB_CDC=1"),
          "USB CDC validation selects native USB CDC serial backend");
    check(containsFlag(usbcdc, "-DARDUINO_USB_MODE=1"),
          "USB CDC validation enables native USB mode");
    check(containsFlag(usbcdc, "-DARDUINO_USB_CDC_ON_BOOT=1"),
          "USB CDC validation enables USB CDC on boot");
}

static NodePtr rowWithNumber(const char* value) {
    auto row = makeRow();
    static_cast<NodeRow*>(row.get())->appendChild(makeNumber(value));
    return row;
}

static NodePtr rowWithVariable(char name) {
    auto row = makeRow();
    static_cast<NodeRow*>(row.get())->appendChild(makeVariable(name));
    return row;
}

static MathVariantTable overlapTable() {
    static constexpr GlyphPartRecord parts[] = {
        { 0x1001, 100, 40, 50, 50, false },
        { 0x1002, 100, 40, 50, 50, true  },
        { 0x1003, 100, 40, 50, 50, false },
    };
    static constexpr MathVariantTable table = {
        0x1000, 50, 40,
        nullptr, 0,
        parts, 3,
        0,
    };
    return table;
}

static MathVariantTable noAssemblyVariantTable() {
    static constexpr SizeVariantRecord variants[] = {
        { 0x2001, 150, 50 },
        { 0x2002, 220, 70 },
    };
    static constexpr MathVariantTable table = {
        0x2000, 100, 40,
        variants, 2,
        nullptr, 0,
        0,
    };
    return table;
}

static void testAssemblyReducesOverlapBeforeAddingExtenders() {
    auto table = overlapTable();
    auto result = DelimiterAssembler::assemble(240, table, 1000);

    check(result.valid, "assembly valid");
    check(result.pieceCount == 3, "Step 2 grows by reducing overlap before adding extenders");
    check(result.totalHeightPx == 240, "Step 2 records exact reduced-overlap height");
    check(result.pieces[0].overlapAfterPx + result.pieces[1].overlapAfterPx == 60,
          "Step 2 stores reduced connector overlaps for renderer");
}

static void testAssemblyStartsWithZeroExtenders() {
    auto table = overlapTable();
    auto result = DelimiterAssembler::assemble(180, table, 1000);

    check(result.valid, "zero-extender assembly valid");
    check(result.pieceCount == 2,
          "Step 1 tries non-extender assembly before adding an extender");
    check(!result.pieces[0].isExtender && !result.pieces[1].isExtender,
          "zero-extender assembly contains only non-extender pieces");
    check(result.totalHeightPx == 180,
          "zero-extender Step 2 reduces overlap to exact target");
}

static void testAssemblyKeepsInsertedExtendersContiguous() {
    auto table = overlapTable();
    auto result = DelimiterAssembler::assemble(360, table, 1000);

    check(result.valid, "large assembly valid");
    check(result.pieceCount == 4, "large assembly uses one extra extender");
    check(!result.pieces[0].isExtender, "top piece remains first");
    check(result.pieces[1].isExtender && result.pieces[2].isExtender,
          "extra extenders stay contiguous in the middle");
    check(!result.pieces[3].isExtender, "bottom piece remains last");
    check(result.totalHeightPx == 360, "large assembly records exact final height");
}

static void testNoAssemblyFallsBackToLargestVariant() {
    auto table = noAssemblyVariantTable();
    auto result = DelimiterAssembler::assemble(400, table, 1000);

    check(result.valid, "no-assembly variant fallback valid");
    check(result.pieceCount == 1, "no-assembly fallback remains single glyph");
    check(result.pieces[0].glyphCp == 0x2002,
          "no-assembly fallback uses largest size variant, not base glyph");
    check(result.widthPx == 70 && result.totalHeightPx == 220,
          "no-assembly fallback reports largest variant metrics");
}

static void testGlyphWidthPxIsBaseWidthOnly() {
    const int16_t sumBaseW = DelimiterAssembler::glyphWidthPx(0x2211, 1000);
    const auto sumDisplay = DelimiterAssembler::glyphMetricsForHeightPx(
        0x2211, 2000, 1000);

    check(sumBaseW == kVarTable_2211.baseWidth,
          "glyphWidthPx returns base glyph width only");
    check(sumDisplay.valid && sumDisplay.widthPx >= sumBaseW,
          "target-aware glyph metrics own variant width selection");
}

static void testNestedPowerReachesScriptScriptLevel() {
    auto innerPower = makePower(rowWithNumber("x"), rowWithNumber("2"));
    auto* innerPowerRaw = innerPower.get();
    auto root = makePower(rowWithNumber("e"), std::move(innerPower));

    FontMetrics fm = defaultFontMetrics();
    FontMetrics fmScript = fm.superscript();
    fm.script = &fmScript;
    root->calculateLayout(fm);

    auto* innerExponent = innerPowerRaw->child(1);
    check(innerPowerRaw->scriptLevel() == 1, "nested power is script level 1");
    check(innerExponent && innerExponent->scriptLevel() == 2,
          "nested exponent reaches scriptscript level 2");
}

static void testSpaceAfterScriptAffectsPowerAndSubscriptWidth() {
    FontMetrics fm = defaultFontMetrics();
    const int16_t scriptSpace = spaceAfterScriptPx(fm);

    auto power = makePower(rowWithNumber("x"), rowWithNumber("2"));
    power->calculateLayout(fm);
    auto* powerNode = static_cast<NodePower*>(power.get());
    const int16_t expectedPowerWidth = static_cast<int16_t>(
        powerNode->base()->layout().width + powerNode->italicCorrectionPx()
        + powerNode->exponent()->layout().width + scriptSpace);
    check(power->layout().width == expectedPowerWidth,
          "NodePower width includes SpaceAfterScript");

    auto subscript = makeSubscript(rowWithNumber("x"), rowWithNumber("1"));
    subscript->calculateLayout(fm);
    auto* subNode = static_cast<NodeSubscript*>(subscript.get());
    const int16_t expectedSubWidth = static_cast<int16_t>(
        subNode->base()->layout().width + subNode->subscript()->layout().width
        + scriptSpace);
    check(subscript->layout().width == expectedSubWidth,
          "NodeSubscript width includes SpaceAfterScript");
}

static FontMetrics stixLikePowerTextMetrics(FontMetrics& scriptOut) {
    FontMetrics fm{};
    fm.charWidth = 10;
    fm.ascent = 12;
    fm.descent = 2;
    fm.lineAscent = 20;
    fm.lineDescent = 11;
    fm.capHeight = 12;
    fm.scriptLevel = 0;
    fm.script = &scriptOut;
    fm.emSize = 18;
    fm.style = MathStyle::TEXT;
    fm.numberAscent = 12;
    fm.numberDescent = 0;

    scriptOut = FontMetrics{};
    scriptOut.charWidth = 7;
    scriptOut.ascent = 8;
    scriptOut.descent = 1;
    scriptOut.lineAscent = 14;
    scriptOut.lineDescent = 8;
    scriptOut.capHeight = 8;
    scriptOut.scriptLevel = 1;
    scriptOut.script = nullptr;
    scriptOut.emSize = 12;
    scriptOut.style = MathStyle::SCRIPT;
    scriptOut.numberAscent = 8;
    scriptOut.numberDescent = 0;
    return fm;
}

static void testPowerSuperscriptPolicyUsesCentralAdjust() {
    FontMetrics scriptFm;
    FontMetrics fm = stixLikePowerTextMetrics(scriptFm);

    auto power = makePower(rowWithNumber("2"), rowWithNumber("2"));
    power->calculateLayout(fm);
    const auto* p = static_cast<const NodePower*>(power.get());
    const auto& baseL = p->base()->layout();
    const auto& expL = p->exponent()->layout();
    const SuperscriptShiftMetrics sup = superscriptShiftMetrics(fm, expL);

    const int16_t expectedShift = sup.shiftPx;
    const int16_t actualShift = static_cast<int16_t>(power->layout().ascent - expL.ascent);
    const int16_t expectedAscent = std::max<int16_t>(
        baseL.ascent,
        static_cast<int16_t>(expectedShift + expL.ascent));

#if defined(NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX)
    constexpr int16_t expectedConfiguredAdjust =
        static_cast<int16_t>(NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX);
#else
    constexpr int16_t expectedConfiguredAdjust = 0;
#endif

    check(kSuperscriptVpamAdjustPx == expectedConfiguredAdjust,
          "superscript VPAM adjustment macro maps to centralized constexpr");
    check(expL.descent == 1 && layoutInkDescentPx(expL) == 0,
          "STIX-like exponent keeps layout descent separate from visible digit ink");
    check(sup.configuredAdjustPx == expectedConfiguredAdjust,
          "superscript helper reports configured VPAM adjustment");
    check(sup.appliedAdjustPx == static_cast<int16_t>(sup.texShiftPx - sup.shiftPx),
          "superscript helper reports applied VPAM adjustment after clamps");
    if (expectedConfiguredAdjust == 0) {
        check(sup.shiftPx == sup.texShiftPx,
              "default superscript shift is TeX/OpenType with no VPAM adjustment");
        check(sup.appliedAdjustPx == 0,
              "default superscript policy applies no VPAM adjustment");
    } else if (expectedConfiguredAdjust == 1) {
        check(sup.shiftPx == static_cast<int16_t>(sup.texShiftPx - 1),
              "opt-in +1 VPAM policy lowers exponent by exactly 1px when unclamped");
        check(sup.appliedAdjustPx == 1,
              "opt-in +1 VPAM policy reports a 1px applied adjustment");
    }
    check(actualShift == expectedShift,
          "power exponent baseline uses centralized superscript shift");
    check(power->layout().ascent == expectedAscent,
          "power layout ascent follows the centralized superscript shift");
}

static void testPowerPublishesVisibleInkBounds() {
    FontMetrics scriptFm;
    FontMetrics fm = stixLikePowerTextMetrics(scriptFm);

    auto power = makePower(rowWithNumber("2"), rowWithNumber("2"));
    power->calculateLayout(fm);
    const auto* p = static_cast<const NodePower*>(power.get());
    const auto& baseL = p->base()->layout();
    const auto& expL = p->exponent()->layout();
    const int16_t expectedShift = superscriptShiftMetrics(fm, expL).shiftPx;

    const int16_t expectedInkAscent = std::max<int16_t>(
        layoutInkAscentPx(baseL),
        static_cast<int16_t>(expectedShift + layoutInkAscentPx(expL)));
    const int16_t expectedInkDescent = std::max<int16_t>(
        layoutInkDescentPx(baseL),
        static_cast<int16_t>(std::max<int16_t>(
            0, static_cast<int16_t>(layoutInkDescentPx(expL) - expectedShift))));

    check(layoutInkAscentPx(power->layout()) == expectedInkAscent,
          "power layout publishes visible ink ascent for fraction diagnostics");
    check(layoutInkDescentPx(power->layout()) == expectedInkDescent,
          "power layout publishes visible ink descent for fraction diagnostics");
    check(layoutInkDescentPx(power->layout()) < power->layout().descent,
          "power visible ink descent is not the conservative layout descent");
}

static void testPowerExponentClearsBaseBaseline() {
    FontMetrics scriptFm;
    FontMetrics fm = stixLikePowerTextMetrics(scriptFm);

    auto power = makePower(rowWithNumber("2"), rowWithNumber("2"));
    power->calculateLayout(fm);

    const auto* p = static_cast<const NodePower*>(power.get());
    const auto& expL = p->exponent()->layout();
    const SuperscriptShiftMetrics sup = superscriptShiftMetrics(fm, expL);

    const int16_t baseBaseline = 100;
    const int16_t expBaseline = static_cast<int16_t>(baseBaseline - sup.shiftPx);
    const int16_t expInkBot = static_cast<int16_t>(
        expBaseline + layoutInkDescentPx(expL));
    const int16_t clearance = static_cast<int16_t>(baseBaseline - expInkBot);

    check(expInkBot < baseBaseline,
          "power exponent visible ink stays above the base baseline");
    check(clearance >= sup.bottomMinPx,
          "power exponent respects OpenType bottom-min clearance");
}

static void checkPowerClearanceAccepted(NodePtr power, const FontMetrics& fm,
                                        const char* name) {
    power->calculateLayout(fm);

    const auto* p = static_cast<const NodePower*>(power.get());
    const auto& expL = p->exponent()->layout();
    const SuperscriptShiftMetrics sup = superscriptShiftMetrics(fm, expL);

    const int16_t baseBaseline = 120;
    const int16_t expBaseline = static_cast<int16_t>(baseBaseline - sup.shiftPx);
    const int16_t expInkBot = static_cast<int16_t>(
        expBaseline + layoutInkDescentPx(expL));
    const int16_t clearance = static_cast<int16_t>(baseBaseline - expInkBot);

#if !defined(NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX) || (NUMOS_SUPERSCRIPT_VPAM_ADJUST_PX == 0)
    check(kSuperscriptVpamAdjustPx == 0,
          (std::string(name) + " default superscript VPAM adjustment is 0").c_str());
    check(sup.shiftPx == sup.texShiftPx,
          (std::string(name) + " default exponent shift equals TeX/OpenType shift").c_str());
#endif
    check(expInkBot < baseBaseline,
          (std::string(name) + " exponent visible ink stays above base baseline").c_str());
    check(clearance >= sup.bottomMinPx,
          (std::string(name) + " exponent clearance status is clear").c_str());
}

static void testAcceptedPowerClearanceMatrix() {
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    checkPowerClearanceAccepted(
        makePower(rowWithNumber("2"), rowWithNumber("2")),
        fm,
        "2^2");
    checkPowerClearanceAccepted(
        makePower(rowWithVariable('x'), rowWithNumber("2")),
        fm,
        "x^2");
    checkPowerClearanceAccepted(
        makePower(rowWithVariable('x'), rowWithNumber("10")),
        fm,
        "x^10");

    auto fracBase = makeFraction(rowWithNumber("2"), rowWithNumber("3"));
    checkPowerClearanceAccepted(
        makePower(makeParen(std::move(fracBase)), rowWithNumber("2")),
        fm,
        "(2/3)^2");

    auto expRow = makeRow();
    static_cast<NodeRow*>(expRow.get())->appendChild(
        makeFraction(rowWithNumber("1"), rowWithNumber("2")));
    checkPowerClearanceAccepted(
        makePower(rowWithNumber("2"), std::move(expRow)),
        fm,
        "2^(1/2)");
}

static void testParenHonorsDelimitedSubFormulaMinHeight() {
    FontMetrics fm = defaultFontMetrics();
    auto paren = makeParen(rowWithNumber("1"));
    paren->calculateLayout(fm);

    const int16_t minHeight = MathConstantsProvider(fm.emSize).delimitedSubFormulaMinHeight();
    check(paren->layout().height() >= minHeight,
          "NodeParen target height honors DelimitedSubFormulaMinHeight");
}

static void testBigOpDisplayLayoutMatchesRendererGeometry() {
    FontMetrics fm = defaultFontMetrics();
    auto big = makeBigOp(BigOpKind::Product,
                         rowWithNumber("i=1"),
                         rowWithNumber("n"),
                         rowWithNumber("x"));
    big->calculateLayout(fm);
    auto* node = static_cast<NodeBigOp*>(big.get());

    check(node->useDisplayLimits(), "product uses display limits in display style");

    const auto& lowerL = node->lower()->layout();
    const auto& upperL = node->upper()->layout();
    const auto& bodyL = node->body()->layout();
    const auto opMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
        node->operatorCodepoint(), largeOperatorTargetHeightPx(fm), fm.emSize);
    const int16_t opH = opMetrics.valid ? opMetrics.heightPx : fm.height();
    const int16_t symbolAscent = largeOperatorGlyphAscentPx(fm, opH);
    const int16_t symbolDescent = largeOperatorGlyphDescentPx(fm, opH);
    const int16_t symbolPadPx = largeOperatorSymbolPadPx(fm);
    const int16_t limitGapPx = MathConstantsProvider(fm.emSize).upperLimitGapMin();
    const int16_t expectedAscent = static_cast<int16_t>(
        std::max<int16_t>(bodyL.ascent,
            static_cast<int16_t>(symbolAscent + symbolPadPx
                                 + limitGapPx + upperL.height())));
    const int16_t expectedDescent = static_cast<int16_t>(
        std::max<int16_t>(bodyL.descent,
            static_cast<int16_t>(symbolDescent + symbolPadPx
                                 + limitGapPx + lowerL.height())));

    check(big->layout().ascent == expectedAscent,
          "BigOp display ascent matches renderer displayLimitGeometry");
    check(big->layout().descent == expectedDescent,
          "BigOp display descent matches renderer displayLimitGeometry");
}

static void testMakeRelationProducesRelationAtom() {
    FontMetrics fm = defaultFontMetrics();
    auto relation = makeRelation(OpKind::Eq);
    relation->calculateLayout(fm);

    check(relation->mathClass() == MathClass::REL,
          "makeRelation produces a relation math atom");

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(rowWithNumber("a"));
    rowNode->appendChild(makeRelation(OpKind::Eq));
    rowNode->appendChild(rowWithNumber("b"));
    row->calculateLayout(fm);

    const int16_t relSpace = interAtomSpacingPx(
        MathClass::ORD, MathClass::REL, fm.style, fm.emSize);
    const int16_t expectedWidth = static_cast<int16_t>(
        rowNode->child(0)->layout().width + relSpace
        + rowNode->child(1)->layout().width + relSpace
        + rowNode->child(2)->layout().width);
    check(row->layout().width == expectedWidth,
          "relation atom participates in TeX inter-atom spacing");
}

static void testAssemblyOverlapDistributionIsEqual() {
    // Verify Step 2 distributes overlap reduction EQUALLY across all seams,
    // not greedily to the first one (Spec §4.2).
    auto table = overlapTable();
    auto result = DelimiterAssembler::assemble(240, table, 1000);

    check(result.valid, "equal-overlap assembly valid");
    // rawH=300, target=240 → overlapBudget=60, 2 seams → each gets 30
    check(result.pieces[0].overlapAfterPx == 30,
          "Step 2 distributes overlap equally: first seam gets 30");
    check(result.pieces[1].overlapAfterPx == 30,
          "Step 2 distributes overlap equally: second seam gets 30");
}

static void testDisplayLimitsHonorsOperatorMinHeightThreshold() {
    FontMetrics fm = defaultFontMetrics();

    // At 18pt: DisplayOperatorMinHeight = 1800 DU → 32 px
    // Integral base height = 700 DU → 13 px (well below threshold)
    const auto symMetrics = DelimiterAssembler::glyphMetricsForHeightPx(
        0x222B, largeOperatorTargetHeightPx(fm), fm.emSize);
    const int16_t symH = symMetrics.valid ? symMetrics.heightPx : fm.height();

    // ∫ at 18pt DISPLAY: height ≈ 17 px < 32 px threshold → should NOT use display limits
    bool usesDisplay = shouldUseDisplayLimits(fm, symH);
    check(!usesDisplay,
          "integral below DisplayOperatorMinHeight rejects display limits");

    // In TEXT style, should always reject display limits regardless of height
    FontMetrics textFm = fm;
    textFm.style = MathStyle::TEXT;
    bool textStyle = shouldUseDisplayLimits(textFm, 999);
    check(!textStyle,
          "inline style always rejects display limits regardless of height");

    // A large-enough operator should use display limits
    bool megaOp = shouldUseDisplayLimits(fm, 50);
    check(megaOp,
          "operator taller than DisplayOperatorMinHeight uses display limits");
}

static void testDelimKindCodepointMapping() {
    check(delimLeftCp(DelimKind::Paren) == 0x0028, "paren left cp");
    check(delimRightCp(DelimKind::Paren) == 0x0029, "paren right cp");
    check(delimLeftCp(DelimKind::Bracket) == 0x005B, "bracket left cp");
    check(delimRightCp(DelimKind::Bracket) == 0x005D, "bracket right cp");
    check(delimLeftCp(DelimKind::Brace) == 0x007B, "brace left cp");
    check(delimRightCp(DelimKind::Brace) == 0x007D, "brace right cp");
    check(delimLeftCp(DelimKind::Bar) == 0x007C, "bar left cp");
    check(delimRightCp(DelimKind::Bar) == 0x007C, "bar right cp (both sides same)");
}

static void testBracketDelimiterLayout() {
    FontMetrics fm = defaultFontMetrics();

    auto bracket = makeBracket(rowWithNumber("x"));
    auto* node = static_cast<NodeParen*>(bracket.get());
    bracket->calculateLayout(fm);

    check(node->delimKind() == DelimKind::Bracket,
          "bracket delimiter preserves its kind");
    check(node->leftCp() == 0x005B && node->rightCp() == 0x005D,
          "bracket delimiter has correct codepoints");
    check(node->parenWidth() > 0,
          "bracket delimiter computes a positive width");
    check(bracket->layout().width >= node->parenWidth() * 2,
          "bracket layout width accounts for both delimiters");

    // Braces and bars also work
    auto brace = makeBrace(rowWithNumber("y"));
    auto* braceNode = static_cast<NodeParen*>(brace.get());
    brace->calculateLayout(fm);
    check(braceNode->delimKind() == DelimKind::Brace, "brace delimiter preserves its kind");
    check(brace->layout().width > 0, "brace layout computes positive width");

    auto bar = makeBar(rowWithNumber("z"));
    auto* barNode = static_cast<NodeParen*>(bar.get());
    bar->calculateLayout(fm);
    check(barNode->delimKind() == DelimKind::Bar, "bar delimiter preserves its kind");
    check(bar->layout().width > 0, "bar layout computes positive width");
}

static void testBigOpUtf8MatchesCodepoint() {
    // Verify the single-source bigOpGlyph mapping is self-consistent:
    // every kind's UTF-8 decodes to its declared codepoint.
    // Uses the canonical vpam::utf8Decode — the single UTF-8 decoder shared
    // by MathAST.cpp, MathRenderer.cpp, and all tests.
    for (int k = 0; k <= 5; ++k) {
        BigOpKind kind = static_cast<BigOpKind>(k);
        BigOpGlyph g = bigOpGlyph(kind);

        const uint8_t* s = reinterpret_cast<const uint8_t*>(g.utf8);
        uint32_t decoded = 0;
        uint8_t len = vpam::utf8Decode(s, decoded);

        check(decoded == g.cp, "BigOpGlyph UTF-8 decodes to declared codepoint");
        check(len >= 1 && len <= 4, "BigOpGlyph UTF-8 byte length is valid");
    }

    // Also verify NodeBigOp constructors set both fields from the mapping
    auto bo = makeBigOp(BigOpKind::Intersection);
    auto* node = static_cast<NodeBigOp*>(bo.get());
    check(node->operatorCodepoint() == 0x22C2, "Intersection codepoint");
    check(node->operatorUtf8()[0] == '\xE2', "Intersection UTF-8 first byte");
}

static void testAxisHeightUsesMathConstant() {
    FontMetrics fm = defaultFontMetrics();  // emSize=18, ascent=14, descent=3

    // STIX Two Math AxisHeight = 258 DU
    // MathConstantsProvider now uses round-to-nearest scaling with
    // sign-correct handling, matching all other MATH constant conversions.
    MathConstantsProvider mc(fm.emSize);
    const int16_t expectedAxis = mc.axisHeight();
    check(fm.axisHeight() == expectedAxis,
          "FontMetrics::axisHeight matches MathConstantsProvider::axisHeight");
    check(fm.axisHeight() != fm.ascent / 2,
          "axisHeight is NOT a crude ascent/2 heuristic");

    // 258 DU → 5 px at 18pt (round-to-nearest: (258*18+500)/1000 = 5)
    check(fm.axisHeight() == 5,
          "AxisHeight 258 DU → 5 px at 18pt em-size via round-to-nearest scaling");
}

// ════════════════════════════════════════════════════════════════════════════
//  Phase 1-4 AUDIT REGRESSIONS
//  Each of these tests was added to lock down a specific bug found in the
//  second-pass audit. Their presence prevents silent recurrence of:
//    • Missing SpaceAfterScript in NodeLogBase
//    • spacingCodeToMu negative-code latent corruption
//    • axisHeight rounding drift vs duToPx
//    • Fraction bar / radical gap not using MATH table constants
//    • Negative inter-atom codes applying in script styles
// ════════════════════════════════════════════════════════════════════════════

static void testLogBaseIncludesSpaceAfterScript() {
    FontMetrics fm = defaultFontMetrics();

    auto logBase = makeLogBase(rowWithNumber("2"), rowWithNumber("x"));
    logBase->calculateLayout(fm);
    auto* lb = static_cast<NodeLogBase*>(logBase.get());

    const int16_t scriptSpace = spaceAfterScriptPx(fm);
    const int16_t expectedW = static_cast<int16_t>(
        lb->labelWidth()
        + lb->base()->layout().width + scriptSpace + NodeLogBase::LABEL_GAP
        + lb->parenWidth() + lb->innerPad()
        + lb->argument()->layout().width + lb->innerPad() + lb->parenWidth());
    check(logBase->layout().width == expectedW,
          "NodeLogBase width includes SpaceAfterScript after subscript base");
    check(scriptSpace >= 0,
          "SpaceAfterScript is non-negative");
}

static void testSpacingCodeToMuNegativeMapping() {
    // Code ±1 → 3 mu, ±2 → 4 mu, ±3 → 5 mu.  Sign restricts to
    // display/text only; magnitude (mu value) is identical.
    check(spacingCodeToMu(1)  == 3, "code +1 → 3 mu (thin space)");
    check(spacingCodeToMu(-1) == 3, "code -1 → 3 mu (thin, display/text only)");
    check(spacingCodeToMu(2)  == 4, "code +2 → 4 mu (medium space)");
    check(spacingCodeToMu(-2) == 4, "code -2 → 4 mu (medium, display/text only)");
    check(spacingCodeToMu(3)  == 5, "code +3 → 5 mu (thick space)");
    check(spacingCodeToMu(-3) == 5, "code -3 → 5 mu (thick, display/text only)");
    check(spacingCodeToMu(0)  == 0, "code 0 → 0 mu (no space)");
    check(spacingCodeToMu(9)  == 0, "code 9 → 0 mu (impossible — treated as zero)");
}

static void testAxisHeightMatchesDuToPx() {
    FontMetrics fm = defaultFontMetrics();

    MathConstantsProvider mc(fm.emSize);
    const int16_t fromDuToPx = mc.axisHeight();
    check(fm.axisHeight() == fromDuToPx,
          "FontMetrics::axisHeight() == MathConstantsProvider::axisHeight()");

    // At script level the axis uses the same round-to-nearest scaling
    FontMetrics fmSmall = fm.superscript();
    MathConstantsProvider mcSmall(fmSmall.emSize);
    const int16_t smallFromDuToPx = mcSmall.axisHeight();
    check(fmSmall.axisHeight() == smallFromDuToPx,
          "superscript axisHeight matches scaled axisHeight at scaled em-size");
    check(fmSmall.axisHeight() >= 1,
          "superscript axisHeight ≥ 1 px (prevents zero-axis fraction-bar collapse)");
}

static void testFractionLayoutUsesMathConstants() {
    FontMetrics fm = defaultFontMetrics();  // DISPLAY_STYLE

    auto frac = makeFraction(rowWithNumber("1"), rowWithNumber("2"));
    frac->calculateLayout(fm);
    auto* f = static_cast<NodeFraction*>(frac.get());

    const auto metrics = fractionLayoutMetrics(fm);
    check(f->ruleThicknessPx() == metrics.ruleThicknessPx,
          "fraction rule thickness from FractionRuleThickness MATH constant");
    check(f->ruleThicknessPx() >= 1,
          "fraction bar ≥ 1 px via duToPxMin guard");

    // Bar is centered on math axis; odd-thickness bar places extra px above (TeX convention)
    check(f->barHalfUpPx() + f->barHalfDownPx() == f->ruleThicknessPx(),
          "bar half-sums equal total rule thickness");
    check(f->barHalfUpPx() >= f->barHalfDownPx(),
          "extra pixel goes ABOVE axis for odd bar thickness (TeXbook convention)");

    // Numerator shift ≥ fractional gap minimum for display style
    check(f->numeratorShiftPx() >= MathConstantsProvider(fm.emSize).fractionNumeratorGapMin(true),
          "numerator shift ≥ FractionNumeratorGapMin (display)");

    // Rule overhang is mu-based, at least 1 px
    check(f->ruleOverhangPx() >= 1,
          "fraction bar overhang ≥ 1 px");
}

static void testRootLayoutUsesMathConstants() {
    FontMetrics fm = defaultFontMetrics();  // DISPLAY_STYLE

    auto root = makeRoot(rowWithNumber("x+1"));
    root->calculateLayout(fm);
    auto* r = static_cast<NodeRoot*>(root.get());

    MathConstantsProvider mc(fm.emSize);
    const int16_t expectedRuleT = mc.radicalRuleThickness();
    check(r->radicalRuleThickness() == expectedRuleT,
          "radical overline thickness from RadicalRuleThickness MATH constant");
    check(r->radicalRuleThickness() >= 1,
          "radical overline ≥ 1 px via scalePxMin guard");

    const int16_t expectedGap = mc.radicalVerticalGap(true);
    check(r->radicalVerticalGap() == expectedGap,
          "radical vertical gap from RadicalDisplayStyleVerticalGap at display style");
    check(r->radicalExtraAscender() >= 1,
          "radical extra ascender ≥ 1 px");
}

static void testNegativeSpacingCodesCollapseInScriptStyles() {
    // Positive codes 1-3 always apply; only negative codes (-1/-2/-3)
    // are restricted to display/text and should yield zero spacing in
    // script styles.  Since kSpacingTable currently has no negative
    // entries, this test validates the mechanism is correct WHEN a
    // negative code IS used.
    FontMetrics fm = defaultFontMetrics();          // DISPLAY_STYLE
    FontMetrics fmScript = fm.superscript();         // SCRIPT

    // ORD → BINARY is code 2 (medium space, 4 mu).  Positive codes always apply.
    const int16_t displaySpace = interAtomSpacingPx(
        MathClass::ORD, MathClass::BINARY, fm.style, fm.emSize);
    const int16_t scriptSpace = interAtomSpacingPx(
        MathClass::ORD, MathClass::BINARY, fmScript.style, fmScript.emSize);
    check(displaySpace > 0, "ORD→BIN spacing is positive in display style");
    check(scriptSpace > 0, "ORD→BIN spacing is positive in script style (code 2 always applies)");
    check(scriptSpace < displaySpace,
          "script inter-atom space scales down with em-size");
}

static bool containsNodeType(const MathNode* node, NodeType type) {
    if (!node) return false;
    if (node->type() == type) return true;
    for (int i = 0; i < node->childCount(); ++i) {
        if (containsNodeType(node->child(i), type)) return true;
    }
    return false;
}

static bool containsDelimiterKind(const MathNode* node, DelimKind kind) {
    if (!node) return false;
    if (node->type() == NodeType::Paren) {
        const auto* paren = static_cast<const NodeParen*>(node);
        if (paren->delimKind() == kind) return true;
    }
    for (int i = 0; i < node->childCount(); ++i) {
        if (containsDelimiterKind(node->child(i), kind)) return true;
    }
    return false;
}

static bool containsStyle(const StressExpressionCase* cases, size_t count,
                          MathStyle style) {
    for (size_t i = 0; i < count; ++i) {
        if (cases[i].style == style) return true;
    }
    return false;
}

static const StressExpressionCase* findStressCase(const char* id) {
    const auto cases = mathStressExpressionCases();
    for (size_t i = 0; i < cases.count; ++i) {
        if (std::string(cases.items[i].id) == id) {
            return &cases.items[i];
        }
    }
    return nullptr;
}

static void testPhase5StressExpressionCatalogCoverage() {
    const auto cases = mathStressExpressionCases();
    check(cases.count >= 15,
          "Phase 5 stress catalog contains at least 15 expressions");
    check(containsStyle(cases.items, cases.count, MathStyle::DISPLAY_STYLE),
          "Phase 5 stress catalog includes display style cases");
    check(containsStyle(cases.items, cases.count, MathStyle::TEXT),
          "Phase 5 stress catalog includes text style cases");

    const auto* quadratic = findStressCase("quadratic_formula");
    check(quadratic != nullptr, "stress catalog includes quadratic formula");
    if (quadratic) {
        auto ast = quadratic->build();
        ast->calculateLayout(defaultFontMetrics());
        check(containsNodeType(ast.get(), NodeType::Fraction),
              "quadratic formula contains a fraction");
        check(containsNodeType(ast.get(), NodeType::Root),
              "quadratic formula contains a radical");
        check(containsNodeType(ast.get(), NodeType::Power),
              "quadratic formula contains scripts");
    }

    const auto* gaussDisplay = findStressCase("gauss_law_display");
    const auto* gaussText = findStressCase("gauss_law_text");
    check(gaussDisplay && gaussDisplay->style == MathStyle::DISPLAY_STYLE,
          "Gauss law display case uses display style");
    check(gaussText && gaussText->style == MathStyle::TEXT,
          "Gauss law text case uses text style");

    const auto* nested = findStressCase("deep_nested_exponent");
    check(nested != nullptr, "stress catalog includes deep nested exponent");
    if (nested) {
        auto ast = nested->build();
        FontMetrics fm = defaultFontMetrics();
        FontMetrics fmScript = fm.superscript();
        fm.script = &fmScript;
        ast->calculateLayout(fm);
        check(containsScriptLevel(ast.get(), 2),
              "deep nested exponent reaches scriptscript layout level");
    }

    const auto* matrix = findStressCase("elastic_matrix_brackets");
    check(matrix != nullptr, "stress catalog includes elastic bracket matrix");
    if (matrix) {
        auto ast = matrix->build();
        ast->calculateLayout(defaultFontMetrics());
        check(containsDelimiterKind(ast.get(), DelimKind::Bracket),
              "matrix stress case contains elastic brackets");
        check(containsDelimiterKind(ast.get(), DelimKind::Brace),
              "matrix stress case contains elastic braces");
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  Row Baseline Regression Tests (drawRow / cursor)
// ════════════════════════════════════════════════════════════════════════════

// Font metric contract regressions: keep LVGL line boxes separate from
// OpenType MATH em scaling and glyph ink layout boxes.

static void testFontMetricsSeparateNominalEmFromLvglLineBox() {
    FontMetrics fm = defaultFontMetrics();
    fm.ascent = 13;       // visual ink box for a representative digit glyph
    fm.descent = 0;
    fm.capHeight = 13;
    fm.lineAscent = 20;   // LVGL STIX 18 line box: line_height 31, base_line 11
    fm.lineDescent = 11;
    fm.emSize = 18;       // OpenType MATH design em for STIX 18

    check(fm.ascent < fm.lineAscent,
          "layout ascent is visual ink, not LVGL line-box ascent");
    check(fm.lineAscent + fm.lineDescent == 31,
          "LVGL line box remains available for drawText baseline conversion");
    check(fm.axisHeight() == 5,
          "math axis uses nominal STIX 18 em, not LVGL line_height 31");

    MathConstantsProvider nominal(fm.emSize);
    MathConstantsProvider inflated(31);
    check(nominal.fractionNumeratorShiftUp(true) == 12,
          "display numerator shift remains STIX 18 nominal size");
    check(inflated.fractionNumeratorShiftUp(true) > nominal.fractionNumeratorShiftUp(true),
          "line_height-scaled MATH constants reproduce the inflated bad geometry");
}

static void testGlyphInkMetricHelpersMirrorLvglBaselineFormula() {
    check(glyphInkAscentPx(13, 0) == 13,
          "glyph ink ascent uses box_h + ofs_y");
    check(glyphInkDescentPx(0) == 0,
          "glyph ink descent is zero when glyph does not go below baseline");
    check(glyphInkAscentPx(13, -2) == 11,
          "negative LVGL ofs_y reduces ascent above baseline");
    check(glyphInkDescentPx(-2) == 2,
          "negative LVGL ofs_y contributes descent below baseline");
    check(glyphInkAscentPx(0, 0) == 0,
          "empty glyph has zero visual ascent");
}

static void testFractionDenominatorUsesInkMetricsNotLineBox() {
    FontMetrics fm = defaultFontMetrics();
    fm.charWidth = 10;
    fm.ascent = 13;
    fm.descent = 0;
    fm.capHeight = 13;
    fm.lineAscent = 20;
    fm.lineDescent = 11;
    fm.emSize = 18;

    auto frac = makeFraction(rowWithNumber("2"), rowWithNumber("3"));
    frac->calculateLayout(fm);
    auto* f = static_cast<NodeFraction*>(frac.get());

    const auto& denL = f->denominator()->layout();
    const int16_t expectedInkShift = std::max<int16_t>(
        MathConstantsProvider(fm.emSize).fractionDenominatorShiftDown(true),
        static_cast<int16_t>(
            MathConstantsProvider(fm.emSize).fractionDenominatorGapMin(true) + denL.ascent));
    const int16_t badLineBoxShift = static_cast<int16_t>(
        MathConstantsProvider(fm.emSize).fractionDenominatorGapMin(true) + fm.lineAscent);

    check(denL.ascent == fm.ascent,
          "denominator digit layout uses visual ink ascent");
    check(f->denominatorShiftPx() == expectedInkShift,
          "fraction denominator shift is based on ink ascent");
    check(f->denominatorShiftPx() < badLineBoxShift,
          "fraction denominator is not sunk by LVGL line-box ascent");
}

static void testCanvasObjectHeightKeepsLvglLineBoxForPlainRows() {
    FontMetrics fm = defaultFontMetrics();
    fm.charWidth = 10;
    fm.ascent = 13;
    fm.descent = 0;
    fm.lineAscent = 20;
    fm.lineDescent = 11;
    fm.capHeight = 13;
    fm.emSize = 18;

    auto row = makeRow();
    auto* r = static_cast<NodeRow*>(row.get());
    r->appendChild(makeNumber("2"));
    r->appendChild(makeOperator(OpKind::Add));
    r->appendChild(makeNumber("2"));
    row->calculateLayout(fm);

    const int16_t inkOnlyHeight = static_cast<int16_t>(row->layout().height() + 6);
    const int16_t objectHeight = mathObjectHeightPx(row->layout(), fm, 6);

    check(row->layout().height() < fm.lineHeight(),
          "plain row ink box can be smaller than LVGL line box");
    check(inkOnlyHeight < objectHeight,
          "canvas object height is not based on ink-only row height");
    check(objectHeight == static_cast<int16_t>(fm.lineHeight() + 6),
          "plain row canvas keeps full LVGL line box plus padding");
}

static void testMathTextNormalizationLeavesPlainTokensUnchanged() {
    char buffer[numos::mathsym::kNormalizedTextBufferBytes] = {};

    const char* numberSource = "123.45";
    const auto number = numos::mathsym::normalizeMathTextNoAlloc(numberSource, buffer, sizeof(buffer));
    check(number.text == std::string("123.45"),
          "plain number text remains unchanged");
    check(number.text == numberSource && number.source == numberSource,
          "plain number normalization returns original source pointer");

    const char* variableSource = "x";
    const auto variable = numos::mathsym::normalizeMathTextNoAlloc(variableSource, buffer, sizeof(buffer));
    check(variable.text == std::string("x"),
          "plain variable text remains unchanged");
    check(variable.text == variableSource && variable.source == variableSource,
          "plain variable normalization returns original source pointer");
}

static void testMathTextNormalizationMapsOperatorsAndSymbols() {
    char buffer[numos::mathsym::kNormalizedTextBufferBytes] = {};

    const char* leqSource = "<=";
    auto leq = numos::mathsym::normalizeMathTextNoAlloc(leqSource, buffer, sizeof(buffer));
    check(leq.text == std::string(numos::mathsym::SYMB_LEQ),
          "operator alias <= maps to less-than-or-equal symbol");
    check(leq.text != leqSource && leq.status == numos::mathsym::NormalizeTextStatus::Buffered,
          "mapped operator uses caller-provided buffer");

    auto alpha = numos::mathsym::normalizeMathTextNoAlloc("\\alpha", buffer, sizeof(buffer));
    check(alpha.text == std::string(numos::mathsym::SYMB_ALPHA),
          "LaTeX alpha maps to UTF-8 alpha");

    auto delta = numos::mathsym::normalizeMathTextNoAlloc("\\Delta", buffer, sizeof(buffer));
    check(delta.text == std::string("\xCE\x94"),
          "extended LaTeX Delta maps to UTF-8 Delta");
}

static void testMathTextNormalizationPreservesUtf8AndSafeOverflow() {
    char buffer[numos::mathsym::kNormalizedTextBufferBytes] = {};

    auto pi = numos::mathsym::normalizeMathTextNoAlloc(numos::mathsym::SYMB_PI, buffer, sizeof(buffer));
    check(pi.text == std::string(numos::mathsym::SYMB_PI),
          "already UTF-8 symbol remains unchanged");
    check(pi.status == numos::mathsym::NormalizeTextStatus::Unchanged,
          "already UTF-8 symbol does not use normalization buffer");

    char small[3] = {};
    auto overflow = numos::mathsym::normalizeMathTextNoAlloc("\\mathbbR", small, sizeof(small));
    check(overflow.text == std::string("\\mathbbR"),
          "insufficient normalization buffer falls back to original text");
    check(overflow.status == numos::mathsym::NormalizeTextStatus::InsufficientBuffer,
          "insufficient normalization buffer is explicit");
    check(small[0] == '\0',
          "insufficient normalization buffer does not write a truncated UTF-8 string");
}

static void testMathRenderVisualCasesAreDeterministicAndLayoutValid() {
    const auto* cases = mathRenderVisualCases();
    const std::size_t count = mathRenderVisualCaseCount();
    FontMetrics fm = defaultFontMetrics();

    check(count == 21, "visual verification catalog has the expected fixed size");

    bool sawTwoSquared = false;
    bool sawXSquared = false;
    bool sawXTen = false;
    bool sawParenPower = false;
    bool sawMixed = false;
    bool sawFractionBasePower = false;
    bool sawTwoHalf = false;
    bool sawMixedGroupPower = false;
    bool sawQuarterNested = false;
    bool sawPoweredNumerator = false;
    bool sawPoweredDenominator = false;
    bool sawPhotoFraction = false;
    bool sawTextFraction = false;
    bool sawFractionPower = false;
    bool sawNestedFraction = false;
    bool sawRoot = false;
    bool sawRootText = false;
    bool sawNegativePeriodic = false;
    bool sawLongPeriodic = false;
    bool sawSummation = false;
    bool sawWide = false;

    for (std::size_t i = 0; i < count; ++i) {
        const auto& c = cases[i];
        auto root = c.build();
        check(root && root->type() == NodeType::Row,
              "visual verification case builds a root row");
        root->calculateLayout(fm);
        const auto& layout = root->layout();
        check(layout.width > 0 && layout.height() > 0,
              "visual verification case has positive layout");

        sawTwoSquared |= std::strcmp(c.id, "power_2_squared") == 0;
        sawXSquared |= std::strcmp(c.id, "power_x_squared") == 0;
        sawXTen |= std::strcmp(c.id, "power_x_ten") == 0;
        sawParenPower |= std::strcmp(c.id, "power_group_x_plus_1_squared") == 0;
        sawMixed |= std::strcmp(c.id, "mixed_row_fraction_power") == 0;
        sawFractionBasePower |= std::strcmp(c.id, "power_fraction_base_squared") == 0;
        sawTwoHalf |= std::strcmp(c.id, "power_two_half") == 0;
        sawMixedGroupPower |= std::strcmp(c.id, "power_mixed_fraction_group_squared") == 0;
        sawQuarterNested |= std::strcmp(c.id, "nested_fraction_quarters") == 0;
        sawPoweredNumerator |= std::strcmp(c.id, "fraction_powered_numerator") == 0;
        sawPoweredDenominator |= std::strcmp(c.id, "fraction_powered_denominator") == 0;
        sawPhotoFraction |= std::strcmp(c.id, "photo_2_plus_2_over_2") == 0;
        sawTextFraction |= std::strcmp(c.id, "text_plus_fraction") == 0;
        sawFractionPower |= std::strcmp(c.id, "fraction_plus_power") == 0;
        sawNestedFraction |= std::strcmp(c.id, "nested_fraction") == 0;
        sawRoot |= std::strcmp(c.id, "root_quadratic") == 0;
        sawRootText |= std::strcmp(c.id, "root_next_to_text") == 0;
        sawNegativePeriodic |= std::strcmp(c.id, "negative_periodic_decimal") == 0;
        sawLongPeriodic |= std::strcmp(c.id, "long_periodic_prefix") == 0;
        sawSummation |= std::strcmp(c.id, "summation_limits") == 0;
        if (std::strcmp(c.id, "wide_scroll") == 0) {
            sawWide = true;
            check(layout.width > 240,
                  "wide visual verification case exceeds display viewport");
        }
    }

    check(sawTwoSquared && sawXSquared && sawXTen && sawParenPower,
          "visual verification catalog includes standalone power cases");
    check(sawMixed && sawFractionBasePower && sawTwoHalf && sawMixedGroupPower,
          "visual verification catalog includes mixed power/fraction cases");
    check(sawQuarterNested && sawPoweredNumerator && sawPoweredDenominator,
          "visual verification catalog includes nested and powered fraction cases");
    check(sawPhotoFraction && sawTextFraction && sawFractionPower && sawNestedFraction,
          "visual verification catalog preserves existing fraction cases");
    check(sawRoot && sawRootText,
          "visual verification catalog includes root cases");
    check(sawNegativePeriodic && sawLongPeriodic,
          "visual verification catalog includes periodic decimal cases");
    check(sawSummation && sawWide,
          "visual verification catalog includes limits and horizontal scroll cases");
}

static void testPeriodicDecimalPrefixLengthCoversFormats() {
    check(periodicDecimalPrefixCharCount(false, 1, 0, 1) == 2,
          "simple repeating decimal prefix is integer part plus decimal point");
    check(periodicDecimalPrefixCharCount(true, 1, 1, 1) == 4,
          "negative decimal with non-repeating prefix counts sign integer point and prefix");
    check(periodicDecimalPrefixCharCount(false, 0, 0, 0) == 0,
          "empty periodic decimal has no prefix");
    check(periodicDecimalPrefixCharCount(false, 0, 0, 1) == 1,
          "repeat-only periodic decimal still draws decimal point prefix");
    check(periodicDecimalPrefixCharCount(false, 1, 90, 1) == 92,
          "long periodic decimal prefix is counted without fixed-buffer truncation");
}

static void checkRowChildTopContract(const NodeRow* row,
                                     int16_t rowYTop,
                                     const char* name) {
    const auto& rowL = row->layout();
    const int16_t rowBaseline = layoutBaselineFromTop(rowL, rowYTop);

    for (int i = 0; i < row->childCount(); ++i) {
        const MathNode* child = row->child(i);
        const auto& childL = child->layout();
        const int16_t childYTop = rowChildTopFromRowTop(rowL, childL, rowYTop);
        const int16_t childBaseline = layoutBaselineFromTop(childL, childYTop);
        const int16_t childYBottom = static_cast<int16_t>(childYTop + childL.height());
        const int16_t rowYBottom = static_cast<int16_t>(rowYTop + rowL.height());

        check(childBaseline == rowBaseline, name);
        check(childYTop >= rowYTop, name);
        check(childYBottom <= rowYBottom, name);
    }
}

static void testRowTopContractAlignsTextAndFraction() {
    FontMetrics fm = defaultFontMetrics();

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeNumber("2"));
    rowNode->appendChild(makeFraction(rowWithNumber("1"), rowWithNumber("3")));
    row->calculateLayout(fm);

    const auto& rowL  = row->layout();
    const auto& numL  = rowNode->child(0)->layout();
    const auto& fracL = rowNode->child(1)->layout();

    // Row ascent = max(child.ascent) — the layout contract
    int16_t maxChildAscent = std::max(numL.ascent, fracL.ascent);
    check(rowL.ascent == maxChildAscent,
          "row ascent is max child ascent");

    const int16_t rowYTop = 80;
    const int16_t rowBaseline = layoutBaselineFromTop(rowL, rowYTop);
    const int16_t numTop = rowChildTopFromRowTop(rowL, numL, rowYTop);
    const int16_t fracTop = rowChildTopFromRowTop(rowL, fracL, rowYTop);

    check(numTop > fracTop,
          "top-contract row lowers shorter text next to taller fraction");
    check(layoutBaselineFromTop(numL, numTop) == rowBaseline,
          "text child baseline matches row baseline through top contract");
    check(layoutBaselineFromTop(fracL, fracTop) == rowBaseline,
          "fraction child baseline matches row baseline through top contract");
    check(layoutTopFromBaseline(rowL, rowBaseline) == rowYTop,
          "row top/baseline conversion round-trips");
    checkRowChildTopContract(rowNode, rowYTop,
                             "text plus fraction children stay inside row top contract");
}

static void testRowTopContractAlignsTextAndPower() {
    FontMetrics fm = defaultFontMetrics();

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeNumber("2"));
    rowNode->appendChild(makePower(rowWithNumber("x"), rowWithNumber("3")));
    row->calculateLayout(fm);

    const int16_t rowYTop = 42;
    checkRowChildTopContract(rowNode, rowYTop,
                             "text plus power children stay inside row top contract");
}

static void testRowTopContractAlignsTextAndRoot() {
    FontMetrics fm = defaultFontMetrics();

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeNumber("2"));
    rowNode->appendChild(makeRoot(rowWithNumber("9")));
    row->calculateLayout(fm);

    const int16_t rowYTop = 64;
    checkRowChildTopContract(rowNode, rowYTop,
                             "text plus root children stay inside row top contract");
}

static void testRowTopContractAlignsFractionAndPower() {
    FontMetrics fm = defaultFontMetrics();

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeFraction(rowWithNumber("1"), rowWithNumber("3")));
    rowNode->appendChild(makePower(rowWithNumber("x"), rowWithNumber("2")));
    row->calculateLayout(fm);

    const int16_t rowYTop = 95;
    checkRowChildTopContract(rowNode, rowYTop,
                             "fraction plus power children stay inside row top contract");
}

static void testRowTopContractAlignsNestedFractionPowerRows() {
    FontMetrics fm = defaultFontMetrics();

    auto nestedNumerator = makeRow();
    auto* nestedNumeratorRow = static_cast<NodeRow*>(nestedNumerator.get());
    nestedNumeratorRow->appendChild(makeNumber("1"));
    nestedNumeratorRow->appendChild(makePower(rowWithNumber("x"), rowWithNumber("2")));

    auto nestedFraction = makeFraction(std::move(nestedNumerator), rowWithNumber("3"));

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeNumber("5"));
    rowNode->appendChild(std::move(nestedFraction));
    rowNode->appendChild(makePower(
        makeFraction(rowWithNumber("a"), rowWithNumber("b")),
        rowWithNumber("2")));
    row->calculateLayout(fm);

    const int16_t rowYTop = 120;
    checkRowChildTopContract(rowNode, rowYTop,
                             "nested row fraction power children stay inside row top contract");
}

static void testCompositeTopContractMatchesRendererPath() {
    FontMetrics fm = defaultFontMetrics();

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeNumber("1"));
    rowNode->appendChild(makeFraction(rowWithNumber("2"), rowWithNumber("3")));
    rowNode->appendChild(makePower(rowWithNumber("x"), rowWithNumber("2")));
    row->calculateLayout(fm);

    const int16_t rowYTop = 100;
    const auto& rowL = row->layout();
    const int16_t rowBaseline = layoutBaselineFromTop(rowL, rowYTop);

    const auto* frac = static_cast<const NodeFraction*>(rowNode->child(1));
    const auto* power = static_cast<const NodePower*>(rowNode->child(2));

    const auto& textL = rowNode->child(0)->layout();
    const auto& fracL = frac->layout();
    const auto& powerL = power->layout();

    const int16_t textTop = rowChildTopFromRowTop(rowL, textL, rowYTop);
    const int16_t fracTop = rowChildTopFromRowTop(rowL, fracL, rowYTop);
    const int16_t powerTop = rowChildTopFromRowTop(rowL, powerL, rowYTop);

    const int16_t textBaseline = layoutBaselineFromTop(textL, textTop);
    const int16_t fracBaseline = layoutBaselineFromTop(fracL, fracTop);
    const int16_t powerBaseline = layoutBaselineFromTop(powerL, powerTop);

    check(textBaseline == rowBaseline,
          "composite public top path aligns text baseline to row baseline");
    check(fracBaseline == rowBaseline,
          "composite public top path aligns fraction baseline to row baseline");
    check(powerBaseline == rowBaseline,
          "composite public top path aligns power baseline to row baseline");

    const auto& numL = frac->numerator()->layout();
    const auto& denL = frac->denominator()->layout();
    const int16_t yAxis =
        static_cast<int16_t>(fracBaseline - fm.axisHeight());
    const int16_t numBaseline =
        static_cast<int16_t>(
            yAxis - frac->barHalfUpPx() - frac->numeratorShiftPx());
    const int16_t denBaseline =
        static_cast<int16_t>(
            yAxis + frac->barHalfDownPx() + frac->denominatorShiftPx());
    const int16_t numTop = layoutTopFromBaseline(numL, numBaseline);
    const int16_t denBottom =
        static_cast<int16_t>(layoutTopFromBaseline(denL, denBaseline) + denL.height());
    const int16_t fracBottom = static_cast<int16_t>(fracTop + fracL.height());

    check(numTop >= fracTop,
          "fraction numerator baseline from renderer path stays inside fraction top");
    check(denBottom <= fracBottom,
          "fraction denominator baseline from renderer path stays inside fraction bottom");

    const auto& baseL = power->base()->layout();
    const auto& expL = power->exponent()->layout();
    const int16_t baseTop = layoutTopFromBaseline(baseL, powerBaseline);
    const int16_t expShift = superscriptShiftPx(fm, expL);
    const int16_t expBaseline = static_cast<int16_t>(powerBaseline - expShift);
    const int16_t expTop = layoutTopFromBaseline(expL, expBaseline);

    check(baseTop >= powerTop,
          "power base baseline from renderer path stays inside power top");
    check(expTop >= powerTop,
          "power exponent baseline from renderer path stays inside power top");
    check(expBaseline < powerBaseline,
          "power exponent baseline remains above main baseline");
}

static void testFractionBarAxisGeometryStillMatchesRenderer() {
    FontMetrics fm = defaultFontMetrics();

    auto frac = makeFraction(rowWithNumber("1"), rowWithNumber("2"));
    frac->calculateLayout(fm);
    auto* f = static_cast<NodeFraction*>(frac.get());

    const auto& fracL = frac->layout();
    const auto& numL  = f->numerator()->layout();
    const auto& denL  = f->denominator()->layout();

    const int16_t axis       = fm.axisHeight();
    const int16_t barHalfUp  = f->barHalfUpPx();
    const int16_t barHalfDown = f->barHalfDownPx();
    const int16_t rulePx     = f->ruleThicknessPx();
    const int16_t numShift   = f->numeratorShiftPx();
    const int16_t denShift   = f->denominatorShiftPx();

    // Bar center is on the math axis
    check(barHalfUp + barHalfDown == rulePx,
          "bar half-sums equal total rule thickness");
    check(barHalfUp >= barHalfDown,
          "extra pixel goes above axis for odd bar thickness (TeX convention)");

    // Pick a screen-space baseline; the bar center is at baseline - axisHeight
    int16_t yBaseline = 100;
    int16_t yAxis     = static_cast<int16_t>(yBaseline - axis);

    // Numerator baseline = yAxis - barHalfUp - numShift  (matches drawFraction)
    int16_t expectedNumBaseline = static_cast<int16_t>(yAxis - barHalfUp - numShift);
    check(expectedNumBaseline
              == static_cast<int16_t>(yBaseline - fracL.ascent + numL.ascent),
          "numerator baseline from fraction ascent is consistent");

    // Denominator baseline = yAxis + barHalfDown + denShift  (matches drawFraction)
    int16_t expectedDenBaseline = static_cast<int16_t>(yAxis + barHalfDown + denShift);
    check(expectedDenBaseline
              == static_cast<int16_t>(yBaseline + fracL.descent - denL.descent),
          "denominator baseline from fraction descent is consistent");

    // Fraction ascent/descent reconcile with calculateLayout formula
    int16_t calcAscent  = static_cast<int16_t>(axis + barHalfUp + numShift + numL.ascent);
    int16_t calcDescent = static_cast<int16_t>(barHalfDown + denShift + denL.descent - axis);
    if (calcDescent < 0) {
        calcAscent  = static_cast<int16_t>(calcAscent - calcDescent);
        calcDescent = 0;
    }
    check(fracL.ascent == calcAscent,
          "fraction ascent matches calculateLayout");
    check(fracL.descent == calcDescent,
          "fraction descent matches calculateLayout");
}

static void testNestedFractionWidthStaysContentBounded() {
    FontMetrics fm = defaultFontMetrics();

    auto inner = makeFraction(rowWithNumber("2"), rowWithNumber("2"));

    auto numerator = makeRow();
    auto* numeratorRow = static_cast<NodeRow*>(numerator.get());
    numeratorRow->appendChild(makeNumber("2"));
    numeratorRow->appendChild(makeOperator(OpKind::Add));
    numeratorRow->appendChild(std::move(inner));

    auto outer = makeFraction(std::move(numerator), rowWithNumber("3"));
    outer->calculateLayout(fm);

    const auto& outerL = outer->layout();
    auto* frac = static_cast<NodeFraction*>(outer.get());
    const auto& numL = frac->numerator()->layout();
    const auto& denL = frac->denominator()->layout();
    const int16_t contentW = std::max(numL.width, denL.width);

    check(outerL.width == static_cast<int16_t>(contentW + 2 * frac->ruleOverhangPx()),
          "nested outer fraction width follows widest child only");
    check(outerL.width < 8 * fm.charWidth,
          "nested fraction width remains content bounded, not viewport sized");

    const int16_t x = 10;
    const int16_t denX = static_cast<int16_t>(
        x + frac->ruleOverhangPx()
        + (outerL.width - 2 * frac->ruleOverhangPx() - denL.width) / 2);
    check(denX > x,
          "short denominator is centered under wider numerator");
}

static NodePtr buildTwoPlusTwoOverTwo() {
    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeNumber("2"));
    rowNode->appendChild(makeOperator(OpKind::Add));
    rowNode->appendChild(makeFraction(rowWithNumber("2"), rowWithNumber("2")));
    return row;
}

static void checkExactOrWithinOnePx(int16_t expected, int16_t actual,
                                    const char* name) {
    if (expected == actual) return;
    const int16_t delta = static_cast<int16_t>(actual - expected);
    if (delta >= -1 && delta <= 1) {
        std::cerr << "NOTE " << name << ": delta=" << static_cast<int>(delta)
                  << "px (within ±1 rounding tolerance)\n";
        return;
    }
    std::cerr << "FAIL: " << name << " expected=" << static_cast<int>(expected)
              << " actual=" << static_cast<int>(actual)
              << " delta=" << static_cast<int>(delta) << "px\n";
    ++failures;
}

struct FractionDrawBounds {
    int16_t drawTop;
    int16_t drawBot;
    int16_t layoutTop;
    int16_t layoutBot;
    int16_t topDelta;
    int16_t botDelta;
    int16_t yAxis;
    int16_t numTop;
    int16_t numBot;   ///< numerator ink bottom (numBase + numDescent)
    int16_t denTop;   ///< denominator ink top (denBase - denAscent)
    int16_t denBot;
    int16_t barTop;
    int16_t barBot;
    int16_t gapAboveBar;  ///< barTop - numBot
    int16_t gapBelowBar;  ///< denTop - barBot
    int16_t visibleNumTop;
    int16_t visibleNumBot;
    int16_t visibleDenTop;
    int16_t visibleDenBot;
    int16_t visibleGapAboveBar;
    int16_t visibleGapBelowBar;
};

static FractionDrawBounds fractionDrawBoundsMirrorDiagFrac(
    const NodeFraction* frac, int16_t fracBaseline, const FontMetrics& fm) {
    const auto& cl = frac->layout();
    const auto& numL = frac->numerator()->layout();
    const auto& denL = frac->denominator()->layout();

    const int16_t barHalfUp = frac->barHalfUpPx();
    const int16_t barHalfDn = frac->barHalfDownPx();
    const int16_t numShift = frac->numeratorShiftPx();
    const int16_t denShift = frac->denominatorShiftPx();
    const int16_t axisH = fm.axisHeight();

    const int16_t yAxis = static_cast<int16_t>(fracBaseline - axisH);
    const int16_t barTop = static_cast<int16_t>(yAxis - barHalfUp);
    const int16_t barBot = static_cast<int16_t>(yAxis + barHalfDn);
    const int16_t numBase = static_cast<int16_t>(yAxis - barHalfUp - numShift);
    const int16_t denBase = static_cast<int16_t>(yAxis + barHalfDn + denShift);
    const int16_t numTop = static_cast<int16_t>(numBase - numL.ascent);
    const int16_t numBot = static_cast<int16_t>(numBase + numL.descent);
    const int16_t denTop = static_cast<int16_t>(denBase - denL.ascent);
    const int16_t denBot = static_cast<int16_t>(denBase + denL.descent);
    const int16_t visibleNumTop = static_cast<int16_t>(
        numBase - layoutInkAscentPx(numL));
    const int16_t visibleNumBot = static_cast<int16_t>(
        numBase + layoutInkDescentPx(numL));
    const int16_t visibleDenTop = static_cast<int16_t>(
        denBase - layoutInkAscentPx(denL));
    const int16_t visibleDenBot = static_cast<int16_t>(
        denBase + layoutInkDescentPx(denL));
    const int16_t drawTop = static_cast<int16_t>(numTop < barTop ? numTop : barTop);
    const int16_t drawBot = static_cast<int16_t>(denBot > barBot ? denBot : barBot);
    const int16_t layoutTop = static_cast<int16_t>(fracBaseline - cl.ascent);
    const int16_t layoutBot = static_cast<int16_t>(fracBaseline + cl.descent);

    FractionDrawBounds out{};
    out.drawTop = drawTop;
    out.drawBot = drawBot;
    out.layoutTop = layoutTop;
    out.layoutBot = layoutBot;
    out.topDelta = static_cast<int16_t>(drawTop - layoutTop);
    out.botDelta = static_cast<int16_t>(drawBot - layoutBot);
    out.yAxis = yAxis;
    out.numTop = numTop;
    out.numBot = numBot;
    out.denTop = denTop;
    out.denBot = denBot;
    out.barTop = barTop;
    out.barBot = barBot;
    out.gapAboveBar = static_cast<int16_t>(barTop - numBot);
    out.gapBelowBar = static_cast<int16_t>(denTop - barBot);
    out.visibleNumTop = visibleNumTop;
    out.visibleNumBot = visibleNumBot;
    out.visibleDenTop = visibleDenTop;
    out.visibleDenBot = visibleDenBot;
    out.visibleGapAboveBar = static_cast<int16_t>(barTop - visibleNumBot);
    out.visibleGapBelowBar = static_cast<int16_t>(visibleDenTop - barBot);
    return out;
}

static void checkAcceptedFractionBounds(const FractionDrawBounds& b,
                                        const char* name) {
    check(b.visibleNumBot < b.barTop,
          (std::string(name) + " numerator visible ink is above bar").c_str());
    check(b.visibleDenTop > b.barBot,
          (std::string(name) + " denominator visible ink is below bar").c_str());
    check(b.visibleGapAboveBar > 0,
          (std::string(name) + " visibleGapAboveBar is positive").c_str());
    check(b.visibleGapBelowBar > 0,
          (std::string(name) + " visibleGapBelowBar is positive").c_str());

    const int16_t visibleDelta = static_cast<int16_t>(
        b.visibleGapAboveBar - b.visibleGapBelowBar);
    check(visibleDelta >= -1 && visibleDelta <= 1,
          (std::string(name) + " visible ink gaps are balanced within +/-1px").c_str());

    checkExactOrWithinOnePx(b.layoutTop, b.drawTop,
                            (std::string(name) + " draw top matches layout top").c_str());
    checkExactOrWithinOnePx(b.layoutBot, b.drawBot,
                            (std::string(name) + " draw bottom matches layout bottom").c_str());
    check(b.topDelta >= -1 && b.topDelta <= 1,
          (std::string(name) + " topDelta within +/-1px").c_str());
    check(b.botDelta >= -1 && b.botDelta <= 1,
          (std::string(name) + " botDelta within +/-1px").c_str());
}

static void testCursorTargetRoleClassifier() {
    FontMetrics fm = defaultFontMetrics();

    auto row = buildTwoPlusTwoOverTwo();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    row->calculateLayout(fm);

    auto* frac = static_cast<const NodeFraction*>(rowNode->child(2));
    const NodeRow* numRow = static_cast<const NodeRow*>(frac->numerator());
    const NodeRow* denRow = static_cast<const NodeRow*>(frac->denominator());

    check(classifyCursorTargetRow(rowNode, rowNode) == CursorTargetRole::Root,
          "2+2/2 root row classifies as root");
    check(classifyCursorTargetRow(rowNode, numRow) == CursorTargetRole::FractionNumerator,
          "2+2/2 numerator row classifies as fraction_numerator");
    check(classifyCursorTargetRow(rowNode, denRow) == CursorTargetRole::FractionDenominator,
          "2+2/2 denominator row classifies as fraction_denominator");
    check(std::strcmp(cursorTargetRoleName(CursorTargetRole::FractionDenominator),
                      "fraction_denominator") == 0,
          "fraction_denominator role name is stable");
}

static void testCursorTargetRoleOnPowerExponent() {
    FontMetrics fm = defaultFontMetrics();

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeNumber("2"));
    rowNode->appendChild(makePower(rowWithNumber("2"), rowWithNumber("3")));
    row->calculateLayout(fm);

    auto* pow = static_cast<const NodePower*>(rowNode->child(1));
    const NodeRow* expRow = static_cast<const NodeRow*>(pow->exponent());

    check(classifyCursorTargetRow(rowNode, expRow) == CursorTargetRole::PowerExponent,
          "power exponent row classifies as power_exponent");
    check(std::strcmp(cursorTargetRoleName(CursorTargetRole::PowerExponent),
                      "power_exponent") == 0,
          "power_exponent role name is stable");
}

static void testFractionSimpleLayoutMatchesDrawBounds() {
    FontMetrics fm = defaultFontMetrics();

    auto frac = makeFraction(rowWithNumber("1"), rowWithNumber("2"));
    frac->calculateLayout(fm);

    const int16_t fracBaseline = 100;
    const auto* f = static_cast<const NodeFraction*>(frac.get());
    const FractionDrawBounds bounds =
        fractionDrawBoundsMirrorDiagFrac(f, fracBaseline, fm);

    checkExactOrWithinOnePx(bounds.layoutTop, bounds.drawTop,
                            "simple 1/2 draw top matches layout top");
    checkExactOrWithinOnePx(bounds.layoutBot, bounds.drawBot,
                            "simple 1/2 draw bottom matches layout bottom");
    check(bounds.topDelta >= -1 && bounds.topDelta <= 1,
          "simple 1/2 topDelta within ±1px");
    check(bounds.botDelta >= -1 && bounds.botDelta <= 1,
          "simple 1/2 botDelta within ±1px");
}

static void testMixedRowFractionChildrenBaselineMatch() {
    FontMetrics fm = defaultFontMetrics();

    auto row = buildTwoPlusTwoOverTwo();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    row->calculateLayout(fm);

    const int16_t rowYTop = 80;
    checkRowChildTopContract(rowNode, rowYTop,
                             "mixed 2+2/2 root children share row baseline");
}

static void testFractionDrawnBoundsIncludeAllParts() {
    FontMetrics fm = defaultFontMetrics();

    auto frac = makeFraction(rowWithNumber("2"), rowWithNumber("3"));
    frac->calculateLayout(fm);

    const int16_t fracBaseline = 120;
    const auto* f = static_cast<const NodeFraction*>(frac.get());
    const FractionDrawBounds bounds =
        fractionDrawBoundsMirrorDiagFrac(f, fracBaseline, fm);

    check(bounds.drawTop <= bounds.numTop,
          "fraction drawn top covers numerator top");
    check(bounds.drawBot >= bounds.denBot,
          "fraction drawn bottom covers denominator bottom");
    check(bounds.drawTop <= bounds.barTop,
          "fraction drawn top covers bar top region");
    check(bounds.drawBot >= bounds.barBot,
          "fraction drawn bottom covers bar bottom region");
    check(bounds.yAxis == static_cast<int16_t>(fracBaseline - fm.axisHeight()),
          "fraction bar axis matches baseline minus axisHeight");
}

// ── Inline/text-style fraction style propagation regressions ──

static void testInlineFractionChildrenAreReadable() {
    // ReadableInlineFractionPolicy (NumOS product decision):
    // Top-level TEXT fraction numerator/denominator must keep readable glyph
    // size on the 320x240 TFT.  Compactness comes from TEXT-style fraction
    // shift/gap constants, NOT from tiny 8px script glyphs.
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeNumber("1"));
    rowNode->appendChild(makeOperator(OpKind::Add));
    rowNode->appendChild(makeFraction(rowWithNumber("2"), rowWithNumber("3")));
    rowNode->appendChild(makeOperator(OpKind::Add));
    rowNode->appendChild(makePower(rowWithNumber("x"), rowWithNumber("2")));
    row->calculateLayout(fm);

    const auto& mainAtom = rowNode->child(0)->layout();   // the "1"
    const auto* frac = static_cast<const NodeFraction*>(rowNode->child(2));
    const auto& numL = frac->numerator()->layout();
    const auto& denL = frac->denominator()->layout();

    // ReadableInlineFractionPolicy: children are TEXT L0 (same readable size).
    const FontMetrics childFm = fractionPartMetrics(fm);
    check(childFm.scriptLevel == 0,
          "TEXT-style fraction children stay at script level 0 (readable)");
    check(childFm.style == MathStyle::TEXT,
          "TEXT-style fraction children use TEXT style");

    // Numerator and denominator have the same readable size as the main atoms.
    check(numL.ascent == mainAtom.ascent,
          "inline fraction numerator ascent matches main-row atom (readable)");
    check(denL.ascent == mainAtom.ascent,
          "inline fraction denominator ascent matches main-row atom (readable)");
}

static void testInlineFractionLayoutMatchesDrawBounds() {
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto frac = makeFraction(rowWithNumber("2"), rowWithNumber("3"));
    frac->calculateLayout(fm);

    const int16_t fracBaseline = 110;
    const auto* f = static_cast<const NodeFraction*>(frac.get());
    const FractionDrawBounds bounds =
        fractionDrawBoundsMirrorDiagFrac(f, fracBaseline, fm);

    checkExactOrWithinOnePx(bounds.layoutTop, bounds.drawTop,
                            "text-style 2/3 draw top matches layout top");
    checkExactOrWithinOnePx(bounds.layoutBot, bounds.drawBot,
                            "text-style 2/3 draw bottom matches layout bottom");
    check(bounds.topDelta >= -1 && bounds.topDelta <= 1,
          "text-style 2/3 topDelta within ±1px");
    check(bounds.botDelta >= -1 && bounds.botDelta <= 1,
          "text-style 2/3 botDelta within ±1px");
}

static void testFractionStyleLatticeMatchesReadablePolicy() {
    // Verifies the ReadableInlineFractionPolicy style lattice:
    //   DISPLAY L0 -> TEXT  L0  (same size, compact shifts)
    //   TEXT    L0 -> TEXT  L0  (same size, compact shifts)
    //   SCRIPT  L1 -> SCRIPTSCRIPT L2  (inside exponent/subscript: reduce)
    //   SCRIPTSCRIPT L2 -> SCRIPTSCRIPT L2 (clamped)
    FontMetrics fm = defaultFontMetrics();

    // DISPLAY L0 -> TEXT L0, same glyph size
    FontMetrics display = fm;
    display.style = MathStyle::DISPLAY_STYLE;
    const FontMetrics fromDisplay = fractionPartMetrics(display);
    check(fromDisplay.style == MathStyle::TEXT,
          "DISPLAY fraction children step to TEXT style");
    check(fromDisplay.scriptLevel == 0,
          "DISPLAY->TEXT keeps script level 0 (readable)");
    check(fromDisplay.ascent == display.ascent,
          "DISPLAY->TEXT preserves glyph size");

    // TEXT L0 -> TEXT L0, same glyph size (readable inline policy)
    FontMetrics text = fm;
    text.style = MathStyle::TEXT;
    const FontMetrics fromText = fractionPartMetrics(text);
    check(fromText.style == MathStyle::TEXT,
          "TEXT fraction children stay TEXT (readable inline policy)");
    check(fromText.scriptLevel == 0,
          "TEXT->TEXT keeps script level 0 (no shrink at top level)");
    check(fromText.ascent == text.ascent,
          "TEXT->TEXT preserves glyph size");

    // SCRIPT L1 -> SCRIPTSCRIPT L2 (fraction inside exponent/subscript)
    FontMetrics scriptFm = fm.superscript();   // simulate exponent context L1
    const FontMetrics fromScript = fractionPartMetrics(scriptFm);
    check(fromScript.style == MathStyle::SCRIPTSCRIPT && fromScript.scriptLevel == 2,
          "SCRIPT fraction children step to SCRIPTSCRIPT (level 2)");
    check(fromScript.ascent <= scriptFm.ascent,
          "SCRIPT->SCRIPTSCRIPT does not grow");

    // SCRIPTSCRIPT L2 -> SCRIPTSCRIPT L2 (clamped)
    const FontMetrics fromScriptScript = fractionPartMetrics(fromScript);
    check(fromScriptScript.style == MathStyle::SCRIPTSCRIPT &&
          fromScriptScript.scriptLevel == 2,
          "SCRIPTSCRIPT fraction children stay clamped at SCRIPTSCRIPT");
}

static void testNestedInlineFractionStaysReadable() {
    // ReadableInlineFractionPolicy: nested editor fractions stay at L0
    // (both outer and inner children keep readable glyph size) because
    // fractionPartMetrics only reduces when scriptLevel >= 1.
    //
    // This is an accepted product policy for NumOS hardware readability.
    // Nested fractions may become visually large; that is a known trade-off
    // documented here and expected to be validated on hardware separately.
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto innerFrac = makeFraction(rowWithNumber("1"), rowWithNumber("2"));
    auto numerator = makeRow();
    auto* numeratorRow = static_cast<NodeRow*>(numerator.get());
    numeratorRow->appendChild(makeNumber("1"));
    numeratorRow->appendChild(makeOperator(OpKind::Add));
    numeratorRow->appendChild(std::move(innerFrac));

    auto outer = makeFraction(std::move(numerator), rowWithNumber("3"));
    outer->calculateLayout(fm);

    const auto* outerFrac = static_cast<const NodeFraction*>(outer.get());
    const auto* numRow = static_cast<const NodeRow*>(outerFrac->numerator());
    const auto* inner = static_cast<const NodeFraction*>(numRow->child(2));

    const auto& outerNumAtom = numRow->child(0)->layout();
    const auto& innerNum = inner->numerator()->layout();

    // Both remain at readable size (L0 stays L0 through fractionPartMetrics).
    check(innerNum.ascent == outerNumAtom.ascent,
          "nested inner fraction children are same readable size as outer (L0 policy)");
}

static void testPowerExponentStyleDoesNotRegress() {
    // Fraction style change must not affect power exponent reduction.
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto power = makePower(rowWithNumber("x"), rowWithNumber("2"));
    power->calculateLayout(fm);

    const auto* p = static_cast<const NodePower*>(power.get());
    const auto& baseL = p->base()->layout();
    const auto& expL = p->exponent()->layout();

    check(expL.ascent < baseL.ascent,
          "power exponent stays reduced relative to base in TEXT style");
}

static void testDisplayFractionBehaviorPreserved() {
    // A DISPLAY-style root keeps a fraction's children at TEXT (full glyph
    // size), not directly at SCRIPT. This preserves display-style fractions.
    FontMetrics fm = defaultFontMetrics();   // DISPLAY_STYLE

    auto frac = makeFraction(rowWithNumber("2"), rowWithNumber("3"));
    frac->calculateLayout(fm);

    const auto* f = static_cast<const NodeFraction*>(frac.get());
    const auto& numL = f->numerator()->layout();

    // In display style, numerator keeps full-size glyph metrics.
    check(numL.ascent == fm.ascent,
          "display-style fraction numerator keeps full glyph ascent");

    const int16_t fracBaseline = 120;
    const FractionDrawBounds bounds =
        fractionDrawBoundsMirrorDiagFrac(f, fracBaseline, fm);
    check(bounds.topDelta >= -1 && bounds.topDelta <= 1,
          "display-style fraction draw/layout still consistent");
}

static void testInlineMixedRowBaselineMatch() {
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto row = buildTwoPlusTwoOverTwo();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    row->calculateLayout(fm);

    const int16_t rowYTop = 70;
    checkRowChildTopContract(rowNode, rowYTop,
                             "inline TEXT 2+2/2 root children share row baseline");
}

static void testFractionInsideExponentReduces() {
    // A fraction inside a power exponent (scriptLevel >= 1) must still reduce,
    // even under the ReadableInlineFractionPolicy.
    // The exponent is at scriptLevel 1; fractionPartMetrics must step it to L2.
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    // Build x^(2/3): power whose exponent is a fraction row.
    auto fracExponent = makeFraction(rowWithNumber("2"), rowWithNumber("3"));
    auto expRow = makeRow();
    static_cast<NodeRow*>(expRow.get())->appendChild(std::move(fracExponent));

    auto power = makePower(rowWithNumber("x"), std::move(expRow));
    power->calculateLayout(fm);

    const auto* p = static_cast<const NodePower*>(power.get());
    const auto* expRowNode = static_cast<const NodeRow*>(p->exponent());
    const auto* innerFrac = static_cast<const NodeFraction*>(expRowNode->child(0));
    const auto& innerNum = innerFrac->numerator()->layout();
    const auto& baseL = p->base()->layout();

    check(innerNum.ascent < baseL.ascent,
          "fraction inside exponent has reduced (smaller) numerator vs base");
    check(innerFrac->numerator()->layout().ascent < fm.ascent,
          "fraction inside exponent numerator is smaller than main ascent");
}

// ── Gap-based fraction bar geometry regressions ──
static void testFractionBarGapsBalanced() {
    // After the gap-based fix, gapAboveBar and gapBelowBar must be equal
    // (or at most ±1 px apart due to integer rounding), and both > 0.
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto frac = makeFraction(rowWithNumber("2"), rowWithNumber("2"));
    frac->calculateLayout(fm);

    const int16_t fracBaseline = 100;
    const auto* f = static_cast<const NodeFraction*>(frac.get());
    const FractionDrawBounds b = fractionDrawBoundsMirrorDiagFrac(f, fracBaseline, fm);

    checkAcceptedFractionBounds(b, "2/2");
}

static void testMixedRowFractionGapsBalanced() {
    // 1 + 2/3 + x^2 in TEXT style: fraction bar balanced + baseline contract.
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto row = makeRow();
    auto* rowNode = static_cast<NodeRow*>(row.get());
    rowNode->appendChild(makeNumber("1"));
    rowNode->appendChild(makeOperator(OpKind::Add));
    rowNode->appendChild(makeFraction(rowWithNumber("2"), rowWithNumber("3")));
    rowNode->appendChild(makeOperator(OpKind::Add));
    rowNode->appendChild(makePower(rowWithNumber("x"), rowWithNumber("2")));
    row->calculateLayout(fm);

    const auto* frac = static_cast<const NodeFraction*>(rowNode->child(2));
    const int16_t fracBaseline = 120;
    const FractionDrawBounds b = fractionDrawBoundsMirrorDiagFrac(frac, fracBaseline, fm);

    checkAcceptedFractionBounds(b, "1+2/3+x^2");

    const int16_t rowYTop = 100;
    checkRowChildTopContract(rowNode, rowYTop, "1+2/3+x^2 root children share row baseline");
}

static void testTwoPlusTwoOverTwoGapsBalanced() {
    // 2 + 2/2 in TEXT style: the canonical calculator expression.
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto row = buildTwoPlusTwoOverTwo();
    row->calculateLayout(fm);
    const auto* rowNode = static_cast<const NodeRow*>(row.get());

    const auto* frac = static_cast<const NodeFraction*>(rowNode->child(2));
    const int16_t fracBaseline = 120;
    const FractionDrawBounds b = fractionDrawBoundsMirrorDiagFrac(frac, fracBaseline, fm);

    checkAcceptedFractionBounds(b, "2+2/2");
}

static void testNestedFractionBarsBetweenParts() {
    // Nested fraction (1+1/2)/3: both outer and inner fractions must have
    // the bar between their own num/den (gapAbove > 0 and gapBelow > 0).
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto innerFrac = makeFraction(rowWithNumber("1"), rowWithNumber("2"));
    auto numerator = makeRow();
    auto* numeratorRow = static_cast<NodeRow*>(numerator.get());
    numeratorRow->appendChild(makeNumber("1"));
    numeratorRow->appendChild(makeOperator(OpKind::Add));
    numeratorRow->appendChild(std::move(innerFrac));

    auto outer = makeFraction(std::move(numerator), rowWithNumber("3"));
    outer->calculateLayout(fm);

    const auto* outerFrac = static_cast<const NodeFraction*>(outer.get());
    const auto* numRow = static_cast<const NodeRow*>(outerFrac->numerator());
    const auto* inner = static_cast<const NodeFraction*>(numRow->child(2));

    const int16_t outerBase = 130;
    const FractionDrawBounds ob = fractionDrawBoundsMirrorDiagFrac(outerFrac, outerBase, fm);
    checkAcceptedFractionBounds(ob, "nested outer fraction");

    // Inner fraction baseline derived from outer geometry.
    const FontMetrics childFm = fractionPartMetrics(fm);
    const int16_t innerBase = static_cast<int16_t>(
        outerBase - fm.axisHeight() - outerFrac->barHalfUpPx() - outerFrac->numeratorShiftPx());
    const FractionDrawBounds ib = fractionDrawBoundsMirrorDiagFrac(inner, innerBase, childFm);
    checkAcceptedFractionBounds(ib, "nested inner fraction");
}

static void testFractionBasePowerPreservesVisibleGaps() {
    // (2/3)^2 keeps the accepted ink-aware fraction geometry while the
    // exponent is positioned by the independent superscript policy.
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto fracNode = makeFraction(rowWithNumber("2"), rowWithNumber("3"));
    const auto* fracRaw = static_cast<const NodeFraction*>(fracNode.get());
    auto power = makePower(makeParen(std::move(fracNode)), rowWithNumber("2"));
    power->calculateLayout(fm);

    const int16_t fracBaseline = 120;
    const FractionDrawBounds b =
        fractionDrawBoundsMirrorDiagFrac(fracRaw, fracBaseline, fm);
    checkAcceptedFractionBounds(b, "(2/3)^2 inner fraction");

    const auto* p = static_cast<const NodePower*>(power.get());
    const auto& expL = p->exponent()->layout();
    const SuperscriptShiftMetrics sup = superscriptShiftMetrics(fm, expL);
    const int16_t baseBaseline = 120;
    const int16_t expBaseline = static_cast<int16_t>(baseBaseline - sup.shiftPx);
    const int16_t expInkBot = static_cast<int16_t>(
        expBaseline + layoutInkDescentPx(expL));
    check(static_cast<int16_t>(baseBaseline - expInkBot) >= sup.bottomMinPx,
          "(2/3)^2 exponent keeps bottom-min clearance from base baseline");
}

static void testPoweredNumeratorAndDenominatorFractionsKeepVisibleGaps() {
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto poweredNumerator = makeFraction(
        makePower(rowWithVariable('x'), rowWithNumber("2")),
        rowWithNumber("3"));
    poweredNumerator->calculateLayout(fm);
    const auto* pn = static_cast<const NodeFraction*>(poweredNumerator.get());
    checkAcceptedFractionBounds(
        fractionDrawBoundsMirrorDiagFrac(pn, 120, fm),
        "(x^2)/3");

    auto poweredDenominator = makeFraction(
        rowWithNumber("2"),
        makePower(rowWithVariable('x'), rowWithNumber("2")));
    poweredDenominator->calculateLayout(fm);
    const auto* pd = static_cast<const NodeFraction*>(poweredDenominator.get());
    checkAcceptedFractionBounds(
        fractionDrawBoundsMirrorDiagFrac(pd, 120, fm),
        "2/(x^2)");
}

static void testFractionDenominatorBaselineFollowsGap() {
    // Verify _denominatorShiftPx == gapPx + denAscent directly.
    // This proves the denominator cursor follows the repositioned denominator.
    FontMetrics fm = defaultFontMetrics();
    fm.style = MathStyle::TEXT;

    auto numRow = makeRow();
    static_cast<NodeRow*>(numRow.get())->appendChild(makeNumber("2"));
    auto denRow = makeRow();
    static_cast<NodeRow*>(denRow.get())->appendChild(makeNumber("3"));

    auto frac = makeFraction(std::move(numRow), std::move(denRow));
    frac->calculateLayout(fm);

    const auto* f = static_cast<const NodeFraction*>(frac.get());
    const auto& numL = f->numerator()->layout();
    const auto& denL = f->denominator()->layout();

    const FractionBarGaps gaps = fractionBarGaps(fm, numL, denL);
    check(f->denominatorShiftPx() == gaps.denominatorShiftPx,
          "denominatorShiftPx matches fractionBarGaps (cursor follows gap formula)");
    check(f->numeratorShiftPx() == gaps.numeratorShiftPx,
          "numeratorShiftPx matches fractionBarGaps");
    check(gaps.gapPx > 0, "fractionBarGaps gapPx is positive");
}

int main() {
    testPlatformioValidationFlagMatrix();
    testAssemblyOverlapDistributionIsEqual();
    testAssemblyReducesOverlapBeforeAddingExtenders();
    testAssemblyStartsWithZeroExtenders();
    testAssemblyKeepsInsertedExtendersContiguous();
    testNoAssemblyFallsBackToLargestVariant();
    testGlyphWidthPxIsBaseWidthOnly();
    testNestedPowerReachesScriptScriptLevel();
    testSpaceAfterScriptAffectsPowerAndSubscriptWidth();
    testPowerSuperscriptPolicyUsesCentralAdjust();
    testPowerPublishesVisibleInkBounds();
    testPowerExponentClearsBaseBaseline();
    testAcceptedPowerClearanceMatrix();
    testParenHonorsDelimitedSubFormulaMinHeight();
    testBigOpDisplayLayoutMatchesRendererGeometry();
    testMakeRelationProducesRelationAtom();
    testDisplayLimitsHonorsOperatorMinHeightThreshold();
    testDelimKindCodepointMapping();
    testBracketDelimiterLayout();
    testBigOpUtf8MatchesCodepoint();
    testAxisHeightUsesMathConstant();

    // ── Phase 1-4 audit regressions ──
    testLogBaseIncludesSpaceAfterScript();
    testSpacingCodeToMuNegativeMapping();
    testAxisHeightMatchesDuToPx();
    testFractionLayoutUsesMathConstants();
    testRootLayoutUsesMathConstants();
    testNegativeSpacingCodesCollapseInScriptStyles();
    testPhase5StressExpressionCatalogCoverage();

    // ── Baseline correction regressions ──
    testFontMetricsSeparateNominalEmFromLvglLineBox();
    testGlyphInkMetricHelpersMirrorLvglBaselineFormula();
    testFractionDenominatorUsesInkMetricsNotLineBox();
    testCanvasObjectHeightKeepsLvglLineBoxForPlainRows();
    testMathTextNormalizationLeavesPlainTokensUnchanged();
    testMathTextNormalizationMapsOperatorsAndSymbols();
    testMathTextNormalizationPreservesUtf8AndSafeOverflow();
    testMathRenderVisualCasesAreDeterministicAndLayoutValid();
    testPeriodicDecimalPrefixLengthCoversFormats();
    testRowTopContractAlignsTextAndFraction();
    testRowTopContractAlignsTextAndPower();
    testRowTopContractAlignsTextAndRoot();
    testRowTopContractAlignsFractionAndPower();
    testRowTopContractAlignsNestedFractionPowerRows();
    testCompositeTopContractMatchesRendererPath();
    testFractionBarAxisGeometryStillMatchesRenderer();
    testNestedFractionWidthStaysContentBounded();
    testCursorTargetRoleClassifier();
    testCursorTargetRoleOnPowerExponent();
    testFractionSimpleLayoutMatchesDrawBounds();
    testMixedRowFractionChildrenBaselineMatch();
    testFractionDrawnBoundsIncludeAllParts();

    // ── Gap-based fraction bar geometry ──
    testFractionBarGapsBalanced();
    testMixedRowFractionGapsBalanced();
    testTwoPlusTwoOverTwoGapsBalanced();
    testNestedFractionBarsBetweenParts();
    testFractionBasePowerPreservesVisibleGaps();
    testPoweredNumeratorAndDenominatorFractionsKeepVisibleGaps();
    testFractionDenominatorBaselineFollowsGap();

    // ── ReadableInlineFractionPolicy regressions ──
    testInlineFractionChildrenAreReadable();
    testInlineFractionLayoutMatchesDrawBounds();
    testFractionStyleLatticeMatchesReadablePolicy();
    testNestedInlineFractionStaysReadable();
    testFractionInsideExponentReduces();
    testPowerExponentStyleDoesNotRegress();
    testDisplayFractionBehaviorPreserved();
    testInlineMixedRowBaselineMatch();

    if (failures != 0) {
        std::cerr << failures << " regression checks failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All math engine phase regression checks passed\n";
    return EXIT_SUCCESS;
}
