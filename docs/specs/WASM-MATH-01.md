# WASM-MATH-01 — NumOS Lab headless Giac module

## Baseline recorded before edits

- Branch: `main`
- HEAD: `69e1b4e4cb95eacf8882fc16a8e0cd7ce9503542`
- Worktree: clean (`git status --porcelain` returned zero entries)
- Existing packaged emulator artifacts (the artifacts predated the baseline
  HEAD and identify themselves as `2eb172ade200-*-dirty`):
  - Release Wasm: 6,730,735 bytes; gzip 2,146,247; Brotli 1,661,454
  - Release loader/runtime: 220,585 bytes; gzip 55,250; Brotli 47,075
  - component: 53,865 bytes; CSS: 6,387 bytes
  - Debug Wasm: 12,898,257 bytes; gzip 3,002,631; Brotli 2,175,214
  - Debug loader/runtime: 377,266 bytes; gzip 90,496; Brotli 74,033
- Complete emulator project closure: 68 translation units from
  `scripts/emulator_sources.py`, closure SHA-256
  `eaae6ef78b886898d601ceaa81ba81631644cb326e93e2a9f435d215e3cc113c`.
  `emulator_web` also appends its existing `Neo*.cpp` web-only closure.
- Giac closure: 40 translation units, closure-list SHA-256
  `4cf1e1e1289471644696a0b2a8a199479e76ed35fbca5d90d88fd6c42a98cb3f`.
  libtommath contributes 161 C translation units after excluding the existing
  native random shim.
- Giac host baseline:
  - engine suite: 177 pass, 0 fail
  - calculus suite: 26 pass, 0 fail
  - Neo suite: 44 pass, 0 fail
  - cross-app suite: 14 pass, 0 fail
  - total: 261 pass, 0 fail
- Existing public Giac API: idempotent `begin`, native-only `shutdown`,
  destructive `reset`, textual evaluate/simplify, structured
  evaluate/transform/calculus/solve, controlled assignment, retained 1D/2D
  numeric compilation/evaluation, generation and native diagnostics.
- Existing internal API: `giacinternal::sharedContext()` is restricted to
  `src/math/giac` and is not used by this module.
- Structured node types: integer, decimal, rational, symbol, add, negate,
  multiply, inverse, power, square root, nth root, function, pi, e, imaginary
  unit, three infinity forms, equation, assignment, list, set, matrix,
  interval, piecewise, complex, unevaluated, undefined and unsupported.
- Parser/serializer boundary: Calculation serializes authored `MathAST` to
  controlled Giac text; `GiacEngine` parses text directly into `giac::gen` and
  converts result gens directly to bounded `EngineResultNode`. Rendered
  `MathRenderer` text is never an input. The headless contract accepts the
  same controlled expression language as text because NumOS Lab has no
  `MathAST`, then applies its own lexical allowlist and the shared
  `EngineContracts` bounds before calling `GiacEngine`.
- Retained lifecycle: movable/non-copyable `CompiledExpression`, private
  implementation, parse once, engine generation stamp, deterministic stale
  failure after reset, destructor-owned release and native live-handle count.
- Angle behavior: `vpam::g_angleMode` is the runtime truth; every Giac entry
  synchronizes radians/degrees. Calculus applies the existing NumOS degree
  derivative transform. Retained evaluation synchronizes on every sample.
- Variable behavior: Giac assignments persist until reset. Calculation mirrors
  A–F plus its private `numos_Ans`/`numos_PreAns`; calculus and graph variables
  remain free. The headless API deliberately owns only A–F and never changes
  Calculation's Ans/PreAns policy.
- Existing binaries:
  - `emulator_pc`: 8,386,968 bytes
  - `emulator_pc_neo_smoke`: 9,219,432 bytes
  - ESP32 production: firmware.bin 5,346,800; ELF 93,559,912 bytes
  - validate: 5,380,576 / 94,071,880 bytes
  - giacdiag: 5,351,008 / 93,703,708 bytes
  - mathdiag: 5,385,680 / 94,160,956 bytes
- Golden state: 18 PPM goldens, 18 masks, 147 emulator scripts. Combined
  sorted golden/mask content SHA-256:
  `e30f40c48f69b3227bd3936401f9ef21400f14e2d9169ec155836941b8dc31a1`.

## Smallest safe source closure and dependency graph

```text
numos-math-client.js
  -> dedicated module Worker (request IDs, copied structured data)
     -> numos-math-worker.js
        -> numos-math-direct.js (private low-level adapter)
           -> generated Emscripten runtime
              -> numos_math_exports.cpp
                 -> NumosMathSession
                    -> GiacEngine public bounded types/API
                       -> 40-TU GiacEngine library closure
                          -> 161-TU libtommath closure
                    -> headless vpam::g_angleMode definition
```

The executable owns exactly four project translation units:

1. `GiacEngine.cpp`
2. `wasm/math/NumosMathSession.cpp`
3. `numos_math_angle_state.cpp`
4. `numos_math_exports.cpp`

There is no dependency edge to SDL2, LVGL, `src/apps`, `src/ui`, display,
renderer, launcher, keypad, HAL, firmware or persistence code. `-sFILESYSTEM=0`
is used. The module has no C++ `main` and exports only the bounded C adapter
plus allocator functions used privately by the direct adapter for copied
1D batches.

The facade lives under `wasm/math`, outside PlatformIO's recursive `src`
closure. `NUMOS_MATH_HEADLESS_WASM=1` is defined only on the
`numos_math_wasm` executable. It selects the isolated khicas/Emscripten
identifier-substitution workaround in `GiacEngine.cpp`; firmware,
`emulator_pc`, and `emulator_web` retain their pre-existing retained-expression
implementation and binary behavior.

## Production deployment decision

