#!/usr/bin/env python3
"""Validate Revision C and generate the bounded production keypad tables."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
LAYOUT = (
    ROOT
    / "hardware"
    / "keyboard"
    / "neocalculator-v1-final-5x10-revision-c-canonical.json"
)
OUTPUT = ROOT / "src" / "input" / "generated" / "ProductionKeypadMap.generated.h"
REPORT = (
    ROOT / "hardware" / "keyboard" / "production-keypad-map.generated.csv"
)
EXPECTED_LAYOUT_SHA256 = (
    "7f6638ae7f830ef0741424d621832756268e5fa41292f2d8f8c363cdbc1a2fc3"
)
SCHEMATIC_SHA256 = (
    "6a5855fe3a239c35263f5d75fe9082fc44e228484b4a8f11a86a66a987a37007"
)
PCB_SHA256 = (
    "fed978049fb6eb03325c0ad8050e56aa8a9662d8e5feabcd52a732c1d24cd848"
)

# Accepted KiCad extraction. Visual coordinates never double as electrical
# coordinates: these transforms are intentionally explicit.
VISUAL_X_UM = (122_101, 135_301, 148_501, 161_701, 174_901)
VISUAL_Y_UM = (
    89_004,
    98_804,
    108_604,
    118_404,
    128_204,
    138_004,
    147_804,
    157_604,
    167_404,
    177_204,
)
SW31_X_UM = 148_501
SW31_Y_UM = 177_133

PRIMARY_KEYCODES = {
    "shift_once": "SHIFT",
    "alpha_once": "ALPHA",
    "cursor_up": "UP",
    "go_home": "HOME",
    "go_back": "BACK",
    "open_variable_picker": "VAR",
    "cursor_left": "LEFT",
    "cursor_down": "DOWN",
    "cursor_right": "RIGHT",
    "open_toolbox": "TOOLBOX",
    "context_independent_variable": "VAR_X",
    "fraction_template": "FRAC",
    "square_root": "SQRT",
    "power_template": "POW",
    "square": "SQUARE",
    "log10": "LOG",
    "ln": "LN",
    "sin": "SIN",
    "cos": "COS",
    "tan": "TAN",
    "store": "STO",
    "constant_pi": "CONST_PI",
    "constant_e": "CONST_E",
    "left_parenthesis": "LPAREN",
    "right_parenthesis": "RPAREN",
    "format_result": "FORMAT",
    "comma": "COMMA",
    "equal": "EQUAL",
    "less_than": "LESS",
    "greater_than": "GREATER",
    "digit_7": "NUM_7",
    "digit_8": "NUM_8",
    "digit_9": "NUM_9",
    "delete": "DEL",
    "clear": "AC",
    "digit_4": "NUM_4",
    "digit_5": "NUM_5",
    "digit_6": "NUM_6",
    "multiply": "MUL",
    "divide": "DIVIDE",
    "digit_1": "NUM_1",
    "digit_2": "NUM_2",
    "digit_3": "NUM_3",
    "add": "ADD",
    "subtract": "SUB",
    "digit_0": "NUM_0",
    "decimal_point": "DOT",
    "scientific_exponent": "EXP",
    "unary_negative": "NEGATE",
    "execute": "EXE",
}

# Compatibility translation for existing app dispatch. SemanticId remains the
# authoritative result; NONE means the receiving editor must consume text or
# the semantic directly.
LEGACY_KEYCODES = {
    **PRIMARY_KEYCODES,
    "execute": "ENTER",
    "cursor_page_up": "UP",
    "cursor_page_down": "DOWN",
    "cursor_line_home": "LEFT",
    "cursor_line_end": "RIGHT",
    "open_settings": "SETUP",
    "open_table": "TABLE",
    "soft_1": "F1",
    "soft_2": "F2",
    "soft_3": "F3",
    "soft_4": "F4",
    "soft_5": "F5",
    "approximate": "FREE_EQ",
    "variable_y": "VAR_Y",
    "ans": "ANS",
    "preans": "PREANS",
    "factorial": "FACT",
}

PLANES = ("primary", "shift", "alpha", "shiftAlpha")
CONTEXTS = ("math", "code", "text")
CPP_KEYWORDS = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand",
    "bitor", "bool", "break", "case", "catch", "char", "char16_t",
    "char32_t", "class", "compl", "concept", "const", "consteval",
    "constexpr", "constinit", "const_cast", "continue", "co_await",
    "co_return", "co_yield", "decltype", "default", "delete", "do",
    "double", "dynamic_cast", "else", "enum", "explicit", "export",
    "extern", "false", "float", "for", "friend", "goto", "if", "inline",
    "int", "long", "mutable", "namespace", "new", "noexcept", "not",
    "not_eq", "nullptr", "operator", "or", "or_eq", "private",
    "protected", "public", "register", "reinterpret_cast", "requires",
    "return", "short", "signed", "sizeof", "static", "static_assert",
    "static_cast", "struct", "switch", "template", "this",
    "thread_local", "throw", "true", "try", "typedef", "typeid",
    "typename", "union", "unsigned", "using", "virtual", "void",
    "volatile", "wchar_t", "while", "xor", "xor_eq",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def semantic_name(definition: dict) -> str:
    return definition.get("semanticId") or definition.get("action") or "none"


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def enum_identifier(value: str) -> str:
    ident = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if ident and ident[0].isdigit():
        ident = "s_" + ident
    if ident in CPP_KEYWORDS:
        ident = "key_" + ident
    require(bool(ident), f"empty semantic identifier for {value!r}")
    return ident


def resolve_definition(key: dict, plane: str, context: str, seen=()) -> dict:
    require(plane not in seen, f"{key['id']}: inheritance cycle {seen + (plane,)}")
    definition = dict(key["planes"][plane])
    if definition.get("kind") == "inherit":
        target = definition.get("inheritFrom")
        require(target in PLANES, f"{key['id']}/{plane}: invalid inheritance")
        definition = resolve_definition(key, target, context, seen + (plane,))
    if definition.get("contextOnly") and definition["contextOnly"] != context:
        fallback = "shift" if plane == "shiftAlpha" else "primary"
        definition = resolve_definition(key, fallback, context, seen + (plane,))
    effective_context = "code" if context == "text" else context
    override = definition.get("contextOverrides", {}).get(effective_context)
    if override:
        merged = dict(definition)
        merged.update(override)
        definition = merged
    return definition


def load_and_validate() -> tuple[dict, list[dict], list[str]]:
    raw = LAYOUT.read_bytes()
    require(
        hashlib.sha256(raw).hexdigest() == EXPECTED_LAYOUT_SHA256,
        "canonical layout hash differs from accepted Revision C",
    )
    layout = json.loads(raw.decode("utf-8"))
    require(layout.get("version") == 3, "layout must be schema version 3")
    require(
        layout.get("schema") == "neocalculator.keyboard-layout",
        "unexpected layout schema",
    )
    require(
        layout.get("matrix") == {
            "visualRows": 10,
            "visualCols": 5,
            "electricalMapping": "separate-from-visual-layout",
        },
        "visual/electrical dimension contract changed",
    )
    keys = layout.get("keys", [])
    require(len(keys) == 50, "layout must contain exactly 50 keys")
    by_visual = {}
    semantics = {"none"}
    for key in keys:
        visual = key.get("visual", {})
        pos = (visual.get("row"), visual.get("col"))
        require(
            isinstance(pos[0], int)
            and isinstance(pos[1], int)
            and 0 <= pos[0] < 10
            and 0 <= pos[1] < 5,
            f"{key.get('id')}: invalid visual position",
        )
        require(pos not in by_visual, f"duplicate visual position {pos}")
        by_visual[pos] = key
        require(set(key.get("planes", {})) == set(PLANES), f"{key['id']}: planes")
        require(
            key.get("repeatPolicy") in ("none", "hold"),
            f"{key['id']}: repeat policy",
        )
        for context in CONTEXTS:
            for plane in PLANES:
                definition = resolve_definition(key, plane, context)
                semantics.add(semantic_name(definition))
    require(set(by_visual) == {(r, c) for r in range(10) for c in range(5)},
            "visual grid is incomplete")
    ordered = [by_visual[(r, c)] for r in range(10) for c in range(5)]
    primary_codes = []
    for key in ordered:
        primary = resolve_definition(key, "primary", "math")
        semantic = semantic_name(primary)
        require(
            semantic in PRIMARY_KEYCODES,
            f"{key['id']}: no primary KeyCode for {semantic}",
        )
        primary_codes.append(PRIMARY_KEYCODES[semantic])
    require(len(set(primary_codes)) == 50, "primary KeyCodes must be unique")
    require(
        layout["modifierPolicy"]["precedence"]
        == ["shiftAlpha", "shift", "alpha", "primary"],
        "modifier precedence differs from Revision C",
    )
    # WHY: KeyEvent::semanticId is zero for every legacy/CAM/web event. Keep
    # `none` explicitly at zero so adding generated semantics can never change
    # the meaning of a zero-initialized event.
    return layout, ordered, ["none"] + sorted(semantics - {"none"})


def emit() -> tuple[str, str]:
    _, visual_order, semantics = load_and_validate()
    semantic_enum = {name: enum_identifier(name) for name in semantics}
    require(
        len(set(semantic_enum.values())) == len(semantic_enum),
        "semantic identifiers collide after C++ sanitization",
    )
    records = []
    definitions = {context: [] for context in CONTEXTS}
    seen_switches = set()
    seen_electrical = set()
    for key in visual_order:
        vr = key["visual"]["row"]
        vc = key["visual"]["col"]
        er = 4 - vc
        ec = vr
        sw = 42 - 10 * vc + vr
        x_um = VISUAL_X_UM[vc]
        y_um = VISUAL_Y_UM[vr]
        if sw == 31:
            x_um, y_um = SW31_X_UM, SW31_Y_UM
        primary = resolve_definition(key, "primary", "math")
        semantic = semantic_name(primary)
        record = {
            "electrical_row": er,
            "electrical_column": ec,
            "visual_row": vr,
            "visual_column": vc,
            "switch": sw,
            "x_um": x_um,
            "y_um": y_um,
            "keycode": PRIMARY_KEYCODES[semantic],
            "semantic": semantic,
            "label": primary.get("label", ""),
            "repeat": key["repeatPolicy"] == "hold",
        }
        require(sw not in seen_switches, f"duplicate SW{sw}")
        require((er, ec) not in seen_electrical, f"duplicate electrical {(er, ec)}")
        seen_switches.add(sw)
        seen_electrical.add((er, ec))
        records.append(record)
        for context in CONTEXTS:
            plane_defs = []
            for plane in PLANES:
                definition = resolve_definition(key, plane, context)
                name = semantic_name(definition)
                plane_defs.append(
                    {
                        "semantic": semantic_enum[name],
                        "legacy": LEGACY_KEYCODES.get(name, "NONE"),
                        "text": definition.get("insert", ""),
                        "consumes": bool(definition.get("consumesModifier", False)),
                    }
                )
            definitions[context].append(plane_defs)
    require(seen_switches == set(range(2, 52)), "SW2-SW51 coverage failed")
    require(
        seen_electrical == {(r, c) for r in range(5) for c in range(10)},
        "5x10 electrical coverage failed",
    )
    records.sort(key=lambda item: (item["electrical_row"], item["electrical_column"]))
    for context in CONTEXTS:
        definitions[context].sort(
            key=lambda plane_defs: 0
        )  # retained below through electrical lookup

    # Re-index definitions from visual order into electrical order.
    visual_index = {
        (key["visual"]["row"], key["visual"]["col"]): i
        for i, key in enumerate(visual_order)
    }
    ordered_definitions = {}
    for context in CONTEXTS:
        ordered_definitions[context] = []
        for record in records:
            idx = visual_index[(record["visual_row"], record["visual_column"])]
            ordered_definitions[context].append(definitions[context][idx])

    lines = [
        "// Generated by scripts/generate-production-keypad.py; DO NOT EDIT.",
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        '#include "../KeyCodes.h"',
        "",
        "namespace numos::input {",
        "",
        f'inline constexpr char kProductionLayoutSha256[] = "{EXPECTED_LAYOUT_SHA256}";',
        f'inline constexpr char kProductionSchematicSha256[] = "{SCHEMATIC_SHA256}";',
        f'inline constexpr char kProductionPcbSha256[] = "{PCB_SHA256}";',
        "",
        "enum class InputContext : uint8_t { Math, Code, Text };",
        "enum class KeyPlane : uint8_t { Primary, Shift, Alpha, ShiftAlpha };",
        "enum class SemanticId : uint16_t {",
    ]
    for name in semantics:
        lines.append(f"    {semantic_enum[name]},")
    lines.extend(
        [
            "};",
            "",
            "struct KeyPlaneDefinition {",
            "    SemanticId semantic;",
            "    KeyCode legacyCode;",
            "    const char* text;",
            "    bool consumesModifier;",
            "};",
            "",
            "struct ProductionKeyMapping {",
            "    uint8_t electricalRow;",
            "    uint8_t electricalColumn;",
            "    uint8_t switchNumber;",
            "    uint8_t visualRow;",
            "    uint8_t visualColumn;",
            "    int32_t pcbXUm;",
            "    int32_t pcbYUm;",
            "    int16_t rotationDegrees;",
            "    SemanticId primarySemantic;",
            "    KeyCode keyCode;",
            "    const char* primaryLabel;",
            "    bool repeatable;",
            "};",
            "",
            "inline constexpr std::array<ProductionKeyMapping, 50> kProductionKeypadMap = {{",
        ]
    )
    for record in records:
        lines.append(
            "    {"
            f"{record['electrical_row']}, {record['electrical_column']}, "
            f"{record['switch']}, {record['visual_row']}, {record['visual_column']}, "
            f"{record['x_um']}, {record['y_um']}, 0, "
            f"SemanticId::{semantic_enum[record['semantic']]}, "
            f"KeyCode::{record['keycode']}, {cpp_string(record['label'])}, "
            f"{str(record['repeat']).lower()}"
            "},"
        )
    lines.extend(["}};", ""])
    for context in CONTEXTS:
        title = context.capitalize()
        lines.append(
            f"inline constexpr std::array<std::array<KeyPlaneDefinition, 4>, 50> "
            f"k{title}PlaneDefinitions = {{{{"
        )
        for plane_defs in ordered_definitions[context]:
            rendered = []
            for definition in plane_defs:
                rendered.append(
                    "{"
                    f"SemanticId::{definition['semantic']}, "
                    f"KeyCode::{definition['legacy']}, "
                    f"{cpp_string(definition['text'])}, "
                    f"{str(definition['consumes']).lower()}"
                    "}"
                )
            lines.append("    {{" + ", ".join(rendered) + "}},")
        lines.extend(["}};", ""])
    lines.extend(
        [
            "constexpr bool productionMapIsComplete() {",
            "    std::array<bool, 50> electrical{};",
            "    std::array<bool, 50> visual{};",
            "    std::array<bool, 50> switches{};",
            "    for (const auto& key : kProductionKeypadMap) {",
            "        const std::size_t e = key.electricalRow * 10U + key.electricalColumn;",
            "        const std::size_t v = key.visualRow * 5U + key.visualColumn;",
            "        const std::size_t s = key.switchNumber - 2U;",
            "        if (e >= 50U || v >= 50U || s >= 50U ||",
            "            electrical[e] || visual[v] || switches[s] ||",
            "            key.keyCode == KeyCode::NONE) return false;",
            "        electrical[e] = visual[v] = switches[s] = true;",
            "    }",
            "    return true;",
            "}",
            "",
            "static_assert(kProductionKeypadMap.size() == 50);",
            "static_assert(productionMapIsComplete(),",
            '              "Production keypad map must cover electrical, visual and SW grids");',
            "",
            "} // namespace numos::input",
            "",
        ]
    )
    report_buffer = io.StringIO(newline="")
    writer = csv.writer(report_buffer, lineterminator="\n")
    writer.writerow(
        (
            "electrical_row",
            "electrical_column",
            "switch",
            "visual_row",
            "visual_column",
            "pcb_x_mm",
            "pcb_y_mm",
            "rotation_degrees",
            "keycode",
            "primary_semantic",
            "primary_label",
        )
    )
    for record in records:
        writer.writerow(
            (
                record["electrical_row"],
                record["electrical_column"],
                f"SW{record['switch']}",
                record["visual_row"],
                record["visual_column"],
                f"{record['x_um'] / 1000:.3f}",
                f"{record['y_um'] / 1000:.3f}",
                0,
                record["keycode"],
                record["semantic"],
                record["label"],
            )
        )
    return "\n".join(lines), report_buffer.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated, report = emit()
    if args.check:
        require(OUTPUT.is_file(), f"missing generated output {OUTPUT}")
        require(
            OUTPUT.read_text(encoding="utf-8") == generated,
            "generated keypad header is stale",
        )
        require(REPORT.is_file(), f"missing generated report {REPORT}")
        require(
            REPORT.read_text(encoding="utf-8") == report,
            "generated keypad report is stale",
        )
        print("production keypad generation: PASS (deterministic)")
        return 0
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(generated, encoding="utf-8", newline="\n")
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(report, encoding="utf-8", newline="\n")
    print(
        f"generated {OUTPUT.relative_to(ROOT)} and "
        f"{REPORT.relative_to(ROOT)}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        print(f"production keypad generation: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
