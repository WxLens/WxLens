# Nimbus — Ground-Up Rewrite Roadmap

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
**app named `Nimbus`** (namespace token `nimbus` used throughout the C++ code and directory
layout), **multi-user/server/login is a long-term stretch goal only**
(don't build it now, don't design around excluding it), and this **feature priority order**:
(1) modernized single-site radar UI, (2) multi-site mesh/mosaic radar, (3) other data layers
(satellite, soundings, jet stream/pressure) + velocity improvements as a lower-priority
sub-track, (4) 3D storm structure rendering — explicitly last.

**New workspace location (confirmed):** `C:\Users\sherw\OneDrive\Apps\Nimbus`. This is a new,
separate directory/repo from the current app at `C:\Users\sherw\OneDrive\Apps\Supercell Wx` — the
latter stays in place, untouched, as the read-only reference source described throughout this
document (§2, and via Option A's submodule/reference approach in §3.1). The very first action
once implementation begins (start of Phase 0) is standing up this new directory and saving this
roadmap to `C:\Users\sherw\OneDrive\Apps\Nimbus\docs\ROADMAP.md`, per §3.2's layout.

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
documentation. The goal isn't just to make `Nimbus` work — it's to make it possible for
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
`C:\Users\sherw\OneDrive\Apps\Supercell Wx`** (not the Nimbus repo this roadmap now lives in) —
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
nimbus/
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
│   ├── source/nimbus/               # C++20 backend, namespace `nimbus`
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
├── test/                           # GTest, mirrors app/source/nimbus/ tree
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
(`app/source/nimbus/objects/`):

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

### 4.4 Measurement as a reusable interaction framework

Not limited to radar-site-to-click; the measurement tool supports several explicit modes, all
built on `util::geographic_lib`'s already-existing WGS84 geodesic math (`GetDistance`, `GetAngle`,
`GetCoordinate` — confirmed Qt-free and directly reusable, see §2):

- **Point → Point:** click A, click B. Shows distance, forward bearing (`GetAngle(a,b)`), reverse
  bearing (`GetAngle(b,a)`), and both coordinates.
- **Radar → Point:** explicitly preserved as a named mode — radar site coordinate as point A
  (already known per-site), clicked point as B, displayed as range/azimuth. This generalizes
  what `radar_coordinate_table.cpp`'s range/azimuth grid already computes implicitly into an
  explicit, user-facing tool.
- **Multi-point path:** extend the existing `MapAnnotationMeasure{a,b}` payload into a path
  payload (`points: vector<Coordinate>`) with per-segment distance/bearing and a running total.
- **Point info:** a clicked point yields lat/lon, range/azimuth from the pane's currently
  selected radar site, and — once Phase 3's data layers exist — whatever satellite/model value
  is present at that point; Phase 1 delivers the radar-relative info only, with the tool designed
  so additional data probes plug in later without a rework.
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
  Qt-signal update notification) into a new `RadarSiteDataService` in `app/source/nimbus/data/`,
  implementing the Data Source role for radar. Multiple panes showing the same site — synced or
  not on any channel — share one instance and one download/cache. Generalize this *pattern*
  (per-source-key singleton + shared cache) to the Phase 2/3 non-radar providers too
  (`MosaicDataService`, `SatelliteDataService`) as they're added, without changing the pipeline
  shape.
- **Pane grid model** (`app/source/nimbus/panes/`): a `QAbstractListModel`-backed `PaneGridModel`
  holding `N = gridWidth*gridHeight` `PaneController` instances, each owning its per-channel sync
  state (§4.1), a binding to its current Data Product (radar site+moment+tilt today, via
  `app/source/nimbus/products/`), and its own MapLibre camera state — a pane's "what data is
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

- A `ThemeManager` C++ singleton (`app/source/nimbus/theme/`) exposes named color/metric roles
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
  Fits under `app/source/nimbus/render/` or a new `app/source/nimbus/velocity/`.
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
  originally assumed) — those four are now vendored at Nimbus's own `external/` too.
