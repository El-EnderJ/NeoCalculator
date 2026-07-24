// Standalone-demo compatibility adapter only. The reusable component module
// does not install window.numos or window.numosReady.
import "./numos-emulator-element.js";

const emulator = document.querySelector("numos-emulator");
if (!emulator) throw new Error("The standalone NumOS demo needs <numos-emulator>");
const parameters = new URLSearchParams(location.search);
if (parameters.get("persistence") === "disabled") {
  emulator.setAttribute("persistence", "disabled");
}
if (parameters.get("persistenceTest")) {
  emulator.setAttribute("persistence-test", parameters.get("persistenceTest"));
}

const api = Object.freeze({
  isReady: () => emulator.state === "ready",
  diagnosticState: () => emulator.diagnosticState(),
  requestShutdown: () => emulator.shutdown(),
  sendLogicalKey: (keyCode, actionCode = 1) =>
    emulator.sendLogicalKey(keyCode, actionCode),
  pressLogicalKey: (keyCode) => emulator.pressLogicalKey(keyCode),
  persistenceState: () => emulator.persistenceState(),
  flushPersistence: () => emulator.flushPersistence(),
  resetPersistentStorage: () => emulator.resetPersistentStorage(),
  start: () => emulator.start(),
  restart: () => emulator.restart(),
});

window.numos = api;
window.numosReady = emulator.ready;
