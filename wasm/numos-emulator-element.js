import {
  NUMOS_LOGICAL_KEY_MAX,
  NUMOS_WEB_KEYPAD_LAYOUT,
} from "./numos-keypad.js";
import { createPersistenceController } from "./numos-persistence.js";

const COMPONENT_CSS = "__NUMOS_INLINE_CSS__";
const LOGICAL_WIDTH = 320;
const LOGICAL_HEIGHT = 240;
const KEY_PRESS = 1;
const KEY_RELEASE = 2;
const ACTIVE_BY_DOCUMENT = new WeakMap();

const ACTIVE_STATES = new Set([
  "loading_manifest", "loading_runtime", "downloading_wasm", "instantiating",
  "hydrating", "booting", "ready", "flushing", "shutting_down",
]);

const TRANSITIONS = Object.freeze({
  idle: new Set(["waiting_for_viewport", "loading_manifest", "stopped", "error"]),
  waiting_for_viewport: new Set(["idle", "loading_manifest", "stopped", "error"]),
  loading_manifest: new Set(["loading_runtime", "shutting_down", "error"]),
  loading_runtime: new Set(["downloading_wasm", "shutting_down", "error"]),
  downloading_wasm: new Set(["instantiating", "shutting_down", "error"]),
  instantiating: new Set(["hydrating", "shutting_down", "error"]),
  hydrating: new Set(["booting", "shutting_down", "error"]),
  booting: new Set(["ready", "shutting_down", "error"]),
  ready: new Set(["flushing", "shutting_down", "error"]),
  flushing: new Set(["ready", "shutting_down", "error"]),
  shutting_down: new Set(["stopped", "error"]),
  stopped: new Set(["waiting_for_viewport", "loading_manifest", "error"]),
  error: new Set(["loading_manifest", "shutting_down", "stopped"]),
});

const STATE_LABELS = Object.freeze({
  idle: "Not started",
  waiting_for_viewport: "Waiting until NumOS approaches the viewport",
  loading_manifest: "Loading asset manifest",
  loading_runtime: "Loading runtime loader",
  downloading_wasm: "Downloading NumOS",
  instantiating: "Compiling and instantiating WebAssembly",
  hydrating: "Hydrating saved NumOS data",
  booting: "Starting NumOS",
  ready: "NumOS launcher ready",
  flushing: "Syncing saved NumOS data",
  shutting_down: "Shutting down NumOS",
  stopped: "NumOS is shut down",
  error: "NumOS could not start",
});

const ERROR_MESSAGES = Object.freeze({
  ASSET_MANIFEST_FAILURE: "The NumOS asset manifest could not be loaded.",
  LOADER_FAILURE: "The NumOS runtime loader could not be loaded.",
  WASM_FETCH_FAILURE: "The NumOS WebAssembly file could not be downloaded.",
  WASM_INSTANTIATION_FAILURE: "NumOS WebAssembly could not be instantiated.",
  UNSUPPORTED_BROWSER: "This browser is missing a feature required by NumOS.",
  PERSISTENCE_FALLBACK: "Saved storage is unavailable; this session is temporary.",
  NUMOS_BOOT_TIMEOUT: "NumOS did not reach the launcher in time.",
  SECOND_ACTIVE_INSTANCE: "Another NumOS emulator is already active in this document.",
  SHUTDOWN_FAILURE: "NumOS did not shut down cleanly.",
  RESET_CANCELLED: "Persistent storage reset was cancelled.",
});

function boundedText(value, limit = 800) {
  const text = value instanceof Error ? value.message : String(value ?? "");
  return text.length <= limit ? text : `${text.slice(0, limit)}…`;
}

function copy(value) {
  return value == null ? value : JSON.parse(JSON.stringify(value));
}

function deferred() {
  let resolve;
  let reject;
  const promise = new Promise((res, rej) => {
    resolve = res;
    reject = rej;
  });
  // The UI handles errors even when an integrator observes only events.
  promise.catch(() => {});
  return { promise, resolve, reject };
}

export class NumosEmulatorError extends Error {
  constructor(code, message = ERROR_MESSAGES[code] || "NumOS error", options = {}) {
    super(message, options.cause ? { cause: options.cause } : undefined);
    this.name = new.target.name;
    this.code = code;
    this.recoverable = options.recoverable !== false;
    this.details = boundedText(options.details || options.cause || "");
  }
}

export class NumosAssetManifestError extends NumosEmulatorError {
  constructor(options) { super("ASSET_MANIFEST_FAILURE", undefined, options); }
}
export class NumosLoaderError extends NumosEmulatorError {
  constructor(options) { super("LOADER_FAILURE", undefined, options); }
}
export class NumosWasmFetchError extends NumosEmulatorError {
  constructor(options) { super("WASM_FETCH_FAILURE", undefined, options); }
}
export class NumosWasmInstantiationError extends NumosEmulatorError {
  constructor(options) { super("WASM_INSTANTIATION_FAILURE", undefined, options); }
}
export class NumosUnsupportedBrowserError extends NumosEmulatorError {
  constructor(options) { super("UNSUPPORTED_BROWSER", undefined, options); }
}
export class NumosPersistenceFallbackError extends NumosEmulatorError {
  constructor(options) {
    super("PERSISTENCE_FALLBACK", undefined, {
      ...options,
      recoverable: false,
    });
  }
}
export class NumosBootTimeoutError extends NumosEmulatorError {
  constructor(options) { super("NUMOS_BOOT_TIMEOUT", undefined, options); }
}
export class NumosSecondActiveInstanceError extends NumosEmulatorError {
  constructor(options) { super("SECOND_ACTIVE_INSTANCE", undefined, options); }
}
export class NumosShutdownError extends NumosEmulatorError {
  constructor(options) { super("SHUTDOWN_FAILURE", undefined, options); }
}

function normalizeError(error, fallbackCode) {
  if (error instanceof NumosEmulatorError) return error;
  if (error?.name === "AbortError") return error;
  return new NumosEmulatorError(fallbackCode, undefined, {
    cause: error,
    details: boundedText(error?.stack || error),
  });
}

