#if defined(NUMOS_MATH_VISUAL_VERIFY) || !defined(ARDUINO)

#include "MathRenderVisualTestApp.h"

#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "../hal/ArduinoCompat.h"
#endif

#include "../math/MathRenderVisualCases.h"
#include "../ui/MathTypography.h"

using namespace vpam;

namespace {
static constexpr uint32_t COL_BG = 0xFFFFFF;
static constexpr uint32_t COL_PANEL = 0xF6F7F9;
static constexpr uint32_t COL_BORDER = 0xC9CED6;
static constexpr uint32_t COL_TEXT = 0x111111;
static constexpr uint32_t COL_ACCENT = 0x1565C0;
}

MathRenderVisualTestApp::MathRenderVisualTestApp()
    : _screen(nullptr),
      _indexLabel(nullptr),
      _caseLabel(nullptr),
      _metricsLabel(nullptr),
      _hintLabel(nullptr),
      _root(nullptr),
      _rootRow(nullptr),
      _caseIndex(0),
      _cursorMode(CursorMode::Off),
      _scrollOffset(0) {
}

MathRenderVisualTestApp::~MathRenderVisualTestApp() {
    end();
}

void MathRenderVisualTestApp::begin() {
    if (_screen) return;

    ui::initMathTypography();

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    _statusBar.create(_screen);
    _statusBar.setTitle("Math Visual");
    _statusBar.setBatteryLevel(100);

    createUI();
}

void MathRenderVisualTestApp::end() {
    _canvas.stopCursorBlink();
    _canvas.setExpression(nullptr, nullptr);
    _canvas.destroy();
    _root.reset();
    _rootRow = nullptr;

    if (_screen) {
        _statusBar.destroy();
        lv_obj_delete(_screen);
        _screen = nullptr;
        _indexLabel = nullptr;
        _caseLabel = nullptr;
        _metricsLabel = nullptr;
        _hintLabel = nullptr;
    }
}

void MathRenderVisualTestApp::load() {
    if (!_screen) begin();
    lv_screen_load_anim(_screen, LV_SCREEN_LOAD_ANIM_FADE_IN, 200, 0, false);
    _statusBar.update();
    showCase(0);
}

