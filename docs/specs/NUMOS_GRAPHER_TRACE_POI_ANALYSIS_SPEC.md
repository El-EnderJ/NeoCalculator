# NumOS — Grapher Trace, POI & Analysis Specification

> Companion to `NUMOS_GRAPHER_GEOMETRY_ENGINE_V2_SPEC.md` (same audit base: `98f61bd`, 2026-07-02). Defines trace architecture, POI/intersection markers, the Calculate-menu support matrix, and tangent/integral semantics. **[V]** = verified current behavior with citation; **[P]** = proposed v2 normative behavior.

---

## A. Trace architecture

### A.1 Current state machine [V]

Graph-tab modes: `GrMode { IDLE, NAVIGATE, TRACE }` (`GrapherApp.h:96`). Entry points into TRACE:

- Tab switch to Graph auto-enters TRACE iff a traceable slot exists, else NAVIGATE (`switchTab`, `GrapherApp.cpp:823-857`); "traceable" = `isExplicit() && visible` (`firstTraceableFunc`, `GrapherApp.cpp:1632-1637`; `isExplicit` = `valid && !implicit`, `GraphModel.h:80`).
- Toolbar "Trace" (`handleToolbar` case 3, `GrapherApp.cpp:2669-2691`) and ENTER in NAVIGATE (`handleGraphNav`, `GrapherApp.cpp:2728-2747`) — both refuse with info-bar text "Trace n/a: no y=f(x) (implicit/inequality)" when nothing is traceable.
- Trace state: `_traceX` (world x), `_traceFn` (slot index) (`GrapherApp.h:230-231`); initial `_traceX = (xMin+xMax)/2` (`GrapherApp.cpp:2681,2738,849`).

Known quirk [V]: the lazy-create branch of `switchTab(GRAPH)` (first-ever entry) deliberately skips `computePOIs`/`drawTraceCursor` — the cursor appears on the first trace key (`GrapherApp.cpp:836-842` comment); the warm branch computes both (`GrapherApp.cpp:854-857`). [P] keep (deterministic first-frame goldens depend on it).

### A.2 explicitY trace [V, retained]

- LEFT/RIGHT move `_traceX` by exactly one screen pixel of world width `(xMax−xMin)/SCREEN_W` (`GrapherApp.cpp:2763,2767-2803`), clamped to viewport, then **camera-follow**: `syncViewportToCursor` re-centers the viewport on (traceX, f(traceX)) and replots + restarts async POIs (`GrapherApp.cpp:2235-2253`). Net effect: cursor visually pinned to center; the world scrolls.
- y is recomputed live per position (`drawTraceCursor`, `GrapherApp.cpp:1929`); NAN/Inf hides dot, crosshair, and pill (`GrapherApp.cpp:1930-1936`) and the info bar prints `y=--- (off domain)` (`GrapherApp.cpp:1981-1983`).
- **Cursor dot**: 8×8 px rounded `lv_obj`, colored per slot (`GrapherApp.cpp:604-611,1942-1944`). **Crosshair**: two 1-px 60 %-opacity lv_lines spanning the graph area (`GrapherApp.cpp:613-628,1946-1956`). **Value pill**: floating bottom pill `x: %.3f   y: %.3f`, or `<POI label>: (x, y)` when snapped (`GrapherApp.cpp:1958-1972`), montserrat_14 (STIX 18 has no space glyph — `GrapherApp.cpp:666-668`). **Info bar**: `Trace: x=…  y=…` / `Trace [Label]: …` (`updateInfoBar`, `GrapherApp.cpp:1975-1997`); mode badge `[Trace]`/`[Pan]` (`GrapherApp.cpp:1999-2008`).
- UP/DOWN switch to next/previous traceable slot keeping x (`GrapherApp.cpp:2804-2833`); tangent overlay retargets if active (`GrapherApp.cpp:2811-2816`).
- ENTER opens the Calculate menu (`GrapherApp.cpp:2834-2837`). AC exits to NAVIGATE, clearing shading/tangent overlays and hiding trace widgets (`GrapherApp.cpp:2838-2851`). MODE/Home is handled by SystemApp above the app and must exit from TRACE without hang (CI gate `grapher_home_return_smoke`, `emulator-build.yml:525`).
- Transform duplication [V→P]: `drawTraceCursor` computes screen coords inline (`GrapherApp.cpp:1938-1939`) instead of `GraphView::worldToScreen*` (`GraphView.cpp:55-64`). v2 replaces the inline math with the GraphView transform (identical numerically today: areaW=SCREEN_W=bufW=320; ticket GR-01).

