/**
 * TutorApp.h — Step-by-step algebraic equation tutor for NumOS.
 *
 * Connects the CAS TRS engine (RuleEngine + AlgebraicRules) to the LVGL
 * display using the Smart Highlighter feature of MathCanvas.
 *
 * User flow
 * ─────────
 *   1. User types an equation (e.g. "2x + 6 = 0") in the input field.
 *   2. Presses ENTER / F5 / the on-screen "Solve" button.
 *   3. The app:
 *        a. Parses the text into a cas::NodePtr (EquationNode).
 *        b. Initialises CasMemoryPool + RuleEngine with AlgebraicRules.
 *        c. Calls engine.applyToFixedPoint(parsedEq).
 *        d. Calls checkNonLinearHandover() for quadratic detection.
 *        e. Builds a scrollable flex column where each row has:
 *             · A numbered description label.
 *             · A MathCanvas showing the equation after that step,
 *               with affectedNode highlighted in blue (intermediate)
 *               or orange (final result / NonLinearHandover).
 *   4. MODE / AC exits back to the launcher.
 *
 * Key bindings
 * ────────────
 *   ENTER / F5   = Solve the equation
 *   AC           = Clear input & results
 *   MODE         = Return to menu (handled by SystemApp)
 *   Arrow keys   = Scroll step list when visible
 *
 * Part of: NumOS CAS-S3-ULTRA — Phase 13C (TRS→Display Bridge)
 */

#pragma once

#include <lvgl.h>
#include <memory>
#include <vector>
#include <string>

#include "../input/KeyCodes.h"
#include "../ui/MathRenderer.h"
#include "../math/MathAST.h"
#include "../math/cas/CasMemory.h"
#include "../math/cas/RuleEngine.h"

// ════════════════════════════════════════════════════════════════════════════
// TutorApp
// ════════════════════════════════════════════════════════════════════════════

class TutorApp {
public:
    TutorApp();
    ~TutorApp();

    // Standard NumOS app lifecycle
    void begin();
    void end();
    void load();
    void handleKey(const KeyEvent& ev);

    bool isActive() const { return _screen != nullptr; }

private:
    // ── Layout constants ────────────────────────────────────────────────────
    static constexpr int SCREEN_W  = 320;
    static constexpr int SCREEN_H  = 240;
    static constexpr int BAR_H     = 22;
    static constexpr int INPUT_H   = 28;
    static constexpr int BTN_H     = 28;
    static constexpr int STEPS_Y   = BAR_H + INPUT_H + BTN_H + 4;
    static constexpr int STEPS_H   = SCREEN_H - STEPS_Y;
    static constexpr int PAD       = 4;
    static constexpr int MAX_INPUT_LEN = 48;

    // Colour palette (dark theme, matching EquationsApp)
    static constexpr uint32_t COL_BG        = 0x1C1C2E;
    static constexpr uint32_t COL_BAR       = 0x12122A;
    static constexpr uint32_t COL_INPUT_BG  = 0x2A2A3E;
    static constexpr uint32_t COL_BTN       = 0x1565C0;  // Solve button (blue)
    static constexpr uint32_t COL_BTN_TXT   = 0xFFFFFF;
    static constexpr uint32_t COL_STEP_BG   = 0x22223A;
    static constexpr uint32_t COL_DESC      = 0x90CAF9;  // Light-blue step description
    static constexpr uint32_t COL_DESC_FINAL= 0xFF8A00;  // Orange for final / handover
    static constexpr uint32_t COL_HL_INTER  = 0x1565C0;  // Blue for intermediate highlight
    static constexpr uint32_t COL_HL_FINAL  = 0xE05500;  // Orange for final highlight

    // ── LVGL objects ────────────────────────────────────────────────────────
    lv_obj_t* _screen        = nullptr;
    lv_obj_t* _titleLabel    = nullptr;
    lv_obj_t* _inputField    = nullptr;   // lv_textarea
    lv_obj_t* _solveBtn      = nullptr;
    lv_obj_t* _statusLabel   = nullptr;   // "Enter equation…" / error / ""
    lv_obj_t* _stepsContainer = nullptr;  // Scrollable flex column

    // ── Step render data (own the VPAM trees + MathCanvas widgets) ──────────
    struct StepRenderData {
        vpam::NodePtr              nodeData;   ///< VPAM tree (owns the memory)
        vpam::MathCanvas           canvas;
        const vpam::MathNode*      highlightNode = nullptr;
        lv_color_t                 highlightColor{};
    };
    std::vector<std::unique_ptr<StepRenderData>> _stepRenderers;

    // ── CAS state (recreated each solve) ────────────────────────────────────
    std::unique_ptr<cas::CasMemoryPool>      _pool;
    std::unique_ptr<cas::RuleEngine>         _engine;

    // ── UI helpers ───────────────────────────────────────────────────────────
    void buildUI();
    void clearSteps();
    void onSolveClicked();
    void showStatus(const char* msg, lv_color_t color);

    static void solveBtnCb(lv_event_t* e);
};
