# WxLens — Ground-Up Rewrite Roadmap

## Context

The user loves what Supercell Wx already does but believes its UI/UX has hit a ceiling and
wants a drastically more modern, more approachable, yet still power-user-capable weather radar
app — something that reads as "open face" like RadarOmega, borrows layout ideas from
RadarScope/other weather apps without copying them, and eventually rivals or beats paid
closed-source tools on features while staying free and open source. The explicit decision
(confirmed with the user) is a **full rewrite**: new UI framework, new app architecture, new
name/brand, built either as a new folder or new repo — not an incremental evolution of the
current `scwx-qt` QWidgets code. The one thing explicitly carried forward is `wxdata`, the
Qt-free NEXRAD Level 2/3 parsing library, since re-deriving ICD binary parsing from scratch
would be months of low-value, high-risk work.

Research before planning turned up an important nuance worth recording here: the *current* app
already has more than the user gave it credit for — linked/unlinked multi-pane grids, efficient
per-site data sharing, an in-progress modern chrome redesign (top bar / control dock / rail
nav), and an advanced `.pal` palette editor. None of that is wasted; it's the reference
implementation this rewrite ports *patterns* from (not code, since the UI framework is
changing). The genuine gap the current app has — and the reason a rewrite is defensible instead
of just re-skinning — is that its theming is native `QPalette`/`QStyle`, which cannot deliver a
truly custom, CSS-like "drastically modern" look. That gap is the central technical justification
for the framework choice below.

This document is meant to be saved and reused as the working roadmap for a multi-year,
multi-phase effort, handed to other AI planning/coding agents (including small-context ones) so
minimal re-planning is needed as work proceeds. Confirmed user decisions baked into this plan:
**full rewrite** of UI/app architecture, **reuse `wxdata` as a library** (not re-derived),
**app named `WxLens`** (namespace token `wxlens` used throughout the C++ code and directory
layout), **multi-user/server/login is a long-term stretch goal only**
(don't build it now, don't design around excluding it), and this **feature priority order**:
(1) modernized single-site radar UI, (2) multi-site mesh/mosaic radar, (3) other data layers
(satellite, soundings, jet stream/pressure) + velocity improvements as a lower-priority
sub-track, (4) 3D storm structure rendering — explicitly last.