### A.3 explicitX trace [P]

- Parameter is **y**: LEFT = y decreases, RIGHT = y increases (screen-down/up motion follows the crosshair vertically; LEFT/RIGHT chosen over UP/DOWN because UP/DOWN are reserved for function switching — ambiguity resolved). Step = `(yMax−yMin)/GRAPH_CANVAS_H` world units (one vertical pixel).
- Cursor at (f(y), y); pill prints `x: %.3f   y: %.3f` in the same order as explicitY (x first) — the display never transposes.
- Camera-follow centers on (f(y), y); off-domain rule identical (§A.2).
- Domain gaps: after a NAN run, stepping continues in the same y direction until a finite point or the viewport edge (same emergent behavior as explicitY, which simply recomputes per pixel [V]).

### A.4 Implicit contour trace [P]

Requires the geometry cache (`GeomStore`, geometry spec §D). Not available when the cache is disabled (OOM fallback) — info bar "Trace n/a (low memory)".

- **Selection**: traceable set expands to include slots of kind Implicit that produced ≥1 polyline. UP/DOWN cycle order: slot index, then `contourId` within the slot (each contour of a multi-branch curve, e.g. both hyperbola branches of `x*y=1`, is a separate trace target).
- **Seed**: the polyline point nearest the viewport center (Euclidean in world units under equal aspect — well-defined since aspect is normalized, geometry spec §E.9). Deterministic: first-found minimum in point order.
- **Parameterization**: index + fractional arclength along the polyline. LEFT/RIGHT move the cursor by a world distance equal to one pixel (`(xMax−xMin)/320`) along the chain, interpolating within segments. Direction convention: RIGHT moves toward increasing point index (build order, geometry spec §D.3); LEFT decreases. On a closed contour (`CLOSED` flag) the index wraps; on an open contour the cursor stops at the endpoint (no wrap, no jump to another polyline).
- **Camera-follow**: same recenter-and-rebuild as explicit trace. **Critical rule**: after a rebuild, the trace re-anchors to the *nearest point of the same funcIdx+contourId* to the previous world position; if that contour vanished (zoomed out of existence), trace exits to NAVIGATE with info bar "trace lost contour" — never silently jumps to another branch.
- **Pill**: `x: %.3f   y: %.3f` (both interpolated); info bar `Trace (implicit): x=…  y=…`.
- **Determinism**: with `--deterministic`, identical key sequences must produce byte-identical frames — seed and step rules above are pure functions of the cache; no wall-clock anywhere.
- ENTER on an implicit trace opens the Calculate menu **with the implicit support column applied** (§C) — most rows disabled, Tangent enabled (§D.3).

### A.5 Inequality boundary trace — decision

**[P] Decision: NOT traceable in v2.** Rationale: the boundary is the same G=0 contour as an implicit slot, but the user's object of interest is the *region*; a cursor pinned to the boundary invites misreading the pill's (x,y) as "a solution point" when strict inequalities exclude the boundary. A user who wants to trace the boundary adds the equality as a second slot. `firstTraceableFunc`'s exclusion of inequalities [V] (`GraphModel.h:69-72`, `GrapherApp.cpp:1632-1637`) is therefore retained verbatim, and the refusal message is extended to name the workaround: "Trace n/a — add G=0 as its own function to trace the boundary" (ticket GR-11).

### A.6 Home/AC and modal interaction invariants

