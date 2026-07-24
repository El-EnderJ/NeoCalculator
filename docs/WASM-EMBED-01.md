# WASM-EMBED-01 — reusable NumOS web component

## Integration

The production distribution contains a short-cache manifest and immutable,
content-addressed assets. Register the component from a deployment-relative
manifest; no repository path or site-root path is required:

```html
<numos-emulator controls="auto"></numos-emulator>
<script type="module">
  const manifestUrl = new URL("./numos-assets.json", import.meta.url);
  const manifest = await fetch(manifestUrl).then((response) => {
    if (!response.ok) throw new Error(`NumOS manifest: HTTP ${response.status}`);
    return response.json();
  });
  await import(new URL(manifest.assets.component.url, manifestUrl));
</script>
```

Importing the definition fetches only the small component and its small static
dependencies. It does not fetch the generated Emscripten loader or Wasm. The
visible Start action calls `start()`.

Viewport loading is opt-in:

```html
<numos-emulator autostart root-margin="320px" controls="auto">
</numos-emulator>
```

`IntersectionObserver` is disconnected when startup begins. Removing an
unstarted component disconnects it without loading NumOS; removing an active
component invalidates the generation, aborts fetches where possible, and runs
shutdown.

## Attributes and properties

| Name | Values | Behavior |
|---|---|---|
| `autostart` | boolean | Start when the element intersects its configured margin. |
| `root-margin` | CSS margin, default `320px` | `IntersectionObserver` startup margin. |
| `controls` | `auto`, `visible`, `hidden` | Initial touch-control policy; the manual toggle remains available. |
| `manifest` | relative or absolute URL | Override the default sibling `numos-assets.json`. |
| `persistence` | `disabled` or omitted | Select explicit ephemeral MEMFS or normal IDBFS. |
| `boot-timeout` | milliseconds | Override the 30-second launcher timeout. |

`autostart` and `controls` also have corresponding properties.

## Instance API

`<numos-emulator>` exposes only bounded instance methods:

- `start(): Promise<ReadySnapshot>`
- `ready: Promise<ReadySnapshot>`
- `shutdown(): Promise<void>`
- `restart(): Promise<ReadySnapshot>`
- `focus()`
- `enterFullscreen(): Promise<void>`
- `exitFullscreen(): Promise<void>`
- `pressLogicalKey(keyCode): boolean`
- `sendLogicalKey(keyCode, actionCode): boolean`
- `persistenceState(): copied object`
- `flushPersistence(): Promise<copied object>`
- `resetPersistentStorage(): Promise<ReadySnapshot>`
- `diagnosticState(): copied object | null`

The module, Wasm memory, heap views, FS, IDBFS, IndexedDB connection,
Giac/LVGL/SDL pointers, and native objects are never returned. The diagnostic
snapshot is copied JSON produced by the existing native-only diagnostic seam.

Reset asks for explicit confirmation, clears only NumOS's `/numos` database,
fully shuts down the old runtime, and starts a new module. It never lets old
in-memory state repopulate the cleared database.

## Events

All events bubble across Shadow DOM with `composed: true` and copied bounded
details:

- `numos-statechange`
- `numos-progress`
- `numos-ready`
- `numos-error`
- `numos-persistencechange`
- `numos-shutdown`

`numos-progress` reports the actual received Wasm byte count, the response or
manifest total when known, and an indeterminate stage otherwise. Compilation
progress is not invented. Stages distinguish manifest, runtime loader, Wasm
download, instantiation, storage hydration, native boot, and launcher ready.

Errors have bounded codes:

- `ASSET_MANIFEST_FAILURE`
- `LOADER_FAILURE`
- `WASM_FETCH_FAILURE`
- `WASM_INSTANTIATION_FAILURE`
- `UNSUPPORTED_BROWSER`
- `PERSISTENCE_FALLBACK`
- `NUMOS_BOOT_TIMEOUT`
- `SECOND_ACTIVE_INSTANCE`
- `SHUTDOWN_FAILURE`

## Lifecycle and ownership

The validated lifecycle is:

```text
idle -> waiting_for_viewport? -> loading_manifest -> loading_runtime
     -> downloading_wasm -> instantiating -> hydrating -> booting -> ready
ready -> flushing -> ready
any active state -> shutting_down -> stopped
recoverable failure -> error -> loading_manifest
stopped -> loading_manifest
```

