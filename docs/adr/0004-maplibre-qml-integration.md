# ADR 0004: MapLibre Native Qt QML integration — verified, and which module to use

## Status
Accepted (2026-08-21)

## Context
`docs/ROADMAP.md` §1 and §9 (Q9) flagged an open factual question blocking Phase 0's close: does
the pinned MapLibre Native Qt expose a `QQuickItem`-native map surface, or only a `QWidget`-based
`QMapLibre::Map` requiring `QQuickWidget` interop? This had to be verified before committing to
the rendering-seam plan in §1/§7 Phase 1 slice 3.

Investigated `external/maplibre-native-qt` (upstream `maplibre/maplibre-native-qt`, tag-less HEAD
at `VERSION.txt` 4.0.0, newer than the `supercell-wx-v0.5.2` / library-version-3.0.0 fork already
vendored in the legacy Supercell Wx repo). It ships **two distinct QML integration paths**:

1. **`src/location/`** — a `QtLocation` geo-services plugin (`import QtLocation` +
   `Plugin { name: "maplibre" }` + `MapView`/`Map`). Backed by `qgeomap.cpp`, which carries
   `SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only` (inherited from
   Qt Company/Mapbox-derived code). Usable under the LGPL-3.0-only option with dynamic linking,
   consistent with this project's existing Qt LGPL discipline (ADR 0001) — but it's the
   more license-encumbered of the two paths and pulls in the full `QtLocation`/`QtPositioning`
   plugin machinery.
2. **`src/quick/`** — a standalone native `QQuickItem` (`map_quick_item.hpp`/`.cpp`, QML type
   `MapLibre`, C++ namespace `QMapLibre`, CMake component `find_package(QMapLibre COMPONENTS Core
   Quick)`), with **no QtLocation dependency**. Every file under `src/quick/` (including
   `map_quick_item.*` and the `declarative_*_parameter` style/source/layer helpers under
   `src/quick/common/` and `src/quick/plugins/`) is `SPDX-License-Identifier: BSD-2-Clause` —
   fully permissive, no GPL/LGPL entanglement at all.

Both confirmed present in `examples/quick/` (`QtLocation`-based) and `examples/quick-standalone/`
(`src/quick`-based) respectively, and both are genuine `QQuickItem`s usable directly inside a
`.qml` file with no `QQuickWidget` fallback needed. Custom-layer compositing (needed for the
ported radar renderer, §1/§7 Phase 1 slice 3) is exposed on the shared core (`Map::addCustomLayer`
+ `CustomLayerHostInterface::render`, `src/core/map.hpp`/`types.hpp`), so it's available to either
QML path.

## Decision
- **Confirmed:** the open verification item is resolved. MapLibre Native Qt exposes a genuine
  `QQuickItem` map surface; no `QQuickWidget` interop fallback is needed. §1's integration mechanics
  and §7 Phase 1 slice 3 proceed as planned.
- **Use `src/quick` (`QMapLibre` / QML type `MapLibre`, CMake component `Quick`), not
  `src/location`.** It's BSD-2-Clause end to end, sidesteps the LGPL/GPL triple-license path
  entirely (simpler than "link dynamically and pick the LGPL option," which the `QtLocation` path
  would require), and doesn't need the extra `QtLocation`/`QtPositioning` plugin registration
  machinery Nimbus has no other use for.
- Nimbus's own `external/maplibre-native-qt` submodule points at the upstream
  `maplibre/maplibre-native-qt` repo (not the `dpaulat` Supercell-Wx-specific fork) at its current
  HEAD (library version 4.0.0), since Nimbus has no dependency on that fork's Supercell-Wx-specific
  patches. Its nested `vendor/maplibre-native` submodule (the native rendering core) is initialized
  shallow (`--depth 1`) for the same clone-cost reasons as ADR 0002.

## Consequences
- `app/CMakeLists.txt` links `QMapLibre::Core` and `QMapLibre::Quick` (not `QMapLibre::Location`).
- `qml/Panes/`'s map host component uses the `MapLibre` QML item directly, not `QtLocation`'s
  `Map`/`MapView`.
- If a future need for `QtLocation`-specific geo-services integration arises, `src/location`
  remains available and license-compatible (LGPL-3.0-only, dynamically linked) — this decision
  doesn't rule it out, it just means Phase 1's default path doesn't need it.