function formatBytes(value) {
  if (!Number.isFinite(value) || value < 0) return "";
  if (value < 1024) return `${value} B`;
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KiB`;
  return `${(value / 1024 / 1024).toFixed(2)} MiB`;
}

function featureCheck() {
  const missing = [];
  if (!globalThis.WebAssembly) missing.push("WebAssembly");
  if (!globalThis.fetch) missing.push("fetch");
  if (!globalThis.customElements) missing.push("Custom Elements");
  if (!globalThis.AbortController) missing.push("AbortController");
  if (!globalThis.ResizeObserver) missing.push("ResizeObserver");
  return missing;
}

export class NumosEmulatorElement extends HTMLElement {
  static observedAttributes = ["controls", "autostart"];

  #shadow;
  #state = "idle";
  #generation = 0;
  #readyDeferred = deferred();
  #readySettled = false;
  #startPromise = null;
  #shutdownPromise = null;
  #module = null;
  #persistence = null;
  #manifest = null;
  #manifestUrl = null;
  #canvas = null;
  #activeGuardOwned = false;
  #startupAbort = null;
  #runtimeAbort = null;
  #intersectionObserver = null;
  #resizeObserver = null;
  #fitFrame = 0;
  #lastFitWidth = 0;
  #bootFrame = 0;
  #heldPointers = new Map();
  #heldLogicalCounts = new Map();
  #heldPhysical = new Map();
  #inputEnabled = false;
  #controlsOverride = null;
  #haptics = false;
  #modifierMode = "none";
  #lastError = null;
  #lastPersistenceState = null;
  #timings = {};
  #connected = false;

  constructor() {
    super();
    this.#shadow = this.attachShadow({ mode: "open" });
    const style = document.createElement("style");
    style.textContent = COMPONENT_CSS;
    this.#shadow.append(style);
    const shell = document.createElement("section");
    shell.className = "shell";
    shell.setAttribute("part", "shell");
    shell.innerHTML = `
      <header class="topbar">
        <div class="brand"><strong>NumOS Emulator</strong><span>Real SDL2 · LVGL · Giac WebAssembly</span></div>
        <div class="actions">
          <button type="button" data-action="start">Start</button>
          <button type="button" data-action="retry" hidden>Retry</button>
          <button type="button" data-action="fullscreen">Fullscreen</button>
          <button type="button" data-action="controls" aria-pressed="false">Show controls</button>
          <button type="button" data-action="haptics" aria-pressed="false" hidden>Haptics off</button>
          <button type="button" data-action="restart" hidden>Restart</button>
          <button type="button" data-action="power" hidden>Power off</button>
        </div>
      </header>
      <div class="content">
        <div class="display-column">
          <div class="display-stage" part="display">
            <div class="canvas-mount"></div>
            <div class="overlay">
              <div class="overlay-card">
                <h2>NumOS is ready to load</h2>
                <p>Start the real calculator runtime when you need it.</p>
                <div class="progress" hidden><span></span></div>
                <button type="button" data-action="overlay-start">Start NumOS</button>
              </div>
            </div>
          </div>
          <div class="status-row">
            <output data-status>Not started</output>
            <span class="persistence"><span class="dot"></span><span data-persistence>Storage not initialized</span></span>
          </div>
        </div>
        <div class="controls" aria-label="NumOS touch controls" hidden></div>
      </div>
      <details>
        <summary>Technical details</summary>
        <div class="details-body">
          <dl>
            <dt>Lifecycle</dt><dd data-detail-state>idle</dd>
            <dt>Build</dt><dd data-detail-build>not loaded</dd>
            <dt>Display</dt><dd data-detail-scale>320×240 logical</dd>
            <dt>Storage</dt><dd data-detail-storage>not initialized</dd>
            <dt>Error</dt><dd data-detail-error>none</dd>
          </dl>
          <button type="button" data-action="clear-storage">Clear saved NumOS data</button>
          <button type="button" data-action="shutdown" disabled>Shut down</button>
        </div>
      </details>
      <div class="sr-only" aria-live="polite" aria-atomic="true" data-live></div>`;
    this.#shadow.append(shell);
    this.#renderKeypad();
    this.#bindShell();
    this.#render();
  }

  connectedCallback() {
    this.#connected = true;
    this.#renderControls();
    if (this.#autostartEnabled()) this.#observeViewport();
  }

  disconnectedCallback() {
    this.#connected = false;
    this.#intersectionObserver?.disconnect();
    this.#intersectionObserver = null;
    if (ACTIVE_STATES.has(this.#state)) this.#ignore(this.shutdown());
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (oldValue === newValue) return;
    if (name === "controls") this.#renderControls();
    if (name === "autostart" && this.#connected &&
        (this.#state === "idle" || this.#state === "waiting_for_viewport")) {
      if (this.#autostartEnabled()) this.#observeViewport();
      else {
        this.#intersectionObserver?.disconnect();
        this.#intersectionObserver = null;
        if (this.#state === "waiting_for_viewport") this.#transition("idle");
      }
    }
  }

  get state() { return this.#state; }
  get ready() { return this.#readyDeferred.promise; }
  get autostart() { return this.#autostartEnabled(); }
  set autostart(value) { this.toggleAttribute("autostart", Boolean(value)); }
  get controls() { return this.getAttribute("controls") || "auto"; }
  set controls(value) { this.setAttribute("controls", value); }

  async start() {
    if (this.#startPromise) return this.#startPromise;
    if (this.#state === "ready") return this.ready;
    if (this.#state === "shutting_down" || this.#state === "flushing") {
      await this.#shutdownPromise;
    }
    if (this.#state === "stopped" || this.#state === "error") this.#renewReady();

    const owner = ACTIVE_BY_DOCUMENT.get(this.ownerDocument);
    if (owner && owner !== this && ACTIVE_STATES.has(owner.state)) {
      const error = new NumosSecondActiveInstanceError({ recoverable: true });
      this.#failVisible(error);
      if (!this.#readySettled) {
        this.#readySettled = true;
        this.#readyDeferred.reject(error);
      }
      throw error;
    }

    const missing = featureCheck();
    if (missing.length) {
      const error = new NumosUnsupportedBrowserError({
        recoverable: false,
        details: `Missing: ${missing.join(", ")}`,
      });
      this.#failVisible(error);
      throw error;
    }

    ACTIVE_BY_DOCUMENT.set(this.ownerDocument, this);
    this.#activeGuardOwned = true;
    this.#intersectionObserver?.disconnect();
    this.#intersectionObserver = null;
    const token = ++this.#generation;
    this.#startupAbort = new AbortController();
    this.#lastError = null;
    this.#lastPersistenceState = null;
    this.#timings = { startAt: performance.now() };
    this.#createCanvas(token);
    this.#attachRuntimeListeners(token);

    let generationPromise;
    generationPromise = this.#startGeneration(token)
      .catch(async (rawError) => {
        if (rawError?.name === "AbortError" || token !== this.#generation) {
          throw rawError;
        }
        const error = normalizeError(rawError, "WASM_INSTANTIATION_FAILURE");
        await this.#cleanupFailedGeneration(token);
        this.#failVisible(error);
        this.#readySettled = true;
        this.#readyDeferred.reject(error);
        throw error;
      })
      .finally(() => {
        if (this.#startPromise === generationPromise) this.#startPromise = null;
        if (token === this.#generation) {
          this.#startupAbort = null;
        }
      });
    this.#startPromise = generationPromise;
    return generationPromise;
  }

  async #startGeneration(token) {
    this.#transition("loading_manifest");
    this.#progress("asset manifest", 0, null);
    const manifestStarted = performance.now();
    const manifest = await this.#loadManifest(token);
    this.#assertCurrent(token);
    this.#timings.manifestMs = performance.now() - manifestStarted;
    this.#manifest = manifest;
    this.#shadow.querySelector("[data-detail-build]").textContent =
      manifest.build?.identity || "unknown";

    this.#transition("loading_runtime");
    this.#progress("runtime loader", 0, null);
    const runtimeUrl = new URL(manifest.assets.runtime.url, this.#manifestUrl).href;
    const loaderStarted = performance.now();
    let factory;
    try {
      const runtime = await import(runtimeUrl);
      factory = runtime.default;
      if (typeof factory !== "function") throw new Error("default module factory is missing");
    } catch (error) {
      throw new NumosLoaderError({ cause: error, details: runtimeUrl });
    }
    this.#assertCurrent(token);
    this.#timings.runtimeLoaderMs = performance.now() - loaderStarted;

    this.#transition("downloading_wasm");
    const wasmEntry = manifest.assets.wasm;
    const wasmUrl = new URL(wasmEntry.url, this.#manifestUrl).href;
    const wasmStarted = performance.now();
    const wasmBinary = await this.#downloadWasm(
      wasmUrl, Number(wasmEntry.bytes) || null, token);
    this.#assertCurrent(token);
    this.#timings.wasmDownloadMs = performance.now() - wasmStarted;
    this.#timings.coldDownloadBytes = wasmBinary.byteLength;

    this.#transition("instantiating");
    this.#progress("compilation and instantiation", wasmBinary.byteLength,
      wasmBinary.byteLength);
    const moduleStarted = performance.now();
    let module;
    try {
      module = await factory({
        canvas: this.#canvas,
        noInitialRun: true,
        wasmBinary,
        locateFile: (path) => path.endsWith(".wasm") ? wasmUrl :
          new URL(path, runtimeUrl).href,
        print: (line) => console.debug(`[NumOS] ${line}`),
        printErr: (line) => console.warn(`[NumOS] ${line}`),
        onAbort: (reason) => this.#runtimeAborted(token, reason),
        numosPersistenceDirty: (operation) =>
          this.#persistence?.markDirty(operation),
      });
    } catch (error) {
      throw new NumosWasmInstantiationError({ cause: error });
    }
    this.#assertCurrent(token);
    this.#module = module;
    this.#timings.moduleFactoryMs = performance.now() - moduleStarted;

    this.#transition("hydrating");
    this.#progress("storage hydration", 0, null);
    const hydrationStarted = performance.now();
    this.#persistence = createPersistenceController(
      manifest.build?.identity || "unknown",
      {
        disabled: this.getAttribute("persistence") === "disabled",
        testMode: this.getAttribute("persistence-test") || "",
        onChange: (state) => this.#persistenceChanged(token, state),
      },
    );
    const persistenceState = await this.#persistence.initialize(module);
    this.#assertCurrent(token);
    this.#timings.hydrationMs = performance.now() - hydrationStarted;
    this.#persistenceChanged(token, persistenceState);

    this.#transition("booting");
    this.#progress("NumOS boot", 0, null);
    const bootStarted = performance.now();
    module.callMain([]);
    await this.#waitForLauncher(token);
    this.#assertCurrent(token);
    this.#timings.bootMs = performance.now() - bootStarted;
    this.#timings.startToLauncherMs = performance.now() - this.#timings.startAt;
    this.#inputEnabled = true;
    const diagnostics = this.diagnosticState();
    const result = Object.freeze({
      ...copy(diagnostics),
      timings: copy(this.#timings),
      persistence: this.persistenceState(),
      build: copy(manifest.build),
    });
    this.#transition("ready");
    this.#readySettled = true;
    this.#readyDeferred.resolve(result);
    this.#emit("numos-ready", result);
    this.focus();
    return result;
  }

  async shutdown() {
    if (this.#shutdownPromise) return this.#shutdownPromise;
    if (this.#state === "idle" || this.#state === "waiting_for_viewport" ||
        this.#state === "stopped") {
      this.#intersectionObserver?.disconnect();
      this.#intersectionObserver = null;
      if (this.#state !== "stopped") this.#transition("stopped");
      return;
    }

    const shutdownToken = ++this.#generation;
    this.#startupAbort?.abort();
    if (!this.#readySettled) {
      this.#readySettled = true;
      this.#readyDeferred.reject(
        new DOMException("NumOS startup was shut down", "AbortError"));
    }
    this.#shutdownPromise = (async () => {
      const started = performance.now();
      let shutdownError = null;
      this.#releaseAllInput();
      try {
        if (this.#state === "ready") this.#transition("flushing");
        if (this.#persistence) {
          try {
            await this.#persistence.flushPersistence();
          } catch (error) {
            shutdownError = error;
          }
        }
        if (this.#state !== "shutting_down") this.#transition("shutting_down");
        if (this.#module) {
          this.#module._numos_request_shutdown();
          await this.#waitForRuntimeShutdown(this.#module, 10000);
          try {
            await this.#persistence?.flushPersistence();
          } catch (error) {
            shutdownError ||= error;
          }
        }
        await this.#persistence?.dispose();
      } catch (error) {
        shutdownError ||= error;
      } finally {
        this.#runtimeAbort?.abort();
        this.#runtimeAbort = null;
        cancelAnimationFrame(this.#bootFrame);
        this.#bootFrame = 0;
        if (this.#fitFrame) {
          this.ownerDocument.defaultView.cancelAnimationFrame(this.#fitFrame);
          this.#fitFrame = 0;
        }
        this.#resizeObserver?.disconnect();
        this.#resizeObserver = null;
        this.#intersectionObserver?.disconnect();
        this.#intersectionObserver = null;
        this.#persistence = null;
        this.#module = null;
        this.#manifest = null;
        this.#startupAbort = null;
        this.#removeCanvas();
        if (this.#activeGuardOwned &&
            ACTIVE_BY_DOCUMENT.get(this.ownerDocument) === this) {
          ACTIVE_BY_DOCUMENT.delete(this.ownerDocument);
        }
        this.#activeGuardOwned = false;
      }
      this.#timings.shutdownMs = performance.now() - started;
      if (shutdownError) {
        const error = new NumosShutdownError({
          cause: shutdownError,
          details: boundedText(shutdownError),
        });
        this.#lastError = error;
        this.#transition("error");
        this.#emitError(error);
        throw error;
      }
      this.#transition("stopped");
      this.#emit("numos-shutdown", {
        generation: shutdownToken,
        shutdownMs: this.#timings.shutdownMs,
        giacContexts: 0,
      });
    })().finally(() => {
      this.#shutdownPromise = null;
    });
    return this.#shutdownPromise;
  }

  async restart() {
    await this.shutdown();
    return this.start();
  }

  focus(options) {
    (this.#canvas || this.#shadow.querySelector('[data-action="start"]'))
      ?.focus(options);
  }

  async enterFullscreen() {
    if (!this.requestFullscreen) {
      const error = new NumosUnsupportedBrowserError({
        details: "Fullscreen API is unavailable",
      });
      this.#emitError(error);
      throw error;
    }
    try {
      await this.requestFullscreen();
      this.#fitCanvas();
      this.focus();
    } catch (error) {
      const wrapped = new NumosUnsupportedBrowserError({
        cause: error,
        details: "Fullscreen request was denied or unavailable",
      });
      this.#emitError(wrapped);
      throw wrapped;
    }
  }

  async exitFullscreen() {
    if (this.ownerDocument.fullscreenElement) {
      await this.ownerDocument.exitFullscreen();
    }
    this.#fitCanvas();
    this.focus();
  }

  pressLogicalKey(keyCode) {
    const pressed = this.sendLogicalKey(keyCode, KEY_PRESS);
    const released = this.sendLogicalKey(keyCode, KEY_RELEASE);
    return Boolean(pressed && released);
  }

  sendLogicalKey(keyCode, actionCode = KEY_PRESS) {
    if (!this.#inputEnabled || !this.#module) return false;
    if (!Number.isInteger(keyCode) || keyCode < 1 ||
        keyCode > NUMOS_LOGICAL_KEY_MAX ||
        !Number.isInteger(actionCode) || actionCode < 1 || actionCode > 3) {
      throw new RangeError("Logical key/action code is outside the audited NumOS range");
    }
    return Boolean(this.#module._numos_send_logical_key(keyCode, actionCode));
  }

  persistenceState() {
    return copy(this.#persistence?.snapshot() || {
      state: "disabled",
      mode: "ephemeral",
      dirty: false,
      lastError: null,
      multiTabSafety: "not_provided",
    });
  }

  diagnosticState() {
    if (!this.#module?._numos_diagnostic_state) return null;
    try {
      return copy(JSON.parse(this.#module.UTF8ToString(
        this.#module._numos_diagnostic_state())));
    } catch {
      return null;
    }
  }

  async flushPersistence() {
    if (!this.#persistence) return this.persistenceState();
    const wasReady = this.#state === "ready";
    if (wasReady) this.#transition("flushing");
    try {
      return copy(await this.#persistence.flushPersistence());
    } finally {
      if (wasReady && this.#state === "flushing") this.#transition("ready");
    }
  }

  async resetPersistentStorage() {
    if (!globalThis.confirm(
      "Clear only NumOS saved settings, variables, and NeoLanguage files? " +
      "The emulator will restart with a fresh in-memory runtime.")) {
      throw new NumosEmulatorError("RESET_CANCELLED");
    }
    if (!this.#persistence) {
      throw new NumosEmulatorError("SHUTDOWN_FAILURE",
        "NumOS storage is not initialized.");
    }
    await this.#persistence.resetPersistentStorage();
    await this.shutdown();
    return this.start();
  }

  #autostartEnabled() {
    return this.hasAttribute("autostart") &&
      this.getAttribute("autostart") !== "false";
  }

  #observeViewport() {
    if (this.#intersectionObserver || ACTIVE_STATES.has(this.#state)) return;
    if (!globalThis.IntersectionObserver) {
      this.#transition("idle");
      return;
    }
    if (this.#state !== "waiting_for_viewport") {
      this.#transition("waiting_for_viewport");
    }
    const rootMargin = this.getAttribute("root-margin") || "320px";
    this.#intersectionObserver = new IntersectionObserver((entries) => {
      if (entries.some((entry) => entry.isIntersecting)) {
        this.#intersectionObserver?.disconnect();
        this.#intersectionObserver = null;
        this.#ignore(this.start());
      }
    }, { rootMargin });
    this.#intersectionObserver.observe(this);
  }

  async #loadManifest(token) {
    const configured = this.getAttribute("manifest");
    const url = new URL(configured || "./numos-assets.json", import.meta.url);
    this.#manifestUrl = url;
    let response;
    try {
      response = await fetch(url, {
        signal: this.#startupAbort.signal,
        cache: "no-cache",
        credentials: "same-origin",
      });
      if (!response.ok) throw new Error(`HTTP ${response.status} ${response.statusText}`);
      const manifest = await response.json();
      this.#assertCurrent(token);
      if (manifest?.schemaVersion !== 1 ||
          !manifest.assets?.runtime?.url ||
          !manifest.assets?.wasm?.url ||
          !Number.isFinite(manifest.assets.wasm.bytes)) {
        throw new Error("manifest schema or required asset records are invalid");
      }
      this.#manifestUrl = new URL(response.url);
      return manifest;
    } catch (error) {
      if (error?.name === "AbortError") throw error;
      throw new NumosAssetManifestError({
        cause: error,
        details: `${url.href}: ${boundedText(error)}`,
      });
    }
  }

  async #downloadWasm(url, expectedBytes, token) {
    let response;
    try {
      response = await fetch(url, {
        signal: this.#startupAbort.signal,
        credentials: "same-origin",
        cache: "force-cache",
      });
      if (!response.ok) throw new Error(`HTTP ${response.status} ${response.statusText}`);
    } catch (error) {
      if (error?.name === "AbortError") throw error;
      throw new NumosWasmFetchError({
        cause: error,
        details: `${url}: ${boundedText(error)}`,
      });
    }
    const headerBytes = Number(response.headers.get("content-length")) || null;
    const total = headerBytes || expectedBytes;
    if (!response.body?.getReader) {
      const buffer = new Uint8Array(await response.arrayBuffer());
      this.#progress("Wasm download", buffer.byteLength, total);
      return buffer;
    }

    const reader = response.body.getReader();
    let storage = total ? new Uint8Array(total) : null;
    const chunks = storage ? null : [];
    let downloaded = 0;
    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        this.#assertCurrent(token);
        if (storage) {
          if (downloaded + value.byteLength > storage.byteLength) {
            const grown = new Uint8Array(Math.max(
              downloaded + value.byteLength, storage.byteLength * 2));
            grown.set(storage);
            storage = grown;
          }
          storage.set(value, downloaded);
        } else {
          chunks.push(value);
        }
        downloaded += value.byteLength;
        this.#progress("Wasm download", downloaded, total);
      }
    } catch (error) {
      if (error?.name === "AbortError") throw error;
      throw new NumosWasmFetchError({ cause: error, details: url });
    } finally {
      reader.releaseLock();
    }
    if (total && downloaded !== total) {
      throw new NumosWasmFetchError({
        details: `Expected ${total} bytes but received ${downloaded}`,
      });
    }
    if (storage) return storage.byteLength === downloaded
      ? storage : storage.slice(0, downloaded);
    const binary = new Uint8Array(downloaded);
    let offset = 0;
    for (const chunk of chunks) {
      binary.set(chunk, offset);
      offset += chunk.byteLength;
    }
    return binary;
  }

  #waitForLauncher(token) {
    const timeout = Number(this.getAttribute("boot-timeout")) || 30000;
    const deadline = performance.now() + timeout;
    return new Promise((resolve, reject) => {
      const poll = () => {
        try {
          this.#assertCurrent(token);
          if (this.#module?._numos_is_ready()) {
            resolve();
            return;
          }
          if (performance.now() >= deadline) {
            reject(new NumosBootTimeoutError({
              details: `Timeout: ${timeout} ms`,
            }));
            return;
          }
          this.#bootFrame = requestAnimationFrame(poll);
        } catch (error) {
          reject(error);
        }
      };
      poll();
    });
  }

  #waitForRuntimeShutdown(module, timeout) {
    const deadline = performance.now() + timeout;
    return new Promise((resolve, reject) => {
      const poll = () => {
        let state = null;
        try {
          state = JSON.parse(module.UTF8ToString(module._numos_diagnostic_state()));
        } catch {}
        if (state?.shutdown) {
          if (state.giac?.activeContexts !== 0) {
            reject(new Error("Giac context remained active after native shutdown"));
          } else {
            resolve();
          }
          return;
        }
        if (performance.now() >= deadline) {
          reject(new Error("native shutdown timed out"));
          return;
        }
        requestAnimationFrame(poll);
      };
      poll();
    });
  }

  #createCanvas(token) {
    this.#removeCanvas();
    const canvas = document.createElement("canvas");
    canvas.width = LOGICAL_WIDTH;
    canvas.height = LOGICAL_HEIGHT;
    canvas.tabIndex = 0;
    canvas.setAttribute("part", "canvas");
    canvas.setAttribute("aria-label", "NumOS calculator display");
    canvas.dataset.generation = String(token);
    this.#shadow.querySelector(".canvas-mount").append(canvas);
    this.#canvas = canvas;
  }

  #removeCanvas() {
    this.#canvas?.remove();
    this.#canvas = null;
  }

  #attachRuntimeListeners(token) {
    this.#runtimeAbort?.abort();
    this.#runtimeAbort = new AbortController();
    const signal = this.#runtimeAbort.signal;
    this.#canvas.addEventListener("pointerdown", () => this.focus(), { signal });
    this.#canvas.addEventListener("keydown", (event) => {
      if (event.repeat) return;
      this.#heldPhysical.set(event.code, {
        code: event.code, key: event.key, location: event.location,
        ctrlKey: event.ctrlKey, shiftKey: event.shiftKey,
        altKey: event.altKey, metaKey: event.metaKey,
      });
    }, { capture: true, signal });
    this.#canvas.addEventListener("keyup", (event) => {
      this.#heldPhysical.delete(event.code);
    }, { capture: true, signal });
    this.#canvas.addEventListener("blur", () => this.#releasePhysicalKeys(),
      { signal });
    this.ownerDocument.addEventListener("visibilitychange", () => {
      if (this.ownerDocument.visibilityState === "hidden") {
        this.#releaseAllInput();
        this.#ignore(this.#persistence?.flushPersistence());
      }
    }, { signal });
    this.ownerDocument.defaultView.addEventListener("pagehide", () => {
      this.#releaseAllInput();
      this.#ignore(this.#persistence?.flushPersistence());
    }, { signal });
    this.ownerDocument.defaultView.addEventListener("resize", () => {
      this.#scheduleCanvasFit();
    }, { signal });
    this.ownerDocument.addEventListener("fullscreenchange", () => {
      this.#fitCanvas();
      this.#renderFullscreen();
      if (this.ownerDocument.fullscreenElement === this) this.focus();
    }, { signal });
    this.ownerDocument.addEventListener("fullscreenerror", (event) => {
      this.#emitError(new NumosUnsupportedBrowserError({
        details: boundedText(event.type),
      }));
    }, { signal });
    this.#resizeObserver = new ResizeObserver((entries) => {
      const width = entries.at(-1)?.contentRect.width || 0;
      if (Math.abs(width - this.#lastFitWidth) < .25) return;
      this.#lastFitWidth = width;
      this.#scheduleCanvasFit();
    });
    this.#resizeObserver.observe(this.#shadow.querySelector(".display-stage"));
    this.#fitCanvas();
    this.#assertCurrent(token);
  }

  #scheduleCanvasFit() {
    if (this.#fitFrame) return;
    this.#fitFrame = this.ownerDocument.defaultView.requestAnimationFrame(() => {
      this.#fitFrame = 0;
      this.#fitCanvas();
    });
  }

  #fitCanvas() {
    if (!this.#canvas) return;
    const stage = this.#shadow.querySelector(".display-stage");
    const width = Math.max(1, stage.clientWidth - 16);
    const viewportHeight = this.ownerDocument.fullscreenElement === this
      ? Math.max(1, this.clientHeight - 170)
      : Math.max(1, this.ownerDocument.defaultView.innerHeight * .68);
    const height = Math.max(1, viewportHeight);
    const fitting = Math.min(width / LOGICAL_WIDTH, height / LOGICAL_HEIGHT);
    const integer = Math.floor(fitting);
    const scale = integer >= 1 ? integer : Math.max(.1, fitting);
    const cssWidth = Math.max(1, Math.round(LOGICAL_WIDTH * scale));
    const cssHeight = Math.max(1, Math.round(LOGICAL_HEIGHT * scale));
    if (this.#canvas.style.width !== `${cssWidth}px`) {
      this.#canvas.style.width = `${cssWidth}px`;
    }
    if (this.#canvas.style.height !== `${cssHeight}px`) {
      this.#canvas.style.height = `${cssHeight}px`;
    }
    this.#shadow.querySelector("[data-detail-scale]").textContent =
      `${LOGICAL_WIDTH}×${LOGICAL_HEIGHT} logical · ${scale.toFixed(3)}× CSS · ` +
      `${globalThis.devicePixelRatio || 1} DPR`;
  }

  #releasePhysicalKeys() {
    if (!this.#canvas || !this.#heldPhysical.size) return;
    for (const held of this.#heldPhysical.values()) {
      this.#canvas.dispatchEvent(new KeyboardEvent("keyup", {
        ...held,
        bubbles: true,
        cancelable: true,
      }));
    }
    this.#heldPhysical.clear();
  }

  #releaseAllInput() {
    this.#releasePhysicalKeys();
    if (this.#module) {
      for (const keyCode of this.#heldLogicalCounts.keys()) {
        this.#module._numos_send_logical_key(keyCode, KEY_RELEASE);
      }
    }
    this.#heldPointers.clear();
    this.#heldLogicalCounts.clear();
    for (const button of this.#shadow.querySelectorAll(".key.is-pressed")) {
      button.classList.remove("is-pressed");
      button.setAttribute("aria-pressed", "false");
    }
    this.#inputEnabled = false;
  }

  #logicalDown(keyCode) {
    if (!this.#inputEnabled) return false;
    const count = this.#heldLogicalCounts.get(keyCode) || 0;
    this.#heldLogicalCounts.set(keyCode, count + 1);
    if (count === 0) this.sendLogicalKey(keyCode, KEY_PRESS);
    return true;
  }

  #logicalUp(keyCode) {
    const count = this.#heldLogicalCounts.get(keyCode) || 0;
    if (count <= 1) {
      this.#heldLogicalCounts.delete(keyCode);
      if (this.#module && this.#inputEnabled) {
        this.sendLogicalKey(keyCode, KEY_RELEASE);
      }
      return true;
    }
    this.#heldLogicalCounts.set(keyCode, count - 1);
    return false;
  }

  #renderKeypad() {
    const controls = this.#shadow.querySelector(".controls");
    for (const group of NUMOS_WEB_KEYPAD_LAYOUT) {
      const section = document.createElement("section");
      section.className = "key-group";
      section.dataset.group = group.name.replace(/\s+/g, "-");
      const title = document.createElement("span");
      title.textContent = group.name;
      const grid = document.createElement("div");
      grid.className = "key-grid";
      for (const key of group.keys) {
        const button = document.createElement("button");
        button.type = "button";
        button.className = `key key-${key.category}`;
        if (key.physicalId === "r9c4") button.classList.add("key-execute");
        if (key.alphaLabel) button.classList.add("key-has-alpha");
        button.dataset.keyCode = String(key.code);
        button.dataset.keyId = key.logicalId;
        button.dataset.physicalId = key.physicalId;
        button.dataset.category = key.category;

        const shiftLegend = document.createElement("span");
        shiftLegend.className = "key-legend key-legend-shift";
        shiftLegend.textContent = key.shiftLabel;
        shiftLegend.hidden = !key.shiftLabel;

        const alphaLegend = document.createElement("span");
        alphaLegend.className = "key-legend key-legend-alpha";
        alphaLegend.textContent = key.alphaLabel;
        alphaLegend.hidden = !key.alphaLabel;

        const primaryLegend = document.createElement("span");
        primaryLegend.className = "key-primary";
        primaryLegend.textContent = key.label;

        button.append(shiftLegend, alphaLegend, primaryLegend);
        button.setAttribute("aria-label", key.ariaLabel);
        button.setAttribute("aria-pressed", "false");
        grid.append(button);
      }
      section.append(title, grid);
      controls.append(section);
    }
  }

  #bindShell() {
    this.#shadow.addEventListener("click", (event) => {
      const action = event.target.closest?.("[data-action]")?.dataset.action;
      if (!action) return;
      if (action === "start" || action === "overlay-start" || action === "retry") {
        this.#ignore(this.start());
      } else if (action === "fullscreen") {
        this.#ignore(this.ownerDocument.fullscreenElement === this
          ? this.exitFullscreen() : this.enterFullscreen());
      } else if (action === "controls") {
        this.#controlsOverride = !this.#controlsVisible();
        this.#renderControls();
      } else if (action === "haptics") {
        this.#haptics = !this.#haptics;
        this.#renderHaptics();
      } else if (action === "restart") {
        this.#resetModifierVisual();
        this.#ignore(this.restart());
      } else if (action === "power") {
        this.#resetModifierVisual();
        this.#ignore(this.shutdown());
      } else if (action === "clear-storage") {
        this.#ignore(this.resetPersistentStorage());
      } else if (action === "shutdown") {
        this.#ignore(this.shutdown());
      }
    });

    const controls = this.#shadow.querySelector(".controls");
    controls.addEventListener("pointerdown", (event) => {
      const button = event.target.closest?.(".key");
      if (!button || !this.#inputEnabled) return;
      event.preventDefault();
      const keyCode = Number(button.dataset.keyCode);
      if (this.#heldPointers.has(event.pointerId)) return;
      try {
        button.setPointerCapture(event.pointerId);
      } catch {
        // Synthetic accessibility tests may not have an active UA pointer;
        // real Pointer Events still use capture when available.
      }
      this.#heldPointers.set(event.pointerId, { keyCode, button });
      this.#logicalDown(keyCode);
      this.#recordModifierPress(button);
      button.classList.add("is-pressed");
      button.setAttribute("aria-pressed", "true");
      if (this.#haptics) globalThis.navigator.vibrate?.(12);
    });
    const releasePointer = (event) => {
      const held = this.#heldPointers.get(event.pointerId);
      if (!held) return;
      this.#heldPointers.delete(event.pointerId);
      const finalRelease = this.#logicalUp(held.keyCode);
      if (finalRelease) {
        held.button.classList.remove("is-pressed");
        held.button.setAttribute("aria-pressed", "false");
        this.#renderModifierVisual();
      }
    };
    for (const type of ["pointerup", "pointercancel", "lostpointercapture"]) {
      controls.addEventListener(type, releasePointer);
    }
    controls.addEventListener("keydown", (event) => {
      const button = event.target.closest?.(".key");
      if (!button || (event.key !== " " && event.key !== "Enter") ||
          button.dataset.keyboardHeld === "true") return;
      event.preventDefault();
      button.dataset.keyboardHeld = "true";
      const keyCode = Number(button.dataset.keyCode);
      this.#logicalDown(keyCode);
      this.#recordModifierPress(button);
      button.classList.add("is-pressed");
      button.setAttribute("aria-pressed", "true");
    });
    controls.addEventListener("keyup", (event) => {
      const button = event.target.closest?.(".key");
      if (!button || button.dataset.keyboardHeld !== "true" ||
          (event.key !== " " && event.key !== "Enter")) return;
      event.preventDefault();
      delete button.dataset.keyboardHeld;
      const keyCode = Number(button.dataset.keyCode);
      this.#logicalUp(keyCode);
      button.classList.remove("is-pressed");
      button.setAttribute("aria-pressed", "false");
      this.#renderModifierVisual();
    });
    controls.addEventListener("click", (event) => {
      if (event.target.closest?.(".key")) event.preventDefault();
    });
  }

  #recordModifierPress(button) {
    const keyId = button.dataset.keyId;

    if (keyId === "SHIFT" || keyId === "ALPHA") {
      if (keyId === "SHIFT") {
        this.#modifierMode = {
          none: "shift",
          shift: "shift-lock",
          alpha: "shift",
          "shift-lock": "none",
          "alpha-lock": "shift",
        }[this.#modifierMode] || "shift";
      } else {
        this.#modifierMode = {
          none: "alpha",
          alpha: "alpha-lock",
          shift: "alpha",
          "alpha-lock": "none",
          "shift-lock": "alpha",
        }[this.#modifierMode] || "alpha";
      }
    } else if (this.#modifierMode === "shift" || this.#modifierMode === "alpha") {
      this.#modifierMode = "none";
    }
    this.#renderModifierVisual();
  }

  #renderModifierVisual() {
    for (const keyId of ["SHIFT", "ALPHA"]) {
      const button = this.#shadow.querySelector(`[data-key-id="${keyId}"]`);
      if (!button) continue;
      const active = this.#modifierMode.startsWith(keyId.toLowerCase());
      const locked = active && this.#modifierMode.endsWith("-lock");
      button.classList.toggle("is-modifier-active", active);
      button.classList.toggle("is-modifier-locked", locked);
      if (!button.classList.contains("is-pressed")) {
        button.setAttribute("aria-pressed", String(active));
      }
      const primary = button.querySelector(".key-primary");
      if (primary) {
        primary.textContent = locked
          ? `${keyId === "SHIFT" ? "SHIFT" : "ALPHA"} LOCK`
          : keyId;
      }
    }
  }

  #resetModifierVisual() {
    this.#modifierMode = "none";
    this.#renderModifierVisual();
  }

  #controlsVisible() {
    if (this.#controlsOverride != null) return this.#controlsOverride;
    const mode = this.controls;
    if (mode === "visible") return true;
    if (mode === "hidden") return false;
    return globalThis.matchMedia?.("(pointer: coarse)")?.matches ||
      globalThis.matchMedia?.("(max-width: 720px)")?.matches || false;
  }

  #renderControls() {
    const visible = this.#controlsVisible();
    const controls = this.#shadow.querySelector(".controls");
    if (!controls) return;
    controls.hidden = !visible;
    const content = this.#shadow.querySelector(".content");
    content.classList.toggle("controls-visible", visible);
    const toggle = this.#shadow.querySelector('[data-action="controls"]');
    toggle.setAttribute("aria-pressed", String(visible));
    toggle.textContent = visible ? "Hide controls" : "Show controls";
    this.#fitCanvas();
  }

  #renderHaptics() {
    const button = this.#shadow.querySelector('[data-action="haptics"]');
    const supported = typeof globalThis.navigator?.vibrate === "function";
    button.hidden = !supported;
    button.setAttribute("aria-pressed", String(this.#haptics));
    button.textContent = this.#haptics ? "Haptics on" : "Haptics off";
  }

  #renderFullscreen() {
    const button = this.#shadow.querySelector('[data-action="fullscreen"]');
    button.textContent = this.ownerDocument.fullscreenElement === this
      ? "Exit fullscreen" : "Fullscreen";
  }

  #transition(next) {
    if (next === this.#state) return;
    if (!TRANSITIONS[this.#state]?.has(next)) {
      throw new Error(`Invalid NumOS lifecycle transition ${this.#state} -> ${next}`);
    }
    const previous = this.#state;
    this.#state = next;
    this.#render();
    this.#emit("numos-statechange", { state: next, previous });
  }

  #render() {
    const label = STATE_LABELS[this.#state] || this.#state;
    this.#shadow.querySelector("[data-status]").textContent = label;
    this.#shadow.querySelector("[data-live]").textContent = label;
    this.#shadow.querySelector("[data-detail-state]").textContent = this.#state;
    const start = this.#shadow.querySelector('[data-action="start"]');
    const retry = this.#shadow.querySelector('[data-action="retry"]');
    const overlayStart = this.#shadow.querySelector('[data-action="overlay-start"]');
    const shutdown = this.#shadow.querySelector('[data-action="shutdown"]');
    const restart = this.#shadow.querySelector('[data-action="restart"]');
    const power = this.#shadow.querySelector('[data-action="power"]');
    const overlay = this.#shadow.querySelector(".overlay");
    const title = overlay.querySelector("h2");
    const description = overlay.querySelector("p");
    const loading = ACTIVE_STATES.has(this.#state) &&
      !["ready", "flushing", "shutting_down"].includes(this.#state);
    start.disabled = ACTIVE_STATES.has(this.#state);
    start.hidden = this.#state === "ready" || this.#state === "error";
    retry.hidden = this.#state !== "error" || !this.#lastError?.recoverable;
    overlayStart.hidden = !["idle", "waiting_for_viewport", "stopped"].includes(this.#state);
    shutdown.disabled = !ACTIVE_STATES.has(this.#state) || this.#state === "shutting_down";
    const runtimeReady = this.#state === "ready" || this.#state === "flushing";
    restart.hidden = !runtimeReady;
    power.hidden = !runtimeReady;
    restart.disabled = this.#state !== "ready";
    power.disabled = this.#state === "shutting_down";
    overlay.hidden = this.#state === "ready" || this.#state === "flushing";
    if (!loading) this.#shadow.querySelector(".progress").hidden = true;
    if (this.#state === "error") {
      title.textContent = "NumOS could not start";
      description.textContent = this.#lastError?.message || label;
      description.className = "error";
    } else {
      title.textContent = label;
      description.className = "";
      description.textContent = loading
        ? "The real NumOS runtime is starting."
        : this.#state === "stopped"
          ? "The runtime has released its resources and can be started again."
          : "Start the real calculator runtime when you need it.";
    }
    this.#renderHaptics();
    this.#renderFullscreen();
  }

  #progress(stage, downloadedBytes, totalBytes) {
    const progress = this.#shadow.querySelector(".progress");
    const bar = progress.querySelector("span");
    progress.hidden = false;
    const determinate = Number.isFinite(totalBytes) && totalBytes > 0;
    progress.classList.toggle("indeterminate", !determinate);
    const percent = determinate
      ? Math.min(100, downloadedBytes / totalBytes * 100) : null;
    progress.style.setProperty("--progress", `${percent || 0}%`);
    const suffix = determinate
      ? `${formatBytes(downloadedBytes)} of ${formatBytes(totalBytes)}`
      : downloadedBytes ? formatBytes(downloadedBytes) : "working";
    this.#shadow.querySelector(".overlay p").textContent = `${stage}: ${suffix}`;
    this.#emit("numos-progress", {
      state: this.#state,
      stage,
      downloadedBytes: Number(downloadedBytes) || 0,
      totalBytes: determinate ? totalBytes : null,
      determinate,
    });
  }

  #persistenceChanged(token, state) {
    if (token !== this.#generation && this.#state !== "shutting_down") return;
    const safe = copy(state);
    const dot = this.#shadow.querySelector(".dot");
    const label = this.#shadow.querySelector("[data-persistence]");
    dot.className = "dot";
    if (safe.state === "persistent_ready") {
      dot.classList.add("ok");
      label.textContent = safe.dirty ? "Persistent storage pending sync" :
        "Persistent storage active";
    } else if (safe.state === "persistent_syncing") {
      dot.classList.add("warn");
      label.textContent = "Syncing persistent storage";
    } else if (safe.state === "persistent_error") {
      dot.classList.add("error");
      label.textContent = "Persistent storage error";
    } else {
      dot.classList.add("warn");
      label.textContent = "Ephemeral storage — changes end with this session";
    }
    this.#shadow.querySelector("[data-detail-storage]").textContent =
      `${safe.state}; ${safe.storageBytes || 0} bytes; multi-tab conflict safety not provided`;
    this.#emit("numos-persistencechange", safe);
    if (safe.state === "ephemeral_fallback" &&
        this.#lastPersistenceState !== "ephemeral_fallback") {
      this.#emitError(new NumosPersistenceFallbackError({
        details: safe.reason || "Persistent browser storage is unavailable.",
      }));
    }
    this.#lastPersistenceState = safe.state;
  }

  #runtimeAborted(token, reason) {
    if (token !== this.#generation) return;
    const error = new NumosWasmInstantiationError({
      recoverable: true,
      details: boundedText(reason),
    });
    this.#lastError = error;
    this.#emitError(error);
  }

  async #cleanupFailedGeneration(token) {
    if (token !== this.#generation) return;
    try {
      if (this.#module) {
        this.#module._numos_request_shutdown();
        await this.#waitForRuntimeShutdown(this.#module, 5000);
      }
    } catch {}
    try { await this.#persistence?.dispose(); } catch {}
    this.#runtimeAbort?.abort();
    this.#runtimeAbort = null;
    if (this.#fitFrame) {
      this.ownerDocument.defaultView.cancelAnimationFrame(this.#fitFrame);
      this.#fitFrame = 0;
    }
    this.#resizeObserver?.disconnect();
    this.#resizeObserver = null;
    this.#persistence = null;
    this.#module = null;
    this.#removeCanvas();
    if (this.#activeGuardOwned &&
        ACTIVE_BY_DOCUMENT.get(this.ownerDocument) === this) {
      ACTIVE_BY_DOCUMENT.delete(this.ownerDocument);
    }
    this.#activeGuardOwned = false;
  }

  #failVisible(error) {
    this.#lastError = error;
    this.#shadow.querySelector("[data-detail-error]").textContent =
      `${error.code}: ${error.details || error.message}`;
    if (this.#state !== "error") this.#transition("error");
    else this.#render();
    this.#emitError(error);
  }

  #emitError(error) {
    this.#emit("numos-error", {
      code: error.code || "UNKNOWN",
      message: boundedText(error.message),
      details: boundedText(error.details),
      recoverable: error.recoverable !== false,
    });
  }

  #emit(type, detail) {
    this.dispatchEvent(new CustomEvent(type, {
      bubbles: true,
      composed: true,
      detail: copy(detail),
    }));
  }

  #ignore(promise) {
    promise?.catch?.(() => {});
  }

  #assertCurrent(token) {
    if (token !== this.#generation || this.#startupAbort?.signal.aborted) {
      throw new DOMException("NumOS startup was superseded", "AbortError");
    }
  }

  #renewReady() {
    this.#readyDeferred = deferred();
    this.#readySettled = false;
  }
}

if (!customElements.get("numos-emulator")) {
  customElements.define("numos-emulator", NumosEmulatorElement);
}
