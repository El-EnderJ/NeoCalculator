import assert from "node:assert/strict";
import { existsSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { chromium, firefox, webkit } from "playwright";
import { startStaticServer } from "./test-server.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const repo = join(here, "..", "..");
const variant = process.argv.includes("--debug") ? "debug" : "release";
const root = join(repo, "out", "wasm-math", "dist", variant);
const server = await startStaticServer(root, 8793);
const engines = { chromium, firefox, webkit };

async function run(browserName, browserType) {
  const fallbacks = {
    chromium: join(homedir(), "Library", "Caches", "ms-playwright",
      "chromium-1181", "chrome-mac", "Chromium.app", "Contents", "MacOS",
      "Chromium"),
    firefox: join(homedir(), "Library", "Caches", "ms-playwright",
      "firefox-1489", "firefox", "Nightly.app", "Contents", "MacOS", "firefox"),
    webkit: join(homedir(), "Library", "Caches", "ms-playwright",
      "webkit-2191", "pw_run.sh"),
  };
  const configured = browserType.executablePath();
  const executablePath = existsSync(configured) ? configured
    : existsSync(fallbacks[browserName]) ? fallbacks[browserName] : undefined;
  const browser = await browserType.launch({ headless: true, executablePath });
  const page = await browser.newPage();
  const errors = [];
  page.on("pageerror", (error) => errors.push(String(error)));
  await page.goto(`${server.origin}/fixture.html`);
  const result = await page.evaluate(async ({ browserName }) => {
    const manifestUrl = new URL(
      "/nested/numos/numos-math-assets.json", location.href).href;
    const manifest = await (await fetch(manifestUrl)).json();
    const clientUrl = new URL(manifest.assets.client.url, manifestUrl).href;
    const directUrl = new URL(manifest.assets.direct.url, manifestUrl).href;
    const { createNumosMath } = await import(clientUrl);
    const { createNumosMathDirect } = await import(directUrl);
    const timings = {};
    const measure = async (name, action) => {
      const start = performance.now();
      const value = await action();
      timings[name] = performance.now() - start;
      return value;
    };
    const math = await measure("workerStartup", () =>
      createNumosMath({ manifestUrl }));
    const initialDiagnostics = await math.diagnostics();

    const twoPlusTwo = await measure("firstEvaluation",
      () => math.evaluate("2+2"));
    const large = await math.evaluate("2^100");
    const rational = await math.evaluate("1/3");
    const radical = await math.evaluate("sqrt(2)");
    const simplified = await math.simplify("x+x");
    const complex = await math.evaluate("2+3*i");
    const infinity = await math.evaluate("+infinity");
    const undefinedValue = await math.evaluate("0/0");
    const list = await math.evaluate("[1,2,3]");
    const matrix = await math.evaluate("matrix([[1,2],[3,4]])");
    const serializationFixture = await math.evaluate("matrix(6,6,0)");
    const serializationStart = performance.now();
    for (let index = 0; index < 1000; ++index)
      JSON.stringify(serializationFixture);
    timings.structuredSerialization =
      (performance.now() - serializationStart) / 1000;
    const piecewise = await math.evaluate("piecewise(x<0,-x,x)");
    const derivative = await math.differentiate("x^2", "x");
    await math.setAngleMode("degree");
    const degreeDerivative = await math.differentiate("sin(x)", "x");
    const integral = await math.integrate("x^2", "x");
    const unevaluated = await math.integrate("exp(x*ln(x))/x", "x");
    await math.setAngleMode("radian");
    const linear = await measure("firstSolve",
      () => math.solve("2*x+4=0", "x"));
    const polynomial = await math.solve("x^2-1=0", "x");
    const system = await math.solve(["x+y=3", "x-y=1"], ["x", "y"]);
    const noSolution = await math.solve("x^2+1=0", "x");
    const identity = await math.solve("x=x", "x");
    const parameter = await math.solve("a*x=1", "x");

    await math.setVariable("A", "1/3");
    const variableRead = await math.evaluate("A+1");
    const variableDiagnostics = await math.diagnostics();
    const isolated = await measure("warmWorkerStartup",
      () => createNumosMath({ manifestUrl }));
    const isolatedA = await isolated.evaluate("A");
    await isolated.shutdown();
    await math.removeVariable("A");
    const removed = await math.evaluate("A");

    const graph = await measure("compile1D",
      () => math.compile1D("x^2", "x"));
    const scalarStart = performance.now();
    const scalar = await graph.evaluate(3);
    timings.scalar1D = performance.now() - scalarStart;
    const values = Array.from({ length: 1000 }, (_, index) =>
      -5 + 10 * index / 999);
    const batch = await measure("batch1000", () => graph.evaluateMany(values));
    const pole = await math.compile1D("1/x", "x");
    const gap = await pole.evaluate(0);
    await math.setAngleMode("degree");
    const trig = await math.compile1D("sin(x)", "x");
    const trigDegree = await trig.evaluate(30);
    await math.setAngleMode("radian");
    let staleCode = "";
    try { await graph.evaluate(2); } catch (error) { staleCode = error.code; }
    await pole.dispose().catch(() => {});
    await trig.dispose().catch(() => {});

    const disposed = await math.compile1D("x", "x");
    await disposed.dispose();
    let disposedCode = "";
    try { await disposed.dispose(); } catch (error) { disposedCode = error.code; }

    const surface = await math.compile2D("(x^2+y^2)-(1)", "x", "y");
    const circle = await surface.evaluate(0.5, 0.5);
    const grid = await measure("grid2D", () => surface.evaluateGrid({
      xMin: -1, xMax: 1, yMin: -1, yMax: 1, width: 32, height: 24,
    }));
    await surface.dispose();

    const bounds = {};
    for (const [name, action] of Object.entries({
      source: () => math.evaluate("1".repeat(2001)),
      depth: () => math.evaluate("(".repeat(41) + "1" + ")".repeat(41)),
      node: () => math.evaluate("1+".repeat(201) + "1"),
      identifier: () => math.setVariable("x", "1"),
      request: () => math.evaluate("system(1)"),
      malformed: () => math.evaluate("1", []),
      grid: async () => {
        const value = await math.compile2D("x+y", "x", "y");
        return value.evaluateGrid({
          xMin: 0, xMax: 1, yMin: 0, yMax: 1, width: 257, height: 1,
        });
      },
    })) {
      try { await action(); } catch (error) { bounds[name] = error.code; }
    }
    const oversizedMatrix = await math.evaluate("matrix(7,7,0)");
    const oversizedList = await math.evaluate(
      `[${Array.from({ length: 33 }, (_, index) => index).join(",")}]`);
    const expandedResult = await math.evaluate(
      `[${Array.from({ length: 7 }, () => "matrix(6,6,0)").join(",")}]`);

    const beforeCancel = await math.diagnostics();
    const cancelled = await math.cancel();
    const afterCancel = await math.evaluate("2+2");
    const shutdownStart = performance.now();
    await math.shutdown();
    timings.shutdown = performance.now() - shutdownStart;
    let shutdownCode = "";
    try { await math.evaluate("1"); } catch (error) { shutdownCode = error.code; }

    // Low-level direct harness remains available for integration testing.
    const direct = await createNumosMathDirect({ manifestUrl });
    const directResult = await direct.evaluate("2+2");
    const directSurface = await direct.compile2D("(x^2+y^2)-(1)", "x", "y");
    const directCircle = await directSurface.evaluate(0.5, 0.5);
    const directShutdown = await direct.shutdown();
    const cycles = [];
    for (let index = 0; index < 5; ++index) {
      const cycle = await createNumosMathDirect({ manifestUrl });
      const diagnostics = await cycle.diagnostics();
      const evaluation = await cycle.evaluate("2+2");
      const shutdown = await cycle.shutdown();
      cycles.push({
        heapBytes: diagnostics.diagnostics.heapBytes,
        usedHeapBytes: diagnostics.diagnostics.usedHeapBytes,
        value: evaluation.result.value,
        activeContexts: shutdown.activeContexts,
      });
    }

    return {
      browserName, timings, twoPlusTwo, large, rational, radical, simplified,
      complex, infinity, undefinedValue, list, matrix, piecewise, derivative,
      degreeDerivative, integral, unevaluated, linear, polynomial, system,
      noSolution, identity, parameter, variableRead, variableDiagnostics,
      isolatedA, removed, scalar, batchLength: batch.length, gap, trigDegree,
      staleCode,
      disposedCode, circle, grid, bounds, beforeCancel, cancelled, afterCancel,
      shutdownCode, directResult, directCircle, directShutdown, cycles,
      initialDiagnostics, oversizedMatrix, oversizedList, expandedResult,
    };
  }, { browserName });
  assert.deepEqual(errors, [], `${browserName} page errors`);
  assert.equal(result.twoPlusTwo.result.kind, "integer");
  assert.equal(result.twoPlusTwo.result.value, "4");
  assert.equal(result.large.result.value, "1267650600228229401496703205376");
  assert.equal(result.rational.result.kind, "rational");
  assert.equal(result.radical.result.kind, "sqrt");
  assert.equal(result.complex.result.kind, "complex");
  assert.equal(result.infinity.result.kind, "positive_infinity");
  assert.equal(result.undefinedValue.result.kind, "undefined");
  assert.equal(result.list.result.kind, "list");
  assert.equal(result.matrix.result.kind, "matrix");
  assert.equal(result.piecewise.result.kind, "piecewise");
  assert.equal(result.derivative.displayText, "2*x");
  assert.match(result.degreeDerivative.displayText, /pi\/180/);
  assert.match(result.integral.displayText, /x\^3/);
  assert.equal(result.unevaluated.unevaluated, true);
  assert.equal(result.linear.result.setKind, "solutions");
  assert.equal(result.system.result.groups[0].values.length, 2);
  assert.equal(result.noSolution.result.setKind, "no_solution");
  assert.equal(result.identity.result.setKind, "all_values");
  assert.equal(result.variableRead.displayText, "4/3");
  assert.equal(result.variableDiagnostics.diagnostics.variables[0].exact, "1/3");
  assert.equal(result.isolatedA.result.kind, "symbol");
  assert.equal(result.removed.result.kind, "symbol");
  assert.equal(result.scalar.kind, "finite");
  assert.equal(result.scalar.value, 9);
  assert.equal(result.batchLength, 1000);
  assert.equal(result.gap.kind, "non_finite");
  assert.ok(Math.abs(result.trigDegree.value - 0.5) < 1e-12);
  assert.equal(result.staleCode, "STALE_HANDLE");
  assert.equal(result.disposedCode, "DISPOSED_HANDLE");
  assert.ok(Math.abs(result.circle.value + 0.5) < 1e-12);
  assert.equal(result.grid.order, "row-major-y-then-x");
  assert.equal(result.grid.values.length, 768);
  assert.deepEqual(result.bounds, {
    source: "SOURCE_TOO_LONG",
    depth: "DEPTH_LIMIT",
    node: "NODE_LIMIT",
    identifier: "INVALID_IDENTIFIER",
    request: "INVALID_REQUEST",
    malformed: "INVALID_REQUEST",
    grid: "INVALID_REQUEST",
  });
  assert.equal(result.oversizedMatrix.result.kind, "opaque");
  assert.equal(result.oversizedMatrix.result.reason, "matrix_dimensions");
  assert.equal(result.oversizedList.result.kind, "opaque");
  assert.equal(result.oversizedList.result.reason, "list_limit");
  assert.equal(result.expandedResult.result.kind, "opaque");
  assert.equal(result.expandedResult.result.reason, "node_limit");
  assert.equal(result.cancelled.stateLost, true);
  assert.equal(result.afterCancel.result.value, "4");
  assert.equal(result.shutdownCode, "SHUTDOWN");
  assert.equal(result.directResult.result.value, "4");
  assert.equal(result.directShutdown.activeContexts, 0);
  assert.equal(result.cycles.length, 5);
  assert.ok(result.cycles.every((cycle) =>
    cycle.value === "4" && cycle.activeContexts === 0));
  assert.ok(result.cycles.every((cycle) =>
    cycle.heapBytes === result.cycles[0].heapBytes));
  assert.ok(result.cycles.every((cycle) =>
    cycle.usedHeapBytes === result.cycles[0].usedHeapBytes));
  assert.equal(result.initialDiagnostics.diagnostics.heapBytes,
    result.beforeCancel.diagnostics.heapBytes);
  await browser.close();
  return result;
}

try {
  const results = [];
  for (const [name, type] of Object.entries(engines))
    results.push(await run(name, type));
  console.log(JSON.stringify({
    variant,
    results: results.map((result) => ({
      browser: result.browserName,
      timingsMs: result.timings,
      initialHeapBytes: result.initialDiagnostics.diagnostics.heapBytes,
      repeatedHeapBytes: result.beforeCancel.diagnostics.heapBytes,
      initialUsedHeapBytes:
        result.initialDiagnostics.diagnostics.usedHeapBytes,
      repeatedUsedHeapBytes: result.beforeCancel.diagnostics.usedHeapBytes,
      fiveCycleHeapBytes: result.cycles.map((cycle) => cycle.heapBytes),
      fiveCycleUsedHeapBytes:
        result.cycles.map((cycle) => cycle.usedHeapBytes),
    })),
  }, null, 2));
} finally {
  await server.close();
}