Transitions not present in the table are rejected. `start()` and `shutdown()`
coalesce in-flight calls. Every start has a monotonically increasing token;
stale fetch, import, factory, persistence, animation-frame, and observer
callbacks cannot mutate a later generation. `restart()` always calls a new
Emscripten factory and creates a new 320×240 canvas.

Version 1 deliberately permits one loading/booting/ready emulator per
document. Multiple idle elements are allowed. A second start rejects with
`NumosSecondActiveInstanceError` / `SECOND_ACTIVE_INSTANCE` and renders the
same visible error. The guard is released only after shutdown has stopped
input, released held keys, flushed storage, cancelled the native loop,
destroyed apps/LVGL/SDL and the Giac context, detached runtime listeners,
disconnected observers, closed the controller's IDBFS connection, cleared
timers, and released module/canvas references.

## Pre-edit ownership audit

At baseline `2eb172ade200c0a156469e5ed60cdd9d84c06459`,
`numos-shell.js` was a one-shot top-level program:

| Object | Pre-edit owner |
|---|---|
| module instantiation | top-level `numos-shell.js` |
| canvas | light-DOM `index.html#canvas`, copied into `Module.canvas` |
| main loop | C++ `emscripten_set_main_loop`, scheduled by the factory closure |
| shell listeners | permanent anonymous window/document/canvas listeners |
| SDL listeners | canvas/document/window callbacks registered by SDL |
| persistence | one private shell controller, without a dispose operation |
| readiness | permanent `window.numosReady` |
| shutdown | one terminal shell `shutdownPromise` |
| public API | permanent `window.numos` |
| asset URLs | fixed imports and loader-relative `numos-emulator.wasm` |

The live audit found a 640×480 canvas, global SDL keyboard listeners on
`window`, shell `resize`, `pointerdown`, `pagehide`, and `visibilitychange`
listeners surviving native shutdown, and public persistence snapshots exposing
the database/store names and metadata record. SDL itself did unregister its
canvas/document/window runtime listeners on `SDL_Quit`.

The Shadow DOM blockers were the document-level `#canvas` lookup in SDL
2.32.10, global keyboard target, document selectors, global readiness/API,
light-DOM ID CSS, and fixed asset paths. The pinned Emscripten 6.0.3 build now
uses its compatibility event-target mapping so SDL's internal `#canvas`
resolves to private `Module.canvas`, including inside Shadow DOM. C++ sets
`SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT` to that canvas, so unfocused components
do not capture keyboard input. No global canvas or module is installed.

The recreation blockers were the one-shot shell constants/promises, permanent
shell listeners, controller debounce/database references, retained canvas, and
the Giac singleton under `EXIT_RUNTIME=0`. All now have explicit generation
ownership and cleanup; Giac has an explicit `shutdown()` called after retained
expressions are destroyed.

## Display, input, fullscreen, and accessibility

NumOS remains exactly 320×240. The browser canvas backing store is also
320×240; `ResizeObserver` changes only its CSS size. The component preserves
4:3, chooses an integer CSS scale whenever both dimensions permit, permits
fractional scaling only below native size, centers the display, and uses
nearest-neighbour presentation. Device pixel ratio never changes NumOS
geometry. SDL continues to map the canvas client rectangle to its logical
coordinates.

Physical keyboard and pointer events remain SDL events consumed by the
existing `dispatchKey` path. Keyboard listeners live on the focused canvas.
Blur, hidden visibility, page hide, disconnection, and shutdown synthesize
paired releases for held physical events and release every held logical touch
key.

The web keypad is one declarative functional layout containing all 68
non-`NONE` logical `KeyCode` values. A static test compares every identifier
and numeric value to `src/input/KeyCodes.h`. It contains no GPIO, CAM,
WROOM-1U, row, or column data. Native buttons provide keyboard semantics and
ARIA labels. Pointer capture, independent pointer IDs, reference-counted
logical holds, `pointerup`, `pointercancel`, and `lostpointercapture` prevent
stuck keys. Auto mode uses coarse-pointer and narrow-viewport queries only as
hints; the manual toggle is always present. Vibration is hidden when
unsupported and disabled by default.