- `ACKNOWLEDGEMENTS.md` must credit MapLibre Native Qt (BSD-2-Clause for the path in use) and note
  the `vendor/maplibre-native` core it wraps.

## Follow-up finding (still Phase 0, same session): QML plugin wiring needs its own work
MapLibre Native Qt ships a helper, `qmaplibre_quick_setup_plugins(<target>)`
(`src/quick/macros.cmake`), that's supposed to make the `import MapLibre` QML module visible to an
app target. **It only works for the `find_package(QMapLibre)` (pre-built/installed) consumption
path** — it reads `IMPORTED_LOCATION_<config>`/`IMPORTED_CONFIGURATIONS` off the `QMapLibre::*`
targets, properties that only exist on targets created by a package's exported/imported config
(what `find_package` generates), not on ordinary in-tree targets from `add_subdirectory` in the
same CMake run (what Nimbus and the legacy Supercell Wx repo's `external/` both use for every other
vendored dependency).

There's no existing precedent to copy in this codebase either: the legacy `scwx-qt` app never uses
MapLibre's QML surface at all — it drives `MLNQtCore` directly from a `QOpenGLWidget`-hosted
QWidgets renderer, which is exactly the architecture this whole rewrite is moving away from.

**Concrete next-slice work (§7 Phase 1 slice 1/2 territory, not resolved yet):** wire the
`declarative_maplibre` QML plugin target (`MLN_QT_QML_PLUGIN` in
`src/quick/plugins/CMakeLists.txt`, aliased `QMapLibre::PluginQml`) into an in-tree `nimbus-app`
build by hand — likely `target_link_libraries(nimbus-app PRIVATE declarative_maplibre)` plus
pointing `QT_QML_IMPORT_PATH`/`QML_IMPORT_PATH` at that plugin's build output directory (it's fixed
to `<build-dir>/.../src/quick/plugins/MapLibre` via `OUTPUT_DIRECTORY "MapLibre"` in that
CMakeLists.txt) rather than relying on `qmaplibre_quick_setup_plugins`. This needs an actual
build/run cycle to verify, which hadn't been reached by the end of this Phase 0 session
(`docs/ROADMAP.md`'s Phase 0 status note) — do this before assuming `import MapLibre` will resolve
at runtime.

## Second follow-up finding (same session): an actual upstream CMake bug blocks the QML plugin target under `add_subdirectory`

While trying to get a real CMake configure to pass (not just reasoning about it), hit a concrete,
reproducible failure: `external/maplibre-native-qt/src/quick/plugins/CMakeLists.txt` builds its
`Plugin_Sources` list (lines 6-14) and its `target_include_directories` (lines 66-69) using
`${CMAKE_SOURCE_DIR}/src/quick/...`. `CMAKE_SOURCE_DIR` is **always the outermost project's root**
in a CMake build — it does not change per-subdirectory. That's correct when MapLibre Native Qt is
built standalone (its own examples and CI do exactly that), but Nimbus consumes it via
`add_subdirectory` from `external/maplibre-native-qt.cmake`, so `CMAKE_SOURCE_DIR` resolves to
Nimbus's own repo root, not `external/maplibre-native-qt/`. The configure fails with "Cannot find
source file: `<nimbus-root>/src/quick/common/declarative_style_parameter.hpp`" — a real path that
doesn't exist, since the actual file lives under `external/maplibre-native-qt/src/quick/common/`.
This should have been `${CMAKE_CURRENT_SOURCE_DIR}` (relative to whichever directory the calling
`CMakeLists.txt` is in) or `${PROJECT_SOURCE_DIR}` (relative to the nearest enclosing `project()`
call, i.e. `QMapLibre`'s own) — either would resolve correctly regardless of nesting. No existing
consumer in this codebase's history exercises this path: the legacy Supercell Wx app builds this
same library via `add_subdirectory` too, but with `MLN_QT_WITH_LOCATION OFF` and never touches
`MLN_QT_WITH_QUICK_PLUGIN` (defaults ON upstream) - actually it does default ON and would hit this
same bug, except the legacy app never actually *uses* any QML surface, so it's plausible this
exact configuration was never exercised there either, or an older library version (3.0.0 vs. the
4.0.0 Nimbus vendors) didn't have this bug. Not root-caused further than "confirmed reproducible
on 4.0.0, not something Nimbus's own CMake glue got wrong."

**Workaround adopted for now:** `external/maplibre-native-qt.cmake` sets
`MLN_QT_WITH_QUICK_PLUGIN OFF`, skipping this broken target entirely. `MLNQtCore` and
`MLNQtQuickPrivate` (the library that actually implements the `QQuickItem`,
`src/quick/plugins/map_quick_item.*`) still build fine — only the QML module *registration*
plugin (`declarative_maplibre`, which makes `import MapLibre` resolve at runtime) is disabled.
This does not touch `external/`'s vendored source at all (consistent with "never edit external/ in
place") — it's a build-option choice in Nimbus's own glue file.

**Concrete options for whoever picks up the actual `import MapLibre` wiring (supersedes the
previous section's plan once this was found):**
1. Patch the vendored `CMakeLists.txt` locally to fix the `CMAKE_SOURCE_DIR` → `CMAKE_CURRENT_SOURCE_DIR`
   bug, accepting an out-of-tree patch that needs to be reapplied (or upstreamed) on every
   submodule update - fastest, but breaks the "external/ is pristine" invariant unless the patch
   is itself tracked (e.g. a `.patch` file applied by a CMake step, not a raw in-place edit).
2. Build `maplibre-native-qt` as its own genuinely-standalone CMake project (e.g. via
   `ExternalProject_Add` or a separate configure+install step invoked from Nimbus's build), then
   consume the installed result via `find_package(QMapLibre)` as the library's own examples and
   `qmaplibre_quick_setup_plugins` helper expect. This is the "supported" consumption path and
   sidesteps the bug entirely (since `CMAKE_SOURCE_DIR` would correctly point at MapLibre Native
   Qt's own root when it's genuinely the top-level project) - more upfront plumbing work, but no
   ongoing patch maintenance.
3. Report the bug upstream (`maplibre/maplibre-native-qt`) and track their fix, using workaround 1
   as a stopgap until it lands.
**No decision made yet on which of these three to take** — flag to the user/next agent before
picking one, since options 1 and 2 are a real architectural tradeoff (patch-and-maintain vs.
build-and-install), not a small implementation detail.

## Resolution (Phase 1 slice 1, 2026-08-22): option 1, tracked patch applied at configure time

Confirmed with the user before implementing (per this ADR's own "flag before picking one" note):
**option 1**, the local tracked patch, not the standalone `ExternalProject_Add`/`find_package`
build (option 2).

- The fix: `src/quick/plugins/CMakeLists.txt`'s `Plugin_Sources` list and
  `target_include_directories` call both replace `${CMAKE_SOURCE_DIR}/src/quick/...` with
  `${CMAKE_CURRENT_SOURCE_DIR}/../...`-relative paths (root cause confirmed exactly as diagnosed
  above - `CMAKE_SOURCE_DIR` resolves to Nimbus's own repo root under `add_subdirectory`, not
  MapLibre Native Qt's). The `${CMAKE_BINARY_DIR}/src/core/include` entry on the same line block
  was investigated too (same-looking bug) but left untouched: `src/quick/CMakeLists.txt` uses the
  identical `${CMAKE_SOURCE_DIR}/src/core` + `${CMAKE_BINARY_DIR}/src/core/include` pair for the
  already-working `MLNQtQuickPrivate` target, so those particular paths are evidently unused
  dead/redundant include entries (core's real headers reach consumers via a correctly-scoped
  PUBLIC/INTERFACE include elsewhere) rather than a second live bug - `target_include_directories`
  doesn't fail at configure time just because a listed directory doesn't exist, unlike the
  `Plugin_Sources` list, which requires every entry to be a real file.
- The fix is captured as `external/patches/0004-mln-qt-plugins-cmake-source-dir.patch` (generated
  via `git diff` inside the submodule, then the submodule working tree was reverted with
  `git checkout --`, so `external/` stays pristine in git per the "never edit external/ in place"
  rule). `external/maplibre-native-qt.cmake` applies it at configure time via
  `execute_process(COMMAND git apply ...)`, idempotently (`git apply --check --reverse` first
  checks whether it's already applied, so re-running `cmake .` doesn't fail on a second apply
  attempt). `MLN_QT_WITH_QUICK_PLUGIN` is now back to `ON`.
- Filing the bug upstream (`maplibre/maplibre-native-qt`) is still worth doing but wasn't done as
  part of this slice - flagged here so it isn't lost, not blocking.
- **Runtime QML plugin resolution** (separate from the configure-time fix above): the
  `declarative_maplibre` target has no `QMapLibre::` namespaced ALIAS in this in-tree build (only
  `Core` and `QuickPrivate` do - confirmed by reading `src/core/CMakeLists.txt` and
  `src/quick/CMakeLists.txt`), so `app/CMakeLists.txt` links the bare target name
  `declarative_maplibre` directly, not a `QMapLibre::` alias.
  `qmaplibre_quick_setup_plugins()` (the upstream-recommended helper, used by the vendored
  `examples/quick-standalone`) still doesn't apply here for the same reason noted above - it needs
  `find_package`-imported targets. Instead: `nimbus-app` sets its `QT_QML_IMPORT_PATH` target
  property to `$<TARGET_FILE_DIR:declarative_maplibre>/..` (for build-time QML tooling), and a
  `POST_BUILD` step copies the plugin's output directory (already named `MapLibre` via that
  target's own `OUTPUT_DIRECTORY` setting) to `$<TARGET_FILE_DIR:nimbus-app>/qml/MapLibre`. This
  mirrors exactly where `windeployqt` already places Qt's own QML modules
  (`<exe-dir>/qml/QtQuick/Window`, etc., confirmed present after Phase 0's verified launch) -
  `QQmlEngine`'s default import path list includes `<app-dir>/qml`, no `qt.conf` needed (confirmed
  none exists in the deployed `Release/bin`), so `import MapLibre` resolves the same way
  `import QtQuick.Window` already does.
- `app/qml/Panes/PaneHost.qml` now hosts a real `MapLibre` QML item (pan/zoom/pinch wired,
  pattern ported from `examples/quick-standalone/main.qml`) as Phase 1 slice 1's proof of this
  rendering seam, per `docs/ROADMAP.md` §0.2's "prove the rendering seam early" rule - it uses the
  public `demotiles.maplibre.org` style as a placeholder, not a real base-map provider choice
  (that's a later Phase 1 settings item).
- Build verification: see `docs/ROADMAP.md` Phase 1 slice 1 status for the actual
  configure/build/launch results.

## Slice 3 findings (2026-08-22): custom-layer registration mechanics and a black-screen bug

Three more discoveries while wiring the first custom layer (the Phase 1 slice 3 rendering-seam
proof), each now a tracked patch under `external/patches/` applied by the same configure-time
mechanism (generalized into `nimbus_apply_mln_qt_patch()` in `external/maplibre-native-qt.cmake`):

1. **Patch 0005 — `MapQuickItem` exposes no path to the core `Map`.** The QML item keeps its
   `QMapLibre::Map` entirely private, so an app can't call `Map::addCustomLayer`. Patch adds
   `Q_INVOKABLE QMapLibre::Map* mapLibreMap()` (raw QObject pointer, not the internal
   shared_ptr, so it's callable across the QML plugin DLL boundary without metatype plumbing),
   a `mapReady()` signal (Map constructed), and a `styleLoaded()` signal
   (`MapChangeDidFinishLoadingStyle`).
2. **Register custom layers on `styleLoaded()`, not `mapReady()`.** `addCustomLayer()` calls
   made after Map construction but before the style finishes loading are silently dropped —
   there's no loaded style to insert the layer into. The legacy app's `MapWidget::mapChanged`
   gates its own `AddLayers()` on `MapChangeDidFinishLoadingStyle` for exactly this reason;
   `app/qml/Panes/PaneHost.qml` now does the same via `onStyleLoaded`.
3. **Patch 0006 — the Qt OpenGL backend cleared the framebuffer inside renderable `bind()`,
   blacking out the whole map whenever ANY custom layer exists.** Root-caused by bisection
   (base map fine with no layer registered; black even with a completely no-op
   `initialize()`/`render()`): mbgl's `DrawableCustomLayerHostTweaker::execute()` calls the
   default renderable's `bind()` MID-FRAME after every custom layer renders (to restore the
   FBO in case the host changed it), and `QtOpenGLRenderableResource::bind()`
   (`src/core/rendering/opengl_renderer_backend.cpp`) did an unconditional
   clear-to-opaque-black there — erasing everything mbgl had already drawn that frame. The
   clear was redundant at its intended point anyway: `mbgl::gl::RenderPass`'s constructor
   performs the proper state-tracked scissor-disable + clear immediately after `bind()`
   (`vendor/maplibre-native/src/mbgl/gl/render_pass.cpp`), which is what every other platform
   backend relies on — none of them clear in `bind()`. Patch removes the clear, keeping the
   FBO rebind + viewport set. If frame artifacts ever appear after a future submodule bump,
   re-check this area rather than reintroducing an unconditional clear.

**Upstream status (filed 2026-08-23).** All of these were reported to
`maplibre/maplibre-native-qt`. Check these before touching a patch or bumping the submodule — if
one is fixed upstream, drop the corresponding patch rather than carrying it forever:

| Nimbus patch / finding | Upstream | Notes |
| --- | --- | --- |
| 0004 (CMake `CMAKE_SOURCE_DIR`) | [#304](https://github.com/maplibre/maplibre-native-qt/issues/304) | New issue |
| 0005 (no way to reach the core `Map`) | [#296](https://github.com/maplibre/maplibre-native-qt/issues/296) | Commented on an existing feature request — another user had independently hit the same wall |
| 0006 (mid-frame framebuffer clear) | [#296](https://github.com/maplibre/maplibre-native-qt/issues/296) | Same thread: that reporter had hit the black framebuffer too, so the root cause went there rather than into a new issue |
| 0007 (signals connected too late) | [#303](https://github.com/maplibre/maplibre-native-qt/issues/303) | New issue, includes the suggested fix |
| Teardown crash in `~Context` (not patched) | [#302](https://github.com/maplibre/maplibre-native-qt/issues/302) | New issue |
| Teardown hang in `~Thread<MainResourceLoaderThread>` (not patched) | [#285](https://github.com/maplibre/maplibre-native-qt/issues/285) | Commented on an existing report with our thread stacks; it had no diagnosis |

## Slice 4 finding (2026-08-22): a lost-signal race that only bites the *second* map

**Patch 0007 — `MapQuickItem` connects the core `Map`'s signals too late.** `MapQuickItemPrivate::
initialize()` constructs the `Map` and immediately calls `setStyleUrl()`, but
`needsRendering`/`mapChanged` are only connected later, in `updatePaintNode()`, when the
scene-graph node is first created. Anything mbgl emits in that window is lost outright.

With a cold style fetch this is invisible: the network round-trip guarantees the paint node exists
long before `MapChangeDidFinishLoadingStyle` arrives. It becomes reproducible the moment a
*second* map loads the *same* style URL, because the cached style resolves almost immediately —
the event fires with nothing connected, and then `m_styleLoaded` stays `false`,
`syncStyleChanges()` never runs, and (with patch 0005 in place) `styleLoaded()` never fires. A
host that gates `addCustomLayer()` on `styleLoaded()` — which patch 0005's own doc comment
correctly instructs you to do — therefore waits forever.

Found by growing Phase 1 slice 4's pane grid from 1×1 to 2×2: pane 0 rendered radar, panes 1-3
silently rendered base map only, with no error anywhere. The log made it unambiguous — only pane 0
ever reached "registering radar sweep layer".

Fix: connect both signals immediately after the `Map` is constructed, before `setStyleUrl()`, and
remove the connections from `updatePaintNode()` (leaving them in both places would double-connect
and deliver every change twice). Note the deliberate ordering dependency this creates — if a
future submodule bump moves the `Map` construction or the style-load kickoff, this patch's whole
point is that the connect must stay *between* them.

This one is the strongest upstream candidate of the four: it silently breaks any Quick-path
application that shows more than one map sharing a style, custom layers or not (`syncStyleChanges`
never running affects declarative style parameters too).

Additional context that matters for the actual radar-sweep port (slice 3's main work, still
ahead): the vendored core uses the **drawables** renderer architecture — a custom layer's
`render()` runs via `DrawableCustomLayerHostTweaker` inside the translucent pass, with
`gl::Context::resetState()` called before it (clean GL state each call) and
`context.setDirtyState()` after (mbgl re-assumes nothing about GL state the host may have
changed). So a custom layer host does NOT need to restore mbgl's state itself beyond not
breaking the FBO — but defensive unbinding (VAO/program) stays cheap and harmless.