void MathRenderVisualTestApp::createUI() {
    // Phase 7I: index ("%d/%d") and case-name labels are plain UI text, separate
    // from the math expression (which is drawn by _canvas / MathCanvas below).
    // Use lv_font_montserrat_14: stix_math_18 has no U+0020 glyph, so case names
    // like "2 + 2/2" tofu'd at every space under LV_USE_FONT_PLACEHOLDER. The
    // MathCanvas rendering is unchanged.
    _indexLabel = lv_label_create(_screen);
    lv_obj_set_style_text_font(_indexLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_indexLabel, lv_color_hex(COL_ACCENT), LV_PART_MAIN);
    lv_obj_set_pos(_indexLabel, PAD, 28);

    _caseLabel = lv_label_create(_screen);
    lv_obj_set_style_text_font(_caseLabel, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_caseLabel, lv_color_hex(COL_TEXT), LV_PART_MAIN);
    lv_obj_set_width(_caseLabel, SCREEN_W - 90);
    lv_label_set_long_mode(_caseLabel, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(_caseLabel, 74, 28);

    _canvas.create(_screen);
    _canvas.setAutoHeightEnabled(false);
    _canvas.setTraceLabel("math_visual");
    lv_obj_set_pos(_canvas.obj(), PAD, CANVAS_Y);
    lv_obj_set_size(_canvas.obj(), CANVAS_W, CANVAS_H);
    lv_obj_add_style(_canvas.obj(), &ui::style_math_primary, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_canvas.obj(), lv_color_hex(COL_PANEL), LV_PART_MAIN);
    lv_obj_set_style_border_color(_canvas.obj(), lv_color_hex(COL_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_width(_canvas.obj(), 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_canvas.obj(), 4, LV_PART_MAIN);
}

int MathRenderVisualTestApp::caseCount() const {
    return static_cast<int>(mathRenderVisualCaseCount());
}

void MathRenderVisualTestApp::showCase(int index) {
    const int count = caseCount();
    if (count <= 0) return;
    if (index < 0) index = count - 1;
    if (index >= count) index = 0;

    _caseIndex = index;
    const auto& visualCase = mathRenderVisualCases()[_caseIndex];

    // Detach canvas before the unique_ptr replacement frees the prior AST.
    _canvas.setExpression(nullptr, nullptr);

    _root = visualCase.build();
    _rootRow = static_cast<NodeRow*>(_root.get());
    _cursorMode = CursorMode::Off;
    _scrollOffset = 0;

    lv_obj_set_pos(_canvas.obj(), PAD, CANVAS_Y);
    lv_obj_set_size(_canvas.obj(), CANVAS_W, CANVAS_H);
    applyCursorMode();
    _canvas.setMathStyle(visualCase.style);
    _canvas.resetScroll();
    updateLabels();
    printSerialReport();
}

void MathRenderVisualTestApp::setCursorAtStart() {
    if (_rootRow) {
        _cursor.init(_rootRow);
    }
}

void MathRenderVisualTestApp::setCursorAtEnd() {
    if (!_rootRow) return;
    _cursor.init(_rootRow);
    for (int i = 0; i < _rootRow->childCount(); ++i) {
        _cursor.moveRight();
    }
}

void MathRenderVisualTestApp::applyCursorMode() {
    if (!_rootRow) return;

    switch (_cursorMode) {
        case CursorMode::Off:
            _canvas.stopCursorBlink();
            _canvas.setExpression(_rootRow, nullptr);
            break;
        case CursorMode::Start:
            setCursorAtStart();
            _canvas.setExpression(_rootRow, &_cursor);
            _canvas.startCursorBlink();
            _canvas.resetCursorBlink();
            break;
        case CursorMode::End:
            setCursorAtEnd();
            _canvas.setExpression(_rootRow, &_cursor);
            _canvas.startCursorBlink();
            _canvas.resetCursorBlink();
            break;
    }
}

void MathRenderVisualTestApp::updateLabels() {
    if (!_rootRow) return;

    const auto& visualCase = mathRenderVisualCases()[_caseIndex];

    char indexBuf[24];
    std::snprintf(indexBuf, sizeof(indexBuf), "%d/%d", _caseIndex + 1, caseCount());
    lv_label_set_text(_indexLabel, indexBuf);
    lv_label_set_text(_caseLabel, visualCase.label);
}

// ── Allocation-free diagnostic helpers ──────────────────────────────────────
// These mirror the production draw path (drawRow, drawNodeWithLayout,
// drawFractionBaseline, drawPowerBaseline) to expose the exact same numbers.
// Called only from printSerialReport() — no per-frame invocation.

static const char* nodeTypeName(NodeType t) {
    switch (t) {
        case NodeType::Number:         return "Num";
        case NodeType::Variable:       return "Var";
        case NodeType::Constant:       return "Const";
        case NodeType::Operator:       return "Op";
        case NodeType::Fraction:       return "Frac";
        case NodeType::Power:          return "Pow";
        case NodeType::Root:           return "Root";
        case NodeType::Paren:          return "Paren";
        case NodeType::Function:       return "Func";
        case NodeType::LogBase:        return "LogBase";
        case NodeType::PeriodicDecimal:return "PerDec";
        case NodeType::DefIntegral:    return "DefInt";
        case NodeType::Summation:      return "Sum";
        case NodeType::Subscript:      return "Sub";
        case NodeType::BigOp:          return "BigOp";
        case NodeType::Symbol:         return "Symbol";
        case NodeType::SpecialValue:   return "Special";
        case NodeType::Collection:     return "Collection";
        case NodeType::Equation:       return "Equation";
        case NodeType::Matrix:         return "Matrix";
        case NodeType::Interval:       return "Interval";
        case NodeType::Piecewise:      return "Piecewise";
        case NodeType::Call:           return "Call";
        case NodeType::Unevaluated:    return "Unevaluated";
        case NodeType::Row:            return "Row";
        case NodeType::Empty:          return "Empty";
        default:                       return "?";
    }
}

#if defined(NUMOS_MATH_VISUAL_VERIFY)
static const lv_font_t* diagFontForScriptLevel(uint8_t scriptLevel) {
    if (scriptLevel >= 2) return ui::mathScriptScriptFont();
    if (scriptLevel == 1) return ui::mathScriptFont();
    return ui::mathPrimaryFont();
}

static const char* diagFontNameForScriptLevel(uint8_t scriptLevel) {
    if (scriptLevel >= 2) return "stix_math_8";
    if (scriptLevel == 1) return "stix_math_12";
    return "stix_math_18";
}

static void diagGlyphText(const char* slot,
                          const char* text,
                          const LayoutResult& layout,
                          int16_t rowBaseline,
                          const FontMetrics& fm,
                          int depth) {
    if (!text || !text[0]) return;

    const lv_font_t* font = diagFontForScriptLevel(fm.scriptLevel);
    const char* fontName = diagFontNameForScriptLevel(fm.scriptLevel);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(text);
    while (*p) {
        uint32_t cp = 0;
        const uint8_t step = vpam::utf8Decode(p, cp);
        if (step == 0) break;

        uint32_t nextCp = 0;
        const uint8_t* nextPtr = p + step;
        if (*nextPtr) {
            vpam::utf8Decode(nextPtr, nextCp);
        }

        lv_font_glyph_dsc_t glyph;
        const bool ok = lv_font_get_glyph_dsc(font, &glyph, cp, nextCp);
        if (ok) {
            const int16_t inkTop = static_cast<int16_t>(
                rowBaseline - glyphInkAscentPx(static_cast<int16_t>(glyph.box_h),
                                               static_cast<int16_t>(glyph.ofs_y)));
            const int16_t inkBot = static_cast<int16_t>(
                rowBaseline + glyphInkDescentPx(static_cast<int16_t>(glyph.ofs_y)));
            Serial.printf(
                "[MATH_VISUAL]%*s  %s glyph cp=U+%04lX font=%s em=%d "
                "scriptLevel=%d layoutAsc=%d layoutDesc=%d "
                "box_w=%d box_h=%d ofs_x=%d ofs_y=%d "
                "visibleInkTop=%d visibleInkBottom=%d\n",
                depth*2, "",
                slot,
                static_cast<unsigned long>(cp),
                fontName,
                (int)fm.emSize,
                (int)fm.scriptLevel,
                (int)layout.ascent,
                (int)layout.descent,
                (int)glyph.box_w,
                (int)glyph.box_h,
                (int)glyph.ofs_x,
                (int)glyph.ofs_y,
                (int)inkTop,
                (int)inkBot);
        } else {
            Serial.printf(
                "[MATH_VISUAL]%*s  %s glyph cp=U+%04lX font=%s "
                "glyphLookup=MISSING\n",
                depth*2, "",
                slot,
                static_cast<unsigned long>(cp),
                fontName);
        }

        p += step;
    }
}

static void diagGlyphInkForNode(const char* slot,
                                const MathNode* node,
                                int16_t rowBaseline,
                                const FontMetrics& fm,
                                int depth);

static void diagGlyphInkForRow(const char* slot,
                               const NodeRow* row,
                               int16_t rowBaseline,
                               const FontMetrics& fm,
                               int depth) {
    if (!row) return;
    for (int i = 0; i < row->childCount(); ++i) {
        diagGlyphInkForNode(slot, row->child(i), rowBaseline, fm, depth);
    }
}

static void diagGlyphInkForNode(const char* slot,
                                const MathNode* node,
                                int16_t rowBaseline,
                                const FontMetrics& fm,
                                int depth) {
    if (!node) return;
    switch (node->type()) {
        case NodeType::Row:
            diagGlyphInkForRow(slot, static_cast<const NodeRow*>(node),
                               rowBaseline, fm, depth);
            break;
        case NodeType::Number:
            diagGlyphText(slot, static_cast<const NodeNumber*>(node)->value().c_str(),
                          node->layout(), rowBaseline, fm, depth);
            break;
        case NodeType::Operator:
            diagGlyphText(slot, static_cast<const NodeOperator*>(node)->symbol(),
                          node->layout(), rowBaseline, fm, depth);
            break;
        case NodeType::Variable:
            diagGlyphText(slot, static_cast<const NodeVariable*>(node)->label(),
                          node->layout(), rowBaseline, fm, depth);
            break;
        case NodeType::Constant:
            diagGlyphText(slot, static_cast<const NodeConstant*>(node)->symbol(),
                          node->layout(), rowBaseline, fm, depth);
            break;
        default:
            break;
    }
}
#endif

// Forward declaration for mutual recursion.
static void diagRow(const NodeRow* row,
                    int16_t rowBaseline,
                    const FontMetrics& fm,
                    int depth,
                    int maxDepth);

static void diagFrac(const NodeFraction* frac,
                     int16_t fracBaseline,
                     const FontMetrics& fm,
                     int depth,
                     int maxDepth) {
    const auto& cl    = frac->layout();
    const auto& numL  = frac->numerator()->layout();
    const auto& denL  = frac->denominator()->layout();

    const int16_t barHalfUp  = frac->barHalfUpPx();
    const int16_t barHalfDn  = frac->barHalfDownPx();
    const int16_t numShift   = frac->numeratorShiftPx();
    const int16_t denShift   = frac->denominatorShiftPx();
    const int16_t axisH      = fm.axisHeight();

    // Child style matches NodeFraction::calculateLayout / drawFractionBaseline.
    const FontMetrics childFm = fractionPartMetrics(fm);

    // Mirror drawFractionBaseline exactly.
    const int16_t yAxis    = static_cast<int16_t>(fracBaseline - axisH);
    const int16_t barTop   = static_cast<int16_t>(yAxis - barHalfUp);
    const int16_t barBot   = static_cast<int16_t>(yAxis + barHalfDn);
    const int16_t numBase  = static_cast<int16_t>(yAxis - barHalfUp  - numShift);
    const int16_t denBase  = static_cast<int16_t>(yAxis + barHalfDn + denShift);
    const int16_t numTop   = static_cast<int16_t>(numBase - numL.ascent);
    const int16_t numBot   = static_cast<int16_t>(numBase + numL.descent);
    const int16_t denTop   = static_cast<int16_t>(denBase - denL.ascent);
    const int16_t denBot   = static_cast<int16_t>(denBase + denL.descent);
    const int16_t drawTop  = static_cast<int16_t>(numTop  < barTop  ? numTop  : barTop);
    const int16_t drawBot  = static_cast<int16_t>(denBot  > barBot  ? denBot  : barBot);
    const int16_t layoutTop = static_cast<int16_t>(fracBaseline - cl.ascent);
    const int16_t layoutBot = static_cast<int16_t>(fracBaseline + cl.descent);
    const int16_t topDelta  = static_cast<int16_t>(drawTop - layoutTop);
    const int16_t botDelta  = static_cast<int16_t>(drawBot - layoutBot);

    // policy= reflects the ReadableInlineFractionPolicy in fractionPartMetrics().
    const char* policy = (childFm.scriptLevel == 0) ? "readable_inline" : "script_reduce";
    Serial.printf(
        "[MATH_VISUAL]%*s  frac style: policy=%s parent=%s(L%d,asc=%d,desc=%d) "
        "num=%s(L%d,asc=%d,desc=%d) den=%s(L%d,asc=%d,desc=%d)\n",
        depth*2, "",
        policy,
        mathStyleName(fm.style), (int)fm.scriptLevel, (int)fm.ascent, (int)fm.descent,
        mathStyleName(childFm.style), (int)childFm.scriptLevel,
        (int)numL.ascent, (int)numL.descent,
        mathStyleName(childFm.style), (int)childFm.scriptLevel,
        (int)denL.ascent, (int)denL.descent);
    Serial.printf(
        "[MATH_VISUAL]%*s  frac axisH=%d axisY=%d barTop=%d barBot=%d\n",
        depth*2, "",
        (int)axisH, (int)yAxis, (int)barTop, (int)barBot);
    Serial.printf(
        "[MATH_VISUAL]%*s  frac numBase=%d numTop=%d numBot=%d (numAsc=%d numDesc=%d numShift=%d)\n",
        depth*2, "",
        (int)numBase, (int)numTop, (int)numBot,
        (int)numL.ascent, (int)numL.descent, (int)numShift);
    Serial.printf(
        "[MATH_VISUAL]%*s  frac denBase=%d denTop=%d denBot=%d (denAsc=%d denDesc=%d denShift=%d)\n",
        depth*2, "",
        (int)denBase, (int)denTop, (int)denBot,
        (int)denL.ascent, (int)denL.descent, (int)denShift);
    Serial.printf(
        "[MATH_VISUAL]%*s  frac drawTop=%d drawBot=%d layoutTop=%d layoutBot=%d "
        "topDelta=%d botDelta=%d\n",
        depth*2, "",
        (int)drawTop, (int)drawBot, (int)layoutTop, (int)layoutBot,
        (int)topDelta, (int)botDelta);
    // Gap diagnostics: bar must be visually between numerator and denominator.
    const int16_t gapAboveBar = static_cast<int16_t>(barTop  - numBot);
    const int16_t gapBelowBar = static_cast<int16_t>(denTop  - barBot);
    Serial.printf(
        "[MATH_VISUAL]%*s  frac gaps: numBot=%d barTop=%d barBot=%d denTop=%d "
        "gapAboveBar=%d gapBelowBar=%d gapDelta=%d\n",
        depth*2, "",
        (int)numBot, (int)barTop, (int)barBot, (int)denTop,
        (int)gapAboveBar, (int)gapBelowBar, (int)(gapAboveBar - gapBelowBar));

#if defined(NUMOS_MATH_VISUAL_VERIFY)
    const int16_t visibleNumTop = static_cast<int16_t>(
        numBase - layoutInkAscentPx(numL));
    const int16_t visibleNumBot = static_cast<int16_t>(
        numBase + layoutInkDescentPx(numL));
    const int16_t visibleDenTop = static_cast<int16_t>(
        denBase - layoutInkAscentPx(denL));
    const int16_t visibleDenBot = static_cast<int16_t>(
        denBase + layoutInkDescentPx(denL));
    const int16_t visibleGapAboveBar = static_cast<int16_t>(
        barTop - visibleNumBot);
    const int16_t visibleGapBelowBar = static_cast<int16_t>(
        visibleDenTop - barBot);
    Serial.printf(
        "[MATH_VISUAL]%*s  frac visible gaps: numInkTop=%d numInkBot=%d "
        "barTop=%d barBot=%d denInkTop=%d denInkBot=%d "
        "visibleGapAboveBar=%d visibleGapBelowBar=%d visibleGapDelta=%d\n",
        depth*2, "",
        (int)visibleNumTop, (int)visibleNumBot,
        (int)barTop, (int)barBot,
        (int)visibleDenTop, (int)visibleDenBot,
        (int)visibleGapAboveBar, (int)visibleGapBelowBar,
        (int)(visibleGapAboveBar - visibleGapBelowBar));

    diagGlyphInkForNode("frac-num", frac->numerator(), numBase, childFm, depth);
    diagGlyphInkForNode("frac-den", frac->denominator(), denBase, childFm, depth);
#endif

    // Recurse into numerator and denominator rows.
    if (depth < maxDepth) {
        Serial.printf("[MATH_VISUAL]%*s  frac-num row:\n", depth*2, "");
        const NodeRow* numRow = static_cast<const NodeRow*>(frac->numerator());
        if (numRow && numRow->type() == NodeType::Row)
            diagRow(numRow, numBase, childFm, depth + 1, maxDepth);

        Serial.printf("[MATH_VISUAL]%*s  frac-den row:\n", depth*2, "");
        const NodeRow* denRow = static_cast<const NodeRow*>(frac->denominator());
        if (denRow && denRow->type() == NodeType::Row)
            diagRow(denRow, denBase, childFm, depth + 1, maxDepth);
    }
}

static void diagPower(const NodePower* pw,
                      int16_t powerBaseline,
                      const FontMetrics& fm,
                      int depth,
                      int maxDepth) {
    if (!pw) return;

    const auto& powerL = pw->layout();
    const auto& baseL = pw->base()->layout();
    const auto& expL = pw->exponent()->layout();
    const FontMetrics fmSup = fm.superscript();
    const SuperscriptShiftMetrics sup = superscriptShiftMetrics(fm, expL);

    const int16_t baseBaseline = powerBaseline;
    const int16_t expBaseline = static_cast<int16_t>(powerBaseline - sup.shiftPx);
    const int16_t baseTop = static_cast<int16_t>(baseBaseline - baseL.ascent);
    const int16_t baseBot = static_cast<int16_t>(baseBaseline + baseL.descent);
    const int16_t expTop = static_cast<int16_t>(expBaseline - expL.ascent);
    const int16_t expBot = static_cast<int16_t>(expBaseline + expL.descent);
    const int16_t drawTop = static_cast<int16_t>(baseTop < expTop ? baseTop : expTop);
    const int16_t drawBot = static_cast<int16_t>(baseBot > expBot ? baseBot : expBot);
    const int16_t layoutTop = static_cast<int16_t>(powerBaseline - powerL.ascent);
    const int16_t layoutBot = static_cast<int16_t>(powerBaseline + powerL.descent);
    const int16_t topDelta = static_cast<int16_t>(drawTop - layoutTop);
    const int16_t botDelta = static_cast<int16_t>(drawBot - layoutBot);

    const int16_t baseInkTop = static_cast<int16_t>(
        baseBaseline - layoutInkAscentPx(baseL));
    const int16_t baseInkBot = static_cast<int16_t>(
        baseBaseline + layoutInkDescentPx(baseL));
    const int16_t expInkTop = static_cast<int16_t>(
        expBaseline - layoutInkAscentPx(expL));
    const int16_t expInkBot = static_cast<int16_t>(
        expBaseline + layoutInkDescentPx(expL));
    const int16_t expInkBottomToBaseInkTop = static_cast<int16_t>(
        expInkBot - baseInkTop);
    const int16_t expInkBottomToBaseBaseline = static_cast<int16_t>(
        baseBaseline - expInkBot);
    const char* clearanceStatus =
        (expInkBottomToBaseBaseline >= sup.bottomMinPx) ? "clear" : "collision";

    Serial.printf(
        "[MATH_VISUAL]%*s  pow style: parent=%s(L%d,asc=%d,desc=%d) "
        "exp=%s(L%d,asc=%d,desc=%d)\n",
        depth*2, "",
        mathStyleName(fm.style), (int)fm.scriptLevel, (int)fm.ascent,
        (int)fm.descent,
        mathStyleName(fmSup.style), (int)fmSup.scriptLevel,
        (int)expL.ascent, (int)expL.descent);
    Serial.printf(
        "[MATH_VISUAL]%*s  pow policy: shiftUp=%d bottomMin=%d "
        "layoutBottomMinShift=%d visibleBottomMinShift=%d texShift=%d "
        "vpamAdjust=%d configuredAdjust=%d expShift=%d\n",
        depth*2, "",
        (int)sup.shiftUpPx, (int)sup.bottomMinPx,
        (int)sup.layoutBottomMinShiftPx,
        (int)sup.visibleBottomMinShiftPx,
        (int)sup.texShiftPx,
        (int)sup.appliedAdjustPx,
        (int)sup.configuredAdjustPx,
        (int)sup.shiftPx);
    Serial.printf(
        "[MATH_VISUAL]%*s  pow baselines: parentRowBaseline=%d "
        "baseBaseline=%d expBaseline=%d baseMatchesParent=%s "
        "powerMatchesParent=YES\n",
        depth*2, "",
        (int)powerBaseline, (int)baseBaseline, (int)expBaseline,
        (baseBaseline == powerBaseline) ? "YES" : "NO");
    Serial.printf(
        "[MATH_VISUAL]%*s  pow layout: baseTop=%d baseBot=%d expTop=%d "
        "expBot=%d drawTop=%d drawBot=%d layoutTop=%d layoutBot=%d "
        "topDelta=%d botDelta=%d\n",
        depth*2, "",
        (int)baseTop, (int)baseBot, (int)expTop, (int)expBot,
        (int)drawTop, (int)drawBot, (int)layoutTop, (int)layoutBot,
        (int)topDelta, (int)botDelta);
    Serial.printf(
        "[MATH_VISUAL]%*s  pow visible: baseInkTop=%d baseInkBot=%d "
        "expInkTop=%d expInkBot=%d expInkBottomToBaseInkTop=%d "
        "expInkBottomToBaseBaseline=%d clearance=%s\n",
        depth*2, "",
        (int)baseInkTop, (int)baseInkBot,
        (int)expInkTop, (int)expInkBot,
        (int)expInkBottomToBaseInkTop,
        (int)expInkBottomToBaseBaseline,
        clearanceStatus);

#if defined(NUMOS_MATH_VISUAL_VERIFY)
    diagGlyphInkForNode("pow-base", pw->base(), baseBaseline, fm, depth);
    diagGlyphInkForNode("pow-exp", pw->exponent(), expBaseline, fmSup, depth);
#endif

    if (depth < maxDepth) {
        Serial.printf("[MATH_VISUAL]%*s  pow-base row:\n", depth*2, "");
        const MathNode* baseNode = pw->base();
        if (baseNode && baseNode->type() == NodeType::Row) {
            const NodeRow* baseRow = static_cast<const NodeRow*>(baseNode);
            diagRow(baseRow, baseBaseline, fm, depth + 1, maxDepth);
        }

        Serial.printf("[MATH_VISUAL]%*s  pow-exp row:\n", depth*2, "");
        const MathNode* expNode = pw->exponent();
        if (expNode && expNode->type() == NodeType::Row) {
            const NodeRow* expRow = static_cast<const NodeRow*>(expNode);
            diagRow(expRow, expBaseline, fmSup, depth + 1, maxDepth);
        }
    }
}

static void diagRow(const NodeRow* row,
                    int16_t rowBaseline,
                    const FontMetrics& fm,
                    int depth,
                    int maxDepth) {
    if (!row) return;
    const auto& rowL    = row->layout();
    const int16_t rowYTop = layoutTopFromBaseline(rowL, rowBaseline);
    const int16_t axisY   = static_cast<int16_t>(rowBaseline - fm.axisHeight());

    Serial.printf(
        "[MATH_VISUAL]%*s row asc=%d desc=%d h=%d rowYTop=%d rowBaseline=%d axisY=%d\n",
        depth*2, "",
        (int)rowL.ascent, (int)rowL.descent, (int)rowL.height(),
        (int)rowYTop, (int)rowBaseline, (int)axisY);

    MathClass prevRight = MathClass::ORD;
    bool hasPrev = false;
    for (int i = 0; i < row->childCount(); ++i) {
        const MathNode* child = row->child(i);
        if (!child) continue;
        const auto& cl = child->layout();

        // Accumulate inter-atom spacing (mirrors NodeRow::calculateLayout and drawRow).
        int16_t gap = 0;
        if (hasPrev)
            gap = interAtomSpacingPx(prevRight, child->leftMathClass(), fm.style, fm.emSize);

        // Mirror drawRow → drawNodeWithLayout:
        //   childYTop     = rowYTop + rowL.ascent - cl.ascent
        //   childBaseline = childYTop + cl.ascent  = rowBaseline  (for all non-special nodes)
        const int16_t childYTop     = static_cast<int16_t>(rowYTop + rowL.ascent - cl.ascent);
        const int16_t childBaseline = static_cast<int16_t>(childYTop + cl.ascent);
        const bool baselineMatch    = (childBaseline == rowBaseline);

        Serial.printf(
            "[MATH_VISUAL]%*s   [%d] %s w=%d h=%d asc=%d desc=%d "
            "gap=%d childYTop=%d childBaseline=%d baselineMatch=%s\n",
            depth*2, "",
            i, nodeTypeName(child->type()),
            (int)cl.width, (int)cl.height(), (int)cl.ascent, (int)cl.descent,
            (int)gap,
            (int)childYTop, (int)childBaseline,
            baselineMatch ? "YES" : "NO");

        if (child->type() == NodeType::Fraction && depth < maxDepth) {
            diagFrac(static_cast<const NodeFraction*>(child),
                     childBaseline, fm, depth + 1, maxDepth);
        }

        if (child->type() == NodeType::Power && depth < maxDepth) {
            diagPower(static_cast<const NodePower*>(child),
                      childBaseline, fm, depth + 1, maxDepth);
        }

        prevRight = child->rightMathClass();
        hasPrev   = true;
    }
}

void MathRenderVisualTestApp::printSerialReport() const {
    if (!_rootRow || !_canvas.obj()) return;

    const auto& visualCase = mathRenderVisualCases()[_caseIndex];
    const_cast<NodeRow*>(_rootRow)->calculateLayout(_canvas.normalMetrics());
    const auto& layout = _rootRow->layout();

    lv_obj_update_layout(_canvas.obj());
    lv_area_t area;
    lv_obj_get_coords(_canvas.obj(), &area);
    const int16_t widgetW = static_cast<int16_t>(area.x2 - area.x1 + 1);
    const int16_t widgetH = static_cast<int16_t>(area.y2 - area.y1 + 1);
    const int16_t rootBaseline = static_cast<int16_t>(
        static_cast<int16_t>(area.y1) + (widgetH + layout.ascent - layout.descent) / 2);
    const int16_t rootYTop = layoutTopFromBaseline(layout, rootBaseline);
    const int16_t viewportW = static_cast<int16_t>(widgetW - REPORT_X_PAD);
    const bool horizontalScrolling = layout.width > viewportW;
    const FontMetrics& fm = _canvas.normalMetrics();

    Serial.printf("[MATH_VISUAL] %d/%d id=%s name=\"%s\"\n",
                  _caseIndex + 1, caseCount(), visualCase.id, visualCase.label);
    Serial.printf("[MATH_VISUAL] canvas obj=(%d,%d,%d,%d) widget=(%d,%d) "
                  "scrollable=%s scrollOffset=%d cursor=%s\n",
                  (int)area.x1, (int)area.y1, (int)area.x2, (int)area.y2,
                  (int)widgetW, (int)widgetH,
                  horizontalScrolling ? "yes" : "no",
                  (int)_scrollOffset,
                  (_cursorMode == CursorMode::Off) ? "off" :
                      ((_cursorMode == CursorMode::Start) ? "start" : "end"));

    // Per-child fraction/row diagnostic (depth-3, mirrors production draw path).
    diagRow(_rootRow, rootBaseline, fm, /*depth=*/0, /*maxDepth=*/3);

    Serial.println("[MATH_VISUAL] controls: UP/DOWN case, LEFT/RIGHT scroll, "
                   "ENTER/EXE cursor off/start/end, MODE back");
}

void MathRenderVisualTestApp::handleKey(const KeyEvent& ev) {
    if (ev.action != KeyAction::PRESS && ev.action != KeyAction::REPEAT) return;

    switch (ev.code) {
        case KeyCode::UP:
        case KeyCode::F1:
            showCase(_caseIndex - 1);
            break;
        case KeyCode::DOWN:
        case KeyCode::F2:
            showCase(_caseIndex + 1);
            break;
        case KeyCode::LEFT:
            _scrollOffset = static_cast<int16_t>(_scrollOffset + SCROLL_STEP);
            if (_scrollOffset > 0) _scrollOffset = 0;
            _canvas.scrollBy(SCROLL_STEP);
            updateLabels();
            printSerialReport();
            break;
        case KeyCode::RIGHT:
            _scrollOffset = static_cast<int16_t>(_scrollOffset - SCROLL_STEP);
            _canvas.scrollBy(-SCROLL_STEP);
            updateLabels();
            printSerialReport();
            break;
        case KeyCode::ENTER:
        case KeyCode::EXE:
            if (_cursorMode == CursorMode::Off) {
                _cursorMode = CursorMode::Start;
            } else if (_cursorMode == CursorMode::Start) {
                _cursorMode = CursorMode::End;
            } else {
                _cursorMode = CursorMode::Off;
                _scrollOffset = 0;
                _canvas.resetScroll();
            }
            applyCursorMode();
            _canvas.invalidate();
            updateLabels();
            printSerialReport();
            break;
        default:
            break;
    }
}

#endif // defined(NUMOS_MATH_VISUAL_VERIFY) || !defined(ARDUINO)