- AC from TRACE → NAVIGATE (never straight to toolbar; `GrapherApp.cpp:2838-2851`); AC from NAVIGATE → IDLE + toolbar focus (`GrapherApp.cpp:2748-2753`); AC chain up to tab bar → Expressions → SystemApp exit (`handleTabBar`, `GrapherApp.cpp:2440-2447`).
- While the Calculate menu or Templates modal is open, ALL keys route to the modal (`handleKey` intercepts, `GrapherApp.cpp:2368-2378`); AC/LEFT close the calc menu without action (`GrapherApp.cpp:3074-3077`).
- `end()` deletes the POI async timer and template timer, destroys canvases before the screen, and nulls every LVGL pointer (`GrapherApp.cpp:167-228`) — reliability invariant L-5 cites this as the reference pattern. Any new v2 widget (dashed boundary needs none; slot error labels do) must join this teardown list.

---

## B. POI / intersection markers

### B.1 Current inventory [V]

`POIType { Root, Min, Max, Intercept, Intersection }` (`GrapherApp.h:101-107`); store `POI _pois[20]` + `_numPOIs` (`GrapherApp.h:209-210`, `MAX_POIS=20` `GrapherApp.h:77`). Computation is async: `computePOIs(fi)` zeroes the store and arms a 10-ms `lv_timer` that appends one function per tick starting at `fi` (`startAsyncPOI`/`poiAsyncTimerCb`, `GrapherApp.cpp:2981-3015`).

Per function (`appendPOIsForFunction`, `GrapherApp.cpp:2052-2185`):
- **Intercept**: f(0) if x=0 in view (`GrapherApp.cpp:2060-2076`).
- **Root**: sign change on the 40-sample grid + 25-iteration bisection (`BISECTION_ITER`, `GrapherApp.h:78`; `GrapherApp.cpp:2078-2112`).
- **Min/Max**: discrete 3-point local extremum on the same grid (`GrapherApp.cpp:2114-2132`).
- **Intersection**: vs each *earlier* slot j<fi, sign change of f_fi − f_j + bisection (`GrapherApp.cpp:2140-2184`).
- Dup tolerance: `|Δx| < step·0.1` where step = xRange/40 (`GrapherApp.cpp:2065,2103,2121,2170`).

**Marker rendering [V]**: ONLY `Intersection`-type POIs are drawn, as filled purple circles r=3 px, color `0xAA00CC` (`replot`, `GrapherApp.cpp:1901-1905`; `drawIntersectionMarker`, `GraphView.cpp:388-408`). Roots/extrema/intercepts exist solely for trace snapping.

**Snapping [V]**: within 8 px screen distance (`POI_SNAP_THRESHOLD_PX`, `GrapherApp.h:79`) the cursor magnetizes to the POI (`snapToPOI`, `GrapherApp.cpp:2193-2232`); moving off a snapped POI sets a 10-move escape window (`GrapherApp.cpp:2772-2799`).

### B.2 Current defects to fix [P]

1. **Coverage gap**: `startAsyncPOI(fi)` iterates slots fi…N−1 (`GrapherApp.cpp:2990,3009-3014`); when trace starts on slot 2, slots 0/1 get no root/extremum POIs and the pair (0,1) gets no intersection POIs (the j<fi inner loop only covers pairs where the outer slot ≥ fi). **[P] Rule: POI computation always covers all valid visible slots 0…N−1 regardless of the traced slot** (ticket GR-05).
2. **Duplicate-tolerance inconsistency**: `step·0.1` (grid-relative) vs `0.01` absolute in `GraphModel::findIntersections` (`GraphModel.cpp:349`). [P] unify to `max(2·px_world, 1e-4·xRange)` (geometry spec §D.6).
3. **Intersections are explicit-only**: based on `evalAt` ⇒ implicit slots never intersect anything. [P] superseded by segment-based intersections on the geometry cache (§B.3).

### B.3 v2 purple-marker semantics [P]

