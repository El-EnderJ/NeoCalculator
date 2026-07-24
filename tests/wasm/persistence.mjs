import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { chromium } from "playwright";

const variant = process.env.NUMOS_WASM_VARIANT || "release";
const port = Number(process.env.NUMOS_WASM_PERSIST_PORT ||
                    (variant === "debug" ? 4184 : 4183));
const root = new URL(`../../out/wasm/dist/${variant}/`, import.meta.url).pathname;
const origin = `http://127.0.0.1:${port}`;
const server = spawn("python3", ["-m", "http.server", String(port),
                                 "--bind", "127.0.0.1", "--directory", root], {
  stdio: ["ignore", "pipe", "pipe"],
});

const delay = (milliseconds) => new Promise((resolve) =>
  setTimeout(resolve, milliseconds));

async function waitForServer() {
  for (let attempt = 0; attempt < 100; ++attempt) {
    try {
      const response = await fetch(`${origin}/index.html`);
      if (response.ok) return;
    } catch {}
    await delay(50);
  }
  throw new Error("local wasm HTTP server did not start");
}

let browser;
try {
  await waitForServer();
  browser = await chromium.launch({
    headless: true,
    executablePath: chromium.executablePath(),
  });
  const page = await browser.newPage({ viewport: { width: 900, height: 720 } });
  const fatalErrors = [];
  let phase = "origin-reset";
  page.on("pageerror", (error) => fatalErrors.push(
    `pageerror during ${phase}: ${error.stack || error.message}`));
  page.on("console", (message) => {
    if (message.type() === "error") fatalErrors.push(
      `console: ${message.text()}`);
  });
  page.on("dialog", (dialog) => dialog.accept());

  const diagnostics = () =>
    page.evaluate(() => window.numos.diagnosticState());
  const persistenceState = () =>
    page.evaluate(() => window.numos.persistenceState());
  const waitReady = async () => {
    await page.waitForFunction(() => Boolean(window.numosReady), null,
                               { timeout: 30000 });
    return page.evaluate(async () => await window.numosReady);
  };
  const waitForApp = (name) => page.waitForFunction(
    (expected) => window.numos?.diagnosticState().app === expected,
    name, { timeout: 30000 });
  const canvas = page.locator("numos-emulator").locator("canvas");
  const clickLogical = async (x, y) => {
    const box = await canvas.boundingBox();
    assert.ok(box, "canvas must have a layout box");
    await canvas.click({
      position: {
        x: x * box.width / 320,
        y: y * box.height / 240,
      },
      delay: 40,
    });
  };
  const reloadReady = async (url = null) => {
    const started = performance.now();
    if (url) {
      await page.goto(url, { waitUntil: "domcontentloaded" });
    } else {
      await page.reload({ waitUntil: "domcontentloaded" });
    }
    const ready = await waitReady();
    return { ready, wallMs: performance.now() - started };
  };
  const openApp = async (id, name) => {
    await waitForApp("Menu");
    for (let step = 0; step < 24; ++step) {
      const state = await diagnostics();
      if (state.menuFocus === id) break;
      await page.evaluate((keyCode) => window.numos.pressLogicalKey(keyCode),
                          state.menuFocus < id ? 16 : 13);
      await delay(80);
    }
    assert.equal((await diagnostics()).menuFocus, id,
                 `could not focus launcher card ${id}`);
    await delay(400); // allow production grid auto-scroll/focus animation
    let focusPoint = (await diagnostics()).menuFocusPoint;
    if (focusPoint.y < 24 || focusPoint.y >= 240) {
      const away = id < 19 ? 16 : 13;
      const back = id < 19 ? 13 : 16;
      await page.evaluate((keyCode) => window.numos.pressLogicalKey(keyCode),
                          away);
      await delay(120);
      await page.evaluate((keyCode) => window.numos.pressLogicalKey(keyCode),
                          back);
      await delay(500);
      focusPoint = (await diagnostics()).menuFocusPoint;
    }
    assert.ok(focusPoint.x >= 0 && focusPoint.y >= 24 && focusPoint.y < 240,
              `focused card is outside canvas: ${JSON.stringify(focusPoint)}`);
    await clickLogical(focusPoint.x, focusPoint.y);
    try {
      await waitForApp(name);
    } catch (error) {
      throw new Error(`launcher failed for ${name}: ${
        JSON.stringify(await diagnostics())}`, { cause: error });
    }
  };
  const returnToMenu = async () => {
    await page.evaluate(() => window.numos.pressLogicalKey(3));
    await waitForApp("Menu");
    await delay(350);
  };
  const flushMeasured = async () => {
    const heapBefore = (await diagnostics()).usedHeapBytes;
    const started = performance.now();
    const result = await page.evaluate(async () =>
      await window.numos.flushPersistence());
    return {
      wallMs: performance.now() - started,
      heapBefore,
      heapAfter: (await diagnostics()).usedHeapBytes,
      state: result,
    };
  };

  // Establish the origin with persistence explicitly disabled, then clear
  // only NumOS's own /numos IDBFS database through the copied reset API.
  await reloadReady(`${origin}/index.html?persistence=disabled`);
  assert.equal((await persistenceState()).state, "disabled");
  await page.evaluate(async () => window.numos.resetPersistentStorage());
  await page.evaluate(async () => window.numos.requestShutdown());

  // First run: hydration precedes C++ main and reports the empty schema.
  const firstLaunch = await reloadReady(`${origin}/index.html`);
  phase = "first-run-writes";
  assert.equal(firstLaunch.ready.ready, true);
  assert.equal(firstLaunch.ready.persistence.state, "persistent_ready");
  assert.equal(firstLaunch.ready.persistence.metadataStatus, "first_run");
  const emptyHydrationMs = firstLaunch.ready.timings.hydrationMs;

  // Real setting: open Settings and toggle the production angle-mode source.
  await openApp(10, "Settings");
  assert.equal((await diagnostics()).storage.angleMode, "rad");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().storage.angleMode === "deg");
  const settingsFlush = await flushMeasured();
  assert.equal(settingsFlush.state.state, "persistent_ready");
  assert.equal(settingsFlush.state.dirty, false);
  await returnToMenu();

  // Real stored variable: Calculation evaluates 7 through Giac, then STO x
  // invokes VariableManager::saveToFlash(/vars.dat).
  await openApp(0, "Calculation");
  await page.keyboard.type("7");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().calculation.exact === "7");
  await page.keyboard.press("Insert");
  await page.keyboard.press("x");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().storage.variableX === "7");
  await returnToMenu();

  // Real Neo editor: its F2 handler writes a temp file and renames it to
  // /neolang.nl through LittleFS. Only copied logical-key injection is used.
  await openApp(18, "NeoLanguage");
  await page.keyboard.press("Enter"); // tab bar -> editor content
  await page.keyboard.type("2+2");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().storage.neoSource === "2+2");
  const beforeNeoSave = await persistenceState();
  await page.evaluate(() => window.numos.pressLogicalKey(6)); // KeyCode::F2
  await page.waitForFunction((renameCount) =>
    window.numos.persistenceState().mutationNotifications.rename > renameCount,
    beforeNeoSave.mutationNotifications.rename);
  const neoFileFlush = await flushMeasured();
  assert.equal(neoFileFlush.state.dirty, false);
  assert.ok(neoFileFlush.state.storageBytes > 0);
  const representativeStorageBytes = neoFileFlush.state.storageBytes;

  // Repeated explicit calls share the in-flight operation. Repeated saves also
  // leave at most one debounce timer pending.
  await page.keyboard.press("Backspace");
  await page.keyboard.press("Backspace");
  await page.keyboard.press("Backspace");
  await page.keyboard.type("3+3");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().storage.neoSource === "3+3");
  for (let count = 0; count < 4; ++count) {
    await page.evaluate(() => window.numos.pressLogicalKey(6));
  }
  const pending = await persistenceState();
  assert.equal(pending.pendingTimer, true);
  const coalescedStarted = performance.now();
  const coalesced = await page.evaluate(async () => Promise.all([
    window.numos.flushPersistence(),
    window.numos.flushPersistence(),
    window.numos.flushPersistence(),
  ]));
  const coalescedFlushMs = performance.now() - coalescedStarted;
  assert.ok(coalesced[2].coalescedFlushCalls >=
            coalesced[0].coalescedFlushCalls);
  assert.equal((await persistenceState()).dirty, false);

  // True reload #1: settings, VariableManager and Neo source all hydrate
  // before NumOS starts reading them.
  const representativeReload = await reloadReady();
  phase = "first-reload-verification";
  let state = await diagnostics();
  assert.equal(state.storage.angleMode, "deg");
  assert.equal(state.storage.variableX, "7");
  await openApp(18, "NeoLanguage");
  state = await diagnostics();
  assert.equal(state.storage.neoSource, "3+3");
  const representativeHydrationMs =
    representativeReload.ready.timings.hydrationMs;

  // Modify all representative state again. The Neo save exercises another
  // production temp-file rename, satisfying file modification/rename coverage.
  await returnToMenu();
  phase = "second-write-sequence";
  await openApp(10, "Settings");
  await page.keyboard.press("Enter"); // DEG -> RAD
  await page.waitForFunction(() =>
    window.numos.diagnosticState().storage.angleMode === "rad");
  await returnToMenu();
  await openApp(0, "Calculation");
  await page.keyboard.type("9");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().calculation.exact === "9");
  await page.keyboard.press("Insert");
  await page.keyboard.press("x");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().storage.variableX === "9");
  await returnToMenu();
  await openApp(18, "NeoLanguage");
  await page.keyboard.press("Enter");
  for (let count = 0; count < 3; ++count) {
    await page.keyboard.press("Backspace");
  }
  await page.keyboard.type("4+4");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().storage.neoSource === "4+4");
  await page.evaluate(() => window.numos.pressLogicalKey(6));
  await flushMeasured();

  // True reload #2 verifies modifications through real runtime diagnostics.
  await reloadReady();
  phase = "second-reload-verification";
  state = await diagnostics();
  assert.equal(state.storage.angleMode, "rad");
  assert.equal(state.storage.variableX, "9");
  await openApp(18, "NeoLanguage");
  assert.equal((await diagnostics()).storage.neoSource, "4+4");

  // Clean shutdown awaits a no-op final flush.
  phase = "clean-shutdown";
  await page.evaluate(async () => window.numos.requestShutdown());
  await page.waitForFunction(() => !window.numos.isReady());

  // Dirty shutdown awaits the write and a final post-teardown flush.
  phase = "dirty-shutdown";
  await reloadReady(`${origin}/index.html`);
  await openApp(10, "Settings");
  await page.keyboard.press("Enter"); // RAD -> DEG, dirty
  await page.waitForFunction(() =>
    window.numos.diagnosticState().storage.angleMode === "deg");
  assert.equal((await persistenceState()).dirty, true);
  await page.evaluate(async () => window.numos.requestShutdown());
  await page.waitForFunction(() => !window.numos.isReady());
  await reloadReady(`${origin}/index.html`);
  assert.equal((await diagnostics()).storage.angleMode, "deg");

  // Inject one flush failure. It is reported as persistent_error, leaves the
  // emulator running, and a repeated explicit flush recovers.
  phase = "flush-failure";
  await reloadReady(`${origin}/index.html?persistenceTest=flush-failure`);
  await openApp(10, "Settings");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().storage.angleMode === "rad");
  const rejected = await page.evaluate(async () => {
    try {
      await window.numos.flushPersistence();
      return null;
    } catch (error) {
      return String(error);
    }
  });
  assert.match(rejected, /injected IDBFS flush failure/);
  assert.equal((await persistenceState()).state, "persistent_error");
  await page.evaluate(async () => Promise.all([
    window.numos.flushPersistence(),
    window.numos.flushPersistence(),
  ]));
  assert.equal((await persistenceState()).state, "persistent_ready");

  // IndexedDB denial and initial sync failure both launch on ordinary MEMFS.
  phase = "indexeddb-denied";
  const denied = await reloadReady(
    `${origin}/index.html?persistenceTest=indexeddb-denied`);
  assert.equal(denied.ready.ready, true);
  assert.equal(denied.ready.persistence.state, "ephemeral_fallback");
  assert.equal(denied.ready.persistence.mode, "ephemeral");
  await openApp(0, "Calculation");
  await page.keyboard.type("2+2");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() =>
    window.numos.diagnosticState().calculation.exact === "4");

  const failedHydration = await reloadReady(
    `${origin}/index.html?persistenceTest=hydrate-failure`);
  phase = "hydrate-failure";
  assert.equal(failedHydration.ready.ready, true);
  assert.equal(failedHydration.ready.persistence.state, "ephemeral_fallback");
  assert.ok(failedHydration.ready.persistence.history.some(
    (entry) => entry.state === "persistent_error"));

  // Explicit reset clears only /numos. A normal reload is a clean first run.
  phase = "explicit-reset";
  await reloadReady(`${origin}/index.html`);
  const resetReady = await page.evaluate(async () =>
    window.numos.resetPersistentStorage());
  assert.equal(resetReady.persistence.metadataStatus, "first_run");
  assert.equal((await persistenceState()).state, "persistent_ready");
  state = await diagnostics();
  assert.equal(state.storage.angleMode, "rad");
  assert.equal(state.storage.variableX, "0");
  const cleanReload = await reloadReady(`${origin}/index.html`);
  phase = "clean-first-run-verification";
  state = await diagnostics();
  assert.equal(cleanReload.ready.persistence.metadataStatus, "current");
  assert.equal(state.storage.angleMode, "rad");
  assert.equal(state.storage.variableX, "0");
  await openApp(18, "NeoLanguage");
  assert.equal((await diagnostics()).storage.neoSource, "");

  assert.equal(fatalErrors.length, 0, fatalErrors.join("\n"));
  const finalState = await diagnostics();
  const measurements = {
    emptyHydrationMs,
    representativeHydrationMs,
    firstReloadToLauncherMs: representativeReload.wallMs,
    settingsFlushMs: settingsFlush.wallMs,
    neoFileFlushMs: neoFileFlush.wallMs,
    coalescedFlushMs,
    representativeStorageBytes,
    settingsHeapDelta:
      settingsFlush.heapAfter - settingsFlush.heapBefore,
    neoFileHeapDelta:
      neoFileFlush.heapAfter - neoFileFlush.heapBefore,
    finalHeapBytes: finalState.heapBytes,
    finalUsedHeapBytes: finalState.usedHeapBytes,
    frameMs: finalState.frameMs,
  };
  console.log(JSON.stringify(measurements, null, 2));
} finally {
  if (browser) await browser.close();
  server.kill("SIGTERM");
}
