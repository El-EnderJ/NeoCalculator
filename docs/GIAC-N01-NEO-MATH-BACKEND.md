# GIAC-N01 NeoLanguage mathematical backend

## Authority contract

`NeoLanguageApp` owns one `NeoMathBackend` for the lifetime of an open app
session. Its initial engine is Giac. `math_engine("native")` and
`math_engine("giac")` are the only selectors. The backend calls the process
singleton `GiacEngine`; it owns no Giac context and never uses `GiacBridge`.
No failure path calls the other engine.

The backend selection is preserved across F5 runs while the app stays open
and resets to Giac when NeoLanguage is reopened. Neo source evaluation,
algebra, calculus, solving, series, numerical symbolic evaluation, limits,
sigma sampling, tables, optimization/ODE symbolic sampling, and plotting all
route through the selected backend.

## Reachable operation matrix

| Neo syntax | Arguments before GIAC-N01 | Previous native authority | Shared return contract | Giac route |
|---|---|---|---|---|
| arithmetic and powers | numeric/exact/symbolic | `NeoValue` plus `SymExpr` factories | exact scalar or structured math | structured evaluation |
| `sin`, `cos`, `tan`, inverse trig, `ln`, `log10`, `exp`, `abs`, `sqrt` | one numeric/symbolic value | libc for numeric, `SymFunc` for symbolic | scalar or structured math | structured evaluation |
| `n(expr)` | numeric/symbolic | `SymExpr::evaluate(0)` | approximate number or error | retained numeric evaluation |
| `simplify(expr)` | mathematical value | `SymSimplify` | exact structured math | typed simplify transform |
| `expand(expr)` | mathematical value | `SymSimplify` distribute pass | exact structured math | typed expand transform |
| `diff(expr,var)` | expression and symbolic identifier | `SymDiff` | exact structured math | typed calculus request |
| `integrate(expr,var)` | expression and symbolic identifier | `SymIntegrate` | exact or explicitly unevaluated math | typed calculus request |
| `solve(expr,var)` | residual/equation and identifier | `OmniSolver` | ordered Neo list | typed structured solve |
| `solve([eq...],[var...])` | equal-sized lists, native limited to 2/3 | `SystemSolver` | ordered list of dictionaries | one structured system call |
| `taylor(expr,var,center,order)` | expression, identifier, numeric center/order | repeated `SymDiff` plus doubles | exact coefficient list | Giac Taylor/series coefficients |
| `limit`, `sigma` | symbolic expression and sample variable | `SymExpr::evaluate` | approximate number or explicit error | retained compiled expression |
| `table` symbolic input | expression/range/step | native function sampling | list of `[x,y]` rows | selected retained expression |
| symbolic `ndsolve`, `minimize`, `maximize` | symbolic or callable input | `SymExpr::evaluate` | existing numerical result | selected retained expression |
| `plot(expr,xmin,xmax)` | symbolic expression and optional domain | stored `SymExpr*` | pending compiled plot | retained compiled expression |

`factor` was not a reachable Neo built-in before GIAC-N01 and remains
unexposed. Non-symbolic statistics, units, finance, bitwise, file, GUI, FFT,
and linear-algebra helpers are Neo runtime facilities rather than CAS
authority routes.

## Structured Neo value contract

Small exact integers and rationals use `NeoValue::Exact`. Other mathematical
structures use `NeoValue::Math`, backed by an engine-neutral
`EngineResultNode` tree owned by `NeoMathBackend`. Lists and dictionaries are
Neo containers with deterministic ordering and backend-owned run lifetime.
The tree covers integers of arbitrary printed size, decimals, rationals,
symbols, sums, products, negation, reciprocals, powers, radicals, functions,
pi, e, the imaginary unit, infinities, equations, lists, and complex values.

`NeoValue::Error` is distinct from a valid unevaluated or opaque mathematical
value. Opaque text is allowed only inside a visibly marked
`EngineNodeKind::Unsupported` leaf and cannot be silently re-evaluated.

Giac uses the shared NumOS RAD/DEG runtime setting. Numeric trig evaluation
and symbolic differentiation therefore follow the same angle mode as the
other Giac-backed apps; integration retains GiacEngine's established
calculus policy. Indefinite integrals do not append an arbitrary `+ C`.

## Environment isolation

The interpreter resolves locals, parameters, closures, shadowing, and globals
before calling the backend. Remaining free symbols are renamed to reserved
per-call identifiers for Giac input and restored structurally on output.
Neo bindings are never assigned into the shared Giac context. Strict retained
expressions substitute only their declared sampling variable; plot samples
perform no context writes.

## Bounds and generation behavior

- Input/result depth: 40.
- Input/result nodes: 400.
- Serialized source: 2,000 bytes.
- Giac calculus and solve bounds remain enforced by `GiacEngine`.
- Systems: at most eight equations in the Giac lane; native behavior remains
  limited to its existing two- and three-variable solvers.
- Taylor order: 0 through 32.
- Sigma/table loops retain their existing 10,000-sample bounds.
- Retained plot handles are generation-tagged. A Giac reset makes a handle
  stale; the next sample recompiles the stored bounded expression once.

No general timeout is claimed. Giac or the native CAS can still spend
unbounded time on a pathological expression that is small enough to pass all
structural limits.

## Final verification: emulator and firmware memory

NeoLanguage remains outside the normal emulator application whitelist.
`NeoIO::file()` still opens a host current-working-directory path directly,
outside the emulator LittleFS root, and the app also carries its editor arenas
and transient plot buffer. The `emulator_pc_neo_smoke` environment is therefore
an opt-in verification target: it compiles the production Neo application
closure and exposes only macro-gated lifecycle, console, counter, and retained
plot observations to NativeHal. The normal emulator whitelist is unchanged.

The apparent 65,536-byte firmware BSS increase is not a Neo allocation.
ESP-IDF's linker script emits `.flash_rodata_dummy` as a `NOLOAD`/`NOBITS`
virtual-address reservation and rounds its end to the next 64 KiB flash-MMU
boundary before `.flash.appdesc`. GIAC-N01 moved the preceding flash text
across one such boundary, increasing that reservation by exactly 0x10000.
The section has no C++ owner or runtime lifetime, contributes no bytes to the
firmware image, and occupies neither internal DRAM, PSRAM, nor RTC memory.
Generic `xtensa-esp32s3-elf-size` reports this address-space padding as BSS;
PlatformIO's unchanged static-RAM result is the applicable RAM measurement.
The framework linker script already asserts that the aligned reservation stays
inside the configured DROM region, so no application static assertion or
replacement arena is appropriate.