- **Purple (0xAA00CC) is reserved for intersections** — between any two *distinct* slots' cached polylines (explicit×explicit, explicit×implicit, implicit×implicit, boundary participates as implicit; geometry spec §D.6). Roots/extrema/intercepts remain snap-only (no marker) — decision: marker clutter at 320×143 outweighs the value; the pill labels them on snap.
- **Self-intersections** of one implicit curve are NOT marked (saddle-cell topology is resolution-limited, geometry spec §C.11).
- **Axis roots**: an explicit slot's Root POI is exactly its intersection with y=0; it is NOT drawn purple (it is not a two-slot intersection) — no double marker when the user also plots `y=0` (dedup: the segment engine skips pairs where one curve is the axis? No — `y=0` as a slot is a real curve; its intersections with f ARE purple. The Root-POI stays unmarked; the purple dot from the y=0 slot pair is drawn. One marker total.)
- **Discontinuities/tangencies**: no marker; `Touch` POIs (geometry spec §C.12) are snap-only.
- **Merging**: dedup tolerance as §B.2.2, first-found wins in deterministic order.
- **Budget**: total POIs ≤ MAX_POIS=20 [V retained]; when the cap is hit, info bar shows "POIs limited to 20" once per build (geometry spec F.6).
- **Visibility**: markers cull to the viewport (already bounds-checked, `GraphView.cpp:392`); markers are re-derived on every cache rebuild — never stale relative to the plot.
- **Relation to trace cursor**: snapping unchanged (8 px, escape window). Snapped pill shows the label: `Intersection: (x, y)` etc. (`GrapherApp.cpp:1963-1971`).

---

## C. Calculate-menu support matrix

### C.1 Current state [V]

Six fixed items, no disabled state: `Find Root, Find Minimum, Find Maximum, Find Intersection, Calculate Integral, Draw Tangent` (`CALC_MENU_LABELS`, `GrapherApp.cpp:2910-2917`; `CALC_MENU_ITEMS=6`, `GrapherApp.h:182-183`). Menu opens only from TRACE-ENTER (`GrapherApp.cpp:2834-2837`), so `_traceFn` is always explicit today. All ops act on `_traceFn` over the full viewport `[xMin,xMax]` (`executeCalcOption`, `GrapherApp.cpp:3083-3183`; backends `math::findRoot/findExtremum/findIntersection/numericalIntegral`, `MathAnalysis.h:69-113`, MAX_ITER=100, Simpson N=100 `MathAnalysis.cpp:236`).

**Existing wrong-affordance hole [V]**: Find Intersection picks the first *other valid* slot including implicit/inequality slots (`GrapherApp.cpp:3117-3121` tests only `_funcs[i].valid`), whose `evalAt` is NAN ⇒ result is always "Not found in range" — a silently wrong "no intersection" answer when a visible crossing exists on screen. (Ticket GR-06/GR-08.)

### C.2 Support matrix (v2 normative)

Rule zero (the "no silent wrong thing" rule): **a menu row is rendered disabled (gray, non-activatable) whenever the semantics below say disabled; ENTER on a disabled row shows pill "n/a for this curve type" and closes nothing.** Disabled state is computed from the traced slot's kind at menu open (ticket GR-08/GR-10).

| Action | explicitY | explicitX [P] | implicit [P] | inequality | UI label |
|---|---|---|---|---|---|
| Find Root | **supported** [V] | **supported**: solve f(y)=0 by bisection in y; report `Root: y=…  x=f(y)` | **disabled** ("root" of a relation is undefined) | disabled | `Find Root` |
| Find Intersection | **supported**, segment-based [P], partner = *any* other visible curve incl. implicit (fixes §C.1 hole) | supported (segment-based) | supported (segment-based) | **partial**: boundary participates as a curve; label row `Intersect (boundary)` | `Find Intersection` |
| Find Minimum | **supported** [V]: min of f over viewport (`MathAnalysis.h:73-86`) | supported: minimal **x** = min f(y) over y-viewport; pill `Min x: …  at y=…` | disabled (min of what? undefined without an objective) | disabled | `Find Minimum` |
| Find Maximum | **supported** [V] | supported (max x) | disabled | disabled | `Find Maximum` |
| Calculate Integral | **supported**, semantics §E | **disabled** in v2 (§E.2) | **disabled** (§E.3) | **disabled** (§E.4) | `Calculate Integral` |
| Draw Tangent | **supported** [V] (§D.1) | **supported** (§D.2) | **supported** (§D.3) | disabled | `Draw Tangent` |

### C.3 Per-action contracts

For every action: pill text on success as today (`GrapherApp.cpp:3098,3105,3112,3133,3140,3152`); on failure `Not found in range` (`GrapherApp.cpp:3176-3181`); [P] on timeout `no result (timeout)` + `[GRAPH] calc timeout` (reliability F.2); cursor moves to the result except Integral (`GrapherApp.cpp:3160-3166`).

