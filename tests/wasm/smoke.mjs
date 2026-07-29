import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

const variant = process.env.NUMOS_WASM_VARIANT || "release";
const port = Number(process.env.NUMOS_WASM_PORT ||
                    (variant === "debug" ? 4174 : 4173));
const root = fileURLToPath(
  new URL(`../../out/wasm/dist/${variant}/`, import.meta.url));
const server = spawn("python3", ["-m", "http.server", String(port),
                                 "--bind", "127.0.0.1", "--directory", root], {
  stdio: ["ignore", "pipe", "pipe"],
});

const delay = (milliseconds) => new Promise((resolve) =>
  setTimeout(resolve, milliseconds));

async function waitForServer() {
  for (let attempt = 0; attempt < 100; ++attempt) {
    try {
      const response = await fetch(`http://127.0.0.1:${port}/index.html`);
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
  page.on("pageerror", (error) => fatalErrors.push(
    `pageerror: ${error.stack || error.message}`));
  page.on("console", (message) => {
    if (message.type() === "error") fatalErrors.push(`console: ${message.text()}`);
  });

  const navigationStarted = performance.now();
  await page.goto(`http://127.0.0.1:${port}/index.html`, {
    waitUntil: "domcontentloaded",
  });
  await page.waitForFunction(() => Boolean(window.numosReady), null,
                             { timeout: 30000 });
  const ready = await page.evaluate(async () => await window.numosReady);
  const launcherWallMs = performance.now() - navigationStarted;
  assert.equal(ready.ready, true, "launcher must report ready");
  assert.equal(ready.logicalWidth, 320);
  assert.equal(ready.logicalHeight, 240);

  const canvas = page.locator("numos-emulator").locator("canvas");
  await canvas.focus();
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
  const diagnostics = () => page.evaluate(() => window.numos.diagnosticState());
  const waitForApp = (name) => page.waitForFunction(
    (expected) => window.numos?.diagnosticState().app === expected,
    name, { timeout: 30000 });

  // Pointer enters the real launcher card; physical keyboard events then flow
  // through SDL_TEXTINPUT/SDL_KEYDOWN and the existing dispatchKey path.
  await clickLogical(55, 72);
  await waitForApp("Calculation");
  const beforeCalculation = await diagnostics();
  const giacStarted = performance.now();
  await page.keyboard.type("2+2");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() => {
    const state = window.numos.diagnosticState();
    return state.calculation.status === "ok" &&
           state.calculation.exact === "4";
  }, null, { timeout: 15000 });
  const firstGiacMs = performance.now() - giacStarted;
  let state = await diagnostics();
  assert.equal(state.calculation.engine, "giac");
  assert.equal(state.calculation.resultKind, "structured");
  assert.equal(state.calculation.exact, "4");
  assert.equal(state.giac.activeContexts, 1);
  assert.equal(state.giac.contextsCreated - state.giac.contextsDestroyed, 1);
  assert.ok(state.giac.structuredEvaluations >
            beforeCalculation.giac.structuredEvaluations);
  const giacGeneration = state.giac.generation;

  await page.keyboard.press("h");
  await waitForApp("Menu");
  await delay(320);

  await clickLogical(155, 72);
  await waitForApp("Grapher");
  const beforeGraph = await diagnostics();
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("Enter");
  await page.keyboard.type("y=x^2");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() => {
    const state = window.numos.diagnosticState();
    return state.grapher.relations === 1 && state.grapher.slot0CompileOk;
  }, null, { timeout: 15000 });
  state = await diagnostics();
  assert.equal(state.grapher.engine, "giac");
  assert.equal(state.grapher.slot0CompileCount, 1);
  assert.ok(state.giac.retainedCompiles > beforeGraph.giac.retainedCompiles);

  await page.keyboard.press("h");
  await waitForApp("Menu");
  await delay(320);

  await clickLogical(255, 72);
  await waitForApp("Equations");
  const beforeSolve = await diagnostics();
  await page.keyboard.press("Enter");
  await page.keyboard.press("Enter");
  await page.keyboard.type("2*x+4=0");
  await page.keyboard.press("Enter");
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("ArrowDown");
  await page.keyboard.press("Enter");
  await page.waitForFunction(() => {
    const state = window.numos.diagnosticState();
    return state.equations.status === "ok" &&
           state.equations.x0Exact === "-2";
  }, null, { timeout: 20000 });
  state = await diagnostics();
  assert.equal(state.equations.engine, "giac");
  assert.equal(state.equations.resultKind, "structured");
  assert.equal(state.equations.solutionCount, 1);
  assert.ok(state.giac.structuredSolves > beforeSolve.giac.structuredSolves);
  assert.equal(state.giac.activeContexts, 1);

  await page.keyboard.press("h");
  await waitForApp("Menu");
  await delay(350);
  const warmRetainedHandles = (await diagnostics()).giac.liveRetainedHandles;
  assert.equal(warmRetainedHandles, 0,
               "Grapher teardown must release every retained expression");

  // All three lazy screens are warm now. Repeat real pointer/keyboard switches,
  // wait for deferred teardown, and sample allocator usage at the same state.
  const heapSamples = [];
  for (let cycle = 0; cycle < 6; ++cycle) {
    for (const x of [55, 155, 255]) {
      await clickLogical(x, 72);
      try {
        await page.waitForFunction(() =>
          window.numos.diagnosticState().app !== "Menu", null,
          { timeout: 30000 });
      } catch (error) {
        const failedState = await diagnostics();
        throw new Error(`launcher click failed at cycle ${cycle}, x=${x}: ` +
                        JSON.stringify({ failedState, fatalErrors }),
                        { cause: error });
      }
      await delay(260); // complete the production 200 ms screen transition
      await page.keyboard.press("h");
      await waitForApp("Menu");
      await delay(350);
    }
    state = await diagnostics();
    heapSamples.push(state.usedHeapBytes);
    assert.equal(state.giac.activeContexts, 1);
    assert.equal(state.giac.contextsCreated - state.giac.contextsDestroyed, 1);
    assert.equal(state.giac.generation, giacGeneration,
                 "app switching must not reset the singleton Giac context");
    assert.equal(state.giac.liveRetainedHandles, warmRetainedHandles,
                 "app switching must not leak retained Giac handles");
  }
  const strictlyIncreasing = heapSamples.every((value, index) =>
    index === 0 || value > heapSamples[index - 1]);
  assert.equal(strictlyIncreasing, false,
               `heap grew after every bounded cycle: ${heapSamples.join(",")}`);
  assert.ok(heapSamples.at(-1) <= heapSamples[0] + 1024 * 1024,
            `bounded heap drift exceeded 1 MiB: ${heapSamples.join(",")}`);

  state = await diagnostics();
  assert.equal(fatalErrors.length, 0, fatalErrors.join("\n"));
  const measurements = {
    moduleReadyMs: ready.timings.moduleFactoryMs,
    browserLauncherMs: ready.timings.startToLauncherMs,
    launcherWallMs,
    firstGiacMs,
    heapSamples,
    finalHeapBytes: state.heapBytes,
    finalUsedHeapBytes: state.usedHeapBytes,
    frameMs: state.frameMs,
    giac: state.giac,
    appLaunches: state.appLaunches,
    menuReturns: state.menuReturns,
  };
  console.log(JSON.stringify(measurements, null, 2));

  const shutdownDetail = await page.evaluate(async () => {
    const emulator = document.querySelector("numos-emulator");
    const eventPromise = new Promise((resolve) =>
      emulator.addEventListener("numos-shutdown",
        (event) => resolve(event.detail), { once: true }));
    await window.numos.requestShutdown();
    return eventPromise;
  });
  assert.equal(shutdownDetail.giacContexts, 0,
               "native shutdown must destroy the Giac context");
  await page.waitForFunction(() => !window.numos.isReady(), null,
                             { timeout: 10000 });
  assert.equal(await page.locator("numos-emulator").locator("canvas").count(), 0,
               "component shutdown must release its canvas");
  await delay(100);
  assert.equal(fatalErrors.length, 0, fatalErrors.join("\n"));
} finally {
  if (browser) await browser.close();
  server.kill("SIGTERM");
}