Fullscreen is a user-gesture action on the component host. State follows
`fullscreenchange`/`fullscreenerror`, scaling is recalculated on both edges,
focus returns to the canvas, and Escape remains browser-owned.

The shell includes native buttons, visible focus, restrained `aria-live`
updates, safe text-only error rendering, reduced-motion support, and a
high-contrast media adjustment. It does not redesign the UI inside NumOS.

## Persistence

The IDBFS database, `/numos` mount, schema version 1, and
`settings:ST01;variables:VR01;neo:source-v1` formats are unchanged. Public
state reports persistent, syncing, error, fallback, or disabled state plus
bounded counts/timings; it omits database/store names, filenames, metadata
contents, filesystem objects, and file contents.

Ephemeral fallback does not block NumOS and is visibly labelled. There is no
multi-tab lock, merge, or conflict-safety claim.

## Production packaging and hosting

Build and validate:

```sh
./wasm/build.sh Release
./wasm/build.sh Debug
node wasm/package.mjs --validate Release
node wasm/package.mjs --validate Debug
```

Ignored outputs are under `out/wasm/dist/release/` and
`out/wasm/dist/debug/`. `numos-assets.json` maps the hashed Wasm, runtime,
component, CSS, persistence, keypad, and demo-shell assets to byte sizes,
SHA-256 hashes, gzip sizes, Brotli sizes, MIME types, build identity,
configuration, Emscripten version, exception mode, and logical display.

All manifest URLs begin with `./` and resolve relative to the final manifest
response URL. `/`, `/numos/`, `/demo/calculator/`, and arbitrary deeper paths
therefore work without rewriting loader text or evaluating fetched JavaScript.
`locateFile` resolves the hashed Wasm, while the progress fetch supplies the
same bytes directly to the factory.

Serve `.wasm` as `application/wasm`. A provider-neutral cache policy is:

```text
/path/numos-assets.json  Cache-Control: no-cache
/path/index.html         Cache-Control: no-cache
/path/*.<hash>.*         Cache-Control: public, max-age=31536000, immutable
Content-Type (*.wasm):   application/wasm
```

Precompress immutable assets with gzip and/or Brotli and configure content
negotiation; do not rename away the content hash. No service worker,
provider-specific deployment feature, root URL, or current working directory
is required.

Local server:

```sh
python3 -m http.server 8000 --directory out/wasm/dist/release
```

## Standalone demo compatibility

`index.html` imports the component and autostarts near the viewport.
`numos-shell.js` installs a demo-only `window.numos`/`window.numosReady`
forwarder for older smoke automation. The component module itself never writes
those globals and does not depend on the adapter.

## Browser support and limitations

The selected build uses native Wasm exceptions, no pthreads, and no Asyncify.
Browser support is claimed only for engines in which the packaged Wasm,
launcher, real Giac input, focused input, and native shutdown tests pass.

| Engine | WASM-EMBED-01 Release verification |
|---|---|
| Chromium (Playwright 1181) | Pass: lazy load, launcher, real Giac, focused SDL input, persistence, shutdown/recreation, fullscreen harness, nested path, touch/high-DPR. |
| Firefox (Playwright 1489) | Pass: packaged Wasm with native exceptions, launcher, real Giac, focused SDL input, persistence, and shutdown. |
| WebKit (Playwright 2191) | Pass: packaged Wasm with native exceptions, launcher, real Giac, focused SDL input, persistence, and shutdown. |

The browser-shell visual pass also covers 390×844 portrait and 844×390
landscape layouts. The automated mobile interaction context uses touch input
at device pixel ratio 3 while retaining the 320×240 logical canvas.

Known limitations:

- one active emulator per document;
- no multi-tab persistence conflict protocol;
- fullscreen still requires a browser-approved user gesture;
- compilation progress is an honest stage, not a fabricated percentage;
- cancelling a JavaScript module import cannot cancel an already completed
  HTTP response, but its generation is invalidated and it cannot start NumOS;
- WebKit versions without the selected native Wasm-exception implementation
  are unsupported; no alternative math backend is substituted;
- this is a browser canvas host, never the production LCD or electrical keypad.