**New workspace location (confirmed):** `C:\Users\sherw\OneDrive\Apps\WxLens`. This is a new,
separate directory/repo from the current app at `C:\Users\sherw\OneDrive\Apps\Supercell Wx` — the
latter stays in place, untouched, as the read-only reference source described throughout this
document (§2, and via Option A's submodule/reference approach in §3.1). The very first action
once implementation begins (start of Phase 0) is standing up this new directory and saving this
roadmap to `C:\Users\sherw\OneDrive\Apps\WxLens\docs\ROADMAP.md`, per §3.2's layout.

**Architecture framing (added after a dedicated architecture audit, see §0.1):** don't build a
radar application that later has to become a weather workstation — build the weather-workstation
architecture now, and make radar the first fully-implemented data domain. Concretely, every
visualization in the app — radar today, satellite/soundings/models later — should flow through
the same pipeline: **Data Source → Data Product → Visualization Layer → View**. A pane hosts a
*View*; it is not permanently "a radar map." This reframing changes nothing about what ships in
Phase 1 (still radar-only, per the user's priority order) but changes how Phase 1 is built, so
Phase 2/3's satellite/model/sounding work plugs into existing seams instead of forcing another
rewrite.

---

## 0. Ground rules for agents executing this roadmap

- This document is the single source of truth for the effort. Update it in place as phases
  complete (check off scope items) rather than creating parallel planning docs. Keep it in the
  new repo at `docs/ROADMAP.md` from Phase 0 onward.
- Every phase states **Goal / Scope / Key technical work / Size**. Size is relative
  AI-agent-execution effort (S/M/L/XL), not calendar time.
- Anything marked **[OPEN QUESTION]** must be resolved with the user (or explicitly deferred)
  before an agent proceeds past it — don't silently decide.
- License discipline carries forward unchanged from the current repo: MIT project license, no
  GPL dependencies, LGPL only as dynamically-linked shared libraries (this already governs Qt
  and GEOS/libiconv today, and must continue to). Any new dependency (e.g. a GRIB2/NetCDF
  library in Phase 3) needs a license check against this rule before adoption.
- Don't pull later-phase technology into an earlier phase just because the final app will
  eventually need it (e.g. no GRIB2/NetCDF, satellite reprojection, or 3D rendering work in
  Phase 1). Earlier phases establish the interfaces/seams that later phases plug into — see §0.1
  and each phase's own scope for what that means concretely.

### 0.1 Locked architectural principles

These conclusions came out of a dedicated architecture-audit pass on this roadmap and are
treated as settled for planning purposes — later agents should build on them, not re-litigate
them, unless the user explicitly reopens one:

1. Full rewrite rather than incremental QWidget modernization.
2. Qt 6/QML for the new UI (§1).
3. `wxdata` remains the radar-parsing foundation, reused not re-derived (§2, §3.1).
4. **Weather-visualization-platform architecture, not a radar-only architecture** — Data Source
   → Data Product → Visualization Layer → View, radar first (see Context above, §3.2, §4.6).
5. Flexible multi-pane workspaces — a pane is a visualization workspace, not a fixed radar
   viewport (§4).
6. Per-property synchronization rather than one global link state (§4.1-4.2).
7. Unified, geographically-anchored `MapObject`s (§4.3).
8. A reusable measurement/interrogation framework, not a radar-only utility (§4.4).
9. Radar → Point geometry includes beam-height calculation (§4.8).
10. Beam-center MSL and terrain-relative AGL remain explicitly distinct — never imply AGL
    accuracy without real terrain data (§4.8).
11. Temporary interrogation (live probe) and persistent analysis objects (pinned/saved) are
    separate concepts (§4.3, §4.8).
12. The User Analysis Layer stays independent of whichever meteorological renderer is active
    underneath (§4.3).
13. Progressive disclosure delivers the approachable/professional dual personality — not by
    removing capability, but by layering it (§5.3).
14. Phase 1's architecture is comprehensive up front, but implementation proceeds in small,
    independently verifiable vertical slices suited to AI-agent execution (§7 Phase 1).
15. Later satellite/model/sounding/3D capabilities plug into this architecture rather than
    forcing another rewrite.

### 0.2 Agent execution rules

This roadmap is the architectural source of truth for every agent that implements it. Do not
simplify or reinterpret the architecture in §4-§5 merely because it would be easier to build a
simpler version. These rules govern *how* implementation proceeds, on top of *what* to build:

**Work in slices, not phases.** Never take "implement Phase 1" (or any phase) as a single task.
Work one vertical slice at a time (§7 Phase 1 lists the starting slice sequence). Each slice
must: inspect the existing implementation first, make the smallest coherent change, build
successfully, run relevant tests, verify behavior, document important discoveries, and leave the
repo in a usable state before the next slice starts.

**Read before replacing.** Before implementing anything that already exists in the current
Supercell Wx app, inspect the actual current source named in §2/§3.1's Critical Files (not
memory or assumption), understand its real behavior, identify reusable algorithms/data
structures, reuse `wxdata` where §2/§3.1 specify it, and port *behavior*, not UI code line-for-line.

**Prove the rendering seam early, before the big port.** Before porting all of `gl/draw/` (§1,
§7 Phase 1 slice 3), first prove the integration chain end-to-end on a trivial case: Qt
Quick/QML → MapLibre Native Qt → Qt RHI/custom rendering → a minimal custom radar draw → GPU
output. If the pinned MapLibre Native Qt version doesn't support the expected QML integration
(§1's open verification item), **stop and document the exact limitation** rather than inventing
an architectural workaround without review.

**QML is presentation only.** C++ owns application state, data access, caching,
synchronization, measurements, `MapObject`s, persistence, business rules, product identity,
provider logic, and rendering coordination (§1's integration mechanics). Don't move logic into
QML just because it's shorter to write there.

**Never make the pane radar-specific.** No `PaneController::radarSite`/`radarProduct`/
`radarTilt` as the fundamental model. The pipeline is Pane → View → Visualization Layer → Data
Product → Data Source (§4.6, principle #4); radar is the first implementation of it, not a
special case baked into the pane.

**Never reintroduce a single global `linked` flag.** Synchronization is per-channel (§4.1) — a
pane can share `Location` while keeping independent `Zoom`, share `Time` while keeping
independent `Product`, etc. That flexibility is intentional; don't collapse it back down for
implementation convenience.

**Keep persistent sync and one-shot actions distinct** (§4.1) — "match this pane" is a one-time
copy, "link these panes" is an ongoing relationship. Don't implement one as a shortcut for the
other.

**Guard against synchronization feedback loops with tests, not just care** (§4.2). Every
propagated change carries an origin (`UserInput`/`ProgrammaticSync`/`DataDriven`/
`FollowPropagation`); a value received *from* sync must never automatically re-trigger another
sync event. Write automated tests for this specifically, not just manual verification.

**`MapObject`s are geographic, never screen-space** (§4.3) — must stay correctly positioned
through pan/zoom/rotation/projection changes. Never store a measurement as pixel coordinates.

**Temporary probes are not `MapObject`s** (§4.3's three-tier lifecycle) — a hover/drag
measurement is transient UI state that disappears when the interaction ends unless explicitly
pinned. Pinned = session object; saved = persistent object. Don't auto-save every measurement.

**Never fabricate meteorological metadata** (§4.7) — especially terrain elevation, antenna
height, beam height, AGL, other radar metadata, or historical data availability. If required
data doesn't exist, represent that explicitly in the UI and the model; don't substitute a
plausible-looking guess.

**Beam-height semantics are strict** (§4.7): elevation angle, beam-center MSL, terrain
elevation, and beam-center AGL stay separate fields with separate meanings. Phase 1 may compute
beam-center MSL; it must not claim authoritative AGL without real terrain data. Verify what
`GetRadarBeamAltititude`'s `height` argument actually represents before using it (§2, §4.7) —
don't assume.

**Palette previews must use the real renderer, never a simplified fake** (§5.1) — the QML
palette editor previews through the same `ColorTable` parsing/rendering path the actual radar
renderer uses, so what's previewed is guaranteed to be what's rendered.

**Keep `.pal` palettes and UI themes separate systems** (§5.1 vs. §5.2) — `.pal` controls
meteorological data visualization; themes control UI chrome (surfaces, controls, typography,
spacing, borders, accents). Don't merge them.

**Don't acquire later-phase dependencies early** (§0, §6) — no GRIB2/NetCDF/GDAL/terrain
datasets/3D libraries in Phase 1 just because a later phase will need them. Build the
interfaces now; build the technology when its phase arrives.

**Measure performance early, not just at the end.** Modest-laptop performance is a requirement
(§1), not final polish. Once the first real radar renderer exists (§7 Phase 1 slice 3),
establish a baseline for FPS/frame time, CPU/GPU usage, memory, radar decode time, and
network/cache performance. Don't prematurely optimize before that baseline exists, but don't
defer measuring it either.

**Log state-changing events as they're built, not after** (§3.4) — product load
started/completed/failed, provider request started/completed/failed, pane sync changes, pane
group joins/leaves, settings changes, archive/live transitions, major render/data-state changes.
Logs should let someone answer "what actually happened?" without attaching a debugger.

**Test the C++ models independently of QML where possible** — especially synchronization,
persistence, `MapObject` scope resolution, measurement calculations, product identity,
data-service caching, and the temporary/pinned/saved lifecycle. UI-level tests verify
presentation and interaction; they shouldn't carry the whole correctness burden.

**"It looks right" is not acceptance.** For every slice, identify what behavior changed, how it
was tested, what acceptance criteria it satisfies (§4.8, or the phase's own Scope list), and
what remains unverified.

**Keep architectural discoveries in the repo, not just the chat.** If implementation reveals
something that changes the architecture's understanding, update `ROADMAP.md`, add/update an ADR
where appropriate (§3.3), and update `AGENTS.md` if it changes how future agents should work.

**Don't silently change locked decisions** (§0.1's list). If implementation makes one of them
genuinely impossible, stop and explain the conflict to the user rather than quietly deviating.

**Don't over-engineer future features.** "Make the architecture capable of supporting satellite"
and "implement satellite architecture now" are different tasks (§0's premature-tech rule) — do
the first, not the second.

**Keep the first-run experience in mind** (§5.3) — easy to understand immediately, deep when
requested, fast for experienced users. Don't solve complexity by permanently hiding capability;
use progressive disclosure.

**Competitor references are inspiration only, never copied** (§5.2's trademark/trade-dress
discipline) — RadarOmega/RadarScope/AWIPS may inform information hierarchy, density, and
interaction patterns, never logos, distinctive visual identity, exact layouts/color schemes, or
proprietary implementation. The app needs its own visual identity.

**Check the repo before asking the user.** Before raising a question, check `ROADMAP.md`, check
`AGENTS.md`, inspect the relevant source, check existing ADRs, and confirm the question isn't
already answered — and confirm it's genuinely one of this document's **[OPEN QUESTION]** items
(§9) before treating it as open.

**Never silently decide an open question** (§0) — resolve it with the user, or defer it only if
this roadmap explicitly permits deferral for that item, and record the decision (roadmap update
or ADR) either way.

**End every slice with a short handoff** so another agent can pick up the work cold:

```
IMPLEMENTED — what changed
TESTED — how it was verified
VERIFIED — what's confirmed working
NOT VERIFIED — what remains unconfirmed
ARCHITECTURAL NOTES — anything that changes understanding of the design
NEXT SLICE — what comes next
```

**The most important rule:** don't optimize for volume of code written. Optimize for correct
architecture, small reviewable changes, reproducible builds, tested behavior, and clear
documentation. The goal isn't just to make `WxLens` work — it's to make it possible for
many different AI agents, over a long period of time, to keep developing the same application
without gradually destroying its architecture.

---

## 1. Tech stack decision

**Recommendation: Qt 6 (C++20) with Qt Quick/QML for the UI**, keeping `wxdata` as the Qt-free
data/parsing core, and MapLibre Native (Qt bindings) + a ported version of the existing
custom-OpenGL-layer rendering pattern for the map.

Why, concretely, against the two next-best alternatives (web shell via Tauri/Electron +
MapLibre GL JS; Flutter):

- **"Modest laptop" constraint** (explicit user requirement): Qt Quick is natively GPU-composited
  (Qt RHI: D3D11/Metal/Vulkan/OpenGL backends) with no embedded browser engine. Electron ships a
  full Chromium (~150–200MB baseline just for chrome); Tauri is lighter but a JS engine is still
  a poor fit for a GL-heavy radar renderer.
- **Reuse of the proven radar rendering pipeline**
  ([scwx-qt/source/scwx/qt/gl/draw/](scwx-qt/source/scwx/qt/gl/draw/)): this custom OpenGL draw-item
  system, composited as MapLibre custom layers, ports to a Qt RHI custom render node behind a
  `QQuickItem` — same GL algorithms, same shader logic
  ([shader_program.cpp](scwx-qt/source/scwx/qt/gl/shader_program.cpp)), new plumbing only. A web
  shell would mean re-deriving this entire renderer in WebGL/JS from zero — the single biggest
  cost driver against that option. Flutter has no mature MapLibre Native custom-layer injection
  story either.
- **Reuse of `wxdata`**: direct C++ linkage, zero FFI boundary. A web shell needs a Rust↔C++ (or
  WASM) bridge; Flutter needs Dart FFI bindings over the C++ ABI.
- **Licensing continuity**: Qt LGPL-3.0-dynamic-link-only is already a validated, working setup
  in this repo (`AGENTS.md`'s "no GPL" rule, `ACKNOWLEDGEMENTS.md`). A web shell means auditing
  an entirely new Node/Electron-or-Tauri dependency tree from scratch.
- **The actual gap-closer — real theming**: QML is a declarative, CSS-adjacent language with
  genuine property animation, states/transitions, and fully custom Qt Quick Controls 2 styling
  with zero native-widget leakage — unlike today's QWidget/QPalette/QStyle approach, which is
  exactly the identified weakness. This is what makes a "drastically modern," RadarOmega-ish
  open-face look actually achievable.
- **Future mobile companion** (Phase 5 stretch): Qt has a real, maintained mobile deployment
  story (Qt for Android/iOS) sharing the *same* QML codebase — the data layer and much of the
  chrome could carry over, unlike Electron (no mobile story) or a from-scratch Flutter app.

This is a recommendation, not a locked decision — flag it back to the user if a hard web/browser
deployment requirement emerges later, since that would flip the calculus.

**Concrete integration mechanics for Phase 0/1:**
- MapLibre Native Qt is already vendored under `external/`. Confirm during Phase 0 whether its
  pinned version exposes a `QQuickItem`-native map surface; if it only exposes a
  `QWidget`-based `QMapLibre::Map`, fall back to `QQuickWidget` interop for the map surface
  specifically (chrome/panes stay pure QML either way) or upgrade the submodule pin. This is a
  factual check, not a design call — do it early since Phase 0 can't fully close without it.
- App shell: `QQmlApplicationEngine` + a thin `main.cpp` (mirrors
  [scwx-qt/source/scwx/qt/main/main.cpp](scwx-qt/source/scwx/qt/main/main.cpp)'s role) registering
  C++ context/controller types via `qmlRegisterType`/`QML_ELEMENT`, loading `qml/Main.qml`.
- Keep all data/state/business logic in C++ (controllers, managers, models) exposed to QML via
  `Q_PROPERTY`/`Q_INVOKABLE`/signals; QML stays presentation-only. This mirrors the current app's
  Manager Pattern (see `AGENTS.md`) almost 1:1, just with QML views instead of `.ui` files.

---

## 2. Repo map of the CURRENT repo (reference source — read-only)

Full detail lives in this repo's own `AGENTS.md`/`CLAUDE.md` — don't duplicate-maintain it, but
an agent working in the *new* repo needs this condensed pointer list without switching context.
**All paths in this section are relative to the Supercell Wx repo root,
`C:\Users\sherw\OneDrive\Apps\Supercell Wx`** (not the WxLens repo this roadmap now lives in) —
resolve them against that location.

- **[wxdata/](wxdata/)** — Qt-free C++20 library, **reused as-is or lightly adapted**.
  - [wsr88d/rda/](wxdata/source/scwx/wsr88d/rda/) — NEXRAD Level 2 (raw radial data) parsing.
  - [wsr88d/rpg/](wxdata/source/scwx/wsr88d/rpg/) — NEXRAD Level 3 parsing;
    [level3_message_factory.cpp](wxdata/source/scwx/wsr88d/rpg/level3_message_factory.cpp) dispatches
    ~90 ICD product codes (reflectivity, velocity, spectrum width, ZDR, KDP/PHI, CC/RHO, HCA,
    VIL, echo tops, precip, meso/hail/TVS detection, TDWR, etc.). **Do not re-derive from ICD
    specs — reuse this.**
  - [awips/](wxdata/source/scwx/awips/) — WMO header/VTEC/UGC text product parsing
    (warnings/watches).
  - [gr/placefile.cpp](wxdata/source/scwx/gr/placefile.cpp) — general-purpose georeferenced vector
    overlay format (icons/lines/polygons/images/text), reusable for any custom overlay. The GR
    Color Table and Place File specs are credited "used with permission" in
    `ACKNOWLEDGEMENTS.md` — **preserve that attribution verbatim** if placefile/`.pal` support
    is carried forward.
  - [provider/](wxdata/source/scwx/provider/) — single-radar-site network data providers only
    (AWS S3, HTTP mirrors, NWS API, IEM API, Ondas). **No satellite/sounding/model/mosaic
    provider exists anywhere in this repo** — confirmed by search; all Phase 2/3 provider work
    is new.
  - [common/color_table.cpp](wxdata/source/scwx/common/color_table.cpp) — the `.pal` file parser
    and color-table rendering engine, already Qt-free. **Reuse this untouched**; only the Qt-side
    editor UI needs a rewrite.
  - `util/logger.hpp`/`.cpp` — spdlog-based per-module logger factory + file sink, reused as-is
    for the new app's logging (§3.4).
- **[scwx-qt/](scwx-qt/)** — Qt6 Widgets GUI, **superseded, not migrated line-for-line**.
  Reference for behavior/patterns only:
  - [manager/radar_product_manager.hpp](scwx-qt/source/scwx/qt/manager/radar_product_manager.hpp) —
    the key reference: per-radar-site singleton via `RadarProductManager::Instance(radarSite)`
    (confirmed present), Qt-signal-based data flow, `EnableRefresh`/cache-limit API (confirmed
    present). Port this *pattern* (per-site singleton + shared cache) into the new data-service
    layer.
  - [map/map_link_policy.cpp](scwx-qt/source/scwx/qt/map/map_link_policy.cpp) (confirmed:
    `ShouldApplyLinkedMapParameterSync` — only propagate pan/zoom sync when a genuine external
    signal source exists, guarding against feedback loops),
    `map_pane_view_link_state.cpp/.hpp` (JSON round-trip of a flat per-pane linked-bool vector
    keyed by grid size), `map_pane_splitter_state.hpp`, `map_popout_frame.cpp` (per-pane
    popout-to-window). **Reference implementation for §4's feedback-loop guard, to be extended
    from a single flag to per-channel sync.**
  - [map/map_annotation_types.hpp](scwx-qt/source/scwx/qt/map/map_annotation_types.hpp) (confirmed:
    already defines a `MapAnnotationObject{id, payload, style}` with a `payload` variant over
    `Polyline`/`Circle`/`Rectangle`/`Measure`, all geo-anchored via `common::Coordinate`, plus a
    `MapAnnotationTool` enum that already includes `Measure`), `map/map_annotation_model.cpp`,
    `map/map_annotation_layer.cpp`, `gl/draw/map_annotations_draw_item.cpp` — **this is real,
    non-trivial precedent for §4.3's unified map-object model; evolve it, don't reinvent it.**
  - [types/marker_types.hpp](scwx-qt/source/scwx/qt/types/marker_types.hpp) (confirmed:
    `MarkerInfo{id, name, latitude, longitude, iconName, iconColor}`) +
    `manager/marker_manager.cpp`, `map/marker_layer.cpp`, `model/marker_model.cpp` — today's
    markers are a separate, simpler, globally-scoped system from map annotations; §4.3 unifies
    both families into one `MapObject` model in the new app.
  - [util/geographic_lib.hpp](scwx-qt/source/scwx/qt/util/geographic_lib.hpp) (confirmed: wraps
    `GeographicLib::Geodesic` for WGS84 geodesic `GetDistance`/`GetAngle`/`GetCoordinate` — and
    is already Qt-free despite living under `qt/util/`, so it's a direct-reuse candidate, not
    just a pattern to port). **Also confirmed present in this same file:
    `GetRadarBeamAltititude(range, elevation, height)`** — a working, non-flat-Earth radar beam
    altitude calculation already exists in the current codebase. This is the direct reuse target
    for §4.8's beam-height requirement; verify exactly what its `height` parameter represents
    (site elevation, antenna height, or a combined effective height) before wiring it up, per
    §4.8's "don't fabricate metadata" rule, but do not re-derive the formula from scratch.
    `manager/radar_coordinate_table.cpp` builds the per-site range/azimuth radial grid used for
    today's implicit radar-relative measurements — the reference for §4.4's explicit
    "Radar → Point" measurement mode.
  - [map/radar_range_layer.cpp](scwx-qt/source/scwx/qt/map/radar_range_layer.cpp) — range-ring
    rendering, a reference for the Circle/RangeRing object type in §4.3.
  - [view/](scwx-qt/source/scwx/qt/view/) — bridges wxdata product records to renderable data.
  - [gl/draw/](scwx-qt/source/scwx/qt/gl/draw/) — per-primitive custom OpenGL draw classes. Port
    target for Phase 1's renderer.
  - [ui/top_bar_widget.cpp](scwx-qt/source/scwx/qt/ui/top_bar_widget.cpp),
    [ui/control_dock_widget.cpp](scwx-qt/source/scwx/qt/ui/control_dock_widget.cpp),
    `ui/radar_toolbox_rail_widget.cpp` — in-progress modern-chrome pieces to match/exceed;
    `ui/palette_editor_dialog.*`, `ui/palette_picker_widget.*` — palette tooling to match/exceed.
  - [util/palette_file.hpp](scwx-qt/source/scwx/qt/util/palette_file.hpp) (confirmed: this is the
    Qt-side `.pal` *editor* model, distinct from the Qt-free rendering engine — its own doc
    comment states the design principle explicitly: it round-trips edits through the real
    `ColorTable`/`GenerateColorTableImage` path for previews rather than reimplementing
    rendering, and is save-as-only, never silently overwriting the opened file). **Preserve this
    "preview through the real renderer" principle in the QML rewrite.**
  - `settings/` — typed settings backed by `QSettings` (OS-registry-coupled on Windows). **Do
    not carry `QSettings` forward as-is** — see §3.2's storage note.
  - `res/palettes/wct/` — bundled default `.pal` files sourced from NOAA's Weather and Climate
    Toolkit (public domain per `ACKNOWLEDGEMENTS.md`). Carry forward as bundled defaults.
- **[test/](test/)** — GTest suite mirroring `wxdata`'s namespace tree plus `qt/`.
- **Build tooling reference**: Conan 2 (`conanfile.py`) + CMake,
  `tools/setup-<platform>-<generator>-<config>.sh|bat`, `tools/conan/profiles/`,
  `CMakePresets.json`, CI matrix in `.github/workflows/ci.yml` (Windows VS2022/2026, Linux
  gcc/clang, macOS Intel+ARM). **Reuse this wholesale**, adding `qtdeclarative`,
  `qtquickcontrols2`, `qtshadertools` to the Qt module list.
- **Licensing files to carry forward and extend**: `LICENSE.txt` (MIT),
  `ACKNOWLEDGEMENTS.md` (dependency/source/asset tables — extend, don't restart), the
  `AGENTS.md`/`CLAUDE.md` pattern (replicate structure, see §3.3).

---

## 3. New project structure

### 3.1 `wxdata` reuse mechanics

Two viable approaches; **start with Option A, revisit Option B once the new project has
momentum**:

- **Option A (do first):** Add the entire current Supercell Wx repo as a git submodule at
  `external/legacy-supercell-wx/` in the new repo, and `add_subdirectory(.../wxdata)` — it
  already builds standalone with no dependency on `scwx-qt`. Fast to stand up; drags the whole
  legacy repo (incl. `scwx-qt`, `external/`, history) into every clone.
- **Option B (fast-follow, not blocking):** Extract `wxdata/` into its own standalone
  MIT-licensed repo via `git subtree split --prefix=wxdata` (preserves history), then have
  *both* the current Supercell Wx repo and the new repo consume it as a submodule (or eventually
  a proper Conan package) — one source of truth instead of two diverging copies.
- **[OPEN QUESTION]** whether to do the Option B extraction against the *live* Supercell Wx repo
  now, since that touches the existing shipping app's repo, or defer it. **Recommendation:
  defer** — start with Option A, revisit once the new project/repo actually exists and has a
  name.

### 3.2 Proposed directory layout (new repo)

```
wxlens/
├── AGENTS.md                       # primary agent guide — mirrors current AGENTS.md structure
├── CLAUDE.md                       # thin pointer + directory map, mirrors current CLAUDE.md
├── docs/
│   ├── ROADMAP.md                  # THIS document, lives here from Phase 0 on, kept current
│   ├── adr/                        # architecture decision records, one file per one-way-door
│   │                                #   decision (e.g. 0001-qt-quick-over-web-shell.md)
│   ├── capability-matrix.md        # Phase 0.5 deliverable: current-app vs. AWIPS vs. planned
│   │                                #   capabilities, product taxonomy, tool/interaction taxonomy
│   └── data-sources.md             # living catalog of §6, updated as agents integrate sources
├── LICENSE.txt                     # MIT, matches wxdata's terms
├── ACKNOWLEDGEMENTS.md             # extend current repo's table, don't restart
├── CMakeLists.txt / CMakePresets.json / conanfile.py
├── external/
│   ├── wxdata/                     # submodule per §3.1
│   ├── maplibre-native-qt/         # submodule, as today
│   └── ...                         # stb, hsluv-c (see §6), etc.
├── app/                            # the Qt Quick application (replaces scwx-qt)
│   ├── qml/
│   │   ├── Main.qml
│   │   ├── Panes/                  # pane grid, per-pane map host, per-channel sync UI (§4)
│   │   ├── Chrome/                 # top bar, side rail, docks — successor to top_bar_widget etc.
│   │   ├── Dialogs/                # settings, palette editor, about
│   │   ├── Controls/               # shared custom Qt Quick Controls style
│   │   └── Theme/                  # theme singleton + built-in theme definitions
│   ├── source/wxlens/               # C++20 backend, namespace `wxlens`
│   │   ├── main/                   # entry point, app bootstrap, CLI args
│   │   ├── data/                   # Data Source layer: per-source data services (successor to
│   │   │                            #   radar_product_manager); radar today, satellite/model/
│   │   │                            #   sounding services plug in here later (§4.6, principle #4)
│   │   ├── provider/               # NEW: satellite/sounding/model/mosaic providers (Phase 2/3)
│   │   ├── products/               # Data Product layer: typed product bindings (e.g. a radar
│   │   │                            #   site+moment+tilt) a pane's View can bind to — the seam
│   │   │                            #   later data domains plug into (§4.6)
│   │   ├── render/                 # Visualization Layer: MapLibre custom-layer draw classes,
│   │   │                            #   ported from gl/draw/ (radar today; satellite/model
│   │   │                            #   renderers register here later)
│   │   ├── panes/                  # View layer: pane grid model + per-channel sync-group state
│   │   │                            #   (§4.1-4.2, §4.6)
│   │   ├── objects/                # unified MapObject store: markers/measurements/drawings/
│   │   │                            #   range rings/annotations/storm tracks + scope resolution
│   │   │                            #   (§4.3-4.4), incl. temporary-probe vs. pinned vs. saved
│   │   │                            #   tiering and radar-geometry/beam-height calc (§4.8)
│   │   ├── theme/                  # ThemeManager exposed to QML, .pal / theme file I/O
│   │   ├── settings/               # structured config, typed accessors
│   │   ├── log/                    # thin wrapper over wxdata's util::Logger
│   │   └── util/
│   ├── res/                        # icons, fonts, bundled .pal files (carry forward res/palettes/wct/)
│   └── CMakeLists.txt
├── test/                           # GTest, mirrors app/source/wxlens/ tree
├── tools/                          # setup scripts, conan profiles — copy pattern from current repo
└── .github/workflows/ci.yml        # copy current matrix, add qtdeclarative/qtquickcontrols2/qtshadertools
```

**Settings/config storage:** do **not** default to `QSettings` (OS-registry-coupled on
Windows). Use structured, portable, plain-text files instead — TOML or JSON under
`QStandardPaths::AppConfigLocation`, one file per category mirroring today's
`settings/*_settings.hpp` split, loaded through a small typed wrapper that keeps the current
`settings_variable`/`settings_interface` *pattern* (validated defaults, Qt-signal change
notification) but retargets the storage backend. This directly satisfies the "shouldn't
actively preclude" multi-user/sync stretch goal — a portable file format could later live in a
per-user directory on a shared server, unlike registry keys — and enables shareable
config/theme files analogous to `.pal`.

### 3.3 Agent-legibility documents (Phase 0 deliverable)

Replicate this repo's proven two-file pattern:
- **`AGENTS.md`** — architecture (`wxdata` external + `app/`), the data-service pattern
  (successor to Manager Pattern), build/Conan/CMake workflow, QML/C++ boundary rules ("no
  business logic in QML," "no Qt Quick includes in wxdata," mirroring today's "no Qt in
  wxdata" rule), code style, testing, common-task recipes ("Adding a New Data Layer Provider,"
  "Adding a New Overlay Draw Primitive," updated "Adding New Radar Product Support").
- **`CLAUDE.md`** — thin pointer to `AGENTS.md` + full directory map, same pattern as today.
  Update both at the *end* of each phase.
- **`docs/adr/`** — new addition (not present in the current repo), recommended because this
  rewrite makes several one-way-door calls (Qt Quick vs. web shell, wxdata extraction, pane
  link-group model, config storage format) that a small-context agent picking this up later
  needs to find fast without git-blame archaeology.

### 3.4 Audit-friendly logging (Phase 0 deliverable)

Reuse `wxdata`'s `util::Logger` (`Initialize()`, `AddFileSink(baseFilename)`,
`Create(name)` → named `spdlog::logger` per subsystem) as-is. Concrete additions for the new
app, to satisfy the user's stated wish for other AI agents to easily audit work:
- One named logger per subsystem matching the directory map (`data`, `render`, `panes`, `theme`,
  `provider.satellite`, `provider.mrms`, etc.), so log lines are greppable by subsystem.
- Structured `info`-level log lines (even in release builds) for state-changing events
  specifically: pane link/unlink, product load start/finish/fail, provider fetch
  start/finish/fail, settings changes. This is what makes "did the agent's change actually do
  the right thing at runtime" auditable from a log file rather than requiring a debugger.
- Document the log file path in `AGENTS.md` so an agent knows where to look without asking.

---

## 4. Multi-pane camera synchronization & first-class map objects

An architecture review of this section (before implementation started) concluded that a single
linked/unlinked boolean — even a grouped one — is too coarse for professional multi-storm
workflows. The revised design below replaces "link groups" with **independent per-property
synchronization channels**, and promotes markers/measurements/drawings/annotations to a
**unified, scope-aware object model** instead of several one-off features. This section
supersedes the simpler group-boolean design in earlier drafts of this roadmap.

What the current app already proves and should be preserved regardless: panes exist in a
rectangular grid, camera sync only propagates when there's a genuine external signal source
(avoiding feedback loops — see `ShouldApplyLinkedMapParameterSync`), and state round-trips
through small, strictly-validated JSON for persistence.

A second audit pass confirmed the direction and added detail without changing the shape: panes
must be generic visualization workspaces (Data Source → Data Product → Visualization Layer →
View, principle #4 in §0.1), not permanently "a radar map" — §4.6 reflects this. It also asked
for an explicit radar beam-geometry/height capability (§4.8) and a temporary-probe vs.
pinned-object distinction for map objects (folded into §4.3).

### 4.1 Per-channel synchronization model (not a single link boolean)

Real workflows the architecture must support without fighting the user: all 4 panes pan/zoom
together; 2 panes linked while 2 others independently track a different storm; 3 panes linked
and 1 fully independent; all panes sharing center but each keeping its own zoom; center+zoom
linked while product/site/palette stay independent; a pane leaving a group without disturbing
the others; one pane following another's location without inheriting its product or site.

**Primitive:** each `PaneController` holds a map from **sync channel** to an optional **sync
group id**: `std::map<SyncChannel, std::optional<SyncGroupId>>`. Two panes are synchronized on a
given channel exactly when they hold the same non-null group id for that channel — independently
of every other channel. Channels:

- `Location` (center lat/lon)
- `Zoom`
- `Bearing` (rotation)
- `Pitch`
- `Time` (timeline cursor / archive time)
- `Animation` (play/pause/speed state)
- `Cursor` (hover/crosshair position, for synchronized data-probing across panes)
- `SelectedStorm` (see note below)
- `RadarSite`
- `Product`
- `Palette`

One primitive, many user-facing modes — this is a strength, not a gap: "same location, 
independent zoom" is just `Location` grouped + `Zoom` ungrouped. "Full camera link" is
`Location`+`Zoom`+`Bearing`+`Pitch` grouped together. "Link everything" groups all channels.
"Follow pane Y" is UI sugar over joining pane X to pane Y's existing groups (created on demand if
Y wasn't in a group yet) — architecturally identical to grouping, just presented as directional
in the UI. The exact channel set and its UI labels (Independent / Follow / Link center / Link
camera / Link everything, etc.) are a UX-design detail to finalize during Phase 1, per the
reviewer's note that terminology can be decided at that point — the architecture above is what
must be locked in now.

**Radar-site default and quick presets (added 2026-09-01):** `RadarSite` remains its own channel;
it must never be made an accidental side effect of linking `Location` or the rest of the camera.
For a fresh installation, however, the user-facing default is **change the radar site in all
panes**: panes begin in one shared `RadarSite` group because that is the least surprising behavior
for an ordinary reflectivity/velocity/CC multi-pane layout. A stable, addressable preference in
the pane/synchronization settings chooses between **All panes** and **Active pane only** for new
workspaces and newly created panes. The setting surface must also offer an explicit apply-to-current-
workspace action; silently regrouping an existing multi-storm workspace when a preference changes
would be destructive. Per-pane independence and custom site groups remain first-class and always
reachable.

The pane's compact Link A/Link B/Unlinked control gains a small preset menu with at least
**Map view only** (`Location`+`Zoom`+`Bearing`+`Pitch`) and **Map view + radar site** (the camera
bundle plus `RadarSite`). **Everything** and custom per-channel membership remain advanced choices.
This is UI sugar over the existing channel groups, not a second global link flag. A radar-site
change still respects the separate **Center map when radar site changes** preference; synchronization
must propagate the site once with the normal origin guard, then apply centering without creating a
site/camera feedback loop.

**Persistent link vs. one-shot action — keep these distinct:** joining a `SyncGroupId` is
persistent (future changes on that channel keep propagating until a pane leaves the group).
"Match this pane's location to another" / "copy view to another pane" is a **one-shot apply**
that copies current channel values without creating an ongoing group membership. Both need to
exist; don't conflate them.

**Simultaneous multi-storm monitoring**, e.g. panes {1,2,3} sharing `Location`+`Zoom` on Storm A
(showing reflectivity/velocity/CC respectively — `Product` stays ungrouped per pane) while pane
4 independently tracks Storm B: this falls directly out of the per-channel model with no special
casing. A pane must never stay synchronized on a channel merely because it once was — leaving a
group on a channel is a first-class, always-available action, not a global "unlink everything."

**`SelectedStorm` channel — scope note:** NEXRAD Level 3's Storm Tracking Information message
(STI, already parsed by `wxdata/wsr88d/rpg/`, see §2) is the natural backing data for "selected
storm" once storm-cell identification/tracking is wired into a pane. Phase 1 should build the
`SelectedStorm` channel's sync plumbing (so the architecture is future-proof, per the reviewer's
explicit ask), but a pane's "selected storm" is only meaningfully populated once STI-based storm
selection exists in the UI — treat that UI as part of Phase 1's scope alongside the sync
plumbing, not deferred to a later phase, since STI parsing is already available for free from
`wxdata`.

### 4.2 Feedback-loop prevention

Extend `ShouldApplyLinkedMapParameterSync`'s guard (only propagate on a genuine external trigger)
per channel, and make the *origin* of a change explicit: `UserInput`, `ProgrammaticSync`,
`DataDriven` (e.g. an animation timer advancing `Time`), or `FollowPropagation`. A channel update
only fans out to grouped/following panes when its origin is `UserInput` or an explicit one-shot
apply action — never when the update was itself the result of an incoming sync (standard
reentrancy guard: a pane applying an incoming synced value suppresses its own outgoing signal for
that application). This must hold per-channel independently so, e.g., a `Time` sync tick can
never cascade into a `Location` change or vice versa.

### 4.3 Unified first-class map objects (markers, measurements, drawings, range rings, annotations, storm tracks)

The current app already has real precedent here, confirmed during review — not a from-scratch
ask: `map_annotation_types.hpp`'s `MapAnnotationObject{id, payload, style}` with a payload
variant over `Polyline`/`Circle`/`Rectangle`/`Measure`, all geo-anchored via `common::Coordinate`
(see §2). Separately, `marker_manager`/`marker_types.hpp` implements simple named/iconed markers
as an unrelated, globally-scoped system. The new app unifies both into one `MapObject` family
(`app/source/wxlens/objects/`):

- **Type:** `Marker | Line | Drawing (freehand/polyline) | Measurement (point-to-point,
  radar-to-point, or multi-segment path) | Polygon | Circle/RangeRing | TextAnnotation |
  StormTrack`.
- **Geometry:** always geographic coordinates (`common::Coordinate`), never screen-space, so
  objects stay correctly positioned as maps pan/zoom/rotate — extends the principle
  `MapAnnotationObject` already follows.
- **Style, optional label, optional persistence** (session-only vs. saved to the structured
  config store from §3.2).
- **Scope** (new field, the reviewer's core ask): `CurrentPaneOnly | SyncGroup(channel,
  groupId) | SameLocation | AllPanes`. Resolution when rendering into a given pane: an object is
  visible if its scope is `AllPanes`, or `CurrentPaneOnly` and it's that pane, or `SyncGroup` and
  the pane shares that group on that channel (`Location` is the natural default channel for
  "linked panes" scope), or `SameLocation` and the pane's current center matches the object's
  origin pane's center within a small tolerance.
- **Rendering:** a dedicated `MapObjectsLayer` composited above the data layers in every pane
  (base map → radar → satellite → model → warnings → storm/report layers → **User Analysis
  Layer**), independent of which meteorological renderer is active underneath — this is the
  reviewer's explicit layer-stack ask, and it's what lets the same marker/measurement be visible
  across compatible panes regardless of what product each is displaying.
- **Store:** a `MapObjectStore` C++ singleton, `QAbstractListModel`-backed for QML, analogous to
  today's `marker_manager`/`map_annotation_model` but unified and scope-aware — one store, one
  object family, instead of parallel marker/annotation systems.
- **Three-tier lifecycle (temporary → pinned → saved):** not every interaction should clutter the
  map with a permanent object. Distinguish: **(1) temporary interrogation** — e.g. dragging a
  measurement endpoint or hovering to probe a point — updates live, is never written to the
  `MapObjectStore`, and disappears when the interaction ends; **(2) pinned** — the user explicitly
  commits a temporary interrogation (or draws a marker/shape directly), creating a real
  `MapObject` with session lifetime; **(3) saved** — a pinned object the user explicitly persists,
  written through the structured config store (§3.2) so it survives restarts, optionally scoped
  to a saved pane-layout/workspace. Only tier (2) and (3) ever populate the `MapObjectStore`;
  tier (1) is pure UI state local to whatever tool is active. This keeps the map from filling up
  with clutter every time someone probes a location, per the explicit ask.
- **Default scope is a user setting, not a hardcoded constant.** The competing apps genuinely
  disagree here, and both are right for their users: RadarOmega draws on the one pane you drew
  in, while RadarScope shows the drawing across the group — which is the better default for the
  common analysis case, because it puts the same annotation over reflectivity *and* velocity
  *and* whatever else the group is showing, so you can see how the feature you outlined looks in
  each product at once. Other users find objects crossing panes actively unwanted. So the default
  belongs in the structured config store (§3.2), settable **per object kind** — drawings,
  measurements, markers and range rings do not have to share one answer — with the per-object
  scope control (§4.5) overriding it for a single object. Do not bake `CurrentPaneOnly` in as a
  constant; read it from settings with `CurrentPaneOnly` as the shipped default.

### 4.4 Measurement as a reusable interaction framework

Not limited to radar-site-to-click; the measurement tool supports several explicit modes, all
built on `util::geographic_lib`'s already-existing WGS84 geodesic math (`GetDistance`, `GetAngle`,
`GetCoordinate` — confirmed Qt-free and directly reusable, see §2):

- **Distance/bearing (one tool, not two modes):** click A, click B. Shows distance, forward
  bearing (`GetAngle(a,b)`), reverse bearing (`GetAngle(b,a)`), and both coordinates. Radar →
  point is **not** a separate mode: the radar site is a snap target (below), so starting a
  measurement near the site produces the radar-relative measurement automatically. Splitting
  these into two buttons forces the user to pick a mode before they know which one they want,
  and the underlying operation — a WGS84 geodesic between two coordinates — is identical either
  way. The *readout* adapts to what was snapped: with A on a radar site, display range/azimuth
  in the radar-native framing `radar_coordinate_table.cpp` already computes implicitly;
  otherwise display plain distance/bearing. Same tool, context-aware result — this is §5.3's
  progressive disclosure applied to measurement.
- **Snap targets (magnetic endpoints):** measurement endpoints snap to meaningful points when
  clicked or dragged within a tolerance. Implement this as a **snap-target registry**, not a
  radar special case, because the same behavior is wanted for several point populations: the
  pane's radar site (and other in-view sites), saved places (§4.9), existing `MapObject`
  vertices and centers, and later storm-track points. Three requirements that are easy to get
  wrong and expensive to retrofit:
  - **Tolerance is screen-space, not geographic.** A fixed-kilometer radius is enormous at
    continental zoom and invisible when zoomed into a single storm. Snap within N *pixels*,
    converted to a geographic tolerance per-frame from the pane's current scale.
  - **Snapping must be visible before it commits.** Highlight the target and show the endpoint
    jump to it while dragging. A silent coordinate rewrite reads as a bug, not a feature.
  - **User-configurable, and suppressible.** Tolerance lives in the structured config store
    (§3.2) — expose it as off/subtle/strong rather than raw pixel entry, per §5.3 — and a held
    modifier key places a point exactly, suppressing snap for one placement without a trip to
    settings. Both are standard in CAD and vector-drawing tools for the same reason: a magnet
    with no escape hatch becomes an obstacle the moment someone wants the point they actually
    clicked.
- **Multi-point path:** extend the existing `MapAnnotationMeasure{a,b}` payload into a path
  payload (`points: vector<Coordinate>`) with per-segment distance/bearing and a running total.
- **Which gesture starts a measurement is a user preference** (added 2026-08-24 from user
  feedback). Slice 7 shipped press-drag-release *and* click-then-click-again simultaneously, on
  the reasoning that neither habit should be punished. That is a sound default but a poor
  mandate: with both live, a click that does not move leaves a measurement half-started and
  waiting, so a stray click on the map arms something the user did not intend and the next click
  lands somewhere they were not aiming. Offer three values in the config store (§3.2) - drag
  only, click-click only, or both - defaulting to both. Note this preference only became
  *evaluable* once the drag path actually worked; see slice 8's grab-steal defect, which made
  click-click look like the only supported gesture.
- **One primary tool with long-press disclosure** (added 2026-08-31 from packaged-app feedback).
  The bottom control surface should show the user's preferred measurement/interrogation tool as
  the single primary action rather than permanently spending space on every variant. A normal
  click activates that tool; click-and-hold (with an equivalent keyboard-accessible menu action)
  opens the available measurement/interrogation modes and lets the user choose the new preferred
  default. Put a compact `?` help affordance beside the tool settings affordance: help explains
  the gestures and readout, while the gear deep-links to the stable `measurement` settings
  section per §4.5. Long-press is progressive disclosure, never the only way to discover or select
  a mode.
- **Tool-deactivation cleanup is a user preference** (added 2026-08-31 from packaged-app
  feedback). Some users expect turning the measuring tool off to clear everything created during
  that tool session; others use pinned measurements as continuing analysis. Add an explicit
  retain-on-deactivate versus clear-session-measurements setting, with the quick tool's gear
  deep-linking to it. Clearing must be scoped to measurements created by that activation/session
  and must never erase separately saved tier-3 objects. Keep live probes transient and preserve
  the temporary/pinned/saved lifecycle rather than making this a blanket `MapObjectStore` clear.
- **Point info:** a clicked point yields lat/lon, range/azimuth from the pane's currently
  selected radar site, and — once Phase 3's data layers exist — whatever satellite/model value
  is present at that point; Phase 1 delivers the radar-relative info only, with the tool designed
  so additional data probes plug in later without a rework.
- **Radar-value reader:** when point info interrogates a rendered radar product, show the decoded
  value and correct product unit (for example dBZ for reflectivity) together with distance and
  azimuth/bearing from the pane's selected radar site, identifying that site in the readout.
  Where available, also show the actual elevation cut and terrain-independent beam-center MSL;
  do not label beam angle as geographic bearing or imply AGL without terrain data. Treat the
  reader as a temporary live probe by default, with an explicit pin action if its result should
  enter the analysis-object lifecycle. The data comes from the active Data Product's probe
  contract, not from palette-color reverse lookup, so categorical and future non-radar products
  can supply honest typed values through the same UI.
- **Units:** driven by the existing global-unit-settings pattern (today's `unit_settings.hpp`/
  `types/unit_types.cpp`), extended to cover distance/bearing display preferences.

Use real geodesic calculations throughout (never flat screen-pixel math), so results stay
accurate at any zoom, pan, or projection.

### 4.5 Quick sync/object controls in pane chrome (not buried in Settings)

Each pane's chrome shows a compact, always-visible indicator of its sync state (e.g.
"Independent," "Following Pane 2," or "Linked: Storm A" with an expandable breakdown by
channel), plus one-tap actions available directly from the pane (toolbar or right-click menu,
successor to `map_pane_context_menu.cpp`): link to another pane, unlink, match location, match
camera, copy view to another pane, make independent, follow another pane. Advanced/less-common
configuration (custom channel combinations, naming a group) can live in a settings surface, but
the common actions must not require opening it.

The ordinary link menu must describe the effect rather than exposing raw channel names: **Map
view only** and **Map view + radar site** are the primary presets. It must visibly indicate when a
pane has a custom combination, and expanding that state shows whether `RadarSite`, `Product`,
`Palette`, and `Time` are linked. This keeps the frequent one-click path simple while making it
impossible for an expert to mistake camera linking for site linking.

**Every quick control links to the setting that governs its default.** The rule above gets you
from Settings to the pane; this is the other direction, and it is the half that is usually
missing. A quick control changes one object, one pane, one time — the moment a user finds
themselves setting it the same way repeatedly, what they actually want is to change the default,
and they should not have to go hunting through a settings tree to find where that lives. So each
inline control (scope, sync, units, product defaults) carries an affordance that opens the
settings surface **already scrolled to and highlighting the specific section** that sets its
default — not the top of Settings, and not a general "Preferences" page. This implies settings
sections need stable addressable ids from the start, so a control can deep-link to one; retrofitting
addressability onto a settings tree built without it is the expensive way to get here.

### 4.6 Pane/data-service architecture underneath the sync model

This is where the Data Source → Data Product → Visualization Layer → View pipeline (principle
#4, §0.1) becomes concrete. Do not give `PaneController` radar-specific fields directly — a pane
binds to a **View**, a View is backed by a **Visualization Layer** (a renderer registered for a
given product kind — radar today, satellite/model/sounding later), and a Visualization Layer
consumes a **Data Product** (e.g. "KTLX, Reflectivity, tilt 0.5°") sourced from a **Data Source**
service. Phase 1 implements exactly one Data Source (radar) end-to-end through this pipeline —
the generalization work is designing the seams, not building speculative satellite/model support
early (§0's "no premature later-phase tech" rule still applies to the *providers*, just not to
the *interfaces* they'll eventually plug into).

- **Per-site data sharing stays a service singleton, not a pane property:** port
  `RadarProductManager`'s pattern (`Instance(siteId)` singleton, shared product cache,
  Qt-signal update notification) into a new `RadarSiteDataService` in `app/source/wxlens/data/`,
  implementing the Data Source role for radar. Multiple panes showing the same site — synced or
  not on any channel — share one instance and one download/cache. Generalize this *pattern*
  (per-source-key singleton + shared cache) to the Phase 2/3 non-radar providers too
  (`MosaicDataService`, `SatelliteDataService`) as they're added, without changing the pipeline
  shape.
- **Pane grid model** (`app/source/wxlens/panes/`): a `QAbstractListModel`-backed `PaneGridModel`
  holding `N = gridWidth*gridHeight` `PaneController` instances, each owning its per-channel sync
  state (§4.1), a binding to its current Data Product (radar site+moment+tilt today, via
  `app/source/wxlens/products/`), and its own MapLibre camera state — a pane's "what data is
  showing" must be swappable independent of its camera state and independent of any other pane,
  unless explicitly grouped on that specific channel. Matches the user's stated need for 1×1 up
  through 3×3+custom grids.
- **Persistence:** extend the JSON round-trip approach from today's `map_pane_view_link_state`
  to the richer schema: `{"gridWidth":.., "gridHeight":.., "panes":[{"radarSite":.., "product":..,
  "syncGroups":{"Location":1,"Zoom":1,"Product":null,...}}, ...], "groups":{"1":{"channels":[...]}}}`
  (shape illustrative, finalize during Phase 1). Store under the structured config format (§3.2).
- **Per-pane popout/dock:** port `map_popout_frame.cpp`'s pattern to a dynamically created QML
  `Window` reparenting the same map render node.
- **Design this in from day one:** Phase 1's MVP builds directly on
  `PaneGridModel`/`PaneController` with the full per-channel model (a 1×1 grid is just the
  degenerate case with no groups), not a special-cased single view generalized later.
- **Audit note carried into Phase 1 and beyond:** watch for and eliminate any code that assumes
  "one pane = one radar" or "linked = every property moves together" — a pane is a flexible
  visualization workspace, not a fixed radar-site viewport, and sync is per-property by
  construction, not an all-or-nothing toggle.

### 4.7 Radar geometry & beam-height interrogation

Explicit requirement from the architecture audit: the Radar → Point measurement mode (§4.4)
should be able to answer "at this distance from the radar, approximately how high is the radar
actually sampling?" — not as a minor statistic, but as a first-class radar interrogation
capability.

- **Reuse, don't rebuild:** `util::geographic_lib::GetRadarBeamAltititude(range, elevation,
  height)` already exists in the current codebase (confirmed, see §2) and already avoids a
  flat-Earth approximation. Before wiring it into the new app, verify exactly what its `height`
  parameter represents (site ground elevation, antenna height, or a combined effective height)
  against real radar site metadata — **do not fabricate or guess missing metadata**; if a needed
  input isn't available from `wxdata`/`config::radar_site` (site lat/lon/elevation, antenna or
  effective height, selected elevation angle), that's a Phase 1 blocker to resolve, not a value
  to approximate silently.
- **Distinguish these terms explicitly, in both the model and the UI**, per the audit:
  - *Elevation angle* — degrees above the local horizon (the radar's selected tilt).
  - *Beam-center altitude* — MSL, computed via `GetRadarBeamAltititude` (or its ported
    equivalent).
  - *Terrain elevation* — MSL, from a DEM/terrain source (not available until a terrain provider
    exists — see §6's new terrain row, targeted for Phase 3+, not Phase 1).
  - *Beam-center height* — AGL, defined as `beam-center MSL − terrain MSL`. **Only compute and
    display this once real terrain data is available.** Phase 1 ships beam-center MSL alone; the
    UI must say "terrain data unavailable" rather than omitting the AGL field silently or
    substituting a guessed value — never imply an AGL number is authoritative without a real DEM
    behind it.
  - *Beam top/bottom* (vertical extent, using beam-width characteristics) — explicitly a later
    enhancement (§8 backlog); don't hard-code an assumption that the beam is an infinitely thin
    ray, so this can be added without a rework, but don't build it in Phase 1.
- **Interactive, not static:** as the user drags the Radar → Point measurement endpoint, the
  radar-geometry readout (range, azimuth, elevation angle, beam-center MSL, terrain MSL when
  available, beam-center AGL when available) updates live — this is a **temporary interrogation**
  per §4.3's tiering, not a persistent object, unless the user explicitly pins it.
- **Progressive disclosure:** the default measurement UI shows the simple range/azimuth/distance
  readout; the fuller radar-geometry breakdown lives in an expandable "Radar Geometry" section so
  the common case stays uncluttered (ties into §5.3).
- **Which rows the breakdown shows is a user setting, not a fixed list** (added 2026-08-24 from
  user feedback on the slice-8 readout). Collapsing the section behind a disclosure header
  declutters the *pane*; it does nothing about the seven rows inside it, and a user who only ever
  wants beam-centre MSL should not have to read past six other lines to find it. So the visible
  row set lives in the structured config store (§3.2) as a per-row toggle, defaulting to all on.
  **One constraint is not negotiable and must be enforced in the settings UI itself:** the terrain
  and beam-height-AGL rows default to *visible*, because this section's own rule is that the app
  must say "terrain data unavailable" rather than let a reader assume the MSL figure is an AGL
  one. Letting a user hide them after they have been told is fine; shipping them hidden is the
  silent omission this section exists to prevent.

### 4.8 Acceptance criteria for this architecture

Before Phase 1 is considered to have correctly implemented this section, verify it supports: (1)
four panes fully camera-linked; (2) four panes sharing location with independent zoom; (3) two
panes tracking Storm A while two independently track Storm B; (4) three panes linked, one fully
independent; (5) a pane leaving a group without affecting the others; (6) location linked
independently of product; (7) time linked independently of camera; (8) a marker scoped to one
pane, a sync group, same-location panes, or all panes; (9) point-to-point measurement anywhere on
the map; (10) radar-to-point measurement; (11) multi-segment path measurement; (12) measurements
and annotations staying geographically anchored through pan/zoom; (13) no synchronization
feedback loops; (14) common sync actions available directly from pane chrome; (15) advanced sync
configuration available without cluttering the default interface; (16) Radar → Point interrogation
shows range/azimuth/elevation-angle/beam-center-MSL live while dragging, without creating a
permanent object until explicitly pinned; (17) beam-center AGL is never displayed as if
authoritative when no real terrain data backs it; (18) a pinned measurement can be promoted to a
saved, persistent object independent of the temporary-probe interaction that created it.

### 4.9 Saved places (personal locations) — a distinct population from analysis objects

§4.3 unifies markers, drawings and measurements into one `MapObject` family, which is correct as
*architecture*. But it flattens two populations users think about completely differently, and
slice 6 shipped only the first:

- **Analysis objects** — drawn during an event, about a storm, mostly session-lifetime, scoped to
  a pane or a sync group. "Circle this couplet." These are what §4.3's temporary → pinned tiers
  describe, and what the slice 6/7 tools currently produce.
- **Saved places** — durable personal context with nothing to do with the current storm, wanted
  in *every* session: home, family, friends, the kids' school, work. Nobody should have to
  re-place these, and they belong on every pane by default, because the question they answer
  ("where is this relative to people I care about?") does not change when the product on screen
  changes.

One store and one object family still serves both — do not build a parallel system, that is the
exact mistake §4.3 exists to undo. What differs is defaults, lifecycle, and management surface:

- Saved places are **always tier-3 (saved)** — created persistent, never session-only.
- Their **default scope is `AllPanes`**, not the `CurrentPaneOnly` default §4.3 sets for drawings.
- They need a **management surface** analysis scribbles do not: a searchable list, rename/edit/
  delete, and import/export (same shareable-plain-file rationale as `.pal` and themes,
  §5.1/§5.2) so places move between machines or get handed to someone else.

**Groups with color coding.** Places want a taxonomy, not just a per-object color swatch: the ask
is to color-code *family* vs. *friends* vs. *work*, which means a named group carries a color and
its members inherit it, so recoloring a group recolors everything in it. Support both a group
color and a per-place override, and let groups do double duty as:

- **visibility toggles** — "show family only" during an event affecting one area, without
  deleting anything;
- **snap-target sets** for §4.4's magnetic endpoints, so "measure from my house to the hook echo"
  is a two-click operation.

A reusable color-picker control is a prerequisite. Slice 9's palette editor builds one for `.pal`
stops, so this work follows it and reuses that component — but note the *widget* is shared while
the *systems* stay separate per §0's rule: `.pal` palettes, UI themes, and place colors are three
independent things that happen to need the same picker.

### 4.10 Radar site markers — static reference data, not a new object family

The bundled site database is already the app's most-used geographic fact and its least visible
one. Choosing a site means opening a searchable list and knowing the four-letter ID or the city;
the map itself never shows where the radars *are*. That inverts the natural question. "Which
radar covers this storm?" is spatial, and the answer is on the map — one click away — as soon as
the sites are drawn on it.

Everything this needs already exists and must be reused rather than rebuilt:

- `data::RadarSites()` returns all 205 bundled sites with coordinates, altitude and time zone;
  `PaneGridModel::radarSites()` already publishes them to QML as a `CONSTANT` list.
- `PaneController::pixelForCoordinate` plus the `cameraTick` re-projection pattern in
  `MapObjectsLayer.qml` is how geographic things are anchored while each pane pans independently.
- `SnapTargetRegistry` already treats the pane's radar site as a screen-space target (§4.4), with
  the nearest-within-N-pixels rule this needs for hit testing.
- The `RadarSite` sync channel and its §4.2 origin guard already propagate a site change.

**One known data gap:** the loader drops the bundled JSON's `type` field, so `RadarSiteInfo` cannot
distinguish the 160 WSR-88D sites from the 45 TDWRs. The markers need that distinction to style
and filter them, so carry the field through the existing loader — do not read the JSON a second
time.

**This is reference data, not user analysis.** Sites are owned by the bundled database exactly as
Level 3 storm graphics are owned by their product. They must not enter `MapObjectStore`, gain
scope resolution, or become saved places. §4.3 exists to stop parallel object systems; it equally
forbids laundering static data through the object store to get it drawn. Concretely, the layer
sits **above** the radar/Level 3 product and **below** `MapObjectsLayer`, because a reference dot
must never occlude something the user drew.

**Selection must have exactly one implementation.** `SitePicker.choose()` currently sets
`paneController.sourceKey` and then conditionally calls `centerOn` based on
`appSettings.centerMapOnSiteChange`. A marker click needs identical behavior, and a second copy of
that two-step will diverge the first time either half changes. Lift it into one invokable on
`PaneController` and have the picker and the marker layer both call it. The default radar-site
scope preference (**All panes** / **Active pane only**) already promised in the Phase 1 settings
gate governs both entry points for the same reason.

**Density is the real design problem, and zoom is the answer.** 205 markers per pane across a 3×3
grid is 1,845 projected items, re-evaluated on every camera change — the wrong side of the line
`MapObjectsLayer`'s own comment draws about when Qt Quick items stop being the right tool. Two
rules keep it cheap and legible:

- **Cull to the viewport before instantiating.** Publish only sites projecting inside the pane
  (plus a margin) so the delegate count is typically under 25, not 205. Culling belongs in C++
  next to the database, not in a QML filter that runs after the items exist.
- **Disclose by zoom** (§5.3): dot only at continental zoom, dot + ID once sites are meaningfully
  separated, dot + ID + place name when zoomed into one. The pane's **active** site is drawn
  distinctly and labelled at every zoom, because that one is answering a different question.

Deliberately **not** in scope: a label-collision/decluttering algorithm. Zoom thresholds plus
active-site emphasis solve the legibility problem at a fraction of the cost, and a declutterer
added speculatively is a permanent maintenance burden.

**Clicks belong to the armed tool.** When an object tool or a measurement mode is active, a click
on a marker must place/measure, not switch sites — and the same site points already serve as §4.4
snap targets in that state, which is the behavior users actually want there. A click that hits no
site must fall through to the map, or panning and object placement break.

**Accessibility, honestly scoped.** 205 map-anchored tab stops would be worse than none. The
keyboard and screen-reader path for choosing a site stays `SitePicker`, which is already
labelled and operable; markers are a pointer affordance and an at-a-glance display. Give the
layer one accessible summary, and do not fabricate per-marker focus.

**Visibility lives with the other overlays.** A "Radar sites" toggle (and a separate TDWR filter,
since many users want the 45 terminal radars off) belongs in the existing Weather Overlays
surface, persisted like any other preference — which makes it part of the Phase 1 settings
coverage gate rather than a runtime-only switch.

---

## 5. Palette/theming system design

Two distinct systems, kept distinct like the current app does:

### 5.1 Radar/data-layer color palettes (`.pal` files) — preserve format, rewrite editor

- **Engine: no change.** [common/color_table.cpp](wxdata/source/scwx/common/color_table.cpp)
  already parses/renders `.pal` and is already Qt-free — link it into the new app unmodified.
  This gives Phase 1 full `.pal` compatibility (including every palette already shared by the
  Supercell Wx community) for free.
- **Editor: rewrite as QML, keep the design principle.** Port
  [palette_file.hpp](scwx-qt/source/scwx/qt/util/palette_file.hpp)'s model (editable "stops" list,
  verbatim-preserve everything else, save-as-only) to a `QAbstractListModel`-of-stops with a QML
  gradient-editor view (drag stops on a gradient bar, per-stop color picker, live preview).
  **Keep "preview through the real `ColorTable` parser" as a hard rule**, not just an
  implementation detail — it's a correctness guarantee that what you preview is what renders.
- **Palette picker:** port the quick popover picker concept to a QML popup; bundle the same
  default set (`res/palettes/wct/*.pal`), carrying the NOAA WCT attribution forward unchanged.
- **Sharing:** `.pal` stays a plain text file, no format change, so existing community palettes
  and sharing workflows keep working unmodified — directly serves "preserve/improve, don't
  regress."

### 5.2 App chrome theme (new capability — closes the identified gap)

- A `ThemeManager` C++ singleton (`app/source/wxlens/theme/`) exposes named color/metric roles
  (background/surface/elevated-surface, primary/accent, danger/warning/success,
  text-primary/secondary, border, per-product accent colors, corner radius, spacing scale) as
  `Q_PROPERTY` values bound throughout QML — changing the active theme live-updates the whole UI
  via Qt's property binding system. This is the mechanism that finally delivers real
  "CSS-capable" theming, which native `QPalette`/`QStyle` could not.
- Persist themes as plain structured files (same TOML/JSON rationale as §3.2), versioned and
  shareable exactly like `.pal` files — directly serves "genuinely open source so end users can
  customize/fork it."
- Style Qt Quick Controls 2 with a fully custom style (not `Fusion`/`Material`/native) driven by
  `ThemeManager` roles, so no native-widget look leaks through — this is what makes a
  RadarOmega-ish "open face," dense-but-modern aesthetic achievable.
- Bundle at least two built-in themes at Phase 1 exit: a dark "operational" theme (low glare,
  radar-viewing-optimized) and a light theme, both using the same shareable format the user
  would use to build their own.
- **Trademark/trade-dress discipline:** use RadarOmega's open-face layout density and
  RadarScope/other weather apps as *layout/information-density* inspiration only — original
  iconography, color choices, and typography throughout. Do not copy any competitor's logo,
  exact color ramp, or distinctive UI chrome shape 1:1.

### 5.3 UI personas & progressive disclosure

The user's dual goal — approachable for a newcomer, dense/powerful for an enthusiast — is not
solved by picking one or the other, and not by maintaining two separate UIs. It's solved by
**progressive disclosure**: one interface, layered, where advanced capability is reachable but
never in the way by default. Two personas to design against throughout Phase 1, not as a
checklist item at the end:

- **Simple/approachable surface** (default state): clean chrome, the current product/site/time
  obviously visible, minimal visual intimidation, easy radar exploration with sensible defaults.
- **Professional surface** (revealed on demand): detailed radar-geometry/interrogation readouts
  (§4.7), full product controls, multi-pane synchronization controls (§4.5), advanced overlays,
  keyboard-driven workflows, and detailed analysis tools.

Concrete mechanism, established by example in §4.7: a simple default readout (e.g. plain
distance/bearing) with an expandable section for the professional depth (e.g. full radar
geometry) — apply this same pattern anywhere else the app has a simple/advanced split (palette
editing, pane sync configuration, settings). Never achieve "approachable" by removing
professional capability; achieve it by not surfacing it until asked for.

**Capability breadth must not become toolbar breadth** (reaffirmed by packaged-app feedback,
2026-08-31). WxLens is intended to compete with paid radar applications on capability while
remaining cleaner and easier to approach; adding a feature therefore does not automatically earn
it a permanent button. Treat persistent chrome as a constrained budget:

- Ship a deliberately minimal default set containing only frequent, broadly useful actions.
- Group related tools behind one preferred action, expandable picker, popover, or overflow menu;
  preserve direct shortcuts and search for expert workflows.
- Let users choose which optional controls appear in the primary toolbar, persist that choice,
  and provide a one-action reset to the curated default. Hiding a control changes presentation,
  never whether the underlying capability exists or whether it remains reachable through the
  complete tools surface, menus, search, or keyboard commands.
- Adapt at narrower windows and dense pane layouts by collapsing labels and moving lower-priority
  actions into overflow instead of shrinking every target or covering the radar panes.
- Require every proposed permanent control to justify its frequency, urgency, and advantage over
  contextual or progressive disclosure. Settings, setup choices, and rarely changed modes do not
  belong in the always-visible bar merely because implementation made a button convenient.

Toolbar customization is for personal workflow, not a second incompatible UI persona. Keep one
information architecture and one curated default so documentation, support, accessibility, and
keyboard behavior remain coherent.

### 5.4 Primary control surface placement — bottom, not the left rail

Slice 1 put tools in a left `SideRail.qml` as a direct successor to
`radar_toolbox_rail_widget.cpp`. That inheritance is not a reason to keep it. The user's stated
preference — and the stronger ergonomic argument — is a **bottom-centered control cluster**, with
the left edge reserved for what side rails are genuinely good at: navigation and settings entry
points, not frequently-hit tools.

Reasons this is more than taste:

- **A left rail taxes every pane column; a bottom bar taxes the layout once.** In a 3×3 grid the
  rail's width comes out of the whole grid's horizontal budget, and radar panes are what gets
  squeezed. On a 16:9 display horizontal space is already the scarcer axis.
- **Bottom-center is a shorter trip from the map.** The pointer spends its time over imagery in
  the middle of the screen; the bottom edge is nearer that resting position than the far-left
  edge, and unlike a left-edge trip the distance does not grow as the window gets wider.
- **It is where the rest of the bottom furniture is going anyway.** Archive/time controls
  (slice 11) are conventionally a bottom scrubber, and playback and tools belong in the same
  reachable zone.

**Design the bottom as one zone, not two independent bars.** The time scrubber and the tool
cluster both want the bottom edge; discovering that after building one of them is how you end up
with two stacked bars eating a third of the window. Lay out both before either ships.

**Floating vs. docked is a real trade, and the resolution is a setting.** A floating cluster
(inset above the bottom edge, map running full-bleed underneath) looks modern and costs no
layout space, but it *occludes* part of the bottom-center pane — which in a 3×3 grid is a real
pane, not dead space — and it gives up edge targeting: a screen edge stops the pointer for you,
while a floating bar has to be aimed at. A docked bar inverts both. Ship floating as the default
per the user's preference, with idle fade to limit the occlusion cost, and make dock-to-bottom a
setting rather than relitigating it.

### 5.5 Iconography — layout glyphs, not numerals

Slice 1 labels controls with text characters standing in for icons: `1`/`2`/`4`/`9` for grid
sizes, `↔`/`◄`/`⋯` for measurement modes, `1`/`G`/`A` for object scope. These are placeholders
and must not survive the UI pass.

The grid-size labels are not merely ugly — they are **less expressive than the model behind
them**. `PaneGridModel` takes a width *and* a height, but a single numeral cannot distinguish
1×2 from 2×1, so a vertical two-pane split is currently unreachable from the UI even though the
model supports it. A glyph showing the actual arrangement (the split-pane idiom used by tiling
window managers, terminal multiplexers, and video editors) closes a functional gap, not just an
aesthetic one, and scales to asymmetric layouts no numeral can name.

Likewise, measurement should read as a **ruler**. The current arrow glyphs say "direction" where
the tool means "measure."

Per §5.2's trade-dress discipline the icon set is original artwork. Treat it as one deliverable
covering layout glyphs, tool icons, and scope indicators together, so the set is coherent rather
than accumulated one control at a time. The user is supplying reference sketches for the layout
glyphs (§9).

---

## 6. Data source strategy

Concrete, free, non-paywalled sources for each new layer, so Phase 2/3 agents have a real
starting point instead of "add satellite support":

| Layer | Primary source | Access pattern | Notes |
|---|---|---|---|
| **Multi-radar mosaic** (Phase 2) | NOAA MRMS | `noaa-mrms-pds` S3 bucket, AWS Open Data (free, unauthenticated — same pattern already used for NEXRAD in `wxdata/provider/aws_*`) | GRIB2 format, needs a GRIB2 decoder (evaluate at Phase 3 kickoff, see below — actually needed a bit earlier for Phase 2 too). National reflectivity/echo-tops/precip mosaic directly serves "see whole storm systems across site boundaries." |
| Mosaic bootstrap | IEM MRMS tile/WMS endpoints (`mesonet.agron.iastate.edu`) | Same operator `wxdata`'s `iem_api_provider.cpp` already talks to for archived warnings | Pre-rendered tiles are a faster first cut than raw GRIB2 if the decoder dependency is deferred. |
| **Satellite imagery** (Phase 3) | NOAA GOES-16/18/19 | `noaa-goes16`/`-goes18`/`-goes19` S3 buckets, AWS Open Data (free) — ABI L1b or L2 CMIP/MCMIP, NetCDF4 | Needs a NetCDF4 decoder plus geostationary-to-web-mercator reprojection. |
| Satellite bootstrap | SSEC RealEarth or NOAA nowCOAST WMS/ArcGIS image services | Pre-rendered PNG/WMS tiles | Faster first cut, same bootstrap-vs-raw tradeoff as mosaic. |
| **Soundings** (Phase 3) | University of Wyoming upper-air archive | Plain HTML/text tables, no auth | Simplest to parse among sounding sources. |
| Soundings alt | `rucsoundings.noaa.gov` (RAOB text) | Plain text, no auth | Real-time-oriented complement to Wyoming's archive. |
| **Jet stream / pressure** (Phase 3) | NOAA NOMADS (GFS/RAP/HRRR) | GRIB2, free, unauthenticated HTTP | 250mb wind = jet stream, MSLP = surface pressure; same GRIB2 decoder as mosaic serves both. |
| Overlay bootstrap | NOAA nowCOAST pre-rendered tiles | WMS/REST | Same bootstrap-first pattern. |
| **Terrain/DEM** (Phase 3+, feeds §4.7's beam-center AGL) | Mapzen/Terrarium elevation tiles | `elevation-tiles-prod` S3 bucket, AWS Open Data (free, unauthenticated) — PNG-encoded elevation raster tiles, standard web-map tile scheme | Not needed for Phase 1 (beam-center MSL ships without it, per §4.7 — AGL waits for this). Chosen over a full GIS/DEM stack (e.g. raw USGS 3DEP or SRTM rasters) because it's already tiled and web-map-ready, keeping the "modest laptop" footprint reasonable; revisit if coverage/resolution proves insufficient for beam-height accuracy needs. |

**New dependency to evaluate at Phase 2/3 kickoff (not Phase 0/1), license-checked per §0:** a
GRIB2 decoder — **eccodes** (ECMWF, Apache-2.0) or **NCEPLIBS-g2c** (NOAA, permissive) — and a
NetCDF4 reader for GOES (**netcdf-c**, MIT-style, or a narrower purpose-built reader if the full
library is too heavy for the "modest laptop" constraint).

**Velocity improvements (Phase 3 sub-track, lower priority per the user):**
- **Dealiasing:** standard region-based 2D dealiasing (the algorithm family behind Py-ART's
  `region_based` dealiasing). Port the published *algorithm*, not code — verify Py-ART's exact
  license (BSD-3) before referencing its implementation at all, and prefer working from the
  published algorithm/papers to stay clean. Operates purely on already-parsed Level 2 radial
  velocity from `wxdata` — self-contained, no new data source.
  Fits under `app/source/wxlens/render/` or a new `app/source/wxlens/velocity/`.
- **Direction-relative color mode** (the user's own idea, explicitly lower priority): instead of
  toward/away-from-radar sign, compute true wind direction (radial velocity sign + azimuth) and
  map compass bearing to a perceptually-uniform circular hue. **`hsluv-c` is already vendored in
  this repo** (`external/`, MIT) — its cylindrical hue dimension is exactly suited to a circular
  0–360° compass colormap (e.g. north-moving red, south-moving green, as the user suggested)
  without perceptual banding. Worth calling out since it's a nonobvious existing asset.

---

## 7. Phase breakdown

Sizes are relative AI-agent-execution effort: **S**mall / **M**edium / **L**arge / **XL**arge.

### Phase 0 — Project scaffolding, wxdata integration, repo map/docs
**Goal:** stand up the new repo so Phase 1 can start writing UI code immediately, with `wxdata`
linked and building, and agent-legible docs in place.
**Scope:** empty-but-buildable Qt Quick app (window + a MapLibre map, no radar overlay yet);
`wxdata` linked with its test suite passing in the new repo's CI; `AGENTS.md`/`CLAUDE.md`/
`docs/adr/` in place; CI matrix green on Windows/Linux/macOS.
**Key technical work:**
- [x] Resolve `wxdata` reuse mechanics (§3.1) — Option A, done. See `docs/adr/0002-wxdata-reuse-strategy.md`:
  `external/legacy-supercell-wx/` (shallow submodule), only `wxdata/` built; discovered along the
  way that `wxdata` also needs `aws-sdk-cpp`/`date`/`units`/`hsluv-c` vendored (not pure-Conan as
  originally assumed) — those four are now vendored at WxLens's own `external/` too.
- [x] Stand up `CMakeLists.txt`/`CMakePresets.json`/`conanfile.py`/`tools/` by copying and adapting
  the current repo's build tooling. Presets currently cover `windows-vs2026-x64` (verified against
  the local dev machine) and a `linux-gcc14` template (not yet verified on real Linux hardware —
  follow-up item, not done tonight).
- [x] Confirm MapLibre Native Qt's QML-item support (§1) — **resolved, see
  `docs/adr/0004-maplibre-qml-integration.md`**: it exposes a genuine `QQuickItem` (`src/quick`,
  BSD-2-Clause, no `QtLocation` dependency), no `QQuickWidget` fallback needed. WxLens vendors the
  upstream `maplibre/maplibre-native-qt` (library version 4.0.0) at `external/maplibre-native-qt`,
  not the `dpaulat` Supercell-Wx-specific fork.
- [x] Write `AGENTS.md`/`CLAUDE.md` for the new repo (§3.3) — done, not stubs, reasonably complete.
- [x] Wire up `util::Logger` with per-subsystem sinks (§3.4) — `app/source/wxlens/log/` wraps
  `scwx::util::Logger`, adding a file sink under `QStandardPaths::AppDataLocation`. Only one
  subsystem exists so far (`main`); extend the naming convention as more subsystems are added in
  Phase 1.
- [x] Decide TOML vs. JSON for config storage — **TOML**, see
  `docs/adr/0003-config-storage-format.md`. `settings/` itself is still an empty directory —
  implementing the wrapper is Phase 1 work (§7 Phase 1 doesn't need it until pane-layout/theme
  persistence), not Phase 0's.
- [x] App is named **WxLens** (repo `wxlens/`, C++ namespace `wxlens`, see §9 Q1/Q3 — resolved).

**Status as of 2026-08-22 (first overnight pass, verified end-to-end):** directory scaffold, all
vendored submodules, build tooling, and docs are in place; `docs/adr/0001`-`0004` written and
updated as real build issues were found and fixed (see below). `wxlens-app` (empty QML shell, no
MapLibre item wired in yet — that's explicitly the next slice, see ADR 0004's Consequences) and
`wxlens-wxdata-test` (the wxdata-only slice of the legacy GTest suite, referenced from
`external/legacy-supercell-wx/test/` rather than duplicated) are wired into the build.

**Conan install, CMake configure, and a full build were run to completion on the Windows VS2026
preset, and the result was launched and verified, not just compiled:**
- `conan install` + `cmake` configure succeed cleanly (Release config).
- `wxlens-app.exe` and `wxlens-wxdata-test.exe` both build and link.
- `wxlens-wxdata-test`: **185 passed, 12 skipped, 3 failed** (200 total). The 3 failures
  (`AwsLevel2DataProvider.Prune`, `IemApiProviderTest.ListTextProducts`/`LoadTextProducts`) are
  live-network/live-data tests hitting real AWS S3/IEM endpoints — the same category the legacy
  repo's own Cursor Cloud notes (`external/legacy-supercell-wx/AGENTS.md`) already flag as
  environment-dependent and excluded from their CI run. Nothing in the failure output points at a
  WxLens-introduced defect (see the git history around this date for the actual assertion text).
  Re-verify before assuming they're still purely environmental if this is re-run much later.
- `wxlens-app.exe` was launched for real (not just built): a genuine OS window titled "WxLens"
  appears, `Responding: True`, no crash, runs indefinitely until closed. Visual pixel content
  couldn't be screenshotted this session (the dev machine's session was locked, so screen capture
  only showed the lock screen) — functional launch is verified, pixel-level rendering isn't.

**Three real build issues were found and fixed this session** (each has a corresponding ADR
update with full detail — read those before touching the same areas):
1. `wxdata`'s actual dependency set is wider than initially assumed — needs `aws-sdk-cpp`, `date`,
   `units`, `hsluv-c` vendored too, each with their own (sometimes multiply-nested) submodule trees
   that must be initialized, not just top-level. See ADR 0002.
2. MapLibre Native Qt's QML plugin target (`MLN_QT_WITH_QUICK_PLUGIN`) has a genuine upstream
   CMake bug (`CMAKE_SOURCE_DIR` misuse) that breaks under `add_subdirectory` consumption — worked
   around by disabling that specific target for now (the QQuickItem library itself still builds
   fine). See ADR 0004's second follow-up finding for the full writeup and unresolved options.
3. `wxdata.cmake` itself doesn't declare `Boost::timer`/`Boost::json` even though its source uses
   both (`boost::timer::cpu_timer`, `boost::json`) — invisible while `wxdata` is only compiled as
   an OBJECT library, surfaces only at final link time in whatever executable consumes it. Fixed
   by adding both to `app/CMakeLists.txt`/`test/test.cmake` directly (not editing `wxdata.cmake`).
   Also needed pinning `external/units` to the exact commit the legacy repo uses (ADR 0002) after
   a newer `units` release introduced an MSVC `/W4 /WX` warning-as-error in `wxdata`'s build.
4. `windeployqt` is not optional on Windows — without it, `wxlens-app.exe` can't find Qt's DLLs or
   the QML plugins backing even `QtQuick.Window` (`import QtQuick.Window` failed at runtime with
   "module ... is not installed" until this ran with `--qmldir` pointing at `app/qml`). Added as an
   automatic `POST_BUILD` step in `app/CMakeLists.txt`, not a manual step to remember.

**Still open:** CI workflow (`.github/workflows/ci.yml`) is written but has never actually been run
on GitHub Actions — treat it as an unverified draft, not a working pipeline, until it's been
exercised for real. The MapLibre QML plugin wiring (making `import MapLibre` actually work in
`Main.qml`) remains explicitly unresolved next-slice work per ADR 0004.
**Size:** M.

### Phase 0.5 — Capability & interaction-taxonomy audit
**Goal:** before Phase 1 implementation goes too deep, write down what already exists, what a
comparable professional tool (AWIPS) publicly documents as capability, and what this app plans —
so later phases have a stable taxonomy to extend instead of inventing one ad hoc per feature.
**Scope:** `docs/capability-matrix.md` containing (a) a capability matrix — current Supercell Wx
capabilities, publicly documented AWIPS capabilities, and planned `WxLens` capabilities,
side by side; (b) a **product taxonomy** (the kinds of Data Products the pipeline in §4.6 will
eventually carry — radar moments, satellite bands, model fields, soundings, etc., even though
only radar ships soon); (c) a **tool/interaction taxonomy** covering the map-interaction
vocabulary the app should eventually support without its architecture treating them as
afterthoughts: Pan, Zoom, Identify, Probe, Measure, Radar range/azimuth, Radar beam geometry,
Storm selection, Cross-section, Time series, Vertical profile, Draw, Annotate, Select, Compare.
**Key technical work:** this is a documentation/research deliverable, not code — research AWIPS's
publicly documented feature set (no proprietary/internal material), cross-reference against what
§4-§6 of this roadmap already plans, and flag gaps. This does **not** mean implementing all of
these tools in Phase 1 — it means Phase 1's architecture (§4) must not make any of them
second-class add-ons later. Cross-check §4's tool list (pan/zoom/measure/probe/draw/annotate are
already covered; cross-section/time-series/vertical-profile are not yet designed — note them as
Phase 3+ candidates once soundings/model data exist) and §4.6's product taxonomy against what
this audit finds.
**Status:** [x] Done — see `docs/capability-matrix.md`. Conclusion: no changes needed to §4's
architecture; the three gaps found (cross-section, time series, vertical profile) all depend on
Phase 3 data sources that don't exist yet, so they're correctly deferred rather than accidentally
precluded. One forward note for whoever designs the Phase 3 sounding/model Data Sources: revisit
whether a Data Product needs "sampled across time" or "sampled across a spatial cross-section" as
first-class query shapes.
**Size:** S.

### Phase 1 — Single-site modernized radar UI (near-term, executable phase)
**Goal:** ship a single-radar-site viewer that is a genuine visual/UX upgrade over the current
app while matching its current *capability* floor — linked/unlinked multi-pane, palette editing,
product switching, live + archived data. This is the phase meant to ship real user value first.
**Scope:**
- 1×1 through 3×3(+) pane grid with the full per-channel synchronization model (§4.1-4.2,
  §4.6) — not just a simple link boolean — working for same-site (e.g. reflectivity + velocity
  side by side) and different-site/multi-storm combos, satisfying §4.8's acceptance criteria.
- Unified `MapObject` model and store (§4.3) with scope resolution (current pane / sync group /
  same-location / all panes), rendered via a dedicated User Analysis Layer above the data layers.
- Measurement tool (§4.4): point-to-point, radar-to-point, and multi-segment path modes, using
  real geodesic math, plus basic markers/drawings/range rings as `MapObject` types.
- Quick sync/object controls in pane chrome (§4.5) — link/unlink/follow/match/copy-view actions
  available without opening Settings.
- Full NEXRAD Level 2 + Level 3 product coverage via `wxdata`, rendered through the ported
  custom-OpenGL-layer draw classes.
- Live data (site auto-refresh) + archived data browsing (site + time picker).
- `.pal` palette editing + quick-picker (§5.1), bundled default palette set.
- App chrome theme system (§5.2) with at least two bundled themes, applied throughout — this is
  where the "drastically modernized, RadarOmega-ish open face" goal actually lands.
- Warnings/alerts overlay (reuse `wxdata/awips/` text-product parsing; port alert-layer
  *behavior*, not code).
- Placefile overlay support (reuse `wxdata/gr/placefile.cpp` unmodified; port rendering
  behavior).
- Settings UI (product defaults, unit preferences, map provider choice, per-object-kind default
  scope per §4.3) backed by the new structured config format, with addressable sections so the
  inline quick controls can deep-link into it (§4.5).
- Structured logging live throughout, not bolted on after.
**Implementation approach — small, independently verifiable vertical slices, not one giant
build:** Phase 1's *architecture* (§4-§5) is designed complete up front, but an AI coding agent
should never be handed "build Phase 1" as one task — that invites giant diffs touching dozens of
unrelated systems at once, which is hard to review and hard to recover from when something's
wrong. Each numbered slice below should be its own build-and-test-passing unit of work before the
next starts:

1. **Application shell** — empty QML app window (builds on Phase 0's scaffold) with the chrome
   frame in place but no real data yet.
2. **One functional radar pane** — a single hardcoded pane fetching and displaying one product
   from one site end-to-end (`RadarSiteDataService` from §4.6, wired to `wxdata`'s existing
   providers) — proves the full data path before any multi-pane complexity is added.
3. **Radar rendering** — port `gl/draw/` to the Qt RHI custom render node (§1) and expand to full
   NEXRAD Level 2 + Level 3 product coverage.
4. **Pane grid model** — generalize slice 2's single hardcoded pane into `PaneGridModel`/
   `PaneController` (§4.6) supporting 1×1 through 3×3(+) grids, still with no cross-pane
   synchronization yet (each pane fully independent).
5. **Synchronization channels** — layer in the per-channel sync model (§4.1-4.2) on top of the
   now-multi-pane grid from slice 4.
6. **Map objects** — `MapObjectStore`/`MapObjectsLayer` with scope resolution (§4.3), starting
   with markers/drawings/range rings; three-tier temporary/pinned/saved lifecycle.
7. **Measurement framework** — point-to-point, radar-to-point, multi-segment path modes (§4.4),
   built on the map-object infrastructure from slice 6.
8. **Radar geometry** — beam-height/geometry interrogation (§4.7), extending slice 7's
   Radar → Point mode; reuse `GetRadarBeamAltititude` per §2/§4.7.
9. **Palette system** — QML palette editor + picker (§5.1), reusing `common::ColorTable`
   unmodified.
10. **Theme system** — `ThemeManager` + at least two bundled themes (§5.2), applied across
    everything built in slices 1-9.
11. **Archive/time controls** — time picker + archived-data browsing wired to `wxdata`'s existing
    AWS/HTTP providers (no new provider code needed).
12. **Warnings/placefiles** — alert overlay (ported behavior from `alert_layer`) and placefile
    overlay (`wxdata/gr/placefile.cpp`, unmodified).
13. **Multi-pane polish + acceptance validation** — quick sync/object controls in chrome (§4.5),
    UI/UX pass checked against RadarOmega/RadarScope for layout-density inspiration only, then
    validate the whole phase against §4.8's acceptance criteria. Include a persisted **Map
    details** surface shared consistently across panes: built-in `Operational`, `Minimal`, and
    `Detailed` presets plus grouped visibility toggles for roads, city/town labels, boundaries,
    buildings, points of interest, water labels, and terrain/hillshade where the active style and
    source support them. `Operational` is the shipped default and should preserve geographic
    context (boundaries, major roads, cities, water) without letting minor roads, buildings, or
    POIs compete with radar data. Keep these controls independent of the chrome theme and the
    dark/light basemap choice added in slice 10. Arbitrary MapLibre style-JSON import is a later
    enhancement requiring validation and safe fallback; Snazzy Maps JSON targets Google Maps and
    is not directly compatible, so any partial converter is explicitly later work rather than
    slice-13 scope.

**Added 2026-08-23 from user feedback on the slice-7 shell.** These are numbered after 13 only
because renumbering breaks cross-references; each names where it actually belongs in the
sequence, and none of them is a tail-end nice-to-have:

14. **Unified measurement + snap targets** (§4.4) — collapse the separate Point→Point and
    Radar→Point modes into one tool backed by a screen-space snap-target registry (radar sites,
    saved places, existing object vertices), with configurable tolerance and a suppress modifier.
    **Runs with or immediately after slice 8**, which is already inside the measurement and
    radar-geometry code; the saved-places snap source lights up when slice 15 lands.
15. **Saved places** (§4.9) — persistent personal locations with color-coded groups, a
    management surface, and import/export. **Runs after slice 9** so it reuses that slice's color
    picker. Extends slice 6's `MapObjectStore`; does not add a parallel store.
16. **Control surface relocation** (§5.4, §5.5) — move the primary tool cluster from the left
    rail to a bottom-centered floating bar, with the original icon set replacing the placeholder
    numerals and arrows. **Runs with slice 11** so the time scrubber and tool cluster are laid
    out as one bottom zone, and **after slice 10** so it is built against `ThemeManager` roles
    rather than inheriting the hardcoded hex literals now in `SideRail.qml`. The layout glyphs
    also unlock 1×2/2×1 grid arrangements the numeral labels cannot express (§5.5).
17. **Settings foundation** (§3.2, §4.5, ADR 0003) — the structured config store, typed
    accessors, and the settings surface itself, with **addressable section ids from the first
    commit** so inline quick controls can deep-link into the exact section that sets their
    default. **Runs next, before slice 14.** This is a gap in the original sequence, not a new
    idea: settings appears in Phase 1's scope list and is referenced by §3.2, §4.3, §4.4, §4.5,
    §4.6, §4.7, §5.1, §5.2 and §5.4, but was never given a slice of its own, so every slice
    since has deferred preferences into a layer nobody was scheduled to build. What is queued
    behind it: per-object-kind default scope (§4.3, currently a hardcoded constant the roadmap
    explicitly forbids), snap tolerance and its suppress modifier (§4.4 - **slice 14 cannot ship
    without this**), unit preferences (§4.4, currently worked around by showing km *and* miles),
    tier-3 `Saved` persistence (§4.3 - **slice 15 depends on it**), pane-layout/workspace
    persistence (§4.6), active theme and palette selection (§5.1/§5.2), floating-vs-docked control
    bar (§5.4), and the two preferences added 2026-08-24 above (geometry row visibility §4.7,
    measurement gesture §4.4). §4.5 also warns that "retrofitting addressability onto a settings
    tree built without it is the expensive way to get here" - and slices 5–8 have each shipped
    inline controls already, so that debt is being taken on now and paid later either way. Doing
    this before slice 14 stops the queue growing and unblocks 14 and 15 at the same time.

**Added 2026-09-04 from user request.** Numbered after 17 for the same cross-reference reason.

18. **Clickable radar-site markers** (§4.10) — draw the bundled radar sites on each pane's map and
    make clicking one select it for that pane. **Depends on nothing unbuilt:** the site database,
    per-pane projection, snap-target registry, `RadarSite` sync channel and settings store all
    exist, which is why this is a small slice rather than a subsystem. Its work is:
    - carry the bundled JSON's `type` (`wsr88d`/`tdwr`) through the existing loader into
      `RadarSiteInfo` — the one genuine data gap;
    - a C++ viewport-culling source that publishes only the sites projecting into a given pane,
      so the delegate count stays bounded (§4.10);
    - `Panes/RadarSiteLayer.qml`, following `MapObjectsLayer`'s `cameraTick` re-projection
      pattern, stacked above the radar/Level 3 product and below the user analysis layer;
    - a single `PaneController` site-selection invokable that `SitePicker` and the marker layer
      both call, so the `centerMapOnSiteChange` and default-scope behavior cannot diverge;
    - nearest-within-tolerance hit testing via `SnapTargetRegistry` rather than per-marker
      `MouseArea`s, with misses falling through to the map and armed tools keeping their clicks;
    - a persisted "Radar sites" visibility toggle and TDWR filter in the Weather Overlays surface.

    **Verification:** unit tests for viewport culling at several cameras, for nearest-site
    resolution with two near-coincident sites, and for marker-selection and picker-selection
    producing identical pane state; then a packaged visual pass at 1×1 and 2×2 with the retest
    driver, including the armed-tool case.

    **Sequencing — decide before starting.** This touches three things that are about to be
    validated: the settings coverage gate (a new persisted preference), §4.8 acceptance (a new
    pointer gesture and map surface), and the accessibility migration. Landing it *after* those
    gates close means reopening them, so the recommendation is to run it **before** the acceptance
    rerun, accepting that it adds scope to a phase we are trying to close. The alternative —
    deferring it to Phase 2 — keeps Phase 1's boundary clean at the cost of a second acceptance
    pass later. This is a scope decision for the project owner, recorded here rather than made
    silently.


Adjust ordering/granularity as real work reveals better seams — this sequence is a starting
structure, not a rigid contract — but keep the principle: each slice buildable and testable on
its own before the next begins.
**Size:** XL — intentionally the biggest phase, ships first per the user's priority order, and
now explicitly includes the full sync/object architecture rather than a simplified first pass —
delivered as the 13 slices above rather than one monolithic build.

**Status as of 2026-08-22 — Slice 1 (Application shell) done, verified end-to-end:**
- Resolved the MapLibre QML plugin blocker left open at the end of Phase 0
  (`docs/adr/0004-maplibre-qml-integration.md`'s "second follow-up finding"): confirmed with the
  user to take option 1 (a tracked patch applied at configure time, not the
  `ExternalProject_Add`/`find_package` standalone-build alternative). The actual upstream bug was
  `${CMAKE_SOURCE_DIR}` used instead of `${CMAKE_CURRENT_SOURCE_DIR}` in
  `src/quick/plugins/CMakeLists.txt`'s source list and include dirs; fixed via
  `external/patches/0004-mln-qt-plugins-cmake-source-dir.patch` (captured with `git diff` inside
  the submodule, then the submodule reverted with `git checkout --` so `external/` stays pristine
  in git), applied idempotently by `external/maplibre-native-qt.cmake` via
  `execute_process(COMMAND git apply ...)` guarded by a `git apply --check --reverse` idempotency
  check. `MLN_QT_WITH_QUICK_PLUGIN` is back to `ON`.
- Second, separate bug found while wiring the plugin into `wxlens-app`: the plugin's own
  `set_target_properties(... LIBRARY_OUTPUT_DIRECTORY/RUNTIME_OUTPUT_DIRECTORY ...)` call (generic,
  non-per-config properties) loses to WxLens's own `tools/wxlens_config.cmake`, which sets
  per-config `CMAKE_*_OUTPUT_DIRECTORY_RELEASE` globally - CMake applies those to every new
  target's per-config property *at creation time*, and a per-config property always wins over a
  later generic-property override on a multi-config generator (confirmed empirically: the actual
  `declarative_maplibre.dll` lands in `Release/lib` alongside every other library in the build,
  while its `qmldir`/`.qmltypes` - written via `configure_file`, unaffected by output-dir
  properties - stay at the plugin's own intended `.../src/quick/plugins/MapLibre/` location). Net
  effect: `$<TARGET_FILE_DIR:declarative_maplibre>` is **not** a reliable way to locate this
  plugin's QML module folder in this build. Worked around by having
  `external/maplibre-native-qt.cmake` capture the real, always-correct qmldir/qmltypes path
  directly as `WXLENS_MLN_QT_QML_PLUGIN_DIR`, and `app/CMakeLists.txt`'s `POST_BUILD` deploy step
  copies that directory *plus* the actual dll (via `$<TARGET_FILE:declarative_maplibre>`, which
  does correctly resolve to wherever the binary really landed) into
  `<wxlens-app exe dir>/qml/MapLibre` - mirroring exactly where `windeployqt` already places Qt's
  own QML modules (`QQmlEngine`'s default import path list includes `<app-dir>/qml`, confirmed
  during Phase 0, no `qt.conf` involved).
- Chrome shell built: `app/qml/Chrome/TopBar.qml` (app name + placeholder site/product/time text)
  and `SideRail.qml` (placeholder tool-icon slots, successor to `radar_toolbox_rail_widget.cpp`),
  composed in `Main.qml`. No `ThemeManager` yet (that's slice 10) - colors are hardcoded
  placeholders for now, consistent with not pulling later-phase work early.
- `app/qml/Panes/PaneHost.qml` hosts a real `MapLibre` QML item (pan/pinch/wheel handlers ported
  from the vendored `examples/quick-standalone/main.qml`), proving the full
  Qt Quick/QML → MapLibre Native Qt → GPU rendering chain end-to-end per §0.2's "prove the
  rendering seam early" rule - **not** yet a `PaneGridModel`/`PaneController` (slice 4), no radar
  data (slice 2+), no per-channel sync (slice 5). Basemap: OpenFreeMap (`docs/data-sources.md`) -
  free, no API key, real OSM detail, dark/light styles from one tile source; swapped in after the
  initial `demotiles.maplibre.org` placeholder proved too sparse for real use (user feedback).
  Not yet a real base-map *provider choice* setting (e.g. an optional user-supplied MapTiler key).
- **Verified for real, not just built:** `wxlens-app.exe` launched, stayed running, and a
  screenshot (both self-captured and one supplied directly by the user) confirms the chrome
  (top bar, side rail) renders correctly and the map renders actual basemap tiles (US outline,
  state/lake borders, labels), centered on CONUS as configured, after a few seconds for the
  network fetch - an earlier screenshot taken too soon after launch showed a black pane before
  tiles loaded, which is expected load latency, not a bug. `wxlens-wxdata-test` still builds
  clean after these changes (not re-run in full this slice - nothing touched wxdata or its test
  wiring, so this was a build-only sanity check, not a full 200-test re-verification).
- **Not verified this slice:** pan/zoom/pinch interaction (handlers are wired per the ported
  example pattern, but not manually exercised), Linux/macOS (Windows-only session, as with Phase
  0), the CI workflow (still unexercised per Phase 0's status).
- **Next slice:** slice 2, one functional radar pane - wire `RadarSiteDataService`
  (`app/source/wxlens/data/`) to `wxdata`'s existing providers and get one hardcoded product from
  one site flowing end-to-end, still with no pane grid/sync yet.

**Status as of 2026-08-22 — Slice 2 (one functional radar pane) done, verified end-to-end:**
- `app/source/wxlens/data/radar_site_data_service.hpp/.cpp`: a deliberately minimal first version
  of the Data Source role for radar (§4.6) - per-site singleton (`Instance(radarSite)`), fetches
  the latest Level 2 volume via `wxdata`'s existing `NexradDataProviderFactory`/`AwsLevel2DataProvider`
  on a background thread (`scwx::util::async`, wxdata's shared `io_context` helper - reused, not a
  new thread pool), emits `LevelTwoDataLoaded`/`LoadFailed` back on the GUI thread. **Explicitly
  not** RadarProductManager's full port: no caching, no multi-product/elevation tracking, no
  refresh scheduling, no `Cleanup()` - those are follow-up work as the pattern matures (slice 3+),
  not deferred by oversight.
- `app/source/wxlens/data/radar_site_database.hpp/.cpp`: loads `res/config/radar_sites.json`
  (bundled, carried forward unmodified from `scwx-qt/res/config/radar_sites.json` - lat/lon/
  elevation/IANA time zone per site) for lookup by site ID. Also the intended home for the site
  lat/lon/elevation §4.7's beam-height work will need later - reuse this, don't rebuild it then.
- `app/source/wxlens/products/radar_product_status.hpp/.cpp`: a minimal, explicitly temporary
  bridge (`wxlens::products::RadarProductStatus`) exposing site/status text to QML via a
  `radarStatus` context property (`main.cpp`) - **not** the real Data Product layer, which arrives
  with `PaneGridModel`/`PaneController` in slice 4 and should supersede this, not extend it.
  Displays elevation-scan/message counts and the volume start time in UTC, the radar site's own
  time zone ("Station"), and the local machine's time zone ("Local") - all three shown at once for
  now per user request; a real switchable *preference* (persisted, UI-driven) belongs to Phase 1's
  dedicated Settings work. Time zone lookups use `std::chrono::get_tzdb()`/`current_zone()`
  (native C++20 tzdb support confirmed present on this toolchain - no `date` library fallback
  needed here), matching the exact pattern already used in the legacy app's
  `qt/config/radar_site.cpp`.
- **Two real build/runtime issues found and fixed this slice** (beyond the feature work itself):
  1. `main.cpp` needed the shared `scwx::util::io_context()` actually started on a worker thread
     pool (with a `work_guard`, exception-recovering `run()` loop) and `Aws::InitAPI`/
     `Aws::ShutdownAPI` around the app's lifetime - ported directly from the legacy app's
     `main.cpp`. Neither existed before since nothing in `wxlens-app` made a network/AWS call
     until this slice's S3-backed provider.
  2. A real NOMINMAX/Windows.h `min`/`max` macro collision: `wxdata.cmake` sets `-DNOMINMAX`
     `PRIVATE` on `wxdata` itself (`external/`, read-only), so it doesn't propagate to consumers.
     `test/test.cmake` already had the same fix for the test target; `app/CMakeLists.txt` never
     needed it until this slice's `radar_product_status.cpp` became the first `wxlens-app` source
     to pull in `scwx::util::TimeString` (→ `date/tz.h` transitively). Fixed by adding the
     identical `target_compile_options(wxlens-app PRIVATE -DNOMINMAX)` under `if (MSVC)` -
     produces a cascade of unrelated-looking `units/core.h` template syntax errors if missed;
     worth recognizing the signature (`warning C4003: not enough arguments for function-like
     macro invocation 'min'` immediately preceding the cascade) if it recurs elsewhere.
- **Verified for real, not just built:** launched `wxlens-app.exe` repeatedly against the live
  NOAA `noaa-nexrad-level2` S3 bucket for KTLX - actual volumes loaded successfully each run
  (9729-9730 messages, 19 elevation scans), confirmed via both the log file and the rendered top
  bar text (screenshots supplied by the user). Station/Local time zones confirmed correct (CDT for
  KTLX/Oklahoma, EDT matching the user's own machine).
- **UX fixes made from live user feedback during this slice**, not just the core data-path work:
  - Mouse-wheel zoom was far too coarse (a full 2x/0.5x per wheel notch, ported as-is from the
    vendored example) - added a `wheelZoomSensitivity` property to `PaneHost.qml` (~19% per notch
    now), tunable if it still isn't right.
  - The map sometimes stayed blank for several seconds after launch before rendering (user
    initially saw this resolve only after scrolling). Investigated the MapLibre Native Qt render-
    trigger chain (`Map::needsRendering` → `QQuickItem::update`, `m_renderQueued` atomic-flag
    reset on every actual render) and found nothing wrong with it - the library's repaint
    signaling looks correctly designed. A later run showed the map finish rendering on its own
    with no interaction, just slower than usual, and the slow run coincided with a large
    concurrent Level 2 S3 download competing for the same network connection. Treating this as
    tile-load latency (occasionally worsened by concurrent large downloads), not a render bug -
    revisit only if it recurs without a plausible bandwidth cause.
- **Not verified this slice:** Level 3 products (only Level 2/reflectivity path exercised), sites
  other than KTLX, behavior when a site has no recent data or the network is unavailable (
  `LoadFailed` path exists and is wired but wasn't deliberately triggered), Linux/macOS.
- **Next slice:** slice 3, radar rendering - port `gl/draw/` to a Qt RHI custom render node and
  expand to full NEXRAD Level 2 + Level 3 product coverage. The data now flowing from this slice
  (`Ar2vFile`/elevation scans) is exactly what that renderer will consume.

**Status as of 2026-08-22 — Slice 3 in progress: custom-layer rendering seam proven, real radar
sweep rendering not yet built.** Per §0.2's "prove the rendering seam early" rule, this pass
proved the MapLibre *custom layer* rendering mechanism end-to-end on a trivial case before
attempting the full `view::RadarProductView`/`gl/draw/radar.*` port (~2000 lines across the
vertex-geometry and shader logic) - that full port is still ahead, not done this pass. Hardcoded
site switched from KTLX to KEAX (Pleasant Hill, MO / Kansas City) per user request; no
architectural significance, just a different demo site.

- `app/source/wxlens/render/radar_site_marker_layer.hpp/.cpp`
  (`wxlens::render::RadarSiteMarkerLayer`): draws one fixed-size colored point at a hardcoded geo
  coordinate via a real `QMapLibre::CustomLayerHostInterface` custom layer - proves registration,
  GL context access, and the lat/lon-to-screen projection (ported unchanged from the legacy app's
  `gl/radar.vert`, minus its `precision mediump float;` line - see below) all work through
  WxLens's actual QML-hosted map, not a synthetic test. Explicitly temporary/superseded once the
  real radar sweep renderer exists, not extended in place.
- `app/source/wxlens/render/radar_layer_controller.hpp/.cpp`
  (`wxlens::render::RadarLayerController`): minimal QML-facing bridge, `Q_INVOKABLE
  attachSiteMarker(QMapLibre::Map*, lat, lon)` called from `PaneHost.qml`'s `onStyleLoaded`.
  Same temporary status as the marker layer.
- **New dependency: glm 1.0.1** (Conan, MIT) - needed for the same MVP-matrix construction the
  real radar shader will also need; added now rather than twice.
- **Three more MapLibre Native Qt patches were required, on top of ADR 0004's original one** (full
  technical detail in that ADR's "Slice 3 findings" section - summary here):
  1. **Patch 0005** - `MapQuickItem` exposed no way to reach the underlying `QMapLibre::Map`,
     which registering a custom layer requires. Adds `Q_INVOKABLE Map* mapLibreMap()` (a raw
     QObject pointer, not the internal `shared_ptr` - crosses the QML plugin's DLL boundary
     cleanly without extra metatype registration), plus `mapReady()` and `styleLoaded()` signals.
     **Confirmed with the user before implementing** (same "flag before picking one" pattern as
     ADR 0004's original CMake bug decision) - chose the tracked-patch approach over rebuilding
     the map hosting in pure C++ around `QMapLibre::Core` directly.
  2. **Register custom layers on `styleLoaded()`, not `mapReady()`.** Confirmed by testing:
     `addCustomLayer()` calls made after the `Map` object exists but before its style finishes
     loading are silently dropped. Matches the legacy app's own `MapWidget::mapChanged`, which
     gates its equivalent `AddLayers()` call on `MapChangeDidFinishLoadingStyle` for the same
     reason.
  3. **Patch 0006 - a real, upstream bug**, found by live bisection with the user (disable the
     custom layer entirely → map renders fine; re-enable with both `initialize()`/`render()`
     reduced to complete no-ops → map still goes solid black): the Qt OpenGL backend's renderable
     `bind()` (`src/core/rendering/opengl_renderer_backend.cpp`) unconditionally cleared the
     framebuffer to opaque black. `bind()` isn't only called at frame start - mbgl's
     `DrawableCustomLayerHostTweaker::execute()` also calls it *mid-frame*, immediately after
     every custom layer's `render()`, to restore the FBO binding in case the host changed it. With
     that clear in place, having *any* custom layer registered (regardless of what it draws)
     wiped every layer mbgl had already drawn that frame, every frame. Root-caused by reading
     `vendor/maplibre-native/src/mbgl/gfx/drawable_custom_layer_host_tweaker.cpp` and
     `src/mbgl/gl/render_pass.cpp` - the latter already performs the correct pass-start clear
     immediately after `bind()`, which is what every other platform backend's `bind()` (Android/
     Linux/Windows GLX/EGL) already relies on instead of clearing itself. Fix: remove the clear
     from `bind()`, keep the FBO rebind + viewport set. All three patches are documented as
     upstream-candidate in ADR 0004; not filed upstream yet.
  4. **Also found:** the legacy shader's `precision mediump float;` line (an ES-only convention)
     is a hard syntax error on this desktop GL 3.3 core driver - omitted from WxLens's copy. Flag
     this when porting `gl/radar.frag` for real; don't carry that line over verbatim.
- **Verified for real:** with all three patches applied, the orange marker renders at KEAX's
  actual coordinates (confirmed against real Kansas City-area geography in a user screenshot),
  the base map renders normally alongside it, and `wxlens-wxdata-test` still builds clean.
- **Not verified this slice:** the actual radar sweep vertex/shader pipeline (not started - this
  pass was entirely the custom-layer plumbing prerequisite), Level 3, sites other than KEAX,
  Linux/macOS (these patches are platform-generic C++/GL fixes with no Windows-specific code, but
  unverified on other platforms regardless).
- **Next slice:** continue slice 3 - port `view::RadarProductView`'s vertex/color-table-LUT
  generation and the real `gl/radar.vert`/`gl/radar.frag` shaders (§2's Critical Files list),
  replacing `RadarSiteMarkerLayer` with the real radar sweep renderer now that the custom-layer
  registration mechanism underneath it is proven and unblocked.

**Status as of 2026-08-22 — Slice 3 complete: real radar reflectivity sweep rendering.** Replaces
`RadarSiteMarkerLayer`/`RadarLayerController::attachSiteMarker` (deleted, per that class's own
"superseded, not extended in place" doc comment) with an actual data-driven sweep renderer,
completing the Data Source → Data Product → Visualization Layer → View pipeline (§0.1 principle
#4) for one product on one site.

- `app/source/wxlens/products/radar_sweep_product.hpp/.cpp`
  (`wxlens::products::RadarSweepProduct`): the Data Product layer. Listens for
  `RadarSiteDataService::LevelTwoDataLoaded`, then ports `view::Level2ProductView`'s
  `ComputeCoordinates`/`ComputeSweep`/`UpdateColorTableLut` (level2_product_view.cpp) - WGS84
  geodesic per-gate lat/lon via the new `wxlens::util::GeodesicDirect` (GeographicLib), the
  triangle-quad/origin-fan vertex layout, the parallel data-moment buffer, and the color-table
  LUT - into an immutable `SweepData` snapshot (`std::shared_ptr<const SweepData>`, published
  under a mutex). Deliberately narrow, matching `RadarSiteDataService`'s own "minimal first
  version" precedent: hardcoded to Level 2 Reflectivity (`DataBlockType::MomentRef`), the lowest
  elevation cut, **no smoothing and no CFP** (both present in the legacy view but settings-driven
  features this slice doesn't have a settings layer for yet), no mouse-picking/tooltip support
  (that's the measurement/interrogation framework, §4.4, not this pass). Bundles
  `app/res/palettes/wct/DR.pal` (NOAA WCT, public domain, copied unmodified from
  `scwx-qt/res/palettes/wct/DR.pal` - the same default the legacy app uses for reflectivity) as
  its fixed color table, loaded via `QFile` + `ColorTable::Load(std::istream&)` since
  `ColorTable::Load(filename)` is a plain `std::ifstream` (wxdata is Qt-free) that can't see
  Qt-resource `:/...` paths directly.
- `app/source/wxlens/render/radar_sweep_layer.hpp/.cpp` (`wxlens::render::RadarSweepLayer`): the
  Visualization Layer, replacing `RadarSiteMarkerLayer` as the registered custom layer. Ported
  from `map::RadarProductLayer` (radar_product_layer.cpp): same MVP construction as the marker
  layer proved, two VBOs (vertices, data moments) + a `GL_TEXTURE_1D` color-table texture bound to
  `aLatLong`/`aDataMoment`/`uTexture`. Re-uploads GPU buffers only when
  `RadarSweepProduct::sweep_data()` returns a new pointer (a cheap `shared_ptr` identity check
  each `render()` call), not every frame. No CFP vertex attribute is ever enabled (location 2 -
  matches the legacy layer's own behavior when CFP data is absent, and `uCFPEnabled` is always set
  false).
- `app/res/gl/radar.vert`/`radar.frag`: ported unchanged from the legacy app's `gl/radar.vert`/
  `gl/radar.frag`, minus `radar.frag`'s `precision mediump float;` line - confirmed during the
  marker-layer proof-of-concept (see the "Slice 3 findings" ADR 0004 section above) to be an
  ES-only convention that's a hard syntax error on this desktop GL 3.3 core driver.
- **New dependency: GeographicLib 2.6** (Conan, MIT) - `wxlens::util::GeodesicDirect`
  (`app/source/wxlens/util/geodesic.hpp/.cpp`) wraps just `GeographicLib::Geodesic::WGS84().
  Direct()`, the one operation this slice needs, not a full port of the legacy app's
  `qt/util/geographic_lib.hpp` wrapper (`Inverse`/`GetDistance`/beam-height etc. belong to later
  measurement/beam-height work, §4.4/§4.8, and should extend this same file rather than
  reinventing it).
- **Not done this slice:** product/elevation selection (still hardcoded Reflectivity + lowest
  tilt - real selection is PaneController-driven, slice 4+), smoothing, CFP, mouse-picking/
  tooltips, Level 3, sites other than KEAX, Linux/macOS.
- **Two real bugs found by launching against live KEAX data**, neither visible from a clean build:
  1. **The sweep never appeared until the user interacted with the map** (a scroll/pan), then
     rendered perfectly. Radar data arrives asynchronously, long after the style loaded and mbgl
     drew its last frame, and *registering a custom layer does not make mbgl repaint on its own*.
     Fixed in `RadarLayerController::attachRadarSweep` by connecting
     `RadarSweepProduct::SweepUpdated` to `QMapLibre::Map::triggerRepaint()` (plus a one-shot
     catch-up call for the case where the data load beats the style load - that order is a network
     race, not guaranteed). This is the same class of problem as slice 2's "map blank until
     scroll" note, but a genuinely different cause, and it supersedes that note's "treat as tile
     latency" conclusion for *custom layer* content specifically: anything drawn by a custom layer
     needs an explicit repaint trigger when its data changes.
  2. **No multisampling**, so the sub-pixel-sized per-gate triangles point-sampled into a speckled
     sweep at wide zoom levels. Fixed in `main.cpp` with `QSurfaceFormat::setSamples(4)` before
     the `QGuiApplication` constructor, matching the legacy app's own
     `map_widget.cpp: surfaceFormat.setSamples(4)`. Still expected/unfixed: at very wide zoom
     (~zoom 4, full CONUS) individual 250 m gates are far smaller than a pixel, so the sweep
     necessarily thins out - a zoom-dependent decimation/overview strategy is real future work,
     not something MSAA alone solves.
- **Debugging note for future agents:** two GL-state hypotheses were tried and disproven along the
  way (back-face culling, and leftover depth/stencil state from mbgl's drawables renderer). Neither
  changed anything and both were reverted - `RadarSweepLayer::render` deliberately sets no more GL
  state than the legacy `RadarProductLayer::Render` does. Don't re-add them speculatively.
- **Verified for real, not just built:** launched against live KEAX data - the reflectivity sweep
  renders correctly positioned/projected over the base map, appears without any interaction, and
  is solid/correctly colored when zoomed in (screenshots confirmed by the user). `wxlens-app` and
  `wxlens-wxdata-test` both build clean.
- **Next slice:** slice 4, `PaneGridModel`/`PaneController` - the real multi-pane grid these
  bridging context properties/objects (`radarStatus`, `radarLayerController`,
  `radarSiteLatitude`/`radarSiteLongitude` in `main.cpp`) are explicitly temporary stand-ins for.

**Status as of 2026-08-22 — Slice 4 complete: the pane grid is real.** 1×1 through 3×3 (capped at
4×4), every pane fully independent. Per this slice's own scope, **no cross-pane synchronization**
was built - that's slice 5, layered onto a working grid rather than designed against a single-pane
special case.

- `app/source/wxlens/panes/pane_controller.hpp/.cpp` (`wxlens::panes::PaneController`): one pane =
  one View. Holds a `products::ProductDescriptor` and its own camera state - deliberately **no
  `radarSite` field**, per §4.6's audit note. `attachLayers(QMapLibre::Map*)` is the single place
  that dispatches on product kind, and is where a second data domain would branch.
- `app/source/wxlens/products/product_descriptor.hpp`: the generic `{kind, sourceKey, product}`
  value type that keeps the pane domain-agnostic. Deliberately *not* a class hierarchy - the point
  is that PaneController never names a radar site, not that speculative satellite/model types get
  modelled now (§0's no-premature-later-phase-tech rule).
- `app/source/wxlens/panes/pane_grid_model.hpp/.cpp`: `QAbstractListModel` of PaneControllers.
  Panes **persist across grid resizes** (growing appends, shrinking drops trailing panes), so
  resizing doesn't discard cameras/sites a user already set up.
- `RadarSweepProduct` gained a per-site `Instance()` singleton mirroring `RadarSiteDataService`'s
  pattern one level up the pipeline (§4.6): N panes on one site share one computed sweep instead
  of each recomputing ~2.2M identical vertices. It also now **triggers its own data load** rather
  than depending on `RadarProductStatus` having done so first - that construction-order coupling
  was fine for one hardcoded site and wrong the moment a second site is possible.
- `RadarLayerController` deleted - superseded by `PaneController`, per its own doc comment.
- QML: new `Panes/PaneGrid.qml`; `PaneHost.qml` is now bound to a `paneController` and holds no
  site/radar state of its own. Grid-size buttons (1/2/4/9) added to `SideRail.qml` purely so the
  grid is drivable - real pane chrome/quick controls (§4.5) are slice 5.
- **A fourth MapLibre Native Qt patch was required (0007), and it's the most severe yet** - full
  detail in ADR 0004's "Slice 4 finding". `MapQuickItem` connects the core `Map`'s
  `needsRendering`/`mapChanged` signals in `updatePaintNode()`, but constructs the `Map` and starts
  its style load earlier, in `initialize()`. A style that resolves fast - notably **the second map
  reusing the first's cached style** - finishes loading in that gap, so
  `MapChangeDidFinishLoadingStyle` is emitted with nothing connected and lost outright:
  `m_styleLoaded` stays false, `syncStyleChanges()` never runs, and `styleLoaded()` never fires.
  Every pane after the first therefore waited forever for a signal that had already been missed.
  Symptom was silent - base map fine, radar simply absent, no error; the log's missing
  "registering radar sweep layer" lines for panes 1-3 is what pinned it. Fix moves both connects
  to immediately after the `Map` is constructed.
- **Not done this slice:** all cross-pane sync (§4.1-4.2), per-pane site/product selection UI,
  pane popout/dock, persistence of the grid (§4.6's JSON schema). Bearing/pitch live on
  PaneController but nothing writes them yet - `MapQuickItem` exposes only `coordinate` and
  `zoomLevel` as QML properties, so observing rotation needs either another patch or the core
  `Map`'s own signals.
- **Verified for real, not just built:** launched, confirmed 1×1 renders KEAX centered on the site,
  then switched to 2×2 and confirmed all four panes independently registered layers and rendered
  radar (log + screenshot), with pane 0 keeping its camera across the resize. Separately stress-
  tested 13 consecutive grid resizes including repeated 3×3 → 1×1 shrinks: no crash, no QML errors.
- **Fixed during slice 4, found by a user-reported crash:** shrinking the grid destroys
  PaneControllers while their QML delegates are still tearing down, so `paneController` goes null
  for a moment. Every binding and handler in `PaneHost.qml` dereferenced it unguarded, throwing a
  burst of `TypeError: Cannot read property ... of null` on **every** shrink. Now guarded behind a
  `hasController` check (verified: 0 TypeErrors across the 13-resize stress test).

> **OPEN DEFECT — crash on application exit (not slice-4-specific, predates it).**
> `wxlens-app.exe` access-violates (`0xc0000005`) inside `Qt6Gui.dll` at a consistent fault offset
> when the window is closed. **The window closes first, so this is an exit-path crash, not a
> data-loss or in-session failure** - but it is real and must not be shipped.
>
> Established by controlled A/B runs (each with its own event-log baseline, since a stale event
> from a prior run is an easy false positive here):
> - Untouched 1×1 grid, closed immediately → crashes. **Not** caused by resizing.
> - Radar custom layer disabled entirely (`attachLayers` short-circuited) → **still crashes.**
>   So WxLens's own `RadarSweepLayer` is not the cause.
> - No `MapLibre` item instantiated at all → **clean exit, no crash event.**
>
> So it is MapLibre Native Qt's own teardown, and it would have been present since slice 1's bare
> map pane. Two candidate fixes were tried and **both failed, and were reverted** rather than kept
> as unverified changes: (a) guarding/leaking our layer's GL objects when no context is current
> (kept only as hardening, with comments saying so - it is not the fix), and (b) calling
> `Map::destroyRenderer()` from `TextureNodeOpenGL`'s destructor while the context is still
> current. Note (b)'s underlying observation still stands and is a genuine lead: `render()` calls
> `createRenderer()` but **nothing in the Quick module ever calls the existing
> `Map::destroyRenderer()`**, so mbgl's renderer is destroyed with the `Map` rather than at a
> point where a GL context is guaranteed current.
>
> **Blocked on tooling, not ideas:** this machine has no debugger (no VS, no cdb/WinDbg), no Qt
> PDBs, and the Release build produces no PDB for `wxlens-app` either, so the fault offset cannot
> be resolved to a symbol. Minidumps *are* being written to
> `%LOCALAPPDATA%\CrashDumps\wxlens-app.exe.*.dmp`. Next step is to make the crash legible rather
> than keep guessing: enable PDB generation for Release (`/Zi` + `/DEBUG`), and/or install a
> DbgHelp-based unhandled-exception handler that logs a module+offset backtrace through the
> existing logger. Do that before attempting another blind fix.

> **Update (2026-08-23): done, and it paid for itself immediately.** Release builds now emit PDBs
> (`/Zi` with `/DEBUG /OPT:REF /OPT:ICF`, so codegen stays a real Release build), and
> `app/source/wxlens/util/crash_handler.*` installs a DbgHelp/StackWalk64 unhandled-exception
> handler writing a symbolized backtrace to `logs/wxlens-crash.log`, plus an all-thread dump armed
> on `aboutToQuit` for hangs (a deadlock produces no crash and no output otherwise). It writes
> with plain Win32 calls, not spdlog, since the fault happens after the logger's sinks may be gone.
>
> The first stack it produced identified a bug of **ours**, not upstream: patch 0005's
> `mapLibreMap()` is a `Q_INVOKABLE`, and Qt gives *JavaScript ownership* to any QObject returned
> from one, so QML's garbage collector was deleting a `Map` that `MapQuickItem`'s `unique_ptr`
> already owned. Fixed with `QQmlEngine::setObjectOwnership(map, CppOwnership)` in
> `PaneController::attachLayers`; confirmed by the stack moving from `QV4::MemoryManager::sweep`
> to `~MapQuickItem`. **Watch for this on any future `Q_INVOKABLE` returning a borrowed QObject.**
>
> Underneath it are two genuine upstream teardown defects, now diagnosed and filed (see ADR 0004's
> upstream table): `mbgl::gl::Context::~Context` calls GL with no current context
> ([#302](https://github.com/maplibre/maplibre-native-qt/issues/302)), and behind *that*,
> `~Thread<MainResourceLoaderThread>` blocks forever on a worker parked in `QEventLoop::exec`
> ([#285](https://github.com/maplibre/maplibre-native-qt/issues/285)). Both fix attempts were
> reverted: calling `destroyRenderer()` from the render thread trades the crash for an indefinite
> hang (worse for users), and leaking the QML engine changes nothing because `QGuiApplication`
> tears the window down itself. **Net behaviour is unchanged** - still a fault after the window
> closes, with nothing left to lose - but it is now auto-logged with a full stack instead of
> opaque. A real fix belongs upstream.

**Status as of 2026-08-23 — Slice 5 complete: per-channel synchronization.** The sync model from
§4.1-4.2, layered onto slice 4's grid. There is deliberately no global "linked" flag anywhere.

- `panes/sync_types.hpp`: `SyncChannel` (all eleven channels from §4.1 declared up front, so the
  enum is not renumbered later when persisted layouts would care), `ChangeOrigin`, `SyncGroupId`.
  Channels with no backing state yet (`Time`, `Animation`, `Cursor`, `SelectedStorm`, `Palette`)
  return an invalid `QVariant`, which makes the coordinator skip them rather than propagate
  nonsense - the plumbing exists, the data does not.
- `PaneController` holds only *its own* per-channel group membership and never learns that other
  panes exist. Fan-out lives in `PaneGridModel::PropagateChannel`, so panes stay independent by
  construction.
- **Feedback prevention (§4.2)** is origin-based, not flag-based: only `ChangeOrigin::UserInput`
  fans out; an applied sync re-emits as `ProgrammaticSync` and stops there.
- `Location` is one channel carrying a lat/lon pair, so a half-updated coordinate can never
  propagate. This is why `centerLatitude`/`centerLongitude` are read-only properties written
  through `setCenter()`.
- Persistent link vs. one-shot apply are kept distinct per §4.1: `setSyncGroup` joins a group,
  `PaneGridModel::copyChannel` copies a value once and creates no ongoing relationship.
- Per-pane "Unlinked / Link A / Link B" control in the pane chrome (§4.5's "reachable without
  opening Settings"). Two groups, not one, so the UI actually demonstrates that groups are
  independent of each other.
- **New test target `wxlens-app-test`** (`test/source/wxlens/`, per this roadmap's "test the C++
  models independently of QML" rule) with 13 tests covering per-channel independence, group
  propagation, cross-group isolation, the no-echo guard, one-shot copy, leaving a group, and
  groups surviving a resize. It compiles the app's sources directly; **if that source list grows
  much further, split the app into a static library + thin `main()` and link that from both
  targets** rather than extending the list.
- **A regression this slice caused and fixed, worth remembering:** the first version pushed the
  controller's camera back into the map on every `cameraChanged`, including the pane's own
  gestures. Zoom-about-cursor moves the centre as part of zooming, so snapping the centre back
  mid-gesture turned wheel-zoom into a sideways/diagonal slide whose direction depended on cursor
  position (found by the user, not by the tests - the tests exercise the model, not the QML
  binding). Fixed with a separate `cameraSynced` signal emitted *only* for coordinator-driven
  changes. **The view follows the map for local input; sync follows the map. Never re-apply a
  pane's own camera to itself.**
- **Not verified this slice:** the QML link control end-to-end under automation. An attempt to
  drive it with synthetic clicks landed in an unrelated window that had focus and was discarded
  rather than reported as passing; the model itself is covered by the unit tests, and the control
  was confirmed manually by the user. `Bearing`/`Pitch` are grouped and propagated but no QML
  property exposes them yet (see slice 4's note), so they are untested in practice.
- **Next slice:** slice 6, `MapObjectStore`/`MapObjectsLayer` with scope resolution (§4.3) -
  markers/drawings/range rings and the temporary/pinned/saved lifecycle. Scope resolution consumes
  the sync groups built here, so `SyncGroup(channel, groupId)` scoping now has something real to
  resolve against.

**Status as of 2026-08-23 — Slice 6 complete: unified map objects with scope resolution.** §4.3's
one object family and one store, replacing the legacy app's parallel marker/annotation systems
rather than reproducing the split.

- `objects/map_object.hpp`: `MapObject` (geometry always in degrees, never screen space),
  `MapObjectType`, `MapObjectLifecycle`, and `MapObjectScope`.
- `objects/map_object_store.*`: the single `QAbstractListModel`-backed store. **Scope resolution
  lives here, not in the view** - whether an object belongs in a pane is a property of the object
  and that pane's state, not of how it is drawn.
- **`SyncGroup` scope is where slice 5 pays off:** "show this on my linked panes" resolves against
  the existing sync groups instead of needing its own sharing mechanism. The group is captured at
  creation (`originGroupId`), so an object stays with the group it was shared into even if its
  author later leaves - the alternative would make shared objects vanish from other panes for no
  visible reason.
- **Lifecycle tier 1 is enforced, not merely documented:** `MapObjectStore::Add` rejects
  `Temporary` objects outright. That is what actually keeps probing the map from littering it.
  Tier 3 (`Saved`) exists in the enum but does not persist yet - it needs the structured config
  store (§3.2), and inventing a second storage mechanism here would have to be undone later.
- `objects/object_tool_controller.*`: active tool, scope for new objects, ring radius. Placement
  rules live in C++ so QML stays presentation-only.
- `qml/Panes/MapObjectsLayer.qml`: the User Analysis Layer, above the radar rendering and
  independent of it. **Rendered as Qt Quick items rather than a GL custom layer** - a handful of
  vector shapes with text labels per pane, where Qt Quick gives crisp text, hit testing and
  styling for free and the radar shader machinery would buy nothing. If object counts ever reach
  the thousands (dense placefiles), this is the piece to move to GL; the store and scope
  resolution behind it would not change.
- Range rings are **true geodesic circles** - 72 points sampled at a fixed ground distance and
  projected individually. A screen-space circle would be visibly wrong away from the equator,
  since Mercator stretches north-south with latitude.
- `PaneController` gained projection helpers (`pixelForCoordinate`, `coordinateForPixel`,
  `distanceMeters`, `coordinateAtOffset`) wrapping the map's own projection, which also keeps the
  `Q_INVOKABLE`-ownership hazard contained to `attachLayers`. `util::GeodesicInverse` was added
  alongside the existing `GeodesicDirect`, as that header anticipated - slice 7's measurement work
  should reuse both rather than reinventing them.
- **Verified for real:** 14 new tests (27 total in `wxlens-app-test`) covering tier-1 rejection,
  geometry validation, every scope kind, the author-leaves-group case, `SameLocation` tolerance,
  filtering, post-creation scope changes, and revision bumping. Then end-to-end in the running
  app: markers and a range ring placed through the actual UI render correctly over live KEAX
  reflectivity, with zero QML warnings.
- **A bug the tests could not catch, found by the user:** objects initially stuck to their
  original *screen pixel* and slid across the map while panning/zooming. `pixelForCoordinate` is
  a method call, so a QML binding cannot know its result went stale; the ring geometry and the
  object list already read a `cameraTick` counter to force re-evaluation, but the marker anchor
  binding did not. Fixed, and `PaneHost` now bumps that tick from the map's own
  `coordinate`/`zoomLevel` signals as well, so objects also re-project when a *synced* pane is
  moved by another pane (where the controller write-back is deliberately suppressed).
  **Every projected value in `MapObjectsLayer.qml` needs that dependency** - it is the easiest
  thing to forget when adding a new object type.
- **Tooling notes for future agents (both cost real time this slice):**
  1. Screen-scraping with `CopyFromScreen` is unreliable - it captures whatever is composited on
     top, and twice produced screenshots of unrelated windows. **Use `PrintWindow` with
     `PW_RENDERFULLCONTENT`**, which captures the window's own surface even when occluded, and
     check `GetForegroundWindow()` before synthesizing input (Windows often refuses
     `SetForegroundWindow` from a background process; the `AttachThreadInput` dance works).
  2. **PowerShell is DPI-unaware, so `GetWindowRect`/`GetClientRect`/`SetCursorPos` speak
     *virtualized* coordinates, while `PrintWindow` renders at *physical* scale.** On this 125%
     display those differ by 1.25x, and mixing them made correct rendering look like a
     placement bug - it was investigated as a real defect before the arithmetic reconciled
     exactly (expected image (488.75, 317.5) vs measured (488.5, 317.5)). When a measured offset
     looks like a clean scale factor, suspect the measurement before the code.
- **Not built this slice:** Line/Polygon/TextAnnotation object types (declared in the enum,
  no tool or renderer yet), object selection/editing/deletion from the map, and `Saved`
  persistence. `SameLocation` scope resolves correctly but has no UI to select it.
- **Next slice:** slice 7, the measurement framework (§4.4) - point-to-point, radar-to-point and
  multi-segment path, built on this slice's object infrastructure and `util::GeodesicInverse`.

**Status as of 2026-08-23 — Slice 7 complete: the measurement framework.** §4.4's three modes,
built on slice 6's object infrastructure rather than as a radar-only utility.

- `objects/measurement_controller.*`: one geometry model behind all modes - an ordered vertex list
  with per-segment distance/bearing and a running total. **All geometry uses real WGS84 geodesics**
  (`util::GeodesicInverse`), never flat pixel math, so results hold at any zoom or projection.
  A flat approximation would still return plausible numbers, which is exactly why the tests pin
  the values against known distances rather than merely checking for non-zero.
- **Modes:** Point→Point; Radar→Point, where the origin is supplied automatically from the pane's
  own source location (that automatic origin is what makes it a distinct named mode rather than
  Point→Point with extra steps, and it reads out as range/azimuth); and multi-segment Path with a
  running total.
- **Tier 1 is honoured throughout (§4.3):** an in-progress measurement lives entirely in the
  controller, updates live with the cursor, and never touches `MapObjectStore`. Only right-click
  commits it as a Pinned `Measurement` object. The live cursor vertex is deliberately excluded
  from what gets pinned - it is where the pointer happens to be, not something the user chose.
  `qml/Panes/MeasurementLayer.qml` draws it dashed, separate from `MapObjectsLayer`, precisely
  because it is not a stored object.
- Bearings are converted to compass angles [0, 360) for display; `GeodesicInverse` returns
  [-180, 180), which reads wrong as a bearing.
- Distances show km and miles together until the unit-settings surface exists (§4.4 defers the
  *preference*, not the measurement). Showing both beats silently picking one.
- Selecting a measurement mode disarms the object tools and vice versa, so the two families can
  never both act on one click.
- **Verified:** 16 new measurement tests (43 total in `wxlens-app-test`), then end-to-end in the
  running app - live rubber-band with midpoint distance, commit to a pinned green measurement, and
  the pinned line staying geographically anchored across a zoom-out. Zero QML warnings.
- **Observed, not fixed:** on this run the basemap rendered white until a scroll forced a repaint,
  with radar and objects drawing correctly over it - only the basemap layer was missing. This is
  the same intermittent "blank until interaction" behaviour first noted in slice 2 and was not
  introduced here, but a white flash is user-visible and it now has a second sighting. Worth
  root-causing rather than continuing to treat as tile latency.
- **Not built this slice:** the "Point info" readout (§4.4's fourth mode - lat/lon plus
  range/azimuth from the pane's site), editing or deleting a pinned measurement, and unit
  preferences. Beam-height interrogation (§4.7) is deliberately slice 8's work, and
  `GetRadarBeamAltititude`'s `height` parameter must be verified against real site metadata before
  wiring - §4.7 forbids approximating it.
- **Next slice:** slice 8, radar geometry - beam-height interrogation extending Radar→Point, being
  careful to keep beam-centre MSL and terrain-relative AGL distinct and to say "terrain data
  unavailable" rather than implying an AGL figure without a real DEM.

**Status as of 2026-08-24 — Slice 8 complete: radar geometry & beam-height interrogation.** §4.7's
readout, built as a *probe of the pane's data source* rather than a radar feature bolted onto the
measurement tool.

- `util/radar_geometry.*`: the 4/3-effective-earth beam model ported from the legacy
  `GetRadarBeamAltititude` (same constant, no units-library wrapping), plus `ProbeRadarBeam`, which
  turns a site + tilt + target coordinate into the full breakdown. Qt-free and I/O-free, so the
  whole thing is unit-testable without a network or a window - which is why the beam math has real
  coverage while only the UI assembly relies on live verification.
- **The seam is `PaneController::probeSourceAt(lat, lon)`, deliberately not `radarGeometryAt()`.**
  §4.6 forbids radar-specific fields on `PaneController`, and §4.4's point-info tool wants "ask
  this pane's source about this coordinate" as a general operation - a satellite or model pane
  should answer the same call with its own fields. So it dispatches on product kind exactly as
  `attachLayers` does, and the returned map's `kind` tells the caller what shape it got. Phase 2/3
  providers plug into this rather than growing a parallel readout.
- **The three terms §4.7 insists on are separate fields, not separate labels on one number:**
  elevation *angle*, beam-centre *altitude* (MSL), and beam-centre height *above the radar* (ARL).
  Beam-centre height above *ground* (AGL) is **absent from the struct entirely** - it cannot be
  computed without a DEM, and a field that does not exist cannot be rendered as authoritative. The
  UI shows "Terrain (MSL): terrain data unavailable" and "Beam height (AGL): requires terrain
  data" as real rows rather than hiding them, because a reader who expects an AGL figure would
  otherwise assume the MSL number *was* one.
- **Two real metadata defects found and fixed, both of the "confidently wrong" kind §4.7 exists to
  prevent:**
  1. **`radar_sites.json`'s `elevation` is feet, and the loader stored it as `elevationMeters`.**
     Nothing had read the field since slice 2, so nothing had noticed. Verified two ways before
     changing it: the legacy loader reads the same field as `units::length::feet`
     (`scwx-qt/config/radar_site.cpp`'s JSON branch), and the values only parse as feet - PAEC
     (Nome, at sea level) reads 90, KMSX (Point Six Mountain, ~2.4 km) reads 7978. Now converted at
     load and named `altitudeMslMeters`. `radar_site_database.test.cpp` pins both the conversion
     and those two extremes so it cannot silently come back.
  2. **The elevation angle was going to be assumed.** `RadarSweepProduct` was discarding the
     `elevationCut` `GetElevationScan` already returns. It now keeps it and exposes it as
     `std::optional<double>` - optional because "no sweep loaded" and "0.5°" are different answers,
     a VCP's lowest cut is not always 0.5°, and defaulting would have the UI report a tilt the
     radar never used. With no sweep the readout says "waiting for sweep data" and every figure
     downstream of the angle stays unreported.
- **Site altitude - what is known and what is not.** The bundled list does not record whether its
  figure is ground level or the antenna on the tower, and a WSR-88D tower is tens of metres. That
  offset is constant with range, so it shifts the whole profile rather than distorting it, and it
  is documented in `radar_site_database.hpp` rather than smoothed over. **The authoritative fix is
  already in the data we parse:** Message 31's volume data block carries site height (m MSL) and
  feedhorn height (m AGL), and `wsr88d/rda/digital_radar_data_generic.cpp` reads both into private
  members with no accessor. Adding those accessors is a wxdata change, so per AGENTS.md it belongs
  upstream in the legacy repo, then the submodule pin advances - it is not a WxLens-side edit.
- `util/unit_format.*` now owns distance/altitude/bearing formatting, with
  `MeasurementController::formatDistance` delegating to it. One implementation, because the range
  the geometry panel reports and the distance the measurement reports are the same number, and the
  two reading differently is a bug a user would spot before we did. It is also where §4.4's
  deferred unit *preference* plugs in when the settings surface exists.
- `qml/Panes/RadarGeometryPanel.qml` holds no geometry at all - every number *and* every
  "unavailable" string comes from `probeSourceAt`, so the wording is tested rather than invented in
  QML. Collapsed by default per §4.7/§5.3; expanding is what asks the fuller question.
- `PaneController::sourceDataChanged` is new, and exists for the same reason `cameraTick` does:
  `probeSourceAt` is a method call, so a QML binding cannot know its answer went stale when the
  sweep finally loads. A readout left open across a data load would otherwise sit on "waiting for
  sweep data" until the cursor next moved.
- **Verified:** 17 new tests (60 total in `wxlens-app-test`, all passing) - the beam model pinned
  against independently computed 4/3-earth values (0.5° at 100 km = 1461.5 m, and three more),
  against the flat-earth ray it must exceed *and* by how much that gap grows with range, plus
  compass-azimuth normalisation, the MSL/ARL separation, the no-elevation-angle path, and the site
  altitude conversion. Then live in the running app against KEAX: Radar→Point measuring 80.2 km at
  058.4° with the collapsed "Radar geometry" header present, zero QML warnings in the log.
  The expanded panel was confirmed afterwards on a live KEAX measurement: all seven rows render,
  and **the elevation angle read 0.48°, not 0.50°** - direct evidence for taking the real
  `elevationCut` rather than assuming the nominal tilt.

- **A press-and-drag defect this slice's verification missed, found by the user (now fixed).** The
  measurement `MouseArea` took the press but the map's `DragHandler` - behind it, with default
  `grabPermissions`, which include `CanTakeOverFromItems` - stole the exclusive grab the moment the
  pointer crossed the drag threshold. So a press dropped the origin and then **panned the map**
  instead of stretching the ruler, and because a stolen grab delivers `onCanceled` rather than
  `onReleased`, the release half never ran and the measurement sat unfinished until the user
  clicked again. Fixed with `preventStealing: true` (which sets `keepMouseGrab`, the flag a handler
  checks before taking over) plus an `onCanceled` reset, so a legitimately lost grab cannot leave
  `measureDragActive` stuck and make the next press reuse a stale origin. The Shift-to-pan escape
  hatch is unaffected: it declines the press outright, so the map owns that gesture from the start
  rather than halfway through it.
- **The verification lesson, which matters more than the bug.** The slice was called verified after
  exercising click-then-hover - the *fallback* path - and never exercising press-drag-release, the
  primary gesture the code was written for. Both paths reach the same `updateCursor`, which is
  exactly why testing one felt like testing both; what differs is only whether a button is held,
  and a held button is the sole condition under which a pointer grab can be stolen. **When an
  interaction has a primary gesture and a fallback, drive both - the fallback passing says nothing
  about the primary.** Re-verified properly: mid-drag capture shows the band stretching while the
  button is held, a basemap strip differs by 0 of 19500 pixels across the drag (proving no pan), and
  release commits the pinned measurement in one gesture.
- **Tooling note, and a second entry for slice 6's list:** `PrintWindow(PW_RENDERFULLCONTENT)`
  paints the **whole window including its frame and title bar**, so sizing the capture bitmap from
  `GetClientRect` silently clips the bottom rows. That looked exactly like a QML layout overflow -
  the readout box appeared to spill its last line onto the map - and was investigated as a real
  defect before a debug overlay showed the layout was correct all along. Size the bitmap from
  `GetWindowRect`. Slice 6 already recorded "when a measured offset looks like a clean scale
  factor, suspect the measurement before the code"; this is the same lesson with an offset instead
  of a factor.
- **Not built this slice:** beam top/bottom (vertical extent) - explicitly §8 backlog, though
  nothing here assumes a thin ray: the elevation angle is a parameter, so top/bottom is two more
  `BeamAltitudeMsl` calls at elevation ± half the beamwidth, not a rework. Also not built: terrain
  (no DEM provider until Phase 3+), §4.4's standalone "Point info" mode (the probe it needs now
  exists), and pinning a geometry interrogation as an object.
- **Next slice:** slice 14, unified measurement + snap targets (§4.4) - it is scheduled to run with
  or immediately after this slice, and this slice's `probeSourceAt` is what makes "the readout
  adapts to what was snapped" cheap: a measurement whose origin snapped to a radar site can ask the
  probe for range/azimuth framing instead of needing a separate Radar→Point mode.

**Status as of 2026-08-24 — Slice 9 complete: palette system.** §5.1's compatible `.pal` picker
and editor, connected to the real radar renderer rather than implemented as an isolated mockup.

- `palettes/PaletteModel` is an editable `QAbstractListModel` for `Color:`/`Color4:` and solid
  stops. It preserves non-color lines and inline comments verbatim, changes only the selected
  stop line, and only writes through **Save as**. Imported community `.pal` files remain plain
  text and are never overwritten implicitly.
- Preview samples are generated by parsing the model's current text with the reused,
  unmodified `scwx::common::ColorTable` and calling `Color()` at 64 values. The QML canvas only
  paints those returned samples; it does not maintain a second interpolation algorithm.
- The QML surface provides a quick bundled-palette row, import/save-as dialogs, draggable stops,
  numeric stop editing, and a reusable RGB `ColorPicker` component (shared widget only; palette,
  theme, and saved-place color state remain separate systems).
- Follow-up from live user review: the selected stop also accepts pasted `#RRGGBB` or Qt-alpha
  `#AARRGGBB`; `Reset <palette>` and `Reset all` reload immutable factory resources without ever
  touching imported/exported user files; and dirty edits prompt Save a copy / Discard / Keep
  editing before close, palette switch, import, or reset. Discard restores the renderer as well
  as the editor, rather than merely hiding the modified UI.
- Palette edits and selections live-update every shared `RadarSweepProduct`. The product retains
  the last decoded volume, rebuilds its immutable sweep/LUT snapshot, and emits the existing
  update signal—no network refetch and no renderer-specific mutation from QML.
- The complete NOAA WCT palette set already carried by the legacy dependency is compiled into
  WxLens resources without modifying `external/`; its existing public-domain attribution is
  retained in `ACKNOWLEDGEMENTS.md`.
- **Verified:** release builds of `wxlens-app` and `wxlens-app-test`; all 84 WxLens model tests
  pass, including three focused tests for real-parser preview generation, preservation/save-as,
  and invalid input. A launch stayed healthy through live radar startup with no QML/error
  diagnostics. The wider inherited 285-test CTest run had 280 passing/skipped and five unrelated
  failures: the pre-existing MapLibre quick test is not runnable from this build tree, and four
  live-network `wxdata` tests failed against changed/unavailable upstream data.
- **Next slice:** slice 10, `ThemeManager` and two bundled chrome themes. Slice 15 can now reuse
  `Controls/ColorPicker.qml` for saved-place groups without coupling place colors to palettes.

**Status as of 2026-08-25 — Slice 10 complete: app chrome themes.** §5.2's live, shareable theme
system now owns the semantic colors and core metrics used by all chrome built through slice 9.

- `theme/ThemeManager` exposes background/surface/control, status, text, border, product-accent,
  corner-radius and spacing roles as notifying QML properties. Switching one name causes existing
  bindings to repaint immediately; QML does not contain a parallel theme lookup table.
- `Operational Dark` and `Daylight` are bundled as the same version-1 TOML format accepted for
  user themes. Theme files validate every required role and metric before activation; a malformed
  file is logged and leaves the current theme untouched.
- Active selection persists in `appearance.toml`. Custom TOML files live under the visible config
  directory's `themes/` folder, are discovered at launch, and can also be imported/exported through
  the C++ API. The addressable `appearance` settings section provides the live bundled/custom
  theme picker.
- Existing hand-built Qt Quick controls remain fully custom and are now driven by semantic roles;
  WxLens does not currently use Qt Quick Controls 2, so there is no native `Fusion`, `Material`, or
  platform widget style to leak through. Literal colors that remain in QML are content colors
  (palette values, map-object translucency, map label outlines, and modal scrims), not chrome roles.
- Follow-up from live review: Appearance has an independently persisted map-theme choice: `Same
  as app` (the shipped default), `Dark`, or `Light`. The effective choice selects OpenFreeMap's
  `dark` or `positron` basemap style. Reloads are assigned explicitly because MapLibre consumes
  the initial QML binding during its first style change; the existing `onStyleLoaded` path then
  reattaches the radar custom layer after MapLibre replaces the style.
- **Verified:** Release builds of `wxlens-app` and `wxlens-app-test`; all 92 WxLens model tests pass,
  including focused coverage for bundled themes, live notification, persisted selection,
  import/export round-tripping, and rejection of malformed/version-incompatible themes.
- **Next slice:** slice 11, archive/time controls. Then slice 12 (warnings/placefiles) and slice 13
  (multi-pane polish and Phase 1 acceptance validation). The separately added slices 14-16 remain
  queued according to their dependency notes; completing slice 10 does not skip the original
  11-13 sequence.

**Status as of 2026-08-25 — Slice 11 implemented: archive/time controls.** Each radar pane now
has compact, theme-driven Live/Archive controls, a UTC time field, and a site field. Archive
selection lists the requested UTC day through wxdata's existing Level 2 provider, chooses the
latest volume no later than the requested time, and reports the actual returned scan time.

- Time is real `PaneController` state and participates in the existing `SyncChannel::Time`; it is
  not a new global linked flag. Independent panes can browse independently, while time-grouped
  panes propagate live/archive selections through the existing feedback-loop guard.
- `RadarSiteDataService` remains the per-site singleton. Archive requests carry ids so results
  cannot be consumed by the wrong time selection. `RadarSweepProduct` is shared by site plus
  selected minute, with weak registry entries: identical selections share decode/geometry work,
  different selections do not overwrite one another, and scrubbing does not retain every volume.
- Invalid and future times stay in the current mode and produce a visible error. State-changing
  live/archive requests and provider outcomes are logged.
- **Test coverage added:** time-channel propagation, returning to Live, and invalid/future input.
- The per-site service refreshes Live data once a minute and suppresses overlapping refreshes.
  Worker completions are marshalled back to the QObject thread before notifying products.
- **Verified:** Release `wxlens-app-test` build and all 94 WxLens model tests pass. The Release app
  and every QML cache unit compile; its final link could not replace `wxlens-app.exe` because the
  user's existing WxLens process was still running. Close that process and rerun the app target
  to complete link/launch verification.
- The build also fixed the MapLibre patch driver's idempotence check: later tracked patches overlap
  earlier ones, so the ordered series now uses its final patch as the completion marker instead of
  incorrectly reverse-checking every earlier patch in isolation. `external/` source remains
  unmodified beyond the already-applied tracked patches.
- **Next slice after verification:** slice 12, warnings/placefiles.

**Status as of 2026-08-25 — Slice 13 implemented, Phase 1 acceptance audited.** Multi-pane chrome
now exposes both persistent camera grouping and a distinct one-shot **Match pane 1 view** action.
The latter copies all four camera channels without creating group membership, preserving §4.1's
important link-vs-copy distinction.

- A shared, persisted **Map details** surface adds `Operational`, `Minimal`, and `Detailed`
  presets plus a `Custom` state and grouped toggles for roads, city/town labels, boundaries,
  buildings, points of interest, water labels, and terrain/hillshade. `Operational` ships with
  roads, places, boundaries, water, and terrain enabled while buildings and POIs stay out of the
  radar's way. It is independent of both chrome theme and dark/light basemap selection.
- The policy is C++ settings state, not QML business logic. Every pane applies the same policy to
  matching layers in its active MapLibre style after style load and on live changes. Unsupported
  groups simply match no layer, as required; switching map style reapplies the policy.
- The top bar provides a direct Map-details deep-link. Existing per-pane camera-group and rail
  object-scope controls remain the uncluttered quick surface; right-click pane actions hold the
  less frequent one-shot camera match.
- **Verified:** Release `wxlens-app-test` build and all 96 model tests pass. QML cache generation
  for every changed surface succeeds. The app target reached final link, which could not replace
  `wxlens-app.exe` because the user's existing WxLens process is running, so live visual
  confirmation of OpenFreeMap's current layer-id classification remains unverified.
- **§4.8 audit:** criteria 1, 4, 5, 7–14, 16, and 17 are represented by the current model/UI and
  automated coverage. Criteria 2, 3, 6, 15, and 18 are not yet fully closed: per-channel advanced
  sync configuration is not exposed, storm selection is not implemented, and tier-3 saved object
  persistence belongs to slice 15. These are recorded gaps, not silently accepted as Phase 1
  complete. Slice 12 (warnings/placefiles) is also still outstanding.
- **Next slice:** slice 12 for the original sequence, or slice 14/15 where their explicit
  dependency ordering takes precedence. Phase 1 acceptance must be rerun after those gaps close.

**Status as of 2026-08-24 — Slice 17 complete: the settings foundation.** §3.2's structured config
store, ADR 0003's TOML backing, and §4.5's addressable sections - built out of order, ahead of
slices 14–16, because it was never given a slice at all and everything else had been deferring
preferences into it.

- `settings/settings_store.*` - TOML under `QStandardPaths::AppConfigLocation`, one file per
  category per ADR 0003. **Hand-editing is a supported workflow, and that drives the entire error
  policy:** a wrong-typed value, an out-of-range number or a malformed file each fall back to the
  default and log what was rejected, rather than crashing, clamping, or taking neighbouring
  settings down with them. Two decisions worth keeping:
  - **Out-of-range falls back rather than clamps.** Clamping turns a typo into a different
    valid-looking setting the user never chose and cannot detect; falling back is at least
    predictable and the log says why.
  - **A file that fails to parse is never overwritten.** `Save()` skips it and returns false.
    Writing our view of a file we could not read would destroy whatever the user was mid-edit,
    which is a worse outcome than ignoring it for one session. Unknown keys survive a save too, so
    a key from a newer build (or a note a user added) is not silently dropped.
- `settings/app_settings.*` - the typed layer (§3.2's `settings_variable` pattern: validated
  defaults + Qt-signal change notification). Writes persist immediately, because a setting that is
  only in memory looks identical to one that stuck until the next launch.
- **Section ids are a contract, not labels** (§4.5). That section warns that retrofitting
  addressability is the expensive path, so ids exist from the first commit and are asserted
  literally in tests: `measurement`, `objects`, `units`, `radar-geometry`. `SettingsDialog.openAt(id)`
  is the primary entry point and plain `open()` is the degenerate case; an unknown id opens the
  first section rather than a blank panel, so a stale deep-link degrades instead of breaking.
- **Two deep-links are wired, to prove the mechanism rather than to be exhaustive**: the radar
  -geometry readout's gear opens `radar-geometry`, and right-clicking the rail's scope control
  opens `objects`. Every future quick control follows that pattern.
- **Preferences wired this slice**, all from §4.3/§4.4/§4.7 rather than invented here:
  - *Measurement gesture* (drag only / click-click / both, default both) - the user-requested one.
  - *Radar-geometry row visibility* - the other user-requested one. **The §4.7 constraint is
    enforced in the model, not left to the UI:** terrain and beam-height-AGL default to visible and
    carry an explanatory note, because shipping them hidden is exactly the silent omission that
    section exists to prevent. A test pins that default so it cannot regress quietly.
  - *Default object scope* - §4.3 explicitly forbids the hardcoded constant that was there.
    Wired in `main.cpp` rather than by giving `ObjectToolController` a settings dependency, so
    `objects` stays independent of `settings` and the coupling direction is visible in one place.
  - *Distance units* - retires slice 7's "show km **and** miles" stopgap. `util::unit_format` owns
    formatting and settings *pushes* the preference into it, so `util` keeps no dependency on the
    config store and stays usable from tests with no settings file.
- **The row catalogue is one source of truth.** Each geometry row carries its id, label, note,
  which `probeSourceAt` key it displays, and when it should render dimmed - so the settings
  checklist and the readout cannot disagree about what rows exist, and the QML needs no id-to-key
  mapping of its own.
- **Verified:** 20 new tests (80 total in `wxlens-app-test`, all passing), covering the hand-edited
  -file failure modes end to end (wrong type, out of range, malformed, unknown keys preserved,
  malformed category not blocking healthy ones), persistence across a simulated relaunch, the
  §4.7 default-visible constraint, section-id stability, and that the unit preference actually
  changes what `FormatGroundDistance`/`FormatAltitude` return.
- **Not built this slice:** snap tolerance (§4.4 - belongs to slice 14, and building it before that
  slice needs it would be speculative), theme and palette selection (slices 9/10 own those),
  pane-layout/workspace persistence (§4.6), and tier-3 `Saved` object persistence (slice 15). All
  four now have a store to land in, which was the point.
- **Next slice:** back to the numbered sequence - slice 14, unified measurement + snap targets,
  which is now unblocked (its tolerance and suppress-modifier preferences have somewhere to live)
  and which slice 8's `probeSourceAt` already serves: a measurement whose origin snapped to a radar
  site can ask the probe for range/azimuth framing instead of needing a separate Radar→Point mode.

**Status as of 2026-08-25 — Slice 14 complete: unified measurement and snap targets.** The separate
Point→Point and Radar→Point tools are now one Distance/Bearing tool. Its origin and endpoint use
the same geodesic path; when the origin snaps to a radar site the readout automatically switches
to radar-native Range/Azimuth wording.

- `SnapTargetRegistry` is a generic C++ screen-space resolver. It currently supplies every bundled
  radar site and the vertices/centres of visible `MapObject`s; saved places join the same registry
  when slice 15 adds that object kind. Candidates are projected through the active pane and ranked
  in pixels, so zoom never changes the apparent magnetic radius.
- Snapping is visible before commit: the live endpoint jumps to the resolved coordinate and a
  labelled halo marks the target. Holding Alt suppresses snapping for one placement.
- Measurement settings now expose Off/Subtle/Strong rather than a raw pixel field. These map to
  0/10/18 pixels in the typed settings model, persist in `measurement.toml`, and reset to Subtle.
- The path tool remains multi-point and uses the same registry for every vertex. All measurement
  geometry remains geographic and WGS84 geodesic; only target selection occurs in screen space.
- **Verified:** Release `wxlens-app-test` builds and all 96 model tests pass. Every changed QML
  cache unit and the app's C++ sources compile. Final linking could not replace `wxlens-app.exe`
  because the user's existing WxLens process is running; live visual gesture confirmation remains
  unverified until that process is closed and the app target is rebuilt/launched.
- **Next slice:** slice 15, saved places, which adds its locations as another snap-target provider.

**Status as of 2026-08-25 — Slice 15 complete: saved places.** Personal locations are durable
`Saved` marker objects in the existing singleton `MapObjectStore`, default to `AllPanes`, and
participate in slice 14's snap registry as labelled `saved-place` targets. `SavedPlaceManager`
adds the distinct taxonomy and management behavior without introducing a parallel render store:
named groups own inherited colors and visibility, places may override their group color, and
group changes immediately update rendering and snapping. A searchable QML management surface
supports create/rename/edit/delete, group visibility toggles, inherited and per-place colors via
the shared slice-9 color picker, and versioned JSON
import/export; the same JSON is atomically persisted under the structured config directory.

- **Verified:** the Release app target and QML cache compile/link successfully and the packaged
  executable remained healthy through an eight-second startup smoke test. Six focused model
  tests cover unified-store/lifecycle/scope defaults, group color inheritance and overrides,
  visibility, persistence/import/export, search, coordinate validation, and malformed imports.
- **Not verified:** manual pointer/keyboard interaction with the management dialog and native file
  pickers on screen.
- **Next slice:** slice 16, control-surface relocation, unless dependency ordering or a newly
  discovered acceptance gap takes precedence.

**Status as of 2026-08-29 — Slice 16 complete: bottom control surface.** The left tool rail and
per-pane time islands are replaced by one bottom-centred zone. Marker, range-ring, measurement,
scope, time, site, active-pane and layout controls now share a single theme-driven surface.

- Layout buttons use drawn split-pane glyphs rather than numeric labels and expose 1x1, 2x1,
  1x2, 2x2 and 3x3 presets. The underlying generic width/height model remains unchanged.
- Archive/live controls target an explicit active pane. The pane selector cycles that target,
  and shrinking a layout clamps it to a surviving pane rather than leaving a dangling QObject.
- Floating remains the shipped default, with idle fade. The adjacent dock control persists the
  user's choice in `appearance.toml`; docked mode reserves layout space while floating mode lets
  the panes remain full-bleed underneath.
- **Verified:** Release app and QML cache units build; all 103 WxLens model tests pass, including
  active-pane shrink safety and floating/docked persistence. Live visual review is still needed
  to assess the open 3x3 occlusion question in §9.
- **Next slice:** slice 12 warnings/placefiles remains outstanding; rerun Phase 1 acceptance after
  it and the remaining recorded gaps close.

**Status as of 2026-08-29 — Slice 12 complete: warnings/watch and placefile overlays.** A shared
`OverlayManager` now owns meteorological overlay state independently of panes, radar products,
and the user-analysis `MapObjectStore`. Every pane projects the same geographic data in the
locked stack position radar → warnings/placefiles → user analysis.

- Live warnings refresh asynchronously once a minute from the established COD endpoint through wxdata's
  `WarningsProvider`; AWIPS text-product files can also be imported. Canceled, expired, and
  locationless segments are excluded, and active polygons use phenomenon-specific outlines.
- Local files and HTTP(S) placefile URLs are managed through an Overlays surface. The existing
  `scwx::gr::Placefile` parser remains unmodified; line, triangle, polygon, text, and icon-sheet
  primitives are flattened into a presentation-neutral geographic model and reproject after
  every camera change. Unsupported image/image-XY primitives are parsed but not rendered yet;
  those need a texture-capable render node rather than silently turning screen-relative geometry
  into geographic geometry.
- Warning and placefile visibility are independent, and remote placefiles can be refreshed or
  removed without affecting radar or analysis objects.
- **Verified:** Release `wxlens-app` compiles, links, deploys Qt Network/Controls/Dialogs, and all
  QML cache units compile. All 105 WxLens model tests pass, including a real wxdata parse of the
  legacy placefile fixture and independent visibility-channel coverage. The packaged app stayed
  alive through an eight-second startup smoke test.
- **Not verified:** live visual appearance and interaction, current COD warning contents, remote
  placefile resource loading, and image/image-XY placefile primitives.
- **Next:** rerun the Phase 1 acceptance audit and close the remaining §4.8 gaps recorded after
  slice 13.

**Acceptance rerun as of 2026-08-29.** The previously unexercised paths above were driven in the
packaged Release application on Windows, with the real MapLibre surface and network providers:

- A KEAX archive request for `2024-05-01 00:00 UTC` selected, decoded, and rendered the real
  `00:03 UTC` Level 2 volume; returning to Live remained available from the same control.
- Two panes were joined to camera group A through their pane chrome and an actual map drag in the
  first pane propagated to the second. The first pane's archive time remained independent, as it
  must because the quick control links camera channels rather than Time.
- Distance measurement was exercised as press-drag-release with the left button held throughout;
  MapLibre did not steal the grab and release committed the visible pinned geodesic measurement.
- The saved-place Import and Export actions opened the Windows native open/save pickers with the
  JSON filter. Acceptance found that the management surface itself ignored Escape; it now takes
  focus when opened, closes on Escape, and provides Ctrl+F search focus. The fixed Release build
  was launched and the keyboard path rerun successfully.
- Floating and docked control bars both switch correctly in a real 3x3 grid, and docked mode
  reserves its intended layout strip. The measured floating bar spans and obscures a substantial
  portion of all three bottom panes at the reference 1280x800 window size. This closes the factual
  uncertainty in open question 11 but not the product choice: retaining the user's preferred
  floating default accepts that occlusion; shipping docked by default avoids it.
- **Verified:** Release app/QML cache build succeeds and all 105 WxLens model tests pass. The broad
  wxdata CTest pass has two unrelated live IEM endpoint failures and one unbuilt optional MapLibre
  test; these do not fail the WxLens model suite or the acceptance paths above.

**Status as of 2026-08-29 — remaining Phase 1 coverage work resumed.** The acceptance rerun above
confirmed the palette bundle was already complete, while product coverage and general drawing
creation were genuine gaps.

- General drawings now use the unified `MapObjectStore`: a bottom-bar tool captures a geographic
  freehand path, keeps the live path in tier-1 tool state, and commits a scoped, pinned `Line` on
  release. The existing object layer renders and hit-tests it like every other analysis object.
- Level 2 selection now covers all seven `wxdata` moment blocks (reflectivity, velocity, spectrum
  width, ZDR, differential phase, correlation coefficient, and clutter-filter power removed).
  Product and actual volume elevation cuts are selectable per pane; the product singleton key
  includes site, moment, elevation and archive minute, so independent panes cannot overwrite one
  another. Product-channel synchronization rebinds the data product rather than changing only its
  label.
- **Verified:** Release application and QML cache build; all 108 C++ model tests pass, including
  drawing geometry/lifecycle and independent/synchronized Level 2 selection.
- **Not verified:** live visual drawing gesture and live non-reflectivity/elevation rendering.
- **Still outstanding:** Level 3. The reference implementation establishes at least four
  behaviors that must remain separate: radial products, raster products, graphic/vector overlays
  (including storm tracking), and graphic/tabular text. Implement this as provider/cache, radial,
  raster, then overlay/text slices; do not claim "full Level 3" after wiring only radial AWIPS IDs.

#### Remaining Phase 1 Level 3 sub-slices

These are continuations of Phase 1 slice 3 (radar rendering), not Phase 2 work. Phase 2 starts
multi-site mosaic/mesh radar; it must not become a bucket for unfinished single-site NEXRAD
coverage. Complete and verify each sub-slice independently in this order:

1. **3A — Level 3 source service and product catalog.** Extend the radar Data Source with live
   and archive Level 3 requests through wxdata's existing providers. Discover the products and
   AWIPS IDs actually available for a site; cache/request by site + AWIPS ID + selected time; add
   typed loading, completion and failure signals. Extend `ProductDescriptor` so Level 2 moments
   and Level 3 AWIPS products have unambiguous identities without making `PaneController`
   radar-specific. Verify provider behavior and catalog/category mapping with bundled fixtures.
   **IMPLEMENTED (2026-08-29):** `RadarSiteDataService` now owns per-AWIPS wxdata providers,
   supports latest and bounded archive requests, caches parsed files by AWIPS ID plus provider
   object key, and publishes distinct catalog/loading/completion/failure signals with request IDs.
   The presentation-neutral catalog filters site availability through wxdata's canonical product,
   category, description and AWIPS tables. `ProductDescriptor` carries an explicit Level 2-moment
   versus Level 3-AWIPS identity without adding radar fields to `PaneController`. Catalog mapping
   and non-colliding identity have WxLens model tests; wxdata's bundled Level 3 fixture/provider
   tests remain the parser/provider acceptance seam. Live UI selection/rendering intentionally
   begins in 3B/3F.
2. **3B — Level 3 radial data products.** Port the behavior of the legacy
   `Level3RadialView`, not its Qt/OpenGL implementation. Convert supported radial packets into an
   immutable, presentation-neutral sweep snapshot and reuse/generalize the existing visualization
   layer where the geometry contract genuinely matches. Preserve Level 3 thresholds, data-level
   decoding, scale/offset metadata, range folding, product timestamp, elevation and default
   palette selection. Verify representative reflectivity, velocity, dual-pol and categorical
   radial fixtures through the real renderer.
   **IMPLEMENTED (2026-08-29):** `BuildLevel3RadialSnapshot` accepts wxdata's parsed Graphic
   Product Messages, prefers packet 16 digital radial data over AF1F radial data, rejects other
   renderer families, applies the product threshold while retaining range-folded bins, and emits
   immutable geographic triangle geometry plus 8-bit moments in `RadarSweepLayer`'s existing
   upload contract. Its presentation-neutral metadata preserves all 16 raw thresholds, decoded
   values/categorical codes, scale/offset, product time, elevation and wxdata's default-palette
   identity. Bundled N0B reflectivity, N1U velocity, N0X dual-pol and N0H categorical fixtures
   exercise that real renderer contract; a non-radial NST fixture verifies family separation.
   Release build and all 115 WxLens model tests pass. Packaged live/archive visual selection remains
   deliberately in 3F, where the product browser and per-pane palette binding are introduced.
3. **3C — Level 3 raster data products.** Port the behavior of `Level3RasterView` for raster and
   precipitation-array packet families. Convert Cartesian bins to geographic GPU geometry in a
   dedicated Data Product implementation rather than pretending they are radial gates. Verify a
   representative raster product and precipitation accumulation product at multiple zooms and
   bearings, including archive selection.
   **PARTIALLY IMPLEMENTED (2026-08-29):** `BuildLevel3RasterSnapshot` now converts wxdata
   packet BA07/BA0F Cartesian bins into immutable geographic triangle geometry for the existing
   GPU upload contract, without presenting the bins as radial gates. It preserves thresholds,
   decoded values/codes, scale/offset, product time, resolution, dimensions and default-palette
   identity. A bundled NCR raster fixture verifies the real geometry contract; packet-family
   detection and the archived DPA fixture verify that packet 17/18 precipitation arrays fail
   explicitly instead of silently rendering empty. Full packet 17/18 rendering is blocked by
   wxdata's public API: both packet classes parse their row payload internally but expose only
   dimensions and `data_size()`. Per ADR 0002 and this roadmap's upstream-first rule, decoded-row
   accessors must land in the legacy wxdata repository before WxLens can consume them; do not
   re-parse Level 3 bytes here or edit the vendored submodule. Packaged visual verification at
   multiple zooms/bearings remains in 3F after live product selection is wired.
4. **3D — Level 3 graphic/vector overlays.** Translate storm IDs/tracks, hail and mesocyclone
   symbols, point features, linked/unlinked contours and vectors, vector arrows, and wind barbs
   into presentation-neutral geographic overlay primitives above radar and below the User
   Analysis Layer. Connect real Storm Tracking Information selection to the already-reserved
   `SelectedStorm` synchronization channel. Do not store meteorological graphics as user-created
   `MapObject`s.
   **PARTIALLY IMPLEMENTED (2026-08-29):** `BuildLevel3GraphicOverlaySnapshot` translates the
   wxdata packet families that currently expose decoded geometry into immutable geographic,
   presentation-neutral primitives: storm IDs, SCIT past/forecast linked tracks, HDA hail,
   mesocyclones, point features/symbols and STI circles. These objects remain product-owned and
   never enter `MapObjectStore`. The bundled NST fixture verifies real storm identities and
   multi-point track geometry. `PaneController` now backs the reserved `SelectedStorm` channel
   with real state; grouped panes propagate selection once with the existing origin guard.
   Rendering/binding this snapshot above radar remains for the next continuation of 3D (and live
   product selection remains in 3F). wxdata currently withholds decoded geometry accessors for
   unlinked vectors, linked/unlinked contours, vector arrows and wind barbs; those accessors must
   land upstream before WxLens can translate those families. Do not re-parse their private packet
   bytes here or edit the vendored submodule.
5. **3E — Level 3 graphic/tabular text.** Expose graphic annotations, tabular alphanumeric
   blocks and text-only products in a product-details surface associated with the pane and selected
   time. Preserve raw/unavailable metadata honestly; do not fabricate geographic placement for
   packets that have none. Verify representative graphic and tabular fixtures.
   **PARTIALLY IMPLEMENTED (2026-08-29):** `BuildLevel3TextSnapshot` exposes graphic annotation packets
   and tabular alphanumeric pages through one immutable, presentation-neutral product-details
   contract, preserving product time/code, AWIPS identity, raw annotation value and page I/J
   coordinates, and explicit block availability. Raw I/J positions are deliberately not promoted
   to geographic coordinates because the graphic page supplies no geographic anchor.
   `BuildTextProductSnapshot` adapts wxdata AWIPS text-only messages to the same contract while
   explicitly recording that geographic placement is unavailable. Real NST graphic and SPD
   tabular fixtures verify both Level 3 families, and a radial fixture verifies honest absent-block
   metadata. Live per-pane binding and the visible details control land with 3F's product browser,
   because the current pane service only loads Level 2 data and adding a second temporary Level 3
   selection path here would violate the slice boundary.
6. **3F — Product browser, palettes and Phase 1 acceptance.** Replace the temporary cycling
   control with a categorized per-pane product browser showing product description, AWIPS ID,
   availability, timestamp, loading/error state, and elevation only where meaningful. Select the
   bundled product-default palette while retaining user overrides and per-channel palette/product
   synchronization. Exercise simultaneous Level 2, Level 3 radial, Level 3 raster, storm overlay
   and text products across independent and synchronized panes; record decode/render performance;
   rerun all Phase 1 acceptance criteria.
   **PARTIALLY IMPLEMENTED (2026-08-29):** each pane now exposes a categorized browser backed by
   the site's discovered Level 3 catalog alongside the seven Level 2 moments. Entries carry an
   unambiguous family/identity tuple, description, AWIPS ID and availability; the surface reports
   catalog loading/failure and the pane's time/loading/error state remains independently exposed.
   Product synchronization now propagates family + canonical identity + name, preventing a Level
   2 label from colliding with a Level 3 AWIPS selection. Palette override state is per pane and
   participates in the existing Palette channel; selecting a different product clears the override
   so its bundled default can take effect. The repeatable performance capture protocol is recorded
   in `docs/performance-baseline.md` without fabricating measurements. Live Level 3 binding into
   the renderer, visible overlay/details surfaces, per-pane LUT application, packaged visual
   family acceptance, actual performance measurements, and the phase-wide release gates remain
   open; therefore neither 3F nor Phase 1 is complete.

   **NEAR-TERM PRODUCT/TILT PRESENTATION FOLLOW-UP (added 2026-09-01):** reorganize the browser
   around meteorological product families, not transport levels. Level 2 is the recommended source
   for base Reflectivity, Velocity, Spectrum Width, ZDR, Correlation Coefficient and Differential
   Phase; label the applicable low-cut data as super-resolution without hiding that the source is
   Level 2. Derived/specialized products such as Echo Tops, VIL, storm-relative velocity,
   hydrometeor classification, accumulation and storm tracking remain first-class Level 3
   families. Level 3 base-product AWIPS variants such as CC `N0C`/`NAC`/`N1C`/`NBC`/`N2C`/`N3C`
   are **tilt/source variants beneath Correlation Coefficient**, not separate ordinary products at
   the same hierarchy level. Apply the same treatment to the corresponding reflectivity,
   velocity, ZDR and KDP families.

   The normal picker shows one friendly family row and a tilt selector. An expert expansion exposes
   source (`Level 2 raw` or `Level 3`), resolution/availability, and the exact AWIPS identity. Add a
   persisted product-display preference with **Angle**, **AWIPS code**, and **Both** choices for
   Level 3 tilt labels; default to **Both** until usability testing demonstrates that angle-only is
   clearer. Where the loaded product reports an actual elevation, display that value; otherwise
   mark the code's nominal angle as approximate rather than fabricating an exact cut. Recommended
   variants must be selected deliberately by canonical default and site availability, never merely
   because an AWIPS id happened to appear first in provider or container order. Preserve the full
   identity internally for caching, archive requests, synchronization and diagnostics even when the
   friendly UI hides it.

   **RUNTIME CONTINUATION (2026-08-29):** the per-pane selection now drives the Level 3 provider
   and binds the returned file into the existing visualization layer. Radial and BA07/BA0F raster
   snapshots publish through the same immutable GPU contract as Level 2; decoded storm/product
   graphics remain product-owned and render in a separate geographic overlay above radar; graphic
   and tabular text appears in a pane-associated details surface. Per-pane palette selection uses
   the real `.pal` parser/LUT builder and participates in the existing Palette sync channel.
   Release compilation, QML AOT compilation, deployment, and an eight-second packaged-process
   launch smoke check pass. The full CTest run reports 324/327 passing: the optional unbuilt
   MapLibre test is not run and two unchanged legacy wxdata tests fail against the live IEM
   endpoint. Live visual selection across every renderer family, storm hit-selection, actual
   performance capture, packet 17/18 upstream accessors, and the release-readiness gates below
   remain open, so Phase 1 is still not complete.

**Level 3 completion rule:** Phase 1 cannot be marked complete until at least one real fixture or
live/archive product from every applicable renderer family (radial, raster, graphic/vector, and
graphic/tabular text) passes its automated tests and the packaged application's visual acceptance
path. A populated product picker or successful parser call alone is not coverage.

#### Phase 1 completion and release-readiness gates

The feature slices above are not, by themselves, permission to call Phase 1 complete or publish a
release. Before that milestone, explicitly close and record evidence for every gate below. Keep
unfinished items visible here rather than treating them as implied polish:

The dated packaged-session defects and progressive-disclosure requests are tracked as actionable
implementation/retest items in `docs/phase1-ux-feedback-2026-08-31.md`. Closing a broad gate below
does not silently close an unchecked item in that record; reconcile both checklists during each
acceptance rerun.

- [ ] **Settings coverage for every promised preference.** Verify persistence, defaults, reset
  behavior, addressable settings-section navigation, and actual runtime application for product
  defaults, unit preferences, map-provider choice, per-object-kind default scope, workspace/pane
  layout, and floating-versus-docked controls. Active theme/palette, preferred measurement tool,
  measurement gesture, retain/clear-on-tool-deactivation behavior, snap tolerance/suppress
  modifier, geometry-row visibility, map details, optional-toolbar-control visibility/order, and
  other preferences already promised elsewhere in this roadmap remain part of the same audit.
  Include the default radar-site scope (**All panes** / **Active pane only**), the explicit
  apply-to-current-workspace action, link-menu preset default, Level 3 tilt-label format
  (**Angle** / **AWIPS code** / **Both**), and - once slice 18 lands - radar-site marker
  visibility and the TDWR filter (§4.10).
  Do not silently resolve open question 11 while adding the floating/docked preference.
- [ ] **Phase-wide UX and acceptance validation.** Rerun every criterion in §4.8 against the
  packaged application, including primary pointer gestures and keyboard paths, and record which
  automated and manual test provides evidence for each criterion. Validate representative dense
  multi-pane layouts as well as the easy 1×1 case. Audit the persistent toolbar against §5.3's
  constrained-chrome rule: the curated default must remain minimal, all hidden capabilities must
  remain discoverable, customization/reset must work, and narrow layouts must overflow cleanly.
- [ ] **Performance baselines on a modest laptop.** Record hardware, display resolution, pane
  layout, products/data sources, and measurement method alongside FPS/frame time (including
  stalls), CPU and GPU utilization, memory/working-set behavior, radar decode latency, network
  request volume/latency, cache hit/miss behavior, and repeated/live-update behavior. Establish
  reproducible baselines before optimization and document regressions or accepted limits.

  **Target class decided 2026-09-03** (the machine the baselines must be acceptable on, not the
  development machine): 14"–15.6" display; Intel Core i5 or AMD Ryzen 5; 16 GB RAM as the
  reference point with **8 GB treated as the low-end floor that must still work**; 256–512 GB SSD.
  Integrated graphics are implied by that CPU class, so the renderer's baselines must hold without
  a discrete GPU. Baselines gathered on the development machine are *reference numbers only* and
  do not close this gate.
- **Packaging, release, and update readiness — moved out of Phase 1** (2026-09-03). Now the
  Release-prep milestone below. It gated Phase 1 on calendar-bound external things (certificate
  issuance, a clean machine, CI runners) rather than on the quality of the code Phase 2 builds on,
  so it stalled architecture work for reasons unrelated to the software. Nothing is dropped; the
  work is sequenced separately and its decisions are already recorded there.

  **Update behavior decided 2026-09-03:** ask the end user, defaulting to automatic updates. The
  first-run path offers the choice, the preference is persisted like any other setting, and
  automatic is the shipped default. Deliberately **not implemented yet**: there is no updater, so
  no dead toggle ships ahead of one — a preference the app cannot honour is worse than none. The
  implementing work owns the `updates` settings section, the first-run prompt, and the mechanism,
  and must honour this decision rather than re-deciding it.
- [ ] **First-run usability and general polish.** Validate a new-user path from empty settings to
  a useful live radar view; accessible names/roles, contrast, text scaling, focus visibility and
  reduced-motion implications; complete keyboard navigation and shortcuts; actionable loading,
  empty, permission, provider-failure, corrupt-data, and offline/cache states; and recovery without
  restarting or losing the workspace. Check error messages and logs without exposing secrets.
- [x] **“WxLens” name and trademark due diligence.** Before registering domains/accounts or
  publishing broadly, search relevant software, weather, and mapping products plus applicable
  trademark databases for confusingly similar names. Record the date, jurisdictions/databases,
  findings, and resulting decision. This is practical collision screening, not a substitute for
  legal advice when registration or material distribution warrants counsel.

  *Screened 2026-09-03 — see [name-screening-2026-09-03.md](name-screening-2026-09-03.md).* No
  collision found for `WxLens` as a product, app, domain or mark. Registration is not being
  pursued, so this closes as collision screening only; the record names what was **not** queried
  (USPTO/WIPO/EUIPO directly) and the conditions that require re-running it.

**Completion rule:** Phase 1 closes only when the Level 3 completion rule and every gate above
has evidence in the repository (tests, CI/release configuration, or a dated validation record),
with any consciously deferred limitation named and accepted rather than silently omitted.

#### Release prep (runs alongside Phase 1+; does not gate it)

Split out of the Phase 1 gates on 2026-09-03. This work is paced by things outside the codebase,
and none of it constrains the architecture later phases build on, so it proceeds on its own
schedule. It **does** gate publishing a build to anyone who is not the developer.

Decisions taken 2026-09-03 (owner: project owner; WxLens is to be a free product, which is why
cost is weighted heavily here):

- **Code signing — ship unsigned for now.** Windows SmartScreen will show an "unknown publisher"
  prompt that users must click through, which is accepted. Revisit if the audience grows beyond
  people who were told where the download came from; the cheapest legitimate path at that point is
  a managed signing service rather than an EV certificate, whose only real advantage is immediate
  SmartScreen reputation that matters at a download volume this project does not have.
- **Installer — Inno Setup, with a portable ZIP alongside.** Gives the uninstall behaviour the
  gate requires, scripts easily in CI, and is well-trodden for Qt + `windeployqt`. MSIX was
  rejected because its sandbox fights an OpenGL application that reads and writes user config and
  `.pal` files from arbitrary paths; MSI/WiX is enterprise complexity with no audience here.
- **Updates — automatic check, manual apply.** The app checks for a newer version on its own and
  tells the user; the user chooses when to install. This delivers the intent ("the user should not
  have to remember to check") without privilege elevation, rollback and delta-update machinery
  that only pay off with an installed base. Full background auto-update stays a later option, and
  the preference that controls it ships with the mechanism, not before it.

- [ ] Clean-machine installation, uninstall and repair behaviour.
- [ ] Runtime dependency and license/attribution packaging (ACKNOWLEDGEMENTS.md ships with the app).
- [ ] CI across supported configurations.
- [ ] Artifact versioning and release-channel mechanics.
- [ ] Implement the update check and its preference together.

### Phase 2 — Multi-site mesh/mosaic radar
**Goal:** see whole storm systems across individual radar site coverage boundaries.
**Scope:** a national/regional mosaic reflectivity (and optionally echo-tops/precip) layer as a
togglable pane content type alongside single-site views, participating in the same per-channel
sync system as single-site panes.
**Key technical work:**
- New `MosaicDataService` following the same per-source-key-singleton pattern as
  `RadarSiteDataService` — the payoff of designing that pattern generically in Phase 1.
- MRMS provider (§6): IEM tile bootstrap first, raw MRMS-on-AWS GRIB2 as the full-control
  follow-up.
- Mosaic tiling/LOD strategy for "modest laptop" perf — likely a coarser base resolution with
  viewport-driven detail loading (standard tile-pyramid pattern). This is genuinely new
  engineering with no direct precedent in the current app.
- **[OPEN QUESTION]** whether the national mosaic layer alone satisfies "see the whole storm
  across site boundaries," or whether bespoke edge-blending between adjacent single-site panes
  is still wanted. Recommend deciding after using the mosaic layer in practice, not before.
**Size:** L.

### Phase 3 — Additional data layers + velocity improvements
**Goal:** togglable overlay stacking (satellite + radar + soundings + jet stream + pressure)
plus the lower-priority velocity-improvement sub-track.
**Scope:** satellite imagery (GOES via AWS), sounding data view (Wyoming/RUC — **[OPEN
QUESTION]** full interactive Skew-T diagram vs. simpler tabular display first), jet stream +
MSLP overlays (NOMADS GRIB2), overlay toggle/opacity UI in chrome; velocity dealiasing;
direction-relative velocity color mode via vendored `hsluv-c`.
**Key technical work:**
- New provider modules (`SatelliteDataService`, `SoundingDataService`, `ModelGridDataService`),
  same singleton-cache pattern.
- GRIB2 + NetCDF4 decoder adoption (license/footprint review per §0/§6 first).
- Geostationary reprojection for GOES imagery — **[OPEN QUESTION]** GDAL (heavy, capable,
  well-trodden) vs. a purpose-built lighter transform; decide at kickoff once real GOES
  size/perf data is in hand.
- Overlay stacking/compositing in the render layer — layer order + opacity per overlay.
- Dealiasing module (self-contained, no new dependency).
- Direction-relative color mode as a per-pane/per-product toggle, alternative to the existing
  `.pal`-driven toward/away coloring.
**Size:** L (data layers) + M (velocity sub-track). Recommend splitting into two sequenced
sub-phases (3a: satellite+model overlays, 3b: velocity improvements) since they're technically
independent and velocity work carries no data-source risk.

### Phase 4 — 3D storm structure rendering
**Goal:** volumetric storm structure visualization (debris ball, etc.) — explicitly last
priority, "last of the last" per the user.
**Scope:** an opt-in 3D view mode reconstructing a volumetric surface from a full
multi-elevation Level 2 volume scan (already parsed via `wxdata/wsr88d/rda/` — no new parsing
work), with debris-ball-style highlighting using co-located low correlation-coefficient + high
reflectivity + TVS detection (already parsed in `wxdata/wsr88d/rpg/`).
**Key technical work:** isosurface/point-cloud reconstruction from stacked elevation cuts (e.g.
marching-cubes-style extraction, or a simpler point-sprite volumetric renderer as a lighter first
cut), rendered via Qt Quick 3D or a dedicated Qt RHI 3D render node; 3D camera controls;
performance work to keep this optional/toggleable so it doesn't tax the "modest laptop" baseline
when unused.
**Size:** XL, genuinely exploratory. Recommend a design-spike sub-task before committing to a
specific volumetric technique — no reference implementation exists anywhere in the current
codebase for this.

### Phase 5 — Stretch: multi-user/server/login/mobile companion
**Goal:** long-term stretch only, explicitly not designed or built now — captured so Phases 0–3's
config/data-model choices don't accidentally preclude it later.
**Scope:** not detailed, per the user's own instruction. Directional notes only:
- "Club-based system" framing (small teams sharing one deployment) points toward a lightweight
  self-hosted sync server rather than public multi-tenant SaaS — informs eventual hosting/auth
  choice without deciding it now.
- The structured, portable config/theme/palette formats chosen in Phases 0/1/5.2 (plain
  TOML/JSON, not registry-coupled) are precisely what makes a later sync layer straightforward,
  provided those choices are honored now.
- Mobile companion: Qt's mobile deployment story means the data layer (`wxdata`, provider
  services) and possibly a trimmed QML chrome could realistically be shared with a phone build
  rather than a from-scratch second app — worth a design-spike when this phase is greenlit.
**Size:** unscoped (XL+ when eventually planned in detail).

---

## 8. Feature backlog (captured now, mostly low/no priority — don't build speculatively)

- Cross-site mesh linking / edge-blending between adjacent single-site panes (Phase 2 open
  question — may be superseded by the mosaic layer).
- Mobile/phone companion app (Phase 5).
- Multi-user/server/login/sync, club-based not public SaaS (Phase 5).
- Named/saved multi-pane layouts (beyond current-layout persistence in §4.6) — natural
  extension once sync groups exist.
- Additional overlay data layers beyond the Phase 3 set (lightning, SPC mesoanalysis, storm
  reports) — natural extension of overlay-stacking architecture once it exists; add if/when
  requested, not speculatively.
- **County boundary overlay** — requested by the user 2026-09-02, wanted fairly soon. Two viable
  paths: (a) near-zero-effort, works today — add a GR-format county-line placefile through the
  existing Weather Overlays dialog (`overlays::OverlayManager` already renders placefile
  Line/Polygon items); (b) a first-class "Counties" toggle with its own styling and real
  performance at national scale needs GPU rendering instead of `WeatherOverlaysLayer.qml`'s
  per-frame QML Canvas redraw. For (b),
  [app/source/wxlens/render/polyline_layer.hpp](../app/source/wxlens/render/polyline_layer.hpp)
  is a salvaged-but-unwired starting point (a generic GL custom-layer line renderer, ported
  2026-09-02 from the abandoned slice-12 branch, tag `archive/slice-12-warnings-placefiles`) —
  not in `app/CMakeLists.txt` yet and unverified against the current codebase; wire it in and
  recompile against current APIs when a consumer (e.g. a `CountyLayer`/`CountyDataService` pair
  loading a bundled Census TIGER/Line county dataset) is actually built.
- Skew-T sounding diagram as a first-class view vs. raw data access (Phase 3 open question).
- Publishing `wxdata` as a proper Conan package (vs. submodule + `add_subdirectory`).

---

## 9. Open questions for the user / other planning agents

1. ~~Final app/brand name~~ — **RESOLVED: the app is named `WxLens`.** (Before registering a
   domain/GitHub org, do a basic trademark/name-collision check against RadarOmega, RadarScope,
   GRLevelX/GR2Analyst, and any existing "WxLens" weather software, as a normal due-diligence
   step — not expected to be a blocker, just unverified as of this roadmap.)
2. **`wxdata` extraction timing** (§3.1): start with Option A and defer the live-repo extraction
   (Option B), or do the extraction against the current shipping app's repo immediately? This
   touches the *existing* repo, so it's the user's call — recommend deferring.
3. ~~Namespace/directory token~~ — **RESOLVED: C++ namespace and directory token is `wxlens`**
   (e.g. `namespace wxlens { ... }`, repo root `wxlens/`, backend source at
   `app/source/wxlens/`).
4. **Cross-site mesh/edge-blending vs. mosaic-layer-is-sufficient** (Phase 2) — decide after the
   mosaic layer is usable in practice, not before, per the recommendation in §7 Phase 2.
5. **Skew-T/sounding UI form factor** (Phase 3) — full interactive diagram, or simpler
   tabular/text display first with a diagram as a later enhancement?
6. **GOES reprojection dependency** (Phase 3) — GDAL vs. a purpose-built lighter transform;
   decide at Phase 3 kickoff with real imagery size/perf data in hand.
7. **GRIB2/NetCDF4 decoder library choice** (eccodes vs. NCEPLIBS-g2c; full netcdf-c vs. a
   narrower reader) — needs a license + binary-footprint review at kickoff, per §0.
8. **Phase 5 hosting/auth approach**, whenever the multi-user stretch goal is greenlit —
   explicitly not to be decided now, listed only so it isn't lost.
9. ~~MapLibre Native Qt's current QML-item support~~ — **RESOLVED, see
   `docs/adr/0004-maplibre-qml-integration.md`**: confirmed genuine `QQuickItem` support via
   `src/quick` (BSD-2-Clause), no `QQuickWidget` fallback needed. WxLens uses the `Quick` module
   (QML type `MapLibre`), not the `Location` (QtLocation, LGPL/GPL) or `Widgets` module.

10. **Grid-layout icon sketches** (§5.5) — the user is drawing reference glyphs for the pane-
    layout buttons. Open until those arrive: whether the layout picker is a fixed preset row, or
    a short preset row plus a custom rows×columns picker for arrangements the presets don't
    cover.
11. ~~**Floating vs. docked default for the bottom control cluster** (§5.4)~~ — **RESOLVED
    2026-09-02: floating is the shipping default.** At 1280x800 in a real 3×3 layout the floating
    bar obscures a substantial strip across all three bottom panes; docked mode correctly reserves
    54 px and obscures none. The user reviewed that tradeoff and explicitly selected floating.

---

## Critical files for implementation (in the current repo — read before writing new-repo code)

- [AGENTS.md](AGENTS.md) and [CLAUDE.md](CLAUDE.md) — the structural pattern to replicate.
- [wxdata/source/scwx/wsr88d/rpg/level3_message_factory.cpp](wxdata/source/scwx/wsr88d/rpg/level3_message_factory.cpp)
  and [wxdata/source/scwx/provider/](wxdata/source/scwx/provider/) — confirms exactly what radar
  parsing/fetching is already solved and must not be re-derived.
- [scwx-qt/source/scwx/qt/manager/radar_product_manager.hpp](scwx-qt/source/scwx/qt/manager/radar_product_manager.hpp) —
  per-site singleton/shared-cache pattern to port.
- [scwx-qt/source/scwx/qt/map/map_link_policy.cpp](scwx-qt/source/scwx/qt/map/map_link_policy.cpp) and
  `map_pane_view_link_state.cpp/.hpp` — the feedback-loop guard to generalize to per-channel sync.
- [scwx-qt/source/scwx/qt/map/map_annotation_types.hpp](scwx-qt/source/scwx/qt/map/map_annotation_types.hpp)
  and `map_annotation_model.cpp`/`map_annotation_layer.cpp` — the existing `MapAnnotationObject`
  model to evolve into the unified, scope-aware `MapObject` family (§4.3).
- [scwx-qt/source/scwx/qt/util/geographic_lib.hpp](scwx-qt/source/scwx/qt/util/geographic_lib.hpp) —
  Qt-free WGS84 geodesic distance/bearing math for the measurement tool (§4.4), **and** the
  already-implemented `GetRadarBeamAltititude` for beam-height interrogation (§4.7) — reuse both,
  don't re-derive.
- [wxdata/source/scwx/common/color_table.cpp](wxdata/source/scwx/common/color_table.cpp) (engine)
  and [scwx-qt/source/scwx/qt/util/palette_file.hpp](scwx-qt/source/scwx/qt/util/palette_file.hpp)
  (editor model) — the two halves of the palette system.
- `conanfile.py`, `CMakeLists.txt`, `CMakePresets.json`, `.github/workflows/ci.yml` — build/CI
  tooling to copy and adapt.

## Verification

This is a planning document, not code — there's nothing to run yet. Verification for Phase 0
once executed: new repo builds on at least one platform via its CI, `wxdata`'s existing GTest
suite passes unmodified inside the new repo, and the empty Qt Quick shell launches and renders a
MapLibre map. Verification for later phases is stated per-phase above (each phase's "Scope"
list is the acceptance criteria for that phase).
