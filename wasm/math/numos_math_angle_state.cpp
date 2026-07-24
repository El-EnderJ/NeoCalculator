#include "math/MathEvaluator.h"

// The headless closure needs only AngleModeRuntime's single source of truth.
// The full emulator obtains this definition from MathEvaluator.cpp; linking
// that UI-facing numeric evaluator into the headless target would be needless.
namespace vpam {
AngleMode g_angleMode = AngleMode::RAD;
}