- [x] Stand up `CMakeLists.txt`/`CMakePresets.json`/`conanfile.py`/`tools/` by copying and adapting
  the current repo's build tooling. Presets currently cover `windows-vs2026-x64` (verified against
  the local dev machine) and a `linux-gcc14` template (not yet verified on real Linux hardware —
  follow-up item, not done tonight).
- [x] Confirm MapLibre Native Qt's QML-item support (§1) — **resolved, see
  `docs/adr/0004-maplibre-qml-integration.md`**: it exposes a genuine `QQuickItem` (`src/quick`,
  BSD-2-Clause, no `QtLocation` dependency), no `QQuickWidget` fallback needed. Nimbus vendors the
  upstream `maplibre/maplibre-native-qt` (library version 4.0.0) at `external/maplibre-native-qt`,
  not the `dpaulat` Supercell-Wx-specific fork.
- [x] Write `AGENTS.md`/`CLAUDE.md` for the new repo (§3.3) — done, not stubs, reasonably complete.
- [x] Wire up `util::Logger` with per-subsystem sinks (§3.4) — `app/source/nimbus/log/` wraps
  `scwx::util::Logger`, adding a file sink under `QStandardPaths::AppDataLocation`. Only one
  subsystem exists so far (`main`); extend the naming convention as more subsystems are added in
  Phase 1.
- [x] Decide TOML vs. JSON for config storage — **TOML**, see
  `docs/adr/0003-config-storage-format.md`. `settings/` itself is still an empty directory —
  implementing the wrapper is Phase 1 work (§7 Phase 1 doesn't need it until pane-layout/theme
  persistence), not Phase 0's.
- [x] App is named **Nimbus** (repo `nimbus/`, C++ namespace `nimbus`, see §9 Q1/Q3 — resolved).

**Status as of 2026-08-22 (first overnight pass, verified end-to-end):** directory scaffold, all
vendored submodules, build tooling, and docs are in place; `docs/adr/0001`-`0004` written and
updated as real build issues were found and fixed (see below). `nimbus-app` (empty QML shell, no
MapLibre item wired in yet — that's explicitly the next slice, see ADR 0004's Consequences) and
`nimbus-wxdata-test` (the wxdata-only slice of the legacy GTest suite, referenced from
`external/legacy-supercell-wx/test/` rather than duplicated) are wired into the build.

**Conan install, CMake configure, and a full build were run to completion on the Windows VS2026
preset, and the result was launched and verified, not just compiled:**
- `conan install` + `cmake` configure succeed cleanly (Release config).
- `nimbus-app.exe` and `nimbus-wxdata-test.exe` both build and link.
- `nimbus-wxdata-test`: **185 passed, 12 skipped, 3 failed** (200 total). The 3 failures
  (`AwsLevel2DataProvider.Prune`, `IemApiProviderTest.ListTextProducts`/`LoadTextProducts`) are
  live-network/live-data tests hitting real AWS S3/IEM endpoints — the same category the legacy
  repo's own Cursor Cloud notes (`external/legacy-supercell-wx/AGENTS.md`) already flag as
  environment-dependent and excluded from their CI run. Nothing in the failure output points at a
  Nimbus-introduced defect (see the git history around this date for the actual assertion text).
  Re-verify before assuming they're still purely environmental if this is re-run much later.
