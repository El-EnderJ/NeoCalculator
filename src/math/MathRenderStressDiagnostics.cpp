/*
 * NumOS - Phase 5 render stress diagnostics.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "MathRenderStressDiagnostics.h"

#ifdef ARDUINO

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <lvgl.h>

#include "../ui/MathRenderer.h"
#include "../ui/MathTypography.h"
#include "MathStressExpressions.h"

namespace vpam {
namespace {

constexpr int16_t kScreenW = 320;
constexpr int16_t kScreenH = 240;
constexpr int16_t kTitleH = 20;
constexpr int64_t kFrameBudgetUs = 16000;

void pumpLvgl(uint16_t ms) {
    const uint32_t endMs = millis() + ms;
    do {
        lv_timer_handler();
        delay(5);
    } while (millis() < endMs);
}

void forceRefresh() {
    lv_timer_handler();
    lv_refr_now(NULL);
}

} // namespace

void runMathRenderStressDiagnostics(uint16_t dwellMs) {
#ifdef NUMOS_MATH_STRESS_DIAGNOSTICS
    ui::initMathTypography();

    const StressExpressionCatalog catalog = mathStressExpressionCases();
    Serial.printf("[MATH-STRESS] begin count=%u frame_budget_us=%lld\n",
                  static_cast<unsigned>(catalog.count),
                  static_cast<long long>(kFrameBudgetUs));

    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(screen);
    lv_obj_set_width(title, kScreenW - 8);
    lv_obj_set_pos(title, 4, 2);
    lv_obj_set_style_text_color(title, lv_color_hex(0x202020), 0);
    lv_obj_set_style_text_font(title, ui::mathScriptFont(), 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);

    MathCanvas canvas;
    canvas.create(screen);
    lv_obj_set_pos(canvas.obj(), 0, kTitleH);
    lv_obj_set_size(canvas.obj(), kScreenW, static_cast<int16_t>(kScreenH - kTitleH));
    lv_obj_add_style(canvas.obj(), &ui::style_math_primary, LV_PART_MAIN);

    lv_screen_load(screen);
    forceRefresh();

    bool heapClean = true;
    bool layoutBudgetClean = true;

    for (std::size_t i = 0; i < catalog.count; ++i) {
        const StressExpressionCase& item = catalog.items[i];
        NodePtr expr = item.build();
        auto* row = static_cast<NodeRow*>(expr.get());

        lv_label_set_text_fmt(title, "%02u/%02u %s",
                              static_cast<unsigned>(i + 1),
                              static_cast<unsigned>(catalog.count),
                              item.label);

        canvas.setExpression(row, nullptr);
        canvas.setMathStyle(item.style);

        // WHY: First render after swapping ASTs can touch LVGL object state.
        // The audited cycle below measures steady-state layout/draw hot paths.
        forceRefresh();

        const size_t heapBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const int64_t layoutStartUs = esp_timer_get_time();
        row->calculateLayout(canvas.normalMetrics());
        const int64_t layoutUs = esp_timer_get_time() - layoutStartUs;

        canvas.invalidate();
        forceRefresh();
        const size_t heapAfter = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const int32_t heapDelta = static_cast<int32_t>(heapAfter)
                                - static_cast<int32_t>(heapBefore);

        const bool zeroHeap = (heapDelta == 0);
        const bool layoutOk = (layoutUs < kFrameBudgetUs);
        heapClean = heapClean && zeroHeap;
        layoutBudgetClean = layoutBudgetClean && layoutOk;

        Serial.printf("[MATH-STRESS] case=%u id=%s style=%u layout_us=%lld "
                      "heap_before=%u heap_after=%u heap_delta=%ld zero_heap=%s "
                      "layout_60fps=%s bbox=%dx%d\n",
                      static_cast<unsigned>(i + 1),
                      item.id,
                      static_cast<unsigned>(item.style),
                      static_cast<long long>(layoutUs),
                      static_cast<unsigned>(heapBefore),
                      static_cast<unsigned>(heapAfter),
                      static_cast<long>(heapDelta),
                      zeroHeap ? "PASS" : "FAIL",
                      layoutOk ? "PASS" : "FAIL",
                      static_cast<int>(row->layout().width),
                      static_cast<int>(row->layout().height()));

        pumpLvgl(dwellMs);
    }

    Serial.printf("[MATH-STRESS] summary zero_heap=%s layout_60fps=%s\n",
                  heapClean ? "PASS" : "FAIL",
                  layoutBudgetClean ? "PASS" : "FAIL");

    // WHY: Loading a disposable blank screen with auto-delete releases the
    // diagnostic screen before normal boot continues to the splash/menu flow.
    canvas.destroy();
    lv_obj_t* blank = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(blank, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(blank, LV_OPA_COVER, 0);
    lv_screen_load_anim(blank, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, true);
    forceRefresh();
#else
    (void)dwellMs;
#endif
}

} // namespace vpam

#else

namespace vpam {

void runMathRenderStressDiagnostics(uint16_t) {}

} // namespace vpam

#endif // ARDUINO
