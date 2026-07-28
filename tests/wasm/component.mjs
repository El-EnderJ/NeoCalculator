import assert from "node:assert/strict";
import { chromium, firefox, webkit } from "playwright";
import { startStaticServer } from "./test-server.mjs";

const variant = process.env.NUMOS_WASM_VARIANT || "release";
const engineName = process.env.NUMOS_BROWSER || "chromium";
const browserType = { chromium, firefox, webkit }[engineName];
assert.ok(browserType, `unknown browser: ${engineName}`);
const port = Number(process.env.NUMOS_WASM_COMPONENT_PORT ||
  ({ chromium: 4201, firefox: 4202, webkit: 4203 }[engineName]) +
  (variant === "debug" ? 10 : 0));
const root = new URL(`../../out/wasm/dist/${variant}/`, import.meta.url).pathname;
const server = await startStaticServer(root, port);
const launchOptions = { headless: true };
if (engineName === "chromium") launchOptions.executablePath = chromium.executablePath();

const browser = await browserType.launch(launchOptions);
const context = await browser.newContext({
  viewport: { width: 900, height: 720 },
  deviceScaleFactor: 1,
});
const page = await context.newPage();
const fatalErrors = [];
let repeatedHeapSamples = [];
let phase = "startup";
page.on("pageerror", (error) => fatalErrors.push(`pageerror: ${error.stack || error}`));
page.on("console", (message) => {
  if (message.type() === "error" &&
      !(phase === "missing-asset" &&
        message.text().includes("Failed to load resource"))) {
    fatalErrors.push(`console: ${message.text()}`);
  }
});

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const fixtureUrl = `${server.origin}/fixture.html`;
const createElement = async (attributes = {}) => page.evaluate((attrs) => {
  const element = document.createElement("numos-emulator");
  for (const [name, value] of Object.entries(attrs)) {
    if (value === true) element.setAttribute(name, "");
    else if (value !== false && value != null) element.setAttribute(name, String(value));
  }
  document.body.append(element);
  window.emulator = element;
}, attributes);
const canvas = () => page.locator("numos-emulator").locator("canvas");
const diagnostics = () => page.evaluate(() => window.emulator.diagnosticState());
const start = async () => {
  const result = await page.evaluate(async () => {
    try {
      return { ok: true, value: await window.emulator.start() };
    } catch (error) {
      return {
        ok: false,
        error: {
          name: error.name, code: error.code, message: error.message,
          details: error.details, stack: error.stack,
        },
      };
    }
  });
  if (!result.ok) {
    throw new Error(`component start failed: ${JSON.stringify(result.error)}`);
  }
  return result.value;
};
const shutdown = () => page.evaluate(() => window.emulator.shutdown());
const waitForApp = (name) => page.waitForFunction(
  (expected) => window.emulator?.diagnosticState()?.app === expected,
  name, { timeout: 30000 });
const clickLogical = async (x, y) => {
  const target = canvas();
  const box = await target.boundingBox();
  assert.ok(box, "component canvas must have a layout box");
  await target.click({
    position: { x: x * box.width / 320, y: y * box.height / 240 },
    delay: 35,
  });
};
const returnToMenu = async () => {
  await page.evaluate(() => window.emulator.pressLogicalKey(3));
  await waitForApp("Menu");
  await delay(350);
};
const openLauncherApp = async (id, name) => {
  await waitForApp("Menu");
  for (let attempt = 0; attempt < 24; ++attempt) {
    const current = await diagnostics();
    if (current.menuFocus === id) break;
    await page.evaluate((keyCode) => window.emulator.pressLogicalKey(keyCode),
      current.menuFocus < id ? 16 : 13);
    await delay(65);
  }
  const current = await diagnostics();
  assert.equal(current.menuFocus, id, `launcher could not focus card ${id}`);
  await delay(350);
  const point = (await diagnostics()).menuFocusPoint;
  await clickLogical(point.x, point.y);
  await waitForApp(name);
};