- **Find Root (explicitY)** [V]: bisection needs a sign change in the viewport (`MathAnalysis.h:56-70`); even-multiplicity roots are not found — pill correctly reports not-found (tangency limitation, geometry spec §C.12). Acceptance: `y=x^2-4` on default view → `Root: x=-2.0000` or `2.0000` (whichever bracket found first — [P] pin: nearest to current `_traceX` wins; requires searching outward from cursor — ticket GR-08). Wrong-semantics risk: none once cursor-nearest is pinned.
- **Find Intersection** [P]: candidate = nearest cached intersection POI to `_traceX` among pairs involving the traced slot; refined by bisection when both curves are explicit (`GraphModel.cpp:274-304`), accepted at segment precision otherwise (pill appends `≈` for unrefined hits). Acceptance tests: `y=x` ∩ `y=x^2` → (0,0) and (1,1), nearest wins; `y=x` ∩ `x^2+y^2=4` → `≈ (±1.414, ±1.414)`. Invalid state: no other visible curve → pill `No second function` [V] (`GrapherApp.cpp:3122-3125`).
- **Find Min/Max (explicitY)** [V]: 200-point scan + golden-section (`MathAnalysis.h:73-86`); on monotone functions returns the viewport-edge extremum — [P] pill appends `(at view edge)` when the result is within 1 px of xMin/xMax so users don't mistake it for a local extremum. Wrong-semantics risk closed by the suffix.
- **Draw Tangent**: §D. **Calculate Integral**: §E.

### C.4 Menu-item lifecycle rule

The menu must never contain a row whose action silently does the wrong thing (task invariant). Enforcement: `openCalcMenu` [P] receives the traced slot kind and renders the disabled set; `executeCalcOption` re-checks (defense in depth) and refuses with the pill message if a disabled option is somehow activated. Any future action added to `CALC_MENU_LABELS` MUST add a column to §C.2 and tests before merge (checked in review; `static_assert(CALC_MENU_ITEMS == …)` already forces label/count sync, `GrapherApp.h:183`).

---

## D. Tangent semantics

### D.1 explicitY [V, retained]

- Derivative: central difference `(f(x+h) − f(x−h)) / 2h` with `h = max(pixel_width, xRange·0.001)` (`TANGENT_FD_STEP_RATIO`, `GrapherApp.cpp:76`; `drawTangentOverlay`, `GrapherApp.cpp:3250-3276`). Any NAN among f(x), f(x±h) or a non-finite slope aborts silently [V] → [P] pill `tangent n/a (domain)`.
- Line drawn across the full viewport in world coords through (x₀, f(x₀)) (`GrapherApp.cpp:3266-3272`); pill `Tangent at x=…` (`GrapherApp.cpp:3152`); [P] pill gains slope: `Tangent: slope=%.4g at x=…` (users currently get no derivative value at all).
- Live retarget: moving the trace >0.25 px or switching function redraws (`TANGENT_REDRAW_THRESHOLD_PIXELS`, `GrapherApp.cpp:75,2855-2862`); cleared on AC (`GrapherApp.cpp:2840-2841,3278-3282`).

### D.2 explicitX [P]

- Compute dx/dy by central difference in y with the mirrored h rule (`h = max(pixel_height, yRange·0.001)`).
- Displayed tangent is a line in x-y space through (f(y₀), y₀) with direction (dx/dy, 1). Pill: `Tangent: dy/dx=%.4g` when dx/dy≠0 (report the conventional slope = 1/(dx/dy)), or `slope: vertical` when |dx/dy| < 1e-12 — never print `inf`.

### D.3 implicit G(x,y)=0 [P]

