/**
 * NeoGUI.h — Script-to-App GUI Bridge for NeoLanguage Phase 7.
 *
 * Provides NeoLanguage built-in functions to create interactive LVGL
 * interfaces from scripts.  On non-LVGL targets (PC simulation, tests)
 * the functions are no-ops that print what would be created.
 *
 * Component creation functions:
 *   gui_label(text)                   — Create a static text label.
 *   gui_button(label, callback_name)  — Create a clickable button.
 *   gui_slider(min, max, callback_name) — Create a horizontal slider.
 *   gui_input(label)                  — Create a text-input field.
 *   gui_clear()                       — Remove all GUI components.
 *   gui_show()                        — Make the GUI screen active.
 *
 * Layout:
 *   Components are arranged vertically in a scrollable flex container.
 *   Each call to a gui_* function appends to the current layout.
 *
 * Callbacks:
 *   Button / slider callbacks are NeoLanguage function names (strings).
 *   When a component fires, the named NeoLanguage function is called
 *   with the current value (button: 1, slider: current position).
 *   The host app (NeoLanguageApp) manages the callback dispatch loop.
 *
 * Thread safety:
 *   All LVGL calls must originate from the LVGL task.  NeoGUI queues
 *   pending component descriptors and the host flushes them on the
 *   LVGL task via NeoGUI::flush().
 *
 * Part of: NeoCalculator / NumOS — NeoLanguage Phase 7
 */
#pragma once

#include <string>
#include <vector>
#include "NeoValue.h"

// ─────────────────────────────────────────────────────────────────────────────
// Platform detection
// ─────────────────────────────────────────────────────────────────────────────

#if defined(LV_VERSION_MAJOR)
  #define NEO_GUI_HAS_LVGL 1
  #include <lvgl.h>
#else
  #define NEO_GUI_HAS_LVGL 0
#endif

namespace NeoGUI {

// ════════════════════════════════════════════════════════════════════
// Component descriptor (platform-independent)
// ════════════════════════════════════════════════════════════════════

enum class ComponentType {
    Label,
    Button,
    Slider,
    Input,
};

struct ComponentDesc {
    ComponentType type;
    std::string   text;         ///< Label text or button label
    std::string   callback;     ///< NeoLanguage function name to call on event
    double        rangeMin = 0; ///< Slider minimum
    double        rangeMax = 1; ///< Slider maximum
};

// ════════════════════════════════════════════════════════════════════
// Global component queue
// ════════════════════════════════════════════════════════════════════

inline std::vector<ComponentDesc>& componentQueue() {
    static std::vector<ComponentDesc> q;
    return q;
}

inline bool& guiDirty() {
    static bool d = false;
    return d;
}

// ════════════════════════════════════════════════════════════════════
// Public API — called from NeoStdLib
// ════════════════════════════════════════════════════════════════════

/**
 * Add a static text label.
 */
inline void addLabel(const std::string& text) {
    componentQueue().push_back({ ComponentType::Label, text, "", 0, 1 });
    guiDirty() = true;
}

/**
 * Add a clickable button.
 * @param label    Button text.
 * @param callback NeoLanguage function called on click (receives value 1).
 */
inline void addButton(const std::string& label, const std::string& callback) {
    componentQueue().push_back({ ComponentType::Button, label, callback, 0, 1 });
    guiDirty() = true;
}

/**
 * Add a horizontal slider.
 * @param min, max  Value range.
 * @param callback  NeoLanguage function called on change (receives slider value).
 */
inline void addSlider(double min, double max, const std::string& callback) {
    componentQueue().push_back({ ComponentType::Slider, "", callback, min, max });
    guiDirty() = true;
}

/**
 * Add a text input field.
 * @param label     Placeholder / label text.
 */
inline void addInput(const std::string& label) {
    componentQueue().push_back({ ComponentType::Input, label, "", 0, 1 });
    guiDirty() = true;
}

/**
 * Clear all pending components and mark for full GUI rebuild.
 */
inline void clearComponents() {
    componentQueue().clear();
    guiDirty() = true;
}

/**
 * Return true if there are pending component changes to flush.
 */
inline bool isDirty() { return guiDirty(); }

/**
 * Mark the component queue as flushed (called by host after rendering).
 */
inline void markFlushed() { guiDirty() = false; }

// ════════════════════════════════════════════════════════════════════
// LVGL rendering (compiled only on ESP32/LVGL target)
// ════════════════════════════════════════════════════════════════════

#if NEO_GUI_HAS_LVGL

/**
 * Render all pending components onto a new LVGL screen.
 * Must be called from the LVGL task (e.g. in the main loop).
 *
 * @param parent  LVGL parent object. If nullptr, uses lv_scr_act().
 * @param onEvent Callback invoked when a component fires.
 *                Parameters: (callback_name, value).
 */
inline void flush(lv_obj_t* parent,
                  void (*onEvent)(const char* cbName, double value, void* ud),
                  void* userdata)
{
    if (!guiDirty()) return;

    lv_obj_t* scr = parent ? parent : lv_scr_act();

    // Create a vertical flex container
    lv_obj_t* cont = lv_obj_create(scr);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 8, 0);

