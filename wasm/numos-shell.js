import createNumosModule from "./numos-emulator.js";

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

const module = await createNumosModule({
  canvas,
  locateFile: (path) => new URL(path, import.meta.url).href,
  print: (line) => console.log(`[NumOS] ${line}`),
  printErr: (line) => console.error(`[NumOS] ${line}`),
});
const moduleReadyMs = performance.now() - loadStarted;

const api = Object.freeze({
  moduleReadyMs,
  isReady: () => Boolean(module._numos_is_ready()),
  diagnosticState: () => JSON.parse(
    module.UTF8ToString(module._numos_diagnostic_state())
  ),
  requestShutdown: () => module._numos_request_shutdown(),
  sendLogicalKey: (keyCode, actionCode = 1) => Boolean(
    module._numos_send_logical_key(keyCode, actionCode)
  ),
  pressLogicalKey(keyCode) {
    const pressed = module._numos_send_logical_key(keyCode, 1);
    const released = module._numos_send_logical_key(keyCode, 2);
    return Boolean(pressed && released);
  },
});

window.numos = api;
window.numosReady = new Promise((resolve, reject) => {
  const deadline = performance.now() + 30000;
  const poll = () => {
    if (api.isReady()) {
      const diagnostics = api.diagnosticState();
      status.textContent = "NumOS ready — click the calculator, then use keyboard or pointer.";
      canvas.focus();
      resolve({
        ...diagnostics,
        moduleReadyMs,
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

window.numosReady.catch((error) => {
  status.textContent = error.message;
  console.error(error);
});
