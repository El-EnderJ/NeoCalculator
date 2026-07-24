import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { NUMOS_LOGICAL_KEYS } from "../../wasm/numos-keypad.js";

const source = await readFile(
  new URL("../../src/input/KeyCodes.h", import.meta.url), "utf8");
const body = source.match(/enum class KeyCode\s*:[^{]+\{([\s\S]*?)\};/)?.[1];
assert.ok(body, "KeyCode enum must be present");
const tokens = body
  .replace(/\/\*[\s\S]*?\*\//g, "")
  .replace(/\/\/.*$/gm, "")
  .split(",")
  .map((token) => token.trim())
  .filter(Boolean);

const expected = [];
let value = -1;
for (const token of tokens) {
  const match = token.match(/^([A-Z][A-Z0-9_]*)(?:\s*=\s*(\d+))?$/);
  assert.ok(match, `unsupported KeyCode declaration: ${token}`);
  value = match[2] ? Number(match[2]) : value + 1;
  if (match[1] !== "NONE") expected.push([match[1], value]);
}

const actual = NUMOS_LOGICAL_KEYS.map(({ id, code }) => [id, code]);
assert.deepEqual(actual, expected,
  "web logical-key catalog must exactly match the C++ enum");
assert.equal(new Set(actual.map(([, code]) => code)).size, actual.length,
  "web logical-key codes must be unique");
console.log(`NumOS web logical-key catalog: ${actual.length}/${expected.length}`);