    struct EventCtx {
        std::string cbName;
        double      value;
        void        (*onEvent)(const char*, double, void*);
        void*       userdata;
    };

    for (const auto& desc : componentQueue()) {
        switch (desc.type) {

        case ComponentType::Label: {
            lv_obj_t* lbl = lv_label_create(cont);
            lv_label_set_text(lbl, desc.text.c_str());
            lv_obj_set_width(lbl, LV_PCT(90));
            break;
        }

        case ComponentType::Button: {
            lv_obj_t* btn = lv_button_create(cont);
            lv_obj_set_width(btn, LV_PCT(80));
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, desc.text.c_str());
            lv_obj_center(lbl);
            // Store callback name in user data for event handler
            auto* ctx = new EventCtx{ desc.callback, 1.0, onEvent, userdata };
            lv_obj_set_user_data(btn, ctx);
            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                auto* ctx = static_cast<EventCtx*>(lv_obj_get_user_data(
                    lv_event_get_target(e)));
                if (ctx && ctx->onEvent)
                    ctx->onEvent(ctx->cbName.c_str(), ctx->value, ctx->userdata);
            }, LV_EVENT_CLICKED, nullptr);
            break;
        }

        case ComponentType::Slider: {
            lv_obj_t* sldr = lv_slider_create(cont);
            lv_obj_set_width(sldr, LV_PCT(80));
            lv_slider_set_range(sldr,
                static_cast<int32_t>(desc.rangeMin),
                static_cast<int32_t>(desc.rangeMax));
            auto* ctx = new EventCtx{ desc.callback, desc.rangeMin, onEvent, userdata };
            lv_obj_set_user_data(sldr, ctx);
            lv_obj_add_event_cb(sldr, [](lv_event_t* e) {
                lv_obj_t* obj = lv_event_get_target(e);
                auto* ctx = static_cast<EventCtx*>(lv_obj_get_user_data(obj));
                if (ctx && ctx->onEvent)
                    ctx->onEvent(ctx->cbName.c_str(),
                                 static_cast<double>(lv_slider_get_value(obj)),
                                 ctx->userdata);
            }, LV_EVENT_VALUE_CHANGED, nullptr);
            break;
        }

        case ComponentType::Input: {
            lv_obj_t* ta = lv_textarea_create(cont);
            lv_obj_set_width(ta, LV_PCT(80));
            lv_textarea_set_placeholder_text(ta, desc.text.c_str());
            lv_textarea_set_one_line(ta, true);
            break;
        }
        }
    }

    markFlushed();
}

#else

/**
 * No-op flush for non-LVGL builds.
 * Prints the component list to stdout (useful for PC simulation/testing).
 */
inline void flush(void* /*parent*/,
                  void (*/*onEvent*/)(const char*, double, void*),
                  void* /*ud*/)
{
    if (!guiDirty()) return;
    for (const auto& desc : componentQueue()) {
        switch (desc.type) {
        case ComponentType::Label:  printf("[GUI Label]  \"%s\"\n",  desc.text.c_str()); break;
        case ComponentType::Button: printf("[GUI Button] \"%s\" -> %s()\n",
                                          desc.text.c_str(), desc.callback.c_str()); break;
        case ComponentType::Slider: printf("[GUI Slider] %.0f..%.0f -> %s()\n",
                                          desc.rangeMin, desc.rangeMax,
                                          desc.callback.c_str()); break;
        case ComponentType::Input:  printf("[GUI Input]  \"%s\"\n",  desc.text.c_str()); break;
        }
    }
    markFlushed();
}

#endif  // NEO_GUI_HAS_LVGL

} // namespace NeoGUI