- Gradient by central differences: `Gx = (G(x+h,y) − G(x−h,y))/2h`, `Gy` analog; `h_x = max(px_w, xRange·0.001)`, `h_y = max(px_h, yRange·0.001)` — the same epsilon policy as §D.1 (single constant `TANGENT_FD_STEP_RATIO` reused).
- Tangent direction = `(−Gy, Gx)` (perpendicular to ∇G). Line through the trace point with that direction, clipped to viewport.
- Degenerate cases: ‖∇G‖ < 1e-12·(|G scale|) ⇒ pill `tangent n/a (singular point)` (critical point of G — e.g. the center of `x^2+y^2=0`); any NAN in the four probes ⇒ `tangent n/a (domain)`.
- Vertical tangent (Gy=0, Gx≠0): direction (0, Gx) — drawn as a vertical line; pill `slope: vertical` (§C.6 of geometry spec).
- Only reachable while tracing an implicit contour (§A.4), so the point is guaranteed near the curve; the tangent is drawn at the cursor's interpolated point, not re-projected onto G=0 (segment interpolation error ≤ 1 cell = 3 px — accepted and documented).

### D.4 Numerical policy summary

| Item | Value | Source |
|---|---|---|
| FD scheme | central difference | `GrapherApp.cpp:3264` |
| ε (all axes) | `max(1 pixel in world units, 0.001·range)` | `GrapherApp.cpp:3256-3257,76` |
| Vertical threshold | \|dx/dy\| or \|Gy/Gx\| ratio guard 1e-12 | [P] |
| Domain failure | abort + pill `tangent n/a (domain)` | [P]; silent abort today (`GrapherApp.cpp:3253,3262,3265`) |

---

## E. Integral semantics (strict)

### E.1 explicitY [V + P]

- **Meaning: signed area** ∫ f(x) dx — Simpson's 1/3, N=100 (`MathAnalysis.h:103-113`, `MathAnalysis.cpp:236`). Below-axis area is negative; the pill must say so: [P] label changes from `Area = %.4f` (`GrapherApp.cpp:3140`) to `∫f dx = %.4f (signed)`. The shading (checkerboard stipple between curve and the y=0 line, `drawIntegralShading`, `GrapherApp.cpp:3189-3241`) is drawn for the same interval.
- **Bounds [V]**: the whole viewport `[_xMin,_xMax]` (`GrapherApp.cpp:3138,3142`) — this changes when the user pans, which is surprising. **[P] Bounds v2**: two-press flow — first activation marks x₀ = current trace x (pill `∫: move cursor, ENTER to set b`), second ENTER computes over [x₀, x₁]. AC cancels. Unambiguous, keyboard-only, no new widgets (state machine inside TRACE; ticket GR-08). Until GR-08 lands, the viewport-bounds behavior is the documented interim and the pill must state the bounds: `∫[a,b] f dx = …`.
- NAN inside the interval [V]: Simpson sums whatever `evalFunc` returns; NAN poisons the total and `res.found` handling produces "Not found in range" only if the backend reports it — [P] rule: any NAN sample ⇒ pill `∫ n/a (domain gap in [a,b])`, no shading.
- Shading baseline is y=0 clamped to the view (`GrapherApp.cpp:3212,3225-3227`) — if the axis is off-screen the stipple runs to the clamped edge [V]; cosmetic, retained.

### E.2 explicitX — disabled in v2

An "area for x=f(y)" is only meaningful as ∫ f(y) dy (area between curve and the **y-axis**). That is a different geometric object than the y=f(x) area, and the current UI has no way to say so. **[P] Decision: `Calculate Integral` is disabled for explicitX until a UI exists that explicitly labels the result `∫x dy (area vs y-axis)`; no silent x/y swap is permitted.** (§C.2 matrix; the enabling ticket is deliberately NOT in the v2 set.)

### E.3 Implicit closed curves — disabled

Enclosed-area computation (e.g. Green's theorem on a cached closed polyline) is well-defined but unimplemented; until a ticket specifies polyline-area semantics (winding, self-intersection handling, truncation error bounds), the row is **disabled** for implicit slots. No partial support.

### E.4 Inequalities — disabled

Region area is not implemented (counting stipple cells is resolution-dependent and wrong near boundaries). **Disabled**, message `n/a for this curve type`.

### E.5 What must be disabled now (summary, enforce via GR-08)

`Calculate Integral` for explicitX / implicit / inequality; `Find Root`/`Min`/`Max` for implicit / inequality; ALL actions for inequality except `Find Intersection (boundary)`; every action when the traced-slot kind cannot be established (defensive default: all disabled).

---

*End of Document 2.*
