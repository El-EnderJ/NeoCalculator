import assert from "node:assert/strict";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium } from "playwright";
import { startStaticServer } from "./test-server.mjs";

const variant = process.env.NUMOS_WASM_VARIANT || "release";
const port = variant === "debug" ? 4222 : 4221;
const root = new URL(`../../out/wasm/dist/${variant}/`, import.meta.url).pathname;
const server = await startStaticServer(root, port);
const profile = await mkdtemp(join(tmpdir(), `numos-wasm-performance-${variant}-`));
const context = await chromium.launchPersistentContext(profile, {
  headless: true,
  executablePath: chromium.executablePath(),
  viewport: { width: 1000, height: 800 },
  deviceScaleFactor: 1,
});
const page = context.pages()[0] || await context.newPage();

const canvas = () => page.locator("numos-emulator").locator("canvas");
const diagnostics = () => page.evaluate(() => window.emulator.diagnosticState());
const clickLogical = async (x, y) => {
  const box = await canvas().boundingBox();
  await canvas().click({
    position: { x: x * box.width / 320, y: y * box.height / 240 },
  });
};

try {
  const navigationStarted = performance.now();
  await page.goto(`${server.origin}/fixture.html`, { waitUntil: "domcontentloaded" });
  await page.waitForFunction(() => customElements.get("numos-emulator"));
  const definitionMs = performance.now() - navigationStarted;
  const shellStarted = performance.now();
  await page.evaluate(() => {
    performance.clearResourceTimings();
    const emulator = document.createElement("numos-emulator");
    emulator.controls = "visible";
    document.body.append(emulator);
    window.emulator = emulator;
  });
  await page.locator("numos-emulator").waitFor({ state: "visible" });
  const firstVisibleShellMs = performance.now() - shellStarted;

  const coldReady = await page.evaluate(() => window.emulator.start());
  const coldEntries = await page.evaluate(() =>
    performance.getEntriesByType("resource")
      .filter((entry) => entry.name.endsWith(".wasm"))
      .map((entry) => ({
        transferSize: entry.transferSize,
        encodedBodySize: entry.encodedBodySize,
        decodedBodySize: entry.decodedBodySize,
        duration: entry.duration,
      })));
  assert.ok(coldEntries.length >= 1);
  const memoryReady = await diagnostics();

  await clickLogical(55, 72);
  await page.waitForFunction(() =>
    window.emulator.diagnosticState().app === "Calculation");
  const giacStarted = performance.now();
  await canvas().focus();
  await page.keyboard.type("2+2");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() =>
    window.emulator.diagnosticState().calculation.exact === "4");
  const firstGiacMs = performance.now() - giacStarted;
  await page.evaluate(() => window.emulator.pressLogicalKey(3));
  await page.waitForFunction(() =>
    window.emulator.diagnosticState().app === "Menu");

  const controlLatencyMs = await page.evaluate(async () => {
    const before = window.emulator.diagnosticState().menuFocus;
    const button = window.emulator.shadowRoot.querySelector('[data-key-id="RIGHT"]');
    const started = performance.now();
    button.dispatchEvent(new PointerEvent("pointerdown", {
      bubbles: true, pointerId: 91, pointerType: "touch", isPrimary: true,
    }));
    button.dispatchEvent(new PointerEvent("pointerup", {
      bubbles: true, pointerId: 91, pointerType: "touch", isPrimary: true,
    }));
    while (window.emulator.diagnosticState().menuFocus === before) {
      await new Promise(requestAnimationFrame);
    }
    return performance.now() - started;
  });

  await new Promise((resolve) => setTimeout(resolve, 600));
  const settled = await diagnostics();
  const shutdownStarted = performance.now();
  await page.evaluate(() => window.emulator.shutdown());
  const shutdownWallMs = performance.now() - shutdownStarted;

  await page.evaluate(() => performance.clearResourceTimings());
  const warmReady = await page.evaluate(() => window.emulator.start());
  const warmEntries = await page.evaluate(() =>
    performance.getEntriesByType("resource")
      .filter((entry) => entry.name.endsWith(".wasm"))
      .map((entry) => ({
        transferSize: entry.transferSize,
        encodedBodySize: entry.encodedBodySize,
        decodedBodySize: entry.decodedBodySize,
        duration: entry.duration,
      })));
  const memoryAfterRestart = await diagnostics();
  await page.evaluate(() => window.emulator.shutdown());

  console.log(JSON.stringify({
    variant,
    definitionMs,
    firstVisibleShellMs,
    cold: {
      timings: coldReady.timings,
      resource: coldEntries.at(-1),
    },
    warm: {
      timings: warmReady.timings,
      resource: warmEntries.at(-1),
    },
    firstGiacMs,
    controlLatencyMs,
    frameMs: settled.frameMs,
    memoryAfterReady: {
      capacityBytes: memoryReady.heapBytes,
      usedBytes: memoryReady.usedHeapBytes,
    },
    memoryAfterRestart: {
      capacityBytes: memoryAfterRestart.heapBytes,
      usedBytes: memoryAfterRestart.usedHeapBytes,
    },
    shutdownWallMs,
  }, null, 2));
} finally {
  await context.close();
  await server.close();
  await rm(profile, { recursive: true, force: true });
}
