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
    NUM_0: "0", DOT: ".", ENTER: "ENTER", LOG_BASE: "logₙ",
    CONST_PI: "π", CONST_E: "e", PREANS: "PreAns", NEGATE: "±",
    LESS: "<", GREATER: ">",
  })[id] || id.replace("ALPHA_", "").replace("_", " "),
  ariaLabel,
})));

const byId = new Map(NUMOS_LOGICAL_KEYS.map((key) => [key.id, key]));

// Final v1 physical face from neocalculator-v1-final-5x10.json.
// Visual coordinates are intentionally separate from the production PCBA
// electrical row/column mapping. Entries are row-major: r0c0 is top-left.
const physicalKey = ([
  physicalId, logicalId, label, shiftLabel, alphaLabel, category,
]) => {
  const logicalKey = byId.get(logicalId);
  if (!logicalKey) throw new Error(`Unknown logical NumOS key: ${logicalId}`);
  const secondary = [
    shiftLabel ? `Shift ${shiftLabel}` : "",
    alphaLabel ? `Alpha ${alphaLabel}` : "",
  ].filter(Boolean).join(", ");
  return Object.freeze({
    physicalId,
    logicalId,
    code: logicalKey.code,
    label,
    shiftLabel,
    alphaLabel,
    category,
    ariaLabel: secondary ? `${label}; ${secondary}` : label,
  });
};

export const NUMOS_WEB_KEYPAD_LAYOUT = Object.freeze([
  Object.freeze({
    name: "physical calculator",
    keys: Object.freeze([
      ["r0c0", "SHIFT", "SHIFT", "LOCK", "", "modifier"],
      ["r0c1", "ALPHA", "ALPHA", "A-LCK", "A-LCK", "modifier"],
      ["r0c2", "UP", "↑", "Pg↑", "", "navigation"],
      ["r0c3", "MODE", "HOME", "SETUP", "", "system"],
      ["r0c4", "AC", "BACK", "QUIT", "", "system"],

      ["r1c0", "STO", "VAR", "MGR", "", "system"],
      ["r1c1", "LEFT", "←", "HOME", "", "navigation"],
      ["r1c2", "DOWN", "↓", "Pg↓", "", "navigation"],
      ["r1c3", "RIGHT", "→", "END", "", "navigation"],
      ["r1c4", "F1", "TOOLS", "CATALOG", "", "system"],

      ["r2c0", "VAR_X", "x", "θ", "A", "function"],
      ["r2c1", "DIV", "frac", "mixed", "B", "function"],
      ["r2c2", "SQRT", "√", "∛", "C", "function"],
      ["r2c3", "POW", "x²", "x³", "D", "function"],
      ["r2c4", "POW", "x^□", "ⁿ√", "E", "function"],

      ["r3c0", "LOG", "log", "10ˣ", "F", "function"],
      ["r3c1", "LN", "ln", "eˣ", "G", "function"],
      ["r3c2", "SIN", "sin", "sin⁻¹", "H", "function"],
      ["r3c3", "COS", "cos", "cos⁻¹", "I", "function"],
      ["r3c4", "TAN", "tan", "tan⁻¹", "J", "function"],

      ["r4c0", "STO", "STO→", "RCL", "K", "system"],
      ["r4c1", "CONST_PI", "π", "i", "L", "function"],
      ["r4c2", "CONST_E", "e", "∞", "M", "function"],
      ["r4c3", "LPAREN", "(", "[", "N", "operator"],
      ["r4c4", "RPAREN", ")", "]", "O", "operator"],

      ["r5c0", "FREE_EQ", "FORMAT", "TABLE", "P", "system"],
      ["r5c1", "F1", ",", ";", "Q", "operator"],
      ["r5c2", "FREE_EQ", "=", "≠", "R", "operator"],
      ["r5c3", "LESS", "<", "≤", "S", "operator"],
      ["r5c4", "GREATER", ">", "≥", "T", "operator"],

      ["r6c0", "NUM_7", "7", "nPr", "U", "number"],
      ["r6c1", "NUM_8", "8", "nCr", "V", "number"],
      ["r6c2", "NUM_9", "9", "rand", "W", "number"],
      ["r6c3", "DEL", "DEL", "UNDO", "", "system"],
      ["r6c4", "AC", "AC", "OFF", "", "system"],

      ["r7c0", "NUM_4", "4", "F4", "X", "number"],
      ["r7c1", "NUM_5", "5", "F5", "Y", "number"],
      ["r7c2", "NUM_6", "6", "F6", "Z", "number"],
      ["r7c3", "MUL", "×", "!", "", "operator"],
      ["r7c4", "DIV", "÷", "%", "", "operator"],

      ["r8c0", "NUM_1", "1", "F1", "", "number"],
      ["r8c1", "NUM_2", "2", "F2", "", "number"],
      ["r8c2", "NUM_3", "3", "F3", "", "number"],
      ["r8c3", "ADD", "+", "Σ", "", "operator"],
      ["r8c4", "SUB", "−", "∫", "", "operator"],

      ["r9c0", "NUM_0", "0", "°", "space", "number"],
      ["r9c1", "DOT", ".", ":", "_", "number"],
      ["r9c2", "POW", "×10ˣ", "ENG", "\"", "function"],
      ["r9c3", "NEG", "(-)", "Ans", "PreAns", "operator"],
      ["r9c4", "ENTER", "EXE", "≈", "", "system"],
    ].map(physicalKey)),
  }),
]);

export const NUMOS_LOGICAL_KEY_MAX = 79;
