import createNumosModule from "./numos-emulator.js";
import { NUMOS_BUILD_IDENTITY } from "./numos-build.js";
import { createPersistenceController } from "./numos-persistence.js";

const canvas = document.querySelector("#canvas");
const status = document.querySelector("#numos-status");
const loadStarted = performance.now();

function fitCanvas() {
  const availableWidth = Math.max(1, document.documentElement.clientWidth - 24);
  const availableHeight = Math.max(1, document.documentElement.clientHeight - 72);
  const wholeScale = Math.floor(Math.min(availableWidth / 320,
                                         availableHeight / 240));
  const scale = wholeScale >= 1
    ? wholeScale
    : Math.min(availableWidth / 320, availableHeight / 240);
  canvas.style.width = `${Math.round(320 * scale)}px`;
  canvas.style.height = `${Math.round(240 * scale)}px`;
}

fitCanvas();
window.addEventListener("resize", fitCanvas);
canvas.addEventListener("pointerdown", () => canvas.focus());

const persistence = createPersistenceController(NUMOS_BUILD_IDENTITY);
const module = await createNumosModule({
  canvas,
  noInitialRun: true,
  locateFile: (path) => new URL(path, import.meta.url).href,
  print: (line) => console.log(`[NumOS] ${line}`),
  printErr: (line) => console.error(`[NumOS] ${line}`),
  // Called only by the C++ LittleFS shim after successful mutations.
  // It remains private inside the modularized loader closure.
  numosPersistenceDirty: (operation) => persistence.markDirty(operation),
});
const moduleReadyMs = performance.now() - loadStarted;

status.textContent = "Hydrating NumOS storage…";
await persistence.initialize(module);
const mainStarted = performance.now();
module.callMain([]);

let shutdownPromise = null;
function waitForShutdown() {
  return new Promise((resolve) => {
    const deadline = performance.now() + 10000;
    const poll = () => {
      let stopped = false;
      try {
        stopped = Boolean(module._numos_diagnostic_state) &&
          JSON.parse(module.UTF8ToString(
            module._numos_diagnostic_state())).shutdown;
      } catch {
        stopped = !module._numos_is_ready();
      }
      if (stopped || performance.now() >= deadline) {
        resolve();
      } else {
        requestAnimationFrame(poll);
      }
    };
    poll();
  });
}

async function requestShutdown() {
  if (shutdownPromise) return shutdownPromise;
  shutdownPromise = (async () => {
    try {
      await persistence.flushPersistence();
    } catch (error) {
      console.warn(`[NumOS] final pre-shutdown persistence flush failed: ${
        error instanceof Error ? error.message : String(error)}`);
    }
    module._numos_request_shutdown();
    await waitForShutdown();
    // App teardown may close a final production file. Flush once more without
    // blocking any browser frame; the main loop has already yielded/cancelled.
    try {
      await persistence.flushPersistence();
    } catch (error) {
      console.warn(`[NumOS] final post-shutdown persistence flush failed: ${
        error instanceof Error ? error.message : String(error)}`);
    }
  })();
  return shutdownPromise;
}

const api = Object.freeze({
  moduleReadyMs,
  isReady: () => Boolean(module._numos_is_ready()),
  diagnosticState: () => JSON.parse(
    module.UTF8ToString(module._numos_diagnostic_state())
  ),
  requestShutdown,
  sendLogicalKey: (keyCode, actionCode = 1) => Boolean(
    module._numos_send_logical_key(keyCode, actionCode)
  ),
  pressLogicalKey(keyCode) {
    const pressed = module._numos_send_logical_key(keyCode, 1);
    const released = module._numos_send_logical_key(keyCode, 2);
    return Boolean(pressed && released);
  },
  persistenceState: () => persistence.snapshot(),
  flushPersistence: () => persistence.flushPersistence(),
  resetPersistentStorage: () => persistence.resetPersistentStorage(),
});

window.numos = api;
window.numosReady = new Promise((resolve, reject) => {
  const deadline = performance.now() + 30000;
  const poll = () => {
    if (api.isReady()) {
      const diagnostics = api.diagnosticState();
      const persistenceState = api.persistenceState();
      const suffix = persistenceState.mode === "persistent"
        ? "persistent storage ready"
        : "ephemeral storage";
      status.textContent =
        `NumOS ready (${suffix}) — click the calculator, then use keyboard or pointer.`;
      canvas.focus();
      resolve({
        ...diagnostics,
        moduleReadyMs,
        hydrationMs: persistenceState.hydrationMs,
        persistence: persistenceState,
        mainToLauncherMs: performance.now() - mainStarted,
        browserLauncherMs: performance.now() - loadStarted,
      });
      return;
    }
    if (performance.now() >= deadline) {
      reject(new Error("NumOS launcher did not become ready within 30 seconds"));
      return;
    }
    requestAnimationFrame(poll);
  };
  poll();
});

function bestEffortFlush() {
  void persistence.flushPersistence().catch((error) => {
    console.warn(`[NumOS] best-effort persistence flush failed: ${
      error instanceof Error ? error.message : String(error)}`);
  });
}

window.addEventListener("pagehide", bestEffortFlush);
document.addEventListener("visibilitychange", () => {
  if (document.visibilityState === "hidden") bestEffortFlush();
});

window.numosReady.catch((error) => {
  status.textContent = error.message;
  console.error(error);
});