The production default is a dedicated module Worker. Giac has no proven safe
in-process hard-interrupt seam, so request timing is observational and the
normal API does not claim hard deadlines. A Worker keeps non-trivial solve,
factor and integration calls off the UI thread. `cancel()` terminates and
recreates the Worker, which is the hard cancellation boundary; variables,
retained handles and native state are intentionally lost. There are no
pthreads, SharedArrayBuffer, Asyncify or shared Wasm memory.

The direct harness remains packaged for low-level testing. One active session
is permitted per direct Emscripten module instance. Worker-backed public
instances each own a distinct Emscripten module and therefore do not share the
emulator's singleton or another Lab session's singleton.

## Facade and lifecycle

`NumosMathSession` owns the bounded session state and borrows the one
`GiacEngine` singleton inside its module instance. Its lifecycle is:

```text
create -> initialize -> ready -> reset* -> shutdown
```

Reset destroys retained trees/handles, clears controlled variables, increments
the opaque handle generation and rebuilds the context. Angle or variable
changes also invalidate handles and rebuild/repopulate the context.

Shutdown clears retained expressions, copied variable expressions, diagnostic
buffers and response trees before calling native `GiacEngine::shutdown`.
The shutdown response confirms zero active contexts. A later call on the same
public object returns `SHUTDOWN`; public recreation creates a new Worker/module.

## Controlled input and stable errors

Source is UTF-8-compatible ASCII Giac expression text under the NumOS
serializer conventions. Before Giac parsing the facade enforces:

- 2,000 source bytes
- 400 lexical/structural nodes
- 40 delimiter levels
- plain identifiers of at most 31 characters
- an allowlist of mathematical call names
- no control characters, quoted strings, semicolon programs, `:=`, shell,
  filesystem, purge/restart/eval commands or unknown callable names
- solve systems of at most four equations and six variables
- 1D batches of at most 10,000 samples
- grids at most 256×256 and 65,536 cells

Stable copied error codes are `NOT_INITIALIZED`, `INVALID_REQUEST`,
`SOURCE_TOO_LONG`, `DEPTH_LIMIT`, `NODE_LIMIT`, `INVALID_IDENTIFIER`,
`PARSE_ERROR`, `EVALUATION_ERROR`, `UNSUPPORTED_RESULT`, `TIME_LIMIT`,
`STALE_HANDLE`, `DISPOSED_HANDLE`, `INVALID_ANGLE_MODE`, `SHUTDOWN` and
`INTERNAL_ERROR`. Messages are capped at 512 bytes. Stack dumps, addresses and
paths are never returned.

## Structured-result JSON schema version 1

Every response has `schemaVersion: 1` and `ok`. Successful expression results
contain `result`, the canonical semantic node. Exact evaluation also includes
`exact`; a bounded approximate tree is supplemental. Display text is capped
at 512 bytes and is never reparsed.

All nodes have a deterministic `kind`. Scalar exact payloads use `value`
strings. Ordered operands use `children`. Functions add `name`; matrices add
`rows` and `columns` with row-major children; intervals add endpoint flags;
opaque fallbacks add a stable `reason`. Solution sets use ordered groups and
request-ordered `{variable,value}` entries.

Integers, including arbitrary-precision values, are decimal strings.
Rationals are exact numerator/denominator child nodes, whose integers are also
strings. Approximate structured decimal nodes retain Giac's bounded decimal
string. Retained numeric samples use a finite JavaScript number only in
`{kind:"finite",value}`; NaN and infinities are tagged strings in
`{kind:"non_finite",value}`.

## Variables, angles and retained expressions

Only A–F are persistent headless variables. x/y/z and other validated names
remain free symbols for calculus, solve and retained expressions. Values are
evaluated once and stored as exact Giac expressions, then copied back after a
context rebuild. Direct assignment syntax is forbidden, so request-local input
cannot leak persistent state. Ans and PreAns do not exist in the Lab session.

Angle modes are `radian` and `degree`; gradian is deliberately absent.
Changing mode invalidates retained handles. Degree semantics use the existing
NumOS/Giac synchronization and calculus transform.

1D and 2D handles contain generation-stamped native IDs behind Worker-local
tokens. Expressions parse once. Batch and grid loops call retained Giac
evaluation without reparsing. 2D output is row-major, y row first then x
column. Invalid, pole and non-real samples are tagged. Dispose is deterministic
and a second dispose returns `DISPOSED_HANDLE`.

## Packaging

Run:

```sh
wasm/math/build.sh Release
wasm/math/build.sh Debug
```

Each build produces `numos-math-assets.json` plus content-hashed Wasm,
runtime, direct adapter, Worker, public client and TypeScript declarations.
Every manifest asset contains relative `./` URL, byte size, SHA-256, gzip and
Brotli sizes. URLs are resolved against the manifest URL, so nested hosting
works without a service worker.

## Verification commands

```sh
cd tests/wasm
npm run math
npm run math -- --debug
```

The suite covers exact/approximate evaluation, structured result forms,
calculus, solve, controlled variables, cross-session isolation, retained
1D/2D operations, grid order, stale/disposed handles, bounds, Worker
termination/recreation, direct module access, zero-context shutdown and five
create/shutdown cycles in Chromium, Firefox and WebKit.

## Current limitations

- In-process Giac calls have no hard cancellation. `operationMs` and
  diagnostics are soft observations only. Use the Worker default; terminating
  it is the emergency boundary.
- A long factorization, symbolic solve, integration, expansion or high-degree
  exact expression can monopolize whichever thread hosts the direct module.
- Cancellation loses all Worker state by design.
- Transferable raw-buffer graph APIs, streaming results and pthreads are not
  part of version 1.
- Approximate structured decimal nodes are strings; only retained numeric
  sample values use JavaScript numbers.
