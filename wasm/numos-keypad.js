// Web-only logical input catalog. These numeric identifiers are audited
// against src/input/KeyCodes.h by tests/wasm/keycode-catalog.mjs. They are not
// GPIOs and intentionally do not describe either hardware key matrix.
export const NUMOS_LOGICAL_KEYS = Object.freeze([
  ["SHIFT", 1, "Shift"], ["ALPHA", 2, "Alpha"], ["MODE", 3, "Home / Mode"],
  ["SETUP", 4, "Setup"], ["F1", 5, "Function 1"], ["F2", 6, "Function 2"],
  ["F3", 7, "Function 3"], ["F4", 8, "Function 4"], ["ON", 9, "On"],
  ["AC", 10, "All clear"], ["DEL", 11, "Delete"], ["FREE_EQ", 12, "Equals / exact decimal"],
  ["LEFT", 13, "Left"], ["UP", 14, "Up"], ["DOWN", 15, "Down"], ["RIGHT", 16, "Right"],
  ["VAR_X", 17, "Variable x"], ["VAR_Y", 18, "Variable y"], ["TABLE", 19, "Table"],
  ["GRAPH", 20, "Graph"], ["ZOOM", 21, "Zoom"], ["TRACE", 22, "Trace"],
  ["SHOW_STEPS", 23, "Show steps"], ["SOLVE", 24, "Solve"],
  ["NUM_7", 25, "7"], ["NUM_8", 26, "8"], ["NUM_9", 27, "9"],
  ["LPAREN", 28, "Left parenthesis"], ["RPAREN", 29, "Right parenthesis"],
  ["DIV", 30, "Divide / fraction"], ["POW", 31, "Power"], ["SQRT", 32, "Square root"],
  ["NUM_4", 33, "4"], ["NUM_5", 34, "5"], ["NUM_6", 35, "6"],
  ["MUL", 36, "Multiply"], ["SUB", 37, "Subtract"], ["SIN", 38, "Sine"],
  ["COS", 39, "Cosine"], ["TAN", 40, "Tangent"],
  ["NUM_1", 41, "1"], ["NUM_2", 42, "2"], ["NUM_3", 43, "3"],
  ["ADD", 44, "Add"], ["NEG", 45, "Negative sign"], ["NUM_0", 46, "0"],
  ["DOT", 47, "Decimal point"], ["ENTER", 48, "Enter"],
  ["F5", 49, "Function 5"], ["EXE", 50, "Execute"], ["LN", 51, "Natural logarithm"],
  ["LOG", 52, "Common logarithm"], ["LOG_BASE", 53, "Logarithm with base"],
  ["CONST_PI", 54, "Pi"], ["CONST_E", 55, "Euler's number"], ["ANS", 56, "Answer"],
  ["PREANS", 57, "Previous answer"], ["STO", 58, "Store"],
  ["ALPHA_A", 59, "Alpha A"], ["ALPHA_B", 60, "Alpha B"], ["ALPHA_C", 61, "Alpha C"],
  ["ALPHA_D", 62, "Alpha D"], ["ALPHA_E", 63, "Alpha E"], ["ALPHA_F", 64, "Alpha F"],
  ["NEGATE", 65, "Negate"], ["FACT", 66, "Factor"], ["LESS", 67, "Less than"],
  ["GREATER", 68, "Greater than"],
  ["HOME", 69, "Home"], ["BACK", 70, "Back"], ["VAR", 71, "Variables"],
  ["TOOLBOX", 72, "Toolbox"], ["FRAC", 73, "Fraction template"],
  ["DIVIDE", 74, "Divide"], ["SQUARE", 75, "Square"],
  ["FORMAT", 76, "Format result"], ["COMMA", 77, "Comma"],
  ["EQUAL", 78, "Equals"], ["EXP", 79, "Scientific exponent"],
].map(([id, code, ariaLabel]) => Object.freeze({
  id,
  code,
  label: Object.freeze({
    FREE_EQ: "S⇔D", LEFT: "←", UP: "↑", DOWN: "↓", RIGHT: "→",
    VAR_X: "x", VAR_Y: "y", SHOW_STEPS: "Steps",
    NUM_7: "7", NUM_8: "8", NUM_9: "9", LPAREN: "(", RPAREN: ")",
    DIV: "÷", POW: "xʸ", SQRT: "√",
    NUM_4: "4", NUM_5: "5", NUM_6: "6", MUL: "×", SUB: "−",
    NUM_1: "1", NUM_2: "2", NUM_3: "3", ADD: "+", NEG: "(−)",
    NUM_0: "0", DOT: ".", ENTER: "EXE", LOG_BASE: "logₙ",
    CONST_PI: "π", CONST_E: "e", PREANS: "PreAns", NEGATE: "±",
    LESS: "<", GREATER: ">",
  })[id] || id.replace("ALPHA_", "").replace("_", " "),
  ariaLabel,
})));

const byId = new Map(NUMOS_LOGICAL_KEYS.map((key) => [key.id, key]));

// Functional web groupings only. Their ordering is a touch-UI choice and has
// no relationship to CAM/WROOM rows, columns, pins, or the production PCBA.
export const NUMOS_WEB_KEYPAD_LAYOUT = Object.freeze([
  ["system", ["SHIFT", "ALPHA", "HOME", "BACK", "VAR", "TOOLBOX",
    "FORMAT", "MODE", "SETUP", "AC", "DEL", "ENTER"]],
  ["navigation", ["LEFT", "UP", "DOWN", "RIGHT", "F1", "F2", "F3", "F4", "F5"]],
  ["apps", ["GRAPH", "TABLE", "ZOOM", "TRACE", "SHOW_STEPS", "SOLVE", "EXE"]],
  ["numbers", ["NUM_7", "NUM_8", "NUM_9", "NUM_4", "NUM_5", "NUM_6",
    "NUM_1", "NUM_2", "NUM_3", "NUM_0", "DOT"]],
  ["operators", ["ADD", "SUB", "MUL", "DIV", "DIVIDE", "FRAC", "POW",
    "SQUARE", "SQRT", "LPAREN", "RPAREN", "COMMA", "EQUAL", "EXP",
    "FREE_EQ", "NEG", "NEGATE", "LESS", "GREATER"]],
  ["functions", ["SIN", "COS", "TAN", "LN", "LOG", "LOG_BASE", "FACT",
    "CONST_PI", "CONST_E"]],
  ["variables", ["VAR_X", "VAR_Y", "ANS", "PREANS", "STO", "ON",
    "ALPHA_A", "ALPHA_B", "ALPHA_C", "ALPHA_D", "ALPHA_E", "ALPHA_F"]],
].map(([name, ids]) => Object.freeze({
  name,
  keys: Object.freeze(ids.map((id) => byId.get(id))),
})));

export const NUMOS_LOGICAL_KEY_MAX = 79;