try {
  await page.goto(fixtureUrl, { waitUntil: "domcontentloaded" });
  await page.waitForFunction(() => customElements.get("numos-emulator"));
  assert.equal(await page.evaluate(() => "numos" in window), false,
    "the reusable component fixture must not install the demo-only global adapter");
  assert.equal(server.requests.some((path) => path.endsWith(".wasm")), false,
    "defining the custom element must not fetch Wasm");
  assert.equal(server.requests.some((path) => path.includes("numos-runtime.")), false,
    "defining the custom element must not fetch the generated loader");
  assert.equal(server.requests.includes("/numos-assets.json"), false,
    "defining the custom element must not fetch the asset manifest");

  await createElement({ controls: "hidden" });
  const preStartRequests = [...server.requests];
  await delay(120);
  assert.deepEqual(server.requests, preStartRequests,
    "an idle component must not make startup requests");
  const progress = [];
  await page.evaluate(() => {
    window.componentProgress = [];
    window.emulator.addEventListener("numos-progress", (event) => {
      window.componentProgress.push(event.detail);
    });
  });
  const ready = await start();
  progress.push(...await page.evaluate(() => window.componentProgress));
  assert.equal(ready.ready, true);
  assert.equal(ready.logicalWidth, 320);
  assert.equal(ready.logicalHeight, 240);
  assert.ok(progress.some((event) => event.stage === "Wasm download" &&
    event.downloadedBytes > 0 && event.totalBytes > 0));
  assert.ok(progress.some((event) => event.stage === "runtime loader"));
  assert.ok(progress.some((event) => event.stage === "storage hydration"));
  assert.equal(await canvas().getAttribute("width"), "320");
  assert.equal(await canvas().getAttribute("height"), "240");

  // Keyboard events are bound to the focused Shadow DOM canvas, not window.
  const initialFocus = (await diagnostics()).menuFocus;
  await page.evaluate(() => {
    const input = document.createElement("input");
    input.id = "outside-input";
    document.body.prepend(input);
    input.focus();
  });
  await page.keyboard.press("ArrowRight");
  await delay(60);
  assert.equal((await diagnostics()).menuFocus, initialFocus,
    "unfocused component must not capture physical keyboard input");
  await canvas().focus();
  await page.keyboard.press("ArrowRight");
  await delay(60);
  assert.notEqual((await diagnostics()).menuFocus, initialFocus);

  // Integer and fractional CSS scales preserve logical pointer coordinates.
  await page.setViewportSize({ width: 1040, height: 820 });
  await clickLogical(300, 220);
  let pointer = (await diagnostics()).pointer;
  assert.ok(Math.abs(pointer.x - 300) <= 2 && Math.abs(pointer.y - 220) <= 2,
    `integer pointer map mismatch: ${JSON.stringify(pointer)}`);
  await page.evaluate(() => { window.emulator.style.width = "278px"; });
  await delay(100);
  await clickLogical(300, 220);
  pointer = (await diagnostics()).pointer;
  assert.ok(Math.abs(pointer.x - 300) <= 2 && Math.abs(pointer.y - 220) <= 2,
    `fractional pointer map mismatch: ${JSON.stringify(pointer)}`);
  await page.evaluate(() => { window.emulator.style.width = ""; });

  // Calculation 2+2 exercises the real SDL/LVGL/Giac path.
  await page.evaluate(() => window.emulator.pressLogicalKey(3));
  await waitForApp("Menu");
  await delay(350);
  await openLauncherApp(0, "Calculation");
  const giacStarted = performance.now();
  await canvas().focus();
  await page.keyboard.type("2+2");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() => {
    const state = window.emulator.diagnosticState();
    return state?.calculation?.status === "ok" &&
      state.calculation.exact === "4";
  });
  const firstGiacMs = performance.now() - giacStarted;
  let state = await diagnostics();
  assert.equal(state.calculation.engine, "giac");
  assert.equal(state.calculation.resultKind, "structured");
  assert.equal(state.giac.activeContexts, 1);

  // Grapher retained compilation and Equations structured solve remain real.
  await returnToMenu();
  await openLauncherApp(1, "Grapher");
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("Enter");
  await page.keyboard.type("y=x^2");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() => {
    const value = window.emulator.diagnosticState();
    return value.grapher.relations === 1 && value.grapher.slot0CompileOk;
  });
  assert.equal((await diagnostics()).grapher.slot0CompileCount, 1);
  await returnToMenu();
  await openLauncherApp(2, "Equations");
  await page.keyboard.press("Enter");
  await page.keyboard.press("Enter");
  await page.keyboard.type("2*x+4=0");
  await page.keyboard.press("Enter");
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() =>
    window.emulator.diagnosticState().equations.x0Exact === "-2");
  assert.equal((await diagnostics()).equations.resultKind, "structured");
  await returnToMenu();

  // Complete logical touch catalog, pointer release/cancel, two-finger input.
  await page.evaluate(() => { window.emulator.controls = "visible"; });
  const keyCount = await page.locator("numos-emulator")
    .locator(".key").count();
  assert.equal(keyCount, 79);
  const right = page.locator("numos-emulator").locator('[data-key-id="RIGHT"]');
  const down = page.locator("numos-emulator").locator('[data-key-id="DOWN"]');
  const focusBeforeTouch = (await diagnostics()).menuFocus;
  await right.dispatchEvent("pointerdown", {
    pointerId: 31, pointerType: "touch", isPrimary: true, bubbles: true,
  });
  await right.dispatchEvent("pointerup", {
    pointerId: 31, pointerType: "touch", isPrimary: true, bubbles: true,
  });
  assert.notEqual((await diagnostics()).menuFocus, focusBeforeTouch);
  assert.equal(await right.getAttribute("aria-pressed"), "false");
  await right.dispatchEvent("pointerdown", {
    pointerId: 32, pointerType: "touch", isPrimary: true, bubbles: true,
  });
  await right.dispatchEvent("pointercancel", {
    pointerId: 32, pointerType: "touch", isPrimary: true, bubbles: true,
  });
  assert.equal(await right.getAttribute("aria-pressed"), "false",
    "pointer cancellation must release visual/logical key state");
  await right.dispatchEvent("pointerdown", {
    pointerId: 33, pointerType: "touch", isPrimary: true, bubbles: true,
  });
  await down.dispatchEvent("pointerdown", {
    pointerId: 34, pointerType: "touch", isPrimary: false, bubbles: true,
  });
  await right.dispatchEvent("pointerup", {
    pointerId: 33, pointerType: "touch", isPrimary: true, bubbles: true,
  });
  await down.dispatchEvent("pointerup", {
    pointerId: 34, pointerType: "touch", isPrimary: false, bubbles: true,
  });
  assert.equal(await right.getAttribute("aria-pressed"), "false");
  assert.equal(await down.getAttribute("aria-pressed"), "false");
  await page.evaluate(() => { window.emulator.controls = "hidden"; });
  assert.equal(await page.locator("numos-emulator").locator(".controls").isHidden(), true);
  await page.evaluate(() => { window.emulator.controls = "auto"; });

  // Fullscreen state logic uses a controlled API harness in headless mode.
  await page.evaluate(() => {
    let fullscreenElement = null;
    Object.defineProperty(document, "fullscreenElement", {
      configurable: true, get: () => fullscreenElement,
    });
    window.emulator.requestFullscreen = async () => {
      fullscreenElement = window.emulator;
      document.dispatchEvent(new Event("fullscreenchange"));
    };
    document.exitFullscreen = async () => {
      fullscreenElement = null;
      document.dispatchEvent(new Event("fullscreenchange"));
    };
  });
  await page.evaluate(() => window.emulator.enterFullscreen());
  assert.equal(await page.evaluate(() => document.fullscreenElement === window.emulator), true);
  await page.evaluate(() => window.emulator.exitFullscreen());
  assert.equal(await page.evaluate(() => document.fullscreenElement), null);

  const persistence = await page.evaluate(() => window.emulator.persistenceState());
  assert.ok(["persistent_ready", "ephemeral_fallback"].includes(persistence.state));
  assert.equal("databaseName" in persistence, false);
  assert.equal("storeName" in persistence, false);
  assert.equal("metadata" in persistence, false);

  // A second simultaneously active component rejects deterministically.
  const secondCode = await page.evaluate(async () => {
    const second = document.createElement("numos-emulator");
    document.body.append(second);
    window.secondEmulator = second;
    try {
      await second.start();
      return "unexpected-success";
    } catch (error) {
      return error.code;
    }
  });
  assert.equal(secondCode, "SECOND_ACTIVE_INSTANCE");

  let shutdownEvent;
  await page.evaluate(() => {
    window.emulator.addEventListener("numos-shutdown", (event) => {
      window.lastShutdown = event.detail;
    }, { once: true });
  });
  await shutdown();
  shutdownEvent = await page.evaluate(() => window.lastShutdown);
  assert.equal(shutdownEvent.giacContexts, 0);
  assert.equal(await page.locator("numos-emulator").locator("canvas").count(), 0);

  // Another element can start after the first fully releases the guard.
  const secondReady = await page.evaluate(() => window.secondEmulator.start());
  assert.equal(secondReady.ready, true);
  await page.evaluate(() => window.secondEmulator.shutdown());
  await page.evaluate(() => {
    window.emulator.remove();
    window.secondEmulator.remove();
    delete window.emulator;
  });

  if (engineName === "chromium") {
    // Viewport lazy loading does not start until intersection.
    await page.evaluate(() => {
      const spacer = document.createElement("div");
      spacer.style.height = "2400px";
      document.body.append(spacer);
      const lazy = document.createElement("numos-emulator");
      lazy.autostart = true;
      lazy.setAttribute("root-margin", "0px");
      document.body.append(lazy);
      window.emulator = lazy;
    });
    const beforeLazyWasm = server.requests.filter((path) => path.endsWith(".wasm")).length;
    await delay(150);
    assert.equal(server.requests.filter((path) => path.endsWith(".wasm")).length,
      beforeLazyWasm);
    await page.locator("numos-emulator").last().scrollIntoViewIfNeeded();
    const lazyReady = await page.evaluate(() => window.emulator.ready);
    assert.equal(lazyReady.ready, true);
    await shutdown();
    await page.evaluate(() => { window.emulator.remove(); });

    // Missing manifest is recoverable and retry creates a clean generation.
    phase = "missing-asset";
    await createElement({ manifest: "./missing-assets.json" });
    const missingCode = await page.evaluate(async () => {
      try { await window.emulator.start(); }
      catch (error) { return error.code; }
    });
    assert.equal(missingCode, "ASSET_MANIFEST_FAILURE");
    await delay(50);
    phase = "missing-retry";
    await page.evaluate(() => window.emulator.removeAttribute("manifest"));
    assert.equal((await start()).ready, true);
    await shutdown();
    await page.evaluate(() => { window.emulator.remove(); });

    // IndexedDB denial degrades to visible ephemeral mode without blocking use.
    await createElement({ "persistence-test": "indexeddb-denied" });
    assert.equal((await start()).ready, true);
    assert.equal((await page.evaluate(() => window.emulator.persistenceState())).state,
      "ephemeral_fallback");
    await shutdown();
    await page.evaluate(() => { window.emulator.remove(); });

    // Five fresh module generations; every native shutdown proves Giac=0.
    const heapSamples = [];
    for (let cycle = 0; cycle < 5; ++cycle) {
      await createElement({ controls: "hidden" });
      await start();
      heapSamples.push((await diagnostics()).usedHeapBytes);
      await page.evaluate(() => {
        window.lastShutdown = null;
        window.emulator.addEventListener("numos-shutdown", (event) => {
          window.lastShutdown = event.detail;
        }, { once: true });
      });
      await shutdown();
      assert.equal((await page.evaluate(() => window.lastShutdown)).giacContexts, 0);
      await page.evaluate(() => { window.emulator.remove(); });
    }
    repeatedHeapSamples = heapSamples;
    const monotonic = heapSamples.every((value, index) =>
      index === 0 || value > heapSamples[index - 1]);
    assert.equal(monotonic, false,
      `fresh-generation heap samples grew monotonically: ${heapSamples}`);

    // Nested subpath resolves manifest, loader, and hashed Wasm relatively.
    await page.goto(`${server.origin}/demo/calculator/fixture.html`,
      { waitUntil: "domcontentloaded" });
    await page.waitForFunction(() => customElements.get("numos-emulator"));
    await createElement();
    assert.equal((await start()).ready, true);
    await shutdown();

    // Touch-capable high-DPR portrait context keeps 320×240 logic and auto UI.
    const mobileContext = await browser.newContext({
      viewport: { width: 390, height: 844 },
      deviceScaleFactor: 3,
      hasTouch: true,
      isMobile: true,
    });
    const mobilePage = await mobileContext.newPage();
    await mobilePage.goto(fixtureUrl, { waitUntil: "domcontentloaded" });
    await mobilePage.waitForFunction(() => customElements.get("numos-emulator"));
    await mobilePage.evaluate(() => {
      const emulator = document.createElement("numos-emulator");
      emulator.controls = "auto";
      document.body.append(emulator);
      window.emulator = emulator;
    });
    const mobileReady = await mobilePage.evaluate(() => window.emulator.start());
    assert.equal(mobileReady.ready, true);
    assert.equal(await mobilePage.evaluate(() => devicePixelRatio), 3);
    assert.equal(await mobilePage.locator("numos-emulator")
      .locator(".controls").isVisible(), true);
    const mobileCanvas = mobilePage.locator("numos-emulator").locator("canvas");
    assert.equal(await mobileCanvas.getAttribute("width"), "320");
    assert.equal(await mobileCanvas.getAttribute("height"), "240");
    const mobileFocus = await mobilePage.evaluate(() =>
      window.emulator.diagnosticState().menuFocus);
    const rightButton = mobilePage.locator("numos-emulator")
      .locator('[data-key-id="RIGHT"]');
    const rightBox = await rightButton.boundingBox();
    await mobilePage.touchscreen.tap(
      rightBox.x + rightBox.width / 2, rightBox.y + rightBox.height / 2);
    await delay(70);
    assert.notEqual(await mobilePage.evaluate(() =>
      window.emulator.diagnosticState().menuFocus), mobileFocus);
    await mobilePage.evaluate(() => window.emulator.shutdown());
    await mobileContext.close();
  }

  assert.deepEqual(fatalErrors, [], fatalErrors.join("\n"));
  console.log(JSON.stringify({
    engine: engineName,
    variant,
    firstGiacMs,
    progressEvents: progress.length,
    requestCount: server.requests.length,
    repeatedHeapSamples,
  }, null, 2));
} finally {
  await context.close();
  await browser.close();
  await server.close();
}
