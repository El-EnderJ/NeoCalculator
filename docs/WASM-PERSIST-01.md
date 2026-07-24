# WASM-PERSIST-01 — browser persistence

## Runtime boundary

The browser build keeps the NumOS filesystem root at `/numos`. In the normal
mode, `numos-shell.js` mounts Emscripten IDBFS there and hydrates it from
IndexedDB before C++ `main()` is called:

1. instantiate the modularized Emscripten module with `noInitialRun`;
2. create `/numos` in MEMFS;
3. mount IDBFS at `/numos`;
4. await `FS.syncfs(true, callback)`;
5. validate the persistence metadata without deleting application files;
6. invoke the real C++ `main()`;
7. resolve `window.numosReady` only after the real launcher is ready.

Neither Asyncify nor pthreads are used. NumOS and the frame loop never wait on
IndexedDB. The Emscripten module, `FS`, `IDBFS`, IndexedDB handles and Wasm
memory remain inside the ES-module closure and are not copied onto
`window.numos`.

Emscripten 6.0.3 derives the IDBFS database name from its mount point. This
build therefore owns:

- IndexedDB database: `/numos`
- object store: `FILE_DATA`
- mounted virtual path: `/numos`

No host filesystem path is reachable. NeoLanguage's browser file built-ins use
the same LittleFS-compatible `/numos` root; desktop Neo I/O and firmware
LittleFS behavior are unchanged.

## Public API

The existing copied API remains compatible. Three persistence operations are
added:

- `window.numos.persistenceState()` returns a copied diagnostic snapshot;
- `window.numos.flushPersistence()` returns a Promise;
- `window.numos.resetPersistentStorage()` returns a Promise and requires an
  explicit caller action.

`requestShutdown()` now returns a Promise. Existing fire-and-forget callers
remain valid, while callers that await it receive a pre-shutdown flush, C++
teardown, and a final flush for files closed during teardown.

The state snapshot reports `persistent_ready`, `persistent_syncing`,
`persistent_error`, `ephemeral_fallback`, or `disabled`, along with bounded
timings, storage size, dirty/flush status and mutation counters. It does not
contain file contents, filesystem objects, database handles or memory views.

For deterministic fault coverage only, the page query accepts
`persistenceTest=indexeddb-denied`, `persistenceTest=hydrate-failure`, or
`persistenceTest=flush-failure`. `persistence=disabled` selects explicit MEMFS.
These switches do not add API methods.

## Dirty writes and flushing

The native LittleFS shim notifies the private JavaScript controller after a
successful mutating close, remove, directory creation, or rename. Settings,
VariableManager and NeoLanguage continue to write through their production
C++ paths. The browser controller:

- uses one 300 ms debounce timer at most;
- coalesces repeated writes and repeated in-flight flush calls;
- performs only asynchronous `syncfs(false)` operations;
- retries only when explicitly flushed again after an error;
- performs best-effort flushes on `pagehide` and when the document becomes
  hidden.

Neo editor saves write `/neolang.nl.tmp` and rename it to `/neolang.nl` in the
web build. This provides a complete-file replacement boundary and exercises
the production rename path.

An operating system or browser process kill cannot guarantee completion of an
IndexedDB transaction already in progress. Applications needing a durable
checkpoint should await `flushPersistence()` before navigation.

## Fallback

If IndexedDB/IDBFS is missing, denied, or initial hydration fails, the IDBFS
mount is removed and `/numos` remains ordinary MEMFS. NumOS still launches and
all in-session behavior works; `persistenceState()` reports
`ephemeral_fallback`. A failed later flush reports `persistent_error` without
stopping the emulator, and a later explicit flush may recover.

Changes in `ephemeral_fallback` or `disabled` mode disappear on reload.

## Metadata and compatibility

`/numos/.numos-persistence.json` contains:

- persistence schema version (`1`);
- NumOS storage formats (`settings:ST01;variables:VR01;neo:source-v1`);
- build identity (Git revision, configuration, and dirty marker);
- last successful sync marker.

First run creates the record without deleting other data. Supported older
metadata is upgraded in place. An unknown newer schema is preserved and
reported as `newer_schema_preserved`; the controller does not overwrite it.
Corrupt metadata is renamed with a `.corrupt-<timestamp>` suffix when possible,
then replaced, while all application files are preserved. A changed build
identity updates metadata but never wipes user data. Existing readers already
treat missing, corrupt, or partially written optional settings, variable and
Neo files as non-fatal.

`resetPersistentStorage()` recursively clears only the `/numos` mount, syncs
the empty tree, and deletes only the `/numos` IndexedDB database. Automatic
migrations never reset application data.

## Multi-tab policy

Version 1 is single-active-tab storage. There is no cross-tab lock or merge
protocol. Concurrent tabs can hydrate independent MEMFS snapshots and IDBFS
uses timestamp reconciliation, so the last successful writer can replace an
earlier tab's changes. Reset can be blocked by another tab holding the
database; that condition is reported rather than forcing destructive recovery.

## Build and tests

The pinned toolchain remains Emscripten 6.0.3:

```sh
./wasm/build.sh Release
./wasm/build.sh Debug
./wasm/test.sh Release
./wasm/test.sh Debug
./wasm/test-persistence.sh Release
./wasm/test-persistence.sh Debug
```

The persistence Playwright smoke performs real page reloads and covers a
setting, `VariableManager` STO, Neo editor save/rename, modifications, explicit
flush coalescing, clean and dirty shutdown, reset, IndexedDB denial, hydration
failure, flush failure/retry, and MEMFS fallback. It also records hydration,
reload, flush, storage, heap and frame measurements.

Generated JS, Wasm, configured build identity and build trees remain below the
ignored `out/wasm/` directory.
