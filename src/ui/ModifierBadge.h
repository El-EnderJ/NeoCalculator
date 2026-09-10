// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../input/KeyboardManager.h"
#ifdef NATIVE_SIM
#include <lvgl.h>
#endif

namespace ui {

// Left-hand modifier label uses the keyboard's established lock notation.
inline const char* modifierBadgeText() {
    return vpam::KeyboardManager::instance().indicatorText();
}

#ifdef NATIVE_SIM
inline bool modifierHeaderFits(lv_obj_t* badge) {
    if (!badge) return false;
    auto* bar = lv_obj_get_parent(badge);
    lv_obj_update_layout(bar);
    lv_area_t bounds;
    lv_obj_get_coords(bar, &bounds);
    const auto visibleContent = [](lv_obj_t* child) {
        return !lv_obj_check_type(child, &lv_label_class) ||
               *lv_label_get_text(child) != '\0';
    };
    for (uint32_t i = 0; i < lv_obj_get_child_count(bar); ++i) {
        auto* child = lv_obj_get_child(bar, i);
        // Clock/angle/battery must remain visible while modifiers are active.
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) return false;
        if (!visibleContent(child)) continue;
        lv_area_t a;
        lv_obj_get_coords(child, &a);
        if (a.x1 < bounds.x1 || a.y1 < bounds.y1 ||
            a.x2 > bounds.x2 || a.y2 > bounds.y2) return false;
        for (uint32_t j = 0; j < i; ++j) {
            auto* other = lv_obj_get_child(bar, j);
            if (!visibleContent(other)) continue;
            lv_area_t b;
            lv_obj_get_coords(other, &b);
            if (a.x1 <= b.x2 && b.x1 <= a.x2 &&
                a.y1 <= b.y2 && b.y1 <= a.y2) return false;
        }
    }
    return true;
}
#endif

} // namespace ui
