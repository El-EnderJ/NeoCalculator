const ROOT = "/numos";
const METADATA_PATH = `${ROOT}/.numos-persistence.json`;
const DATABASE_NAME = ROOT;
const STORE_NAME = "FILE_DATA";
const SCHEMA_VERSION = 1;
const STORAGE_FORMAT_VERSION =
  "settings:ST01;variables:VR01;neo:source-v1";
const DEFAULT_DEBOUNCE_MS = 300;

function errorMessage(error) {
  if (error instanceof Error) return error.message;
  if (error && typeof error.message === "string") return error.message;
  return String(error);
}

export function createPersistenceController(buildIdentity, options = {}) {
  const injected = options.testMode || "";
  const configuration = {
    disabled: options.disabled === true,
    denyIndexedDB: injected === "indexeddb-denied",
    failHydration: injected === "hydrate-failure",
    failNextFlush: injected === "flush-failure",
  };
  const onChange = typeof options.onChange === "function"
    ? options.onChange
    : () => {};
  let module;
  let mounted = false;
  let resetSelected = false;
  let timer = null;
  let inFlight = null;
  let dirtyGeneration = 0;
  let syncedGeneration = 0;
  let metadata = null;
  let metadataWritable = true;
  let failNextFlush = configuration.failNextFlush;
  let disposed = false;

  const detail = {
    state: "disabled",
    mode: "ephemeral",
    databaseName: DATABASE_NAME,
    storeName: STORE_NAME,
    schemaVersion: SCHEMA_VERSION,
    storageFormatVersion: STORAGE_FORMAT_VERSION,
    buildIdentity,
    metadataStatus: "not_loaded",
    dirty: false,
    pendingTimer: false,
    flushInProgress: false,
    dirtyNotifications: 0,
    mutationNotifications: {
      close: 0,
      remove: 0,
      mkdir: 0,
      rename: 0,
      other: 0,
    },
    hydrationMs: 0,
    lastFlushMs: 0,
    lastFlushAt: null,
    flushCount: 0,
    coalescedFlushCalls: 0,
    storageBytes: 0,
    lastError: null,
    history: [],
  };

  function transition(state, error = null) {
    detail.state = state;
    detail.lastError = error ? errorMessage(error) : null;
    detail.history.push({
      state,
      atMs: Math.round(performance.now() * 10) / 10,
      error: detail.lastError,
    });
    if (detail.history.length > 16) detail.history.shift();
    notify();
  }

  function notify() {
    if (disposed) return;
    queueMicrotask(() => {
      if (!disposed) onChange(snapshot());
    });
  }

  function ensureRoot() {
    try {
      module.FS.mkdir(ROOT);
    } catch (error) {
      if (!error || error.errno !== 20) throw error; // EEXIST in Emscripten FS
    }
  }

  function rawSync(populate) {
    return new Promise((resolve, reject) => {
      if (populate && configuration.failHydration) {
        reject(new Error("injected IDBFS hydration failure"));
        return;
      }
      if (!populate && failNextFlush) {
        failNextFlush = false;
        reject(new Error("injected IDBFS flush failure"));
        return;
      }
      module.FS.syncfs(populate, (error) => {
        if (error) reject(error);
        else resolve();
      });
    });
  }

  function writeMetadata(nextMetadata) {
    module.FS.writeFile(
      METADATA_PATH,
      `${JSON.stringify(nextMetadata, null, 2)}\n`,
      { encoding: "utf8" },
    );
    metadata = nextMetadata;
  }

  function hydrateMetadata() {
    let text;
    try {
      text = module.FS.readFile(METADATA_PATH, { encoding: "utf8" });
    } catch {
      detail.metadataStatus = "first_run";
      writeMetadata({
        schemaVersion: SCHEMA_VERSION,
        numosStorageFormatVersion: STORAGE_FORMAT_VERSION,
        buildIdentity,
        lastSuccessfulSync: null,
      });
      markDirty(0);
      return;
    }

    try {
      const parsed = JSON.parse(text);
      if (!Number.isInteger(parsed.schemaVersion)) {
        throw new Error("metadata schemaVersion is missing");
      }
      metadata = parsed;
      if (parsed.schemaVersion > SCHEMA_VERSION) {
        detail.metadataStatus = "newer_schema_preserved";
        metadataWritable = false;
        return;
      }
      if (parsed.schemaVersion < SCHEMA_VERSION) {
        detail.metadataStatus = "older_schema_migrated";
      } else {
        detail.metadataStatus = "current";
      }
      const needsUpdate =
        parsed.schemaVersion !== SCHEMA_VERSION ||
        parsed.numosStorageFormatVersion !== STORAGE_FORMAT_VERSION ||
        parsed.buildIdentity !== buildIdentity;
      if (needsUpdate) {
        writeMetadata({
          ...parsed,
          schemaVersion: SCHEMA_VERSION,
          numosStorageFormatVersion: STORAGE_FORMAT_VERSION,
          buildIdentity,
        });
        markDirty(0);
      }
    } catch (error) {
      const preservedPath =
        `${METADATA_PATH}.corrupt-${Date.now()}`;
      try {
        module.FS.rename(METADATA_PATH, preservedPath);
      } catch {
        // Preserve all other files even if the corrupt record cannot rename.
      }
      detail.metadataStatus = "corrupt_preserved";
      detail.lastError = errorMessage(error);
      writeMetadata({
        schemaVersion: SCHEMA_VERSION,
        numosStorageFormatVersion: STORAGE_FORMAT_VERSION,
        buildIdentity,
        lastSuccessfulSync: null,
        recoveredFromCorruptMetadata: true,
      });
      markDirty(0);
    }
  }

  function calculateStorageBytes(path = ROOT) {
    let total = 0;
    for (const name of module.FS.readdir(path)) {
      if (name === "." || name === "..") continue;
      const child = `${path}/${name}`;
      try {
        const stat = module.FS.lstat(child);
        if (module.FS.isDir(stat.mode)) total += calculateStorageBytes(child);
        else total += Number(stat.size) || 0;
      } catch {
        // A corrupt/partially removed optional file is non-fatal.
      }
    }
    return total;
  }

  function refreshStorageBytes() {
    try {
      detail.storageBytes = calculateStorageBytes();
    } catch {
      detail.storageBytes = 0;
    }
  }

  function scheduleFlush() {
    if (timer || inFlight || resetSelected ||
        detail.state !== "persistent_ready") {
      return;
    }
    timer = globalThis.setTimeout(() => {
      timer = null;
      detail.pendingTimer = false;
      flushPersistence().catch(() => {
        // State and bounded diagnostics are reported through onChange.
      });
    }, DEFAULT_DEBOUNCE_MS);
    detail.pendingTimer = true;
  }

  function markDirty(operation = 0) {
    detail.dirtyNotifications += 1;
    const names = {
      1: "close",
      2: "remove",
      3: "mkdir",
      4: "rename",
    };
    const name = names[operation] || "other";
    detail.mutationNotifications[name] += 1;
    dirtyGeneration += 1;
    detail.dirty = true;
    scheduleFlush();
    notify();
  }

  async function fallback(error, state = "ephemeral_fallback") {
    if (mounted) {
      try {
        module.FS.unmount(ROOT);
      } catch {}
      mounted = false;
    }
    ensureRoot();
    detail.mode = "ephemeral";
    transition(state, error);
    refreshStorageBytes();
  }

  async function initialize(nextModule) {
    if (disposed) throw new Error("persistence controller is disposed");
    module = nextModule;
    const started = performance.now();
    ensureRoot();

    if (configuration.disabled) {
      detail.mode = "ephemeral";
      transition("disabled");
      detail.hydrationMs = performance.now() - started;
      notify();
      return snapshot();
    }
    if (configuration.denyIndexedDB || !globalThis.indexedDB ||
        !module.IDBFS) {
      await fallback(new Error("IndexedDB/IDBFS unavailable"));
      detail.hydrationMs = performance.now() - started;
      notify();
      return snapshot();
    }

    try {
      transition("persistent_syncing");
      module.FS.mount(module.IDBFS, {}, ROOT);
      mounted = true;
      await rawSync(true);
      detail.mode = "persistent";
      transition("persistent_ready");
      hydrateMetadata();
      refreshStorageBytes();
    } catch (error) {
      transition("persistent_error", error);
      await fallback(error);
    }
    detail.hydrationMs = performance.now() - started;
    notify();
    return snapshot();
  }

  function flushPersistence() {
    if (timer) {
      clearTimeout(timer);
      timer = null;
      detail.pendingTimer = false;
    }
    if (inFlight) {
      detail.coalescedFlushCalls += 1;
      return inFlight;
    }
    if (resetSelected || !mounted ||
        detail.state === "disabled" ||
        detail.state === "ephemeral_fallback") {
      refreshStorageBytes();
      notify();
      return Promise.resolve(snapshot());
    }
    if (!detail.dirty) {
      refreshStorageBytes();
      notify();
      return Promise.resolve(snapshot());
    }

    const targetGeneration = dirtyGeneration;
    const started = performance.now();
    detail.flushInProgress = true;
    transition("persistent_syncing");
    if (metadataWritable) {
      writeMetadata({
        ...(metadata || {}),
        schemaVersion: SCHEMA_VERSION,
        numosStorageFormatVersion: STORAGE_FORMAT_VERSION,
        buildIdentity,
        lastSuccessfulSync: new Date().toISOString(),
      });
    }

    inFlight = rawSync(false)
      .then(() => {
        syncedGeneration = Math.max(syncedGeneration, targetGeneration);
        detail.dirty = dirtyGeneration > syncedGeneration;
        detail.flushCount += 1;
        detail.lastFlushMs = performance.now() - started;
        detail.lastFlushAt =
          metadata?.lastSuccessfulSync || new Date().toISOString();
        transition("persistent_ready");
        refreshStorageBytes();
        notify();
        return snapshot();
      })
      .catch((error) => {
        detail.dirty = true;
        detail.lastFlushMs = performance.now() - started;
        transition("persistent_error", error);
        throw error;
      })
      .finally(() => {
        inFlight = null;
        detail.flushInProgress = false;
        if (detail.dirty && detail.state === "persistent_ready") {
          scheduleFlush();
        }
      });
    return inFlight;
  }

  function removeTreeContents(path) {
    for (const name of module.FS.readdir(path)) {
      if (name === "." || name === "..") continue;
      const child = `${path}/${name}`;
      const stat = module.FS.lstat(child);
      if (module.FS.isDir(stat.mode)) {
        removeTreeContents(child);
        module.FS.rmdir(child);
      } else {
        module.FS.unlink(child);
      }
    }
  }

  function deleteOwnDatabase() {
    return new Promise((resolve, reject) => {
      if (!globalThis.indexedDB) {
        resolve();
        return;
      }
      const cached = module.IDBFS?.dbs?.[DATABASE_NAME];
      if (cached) {
        cached.close();
        delete module.IDBFS.dbs[DATABASE_NAME];
      }
      let request;
      try {
        request = indexedDB.deleteDatabase(DATABASE_NAME);
      } catch (error) {
        reject(error);
        return;
      }
      request.onsuccess = () => resolve();
      request.onerror = () => reject(request.error ||
        new Error("could not delete NumOS IndexedDB database"));
      request.onblocked = () => reject(
        new Error("NumOS IndexedDB reset is blocked by another active tab"));
    });
  }

  async function resetPersistentStorage() {
    if (timer) {
      clearTimeout(timer);
      timer = null;
      detail.pendingTimer = false;
    }
    if (inFlight) {
      try {
        await inFlight;
      } catch {
        // Reset remains an explicit recovery path after a failed flush.
      }
    }
    resetSelected = true;
    removeTreeContents(ROOT);
    metadata = null;
    detail.dirty = false;
    dirtyGeneration = 0;
    syncedGeneration = 0;

    if (mounted) {
      transition("persistent_syncing");
      try {
        await rawSync(false);
      } catch {
        // Deleting the product-specific database below is authoritative.
      }
    }
    await deleteOwnDatabase();
    detail.mode = "ephemeral";
    detail.metadataStatus = "reset";
    transition("disabled");
    refreshStorageBytes();
    notify();
    return snapshot();
  }

  function snapshot() {
    return Object.freeze({
      state: detail.state,
      mode: detail.mode,
      schemaVersion: detail.schemaVersion,
      storageFormatVersion: detail.storageFormatVersion,
      buildIdentity: detail.buildIdentity,
      metadataStatus: detail.metadataStatus,
      dirty: dirtyGeneration > syncedGeneration,
      pendingTimer: Boolean(timer),
      flushInProgress: Boolean(inFlight),
      dirtyNotifications: detail.dirtyNotifications,
      mutationNotifications: { ...detail.mutationNotifications },
      hydrationMs: detail.hydrationMs,
      lastFlushMs: detail.lastFlushMs,
      lastFlushAt: detail.lastFlushAt,
      flushCount: detail.flushCount,
      coalescedFlushCalls: detail.coalescedFlushCalls,
      storageBytes: detail.storageBytes,
      lastError: detail.lastError,
      history: detail.history.map((entry) => ({ ...entry })),
      debounceMs: DEFAULT_DEBOUNCE_MS,
      multiTabSafety: "not_provided",
    });
  }

  async function dispose() {
    if (disposed) return;
    if (timer) {
      clearTimeout(timer);
      timer = null;
      detail.pendingTimer = false;
    }
    if (inFlight) {
      try {
        await inFlight;
      } catch {
        // The caller already receives/report the flush failure.
      }
    }
    const cached = module?.IDBFS?.dbs?.[DATABASE_NAME];
    if (cached) {
      cached.close();
      delete module.IDBFS.dbs[DATABASE_NAME];
    }
    disposed = true;
    module = null;
  }

  return Object.freeze({
    initialize,
    markDirty,
    flushPersistence,
    resetPersistentStorage,
    snapshot,
    dispose,
  });
}