- `nimbus-app.exe` was launched for real (not just built): a genuine OS window titled "Nimbus"
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
4. `windeployqt` is not optional on Windows — without it, `nimbus-app.exe` can't find Qt's DLLs or
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
capabilities, publicly documented AWIPS capabilities, and planned `Nimbus` capabilities,
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
- Settings UI (product defaults, unit preferences, map provider choice) backed by the new
  structured config format.
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
    validate the whole phase against §4.8's acceptance criteria.

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
- Second, separate bug found while wiring the plugin into `nimbus-app`: the plugin's own
  `set_target_properties(... LIBRARY_OUTPUT_DIRECTORY/RUNTIME_OUTPUT_DIRECTORY ...)` call (generic,
  non-per-config properties) loses to Nimbus's own `tools/nimbus_config.cmake`, which sets
  per-config `CMAKE_*_OUTPUT_DIRECTORY_RELEASE` globally - CMake applies those to every new
  target's per-config property *at creation time*, and a per-config property always wins over a
  later generic-property override on a multi-config generator (confirmed empirically: the actual
  `declarative_maplibre.dll` lands in `Release/lib` alongside every other library in the build,
  while its `qmldir`/`.qmltypes` - written via `configure_file`, unaffected by output-dir
  properties - stay at the plugin's own intended `.../src/quick/plugins/MapLibre/` location). Net
  effect: `$<TARGET_FILE_DIR:declarative_maplibre>` is **not** a reliable way to locate this
  plugin's QML module folder in this build. Worked around by having
  `external/maplibre-native-qt.cmake` capture the real, always-correct qmldir/qmltypes path
  directly as `NIMBUS_MLN_QT_QML_PLUGIN_DIR`, and `app/CMakeLists.txt`'s `POST_BUILD` deploy step
  copies that directory *plus* the actual dll (via `$<TARGET_FILE:declarative_maplibre>`, which
  does correctly resolve to wherever the binary really landed) into
  `<nimbus-app exe dir>/qml/MapLibre` - mirroring exactly where `windeployqt` already places Qt's
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
- **Verified for real, not just built:** `nimbus-app.exe` launched, stayed running, and a
  screenshot (both self-captured and one supplied directly by the user) confirms the chrome
  (top bar, side rail) renders correctly and the map renders actual basemap tiles (US outline,
  state/lake borders, labels), centered on CONUS as configured, after a few seconds for the
  network fetch - an earlier screenshot taken too soon after launch showed a black pane before
  tiles loaded, which is expected load latency, not a bug. `nimbus-wxdata-test` still builds
  clean after these changes (not re-run in full this slice - nothing touched wxdata or its test
  wiring, so this was a build-only sanity check, not a full 200-test re-verification).
- **Not verified this slice:** pan/zoom/pinch interaction (handlers are wired per the ported
  example pattern, but not manually exercised), Linux/macOS (Windows-only session, as with Phase
  0), the CI workflow (still unexercised per Phase 0's status).
- **Next slice:** slice 2, one functional radar pane - wire `RadarSiteDataService`
  (`app/source/nimbus/data/`) to `wxdata`'s existing providers and get one hardcoded product from
  one site flowing end-to-end, still with no pane grid/sync yet.

**Status as of 2026-08-22 — Slice 2 (one functional radar pane) done, verified end-to-end:**
- `app/source/nimbus/data/radar_site_data_service.hpp/.cpp`: a deliberately minimal first version
  of the Data Source role for radar (§4.6) - per-site singleton (`Instance(radarSite)`), fetches
  the latest Level 2 volume via `wxdata`'s existing `NexradDataProviderFactory`/`AwsLevel2DataProvider`
  on a background thread (`scwx::util::async`, wxdata's shared `io_context` helper - reused, not a
  new thread pool), emits `LevelTwoDataLoaded`/`LoadFailed` back on the GUI thread. **Explicitly
  not** RadarProductManager's full port: no caching, no multi-product/elevation tracking, no
  refresh scheduling, no `Cleanup()` - those are follow-up work as the pattern matures (slice 3+),
  not deferred by oversight.
- `app/source/nimbus/data/radar_site_database.hpp/.cpp`: loads `res/config/radar_sites.json`
  (bundled, carried forward unmodified from `scwx-qt/res/config/radar_sites.json` - lat/lon/
  elevation/IANA time zone per site) for lookup by site ID. Also the intended home for the site
  lat/lon/elevation §4.7's beam-height work will need later - reuse this, don't rebuild it then.
- `app/source/nimbus/products/radar_product_status.hpp/.cpp`: a minimal, explicitly temporary
  bridge (`nimbus::products::RadarProductStatus`) exposing site/status text to QML via a
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
     `main.cpp`. Neither existed before since nothing in `nimbus-app` made a network/AWS call
     until this slice's S3-backed provider.
  2. A real NOMINMAX/Windows.h `min`/`max` macro collision: `wxdata.cmake` sets `-DNOMINMAX`
     `PRIVATE` on `wxdata` itself (`external/`, read-only), so it doesn't propagate to consumers.
     `test/test.cmake` already had the same fix for the test target; `app/CMakeLists.txt` never
     needed it until this slice's `radar_product_status.cpp` became the first `nimbus-app` source
     to pull in `scwx::util::TimeString` (→ `date/tz.h` transitively). Fixed by adding the
     identical `target_compile_options(nimbus-app PRIVATE -DNOMINMAX)` under `if (MSVC)` -
     produces a cascade of unrelated-looking `units/core.h` template syntax errors if missed;
     worth recognizing the signature (`warning C4003: not enough arguments for function-like
     macro invocation 'min'` immediately preceding the cascade) if it recurs elsewhere.
- **Verified for real, not just built:** launched `nimbus-app.exe` repeatedly against the live
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

- `app/source/nimbus/render/radar_site_marker_layer.hpp/.cpp`
  (`nimbus::render::RadarSiteMarkerLayer`): draws one fixed-size colored point at a hardcoded geo
  coordinate via a real `QMapLibre::CustomLayerHostInterface` custom layer - proves registration,
  GL context access, and the lat/lon-to-screen projection (ported unchanged from the legacy app's
  `gl/radar.vert`, minus its `precision mediump float;` line - see below) all work through
  Nimbus's actual QML-hosted map, not a synthetic test. Explicitly temporary/superseded once the
  real radar sweep renderer exists, not extended in place.
- `app/source/nimbus/render/radar_layer_controller.hpp/.cpp`
  (`nimbus::render::RadarLayerController`): minimal QML-facing bridge, `Q_INVOKABLE
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
     is a hard syntax error on this desktop GL 3.3 core driver - omitted from Nimbus's copy. Flag
     this when porting `gl/radar.frag` for real; don't carry that line over verbatim.
- **Verified for real:** with all three patches applied, the orange marker renders at KEAX's
  actual coordinates (confirmed against real Kansas City-area geography in a user screenshot),
  the base map renders normally alongside it, and `nimbus-wxdata-test` still builds clean.
- **Not verified this slice:** the actual radar sweep vertex/shader pipeline (not started - this
  pass was entirely the custom-layer plumbing prerequisite), Level 3, sites other than KEAX,
  Linux/macOS (these patches are platform-generic C++/GL fixes with no Windows-specific code, but
  unverified on other platforms regardless).
- **Next slice:** continue slice 3 - port `view::RadarProductView`'s vertex/color-table-LUT
  generation and the real `gl/radar.vert`/`gl/radar.frag` shaders (§2's Critical Files list),
  replacing `RadarSiteMarkerLayer` with the real radar sweep renderer now that the custom-layer
  registration mechanism underneath it is proven and unblocked.

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
- Skew-T sounding diagram as a first-class view vs. raw data access (Phase 3 open question).
- Publishing `wxdata` as a proper Conan package (vs. submodule + `add_subdirectory`).

---

## 9. Open questions for the user / other planning agents

1. ~~Final app/brand name~~ — **RESOLVED: the app is named `Nimbus`.** (Before registering a
   domain/GitHub org, do a basic trademark/name-collision check against RadarOmega, RadarScope,
   GRLevelX/GR2Analyst, and any existing "Nimbus" weather software, as a normal due-diligence
   step — not expected to be a blocker, just unverified as of this roadmap.)
2. **`wxdata` extraction timing** (§3.1): start with Option A and defer the live-repo extraction
   (Option B), or do the extraction against the current shipping app's repo immediately? This
   touches the *existing* repo, so it's the user's call — recommend deferring.
3. ~~Namespace/directory token~~ — **RESOLVED: C++ namespace and directory token is `nimbus`**
   (e.g. `namespace nimbus { ... }`, repo root `nimbus/`, backend source at
   `app/source/nimbus/`).
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
   `src/quick` (BSD-2-Clause), no `QQuickWidget` fallback needed. Nimbus uses the `Quick` module
   (QML type `MapLibre`), not the `Location` (QtLocation, LGPL/GPL) or `Widgets` module.

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
