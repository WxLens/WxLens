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
  machinery WxLens has no other use for.
- WxLens's own `external/maplibre-native-qt` submodule points at the upstream
  `maplibre/maplibre-native-qt` repo (not the `dpaulat` Supercell-Wx-specific fork) at its current
  HEAD (library version 4.0.0), since WxLens has no dependency on that fork's Supercell-Wx-specific
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
same CMake run (what WxLens and the legacy Supercell Wx repo's `external/` both use for every other
vendored dependency).

There's no existing precedent to copy in this codebase either: the legacy `scwx-qt` app never uses
MapLibre's QML surface at all — it drives `MLNQtCore` directly from a `QOpenGLWidget`-hosted
QWidgets renderer, which is exactly the architecture this whole rewrite is moving away from.

**Concrete next-slice work (§7 Phase 1 slice 1/2 territory, not resolved yet):** wire the
`declarative_maplibre` QML plugin target (`MLN_QT_QML_PLUGIN` in
`src/quick/plugins/CMakeLists.txt`, aliased `QMapLibre::PluginQml`) into an in-tree `wxlens-app`
build by hand — likely `target_link_libraries(wxlens-app PRIVATE declarative_maplibre)` plus
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
built standalone (its own examples and CI do exactly that), but WxLens consumes it via
`add_subdirectory` from `external/maplibre-native-qt.cmake`, so `CMAKE_SOURCE_DIR` resolves to
WxLens's own repo root, not `external/maplibre-native-qt/`. The configure fails with "Cannot find
source file: `<wxlens-root>/src/quick/common/declarative_style_parameter.hpp`" — a real path that
doesn't exist, since the actual file lives under `external/maplibre-native-qt/src/quick/common/`.
This should have been `${CMAKE_CURRENT_SOURCE_DIR}` (relative to whichever directory the calling
`CMakeLists.txt` is in) or `${PROJECT_SOURCE_DIR}` (relative to the nearest enclosing `project()`
call, i.e. `QMapLibre`'s own) — either would resolve correctly regardless of nesting. No existing
consumer in this codebase's history exercises this path: the legacy Supercell Wx app builds this
same library via `add_subdirectory` too, but with `MLN_QT_WITH_LOCATION OFF` and never touches
`MLN_QT_WITH_QUICK_PLUGIN` (defaults ON upstream) - actually it does default ON and would hit this
same bug, except the legacy app never actually *uses* any QML surface, so it's plausible this
exact configuration was never exercised there either, or an older library version (3.0.0 vs. the
4.0.0 WxLens vendors) didn't have this bug. Not root-caused further than "confirmed reproducible
on 4.0.0, not something WxLens's own CMake glue got wrong."

**Workaround adopted for now:** `external/maplibre-native-qt.cmake` sets
`MLN_QT_WITH_QUICK_PLUGIN OFF`, skipping this broken target entirely. `MLNQtCore` and
`MLNQtQuickPrivate` (the library that actually implements the `QQuickItem`,
`src/quick/plugins/map_quick_item.*`) still build fine — only the QML module *registration*
plugin (`declarative_maplibre`, which makes `import MapLibre` resolve at runtime) is disabled.
This does not touch `external/`'s vendored source at all (consistent with "never edit external/ in
place") — it's a build-option choice in WxLens's own glue file.

**Concrete options for whoever picks up the actual `import MapLibre` wiring (supersedes the
previous section's plan once this was found):**
1. Patch the vendored `CMakeLists.txt` locally to fix the `CMAKE_SOURCE_DIR` → `CMAKE_CURRENT_SOURCE_DIR`
   bug, accepting an out-of-tree patch that needs to be reapplied (or upstreamed) on every
   submodule update - fastest, but breaks the "external/ is pristine" invariant unless the patch
   is itself tracked (e.g. a `.patch` file applied by a CMake step, not a raw in-place edit).
2. Build `maplibre-native-qt` as its own genuinely-standalone CMake project (e.g. via
   `ExternalProject_Add` or a separate configure+install step invoked from WxLens's build), then
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
  above - `CMAKE_SOURCE_DIR` resolves to WxLens's own repo root under `add_subdirectory`, not
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
  `find_package`-imported targets. Instead: `wxlens-app` sets its `QT_QML_IMPORT_PATH` target
  property to `$<TARGET_FILE_DIR:declarative_maplibre>/..` (for build-time QML tooling), and a
  `POST_BUILD` step copies the plugin's output directory (already named `MapLibre` via that
  target's own `OUTPUT_DIRECTORY` setting) to `$<TARGET_FILE_DIR:wxlens-app>/qml/MapLibre`. This
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
mechanism (generalized into `wxlens_apply_mln_qt_patch()` in `external/maplibre-native-qt.cmake`):

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

| WxLens patch / finding | Upstream | Notes |
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

## Slice 10 follow-up (2026-08-25): the QML style setter does not reload a live map

**Patch 0008 — `MapQuickItem::setStyle()` only stores the new URL.** The stored value is consumed
by `MapQuickItemPrivate::initialize()`, so the initial style works, but assigning the QML `style`
property after initialization never calls the live core `Map`. This left later theme-driven
basemap changes stuck on the already-loaded style.

The patch forwards a changed style to `Map::setStyleUrl()` whenever the map already exists. Its
normal `mapChanged` events reset the style-loaded state, and patch 0005's `styleLoaded()` signal
lets WxLens reattach the radar custom layer after the replacement style finishes loading. Keep
this patch with the QML-facing Quick integration; recreating every pane would discard map state
and hide a genuine missing setter behavior in the dependency.

## macOS finding (2026-09-10): the rendering core emits ES shaders desktop GL cannot compile

**Patch 0009 — mbgl hardcodes `#version 300 es` on every OpenGL platform.** The first patch in
this series that targets the *rendering core* (`vendor/maplibre-native`) rather than the Qt
wrapper, so it carries paths relative to that nested submodule and is applied from there —
`git apply` run in the outer repository refuses paths that cross into a submodule.
`external/maplibre-native-qt.cmake` therefore now drives two series through one
`wxlens_apply_patch_series()` helper.

Desktop OpenGL accepts the ES shading language only through `GL_ARB_ES3_compatibility`. Windows,
Linux and Mesa drivers all expose it, which is why the hardcoded ES version string has never been
a problem on those platforms. Apple's OpenGL does not: macOS tops out at 4.1 Core with no ES3
compatibility path, so *every* mbgl shader fails to compile there. `ShaderProgramGL::create()`
throws (`src/mbgl/gl/context.cpp`, "shader failed to compile"), the exception escapes
`QMapLibre::Map::render()` into Qt's event loop, which has no handler, and the process aborts on
the first frame. Reported as `EXC_CRASH (SIGABRT)` with `__cxa_throw` inside QMapLibre beneath
`TextureNodeOpenGL::render`.

The fix is a version directive swap on Apple only — `#version 330 core` instead of
`#version 300 es`, in both the drawable path (`src/mbgl/shaders/gl/shader_program_gl.cpp`) and the
legacy one (`src/mbgl/shaders/gl/legacy/program_base.hpp`, still compiled via `cmake/opengl.cmake`
and still used, since `clipping_mask_program` draws the stencil clip). Patching only the first
leaves macOS crashing on the second.

This is a supported upstream path rather than a hack: `include/mbgl/shaders/gl/prelude.hpp`
already branches on `GL_ES` and, for the non-ES case, `#define`s `lowp`/`mediump`/`highp` away so
the shared shader bodies compile unchanged as desktop GLSL. Only the version line forced ES.

Deliberately scoped to `__APPLE__`. Windows and Linux compile the ES source today via
`ARB_ES3_compatibility`, and switching them to desktop GLSL would flip the prelude onto its other
branch on two platforms that already work, for no benefit.

**Not filed upstream yet.** The honest upstream fix is to select the directive from the context's
actual capabilities rather than the host OS; this patch is the narrow version of that.

| WxLens patch / finding | Upstream | Notes |
| --- | --- | --- |
| 0009 (ES shader version on desktop GL) | — | Not filed yet; see above |

## macOS finding (2026-09-10, second): the error path destroys the error

**Patch 0010 — `Context::verifyProgramLinkage()` throws `std::bad_alloc` while reporting a link
failure.** With patch 0009 in place the tester's M4 reached a 4.1 Core context (confirmed:
`OpenGL VENDOR: Apple RENDERER: Apple M4 Max VERSION: 4.1 Metal - 90.5`, `QSurfaceFormat` reporting
`version 4.1 ... profile CoreProfile`) and still aborted on `std::bad_alloc`, with no MapLibre
diagnostic of any kind on stderr.

`src/mbgl/gl/context.cpp` declared an uninitialized `GLint logLength`, queried
`GL_INFO_LOG_LENGTH` into it, and then called `std::make_unique<GLchar[]>(logLength)`
*unconditionally, before* the `if (logLength > 0)` test that exists directly beneath it. When the
driver leaves the out-param unwritten, that allocates a garbage-sized buffer - negative as `GLint`,
astronomical as `size_t` - and throws `std::bad_alloc` before `Log::Error` can print the driver's
explanation. The failure destroys its own diagnosis, and a perfectly diagnosable link failure
presents as an out-of-memory abort on the first frame. `createShader()` has the same uninitialized
declaration but places its allocation *inside* the guard, which is why a shader compile failure
surfaces as `std::runtime_error` while a link failure does not.

The patch initializes both to `0`, moves the program-log allocation inside the guard to match
`createShader`, and adds an explicit "driver supplied no info log" branch to each so a failure with
an empty log still says something rather than nothing.

This is diagnostic infrastructure, not a fix for the underlying failure. What it buys is the
driver's own message for the *actual* problem, which is a program link failure on macOS - the
thing that has been invisible behind the `bad_alloc` from the very first report. Note this also
means the original 2.1-context `std::bad_alloc` may always have been this same masked failure
rather than a genuine allocation problem.

Unrelated but worth recording: on a core profile `glGetString(GL_EXTENSIONS)` returns `NULL`, so
`Context::initializeExtensions()` skips its whole body. That is harmless - the block only wires up
the debugging and Tracy-timestamp extensions - but it does mean the "GPU Identifier: ..." log line
never appears on macOS, which is not evidence that MapLibre logging is broken.

**Not filed upstream yet.** Patch 0010 is a straightforward correctness fix and a good upstream
candidate independent of anything WxLens-specific.

| WxLens patch / finding | Upstream | Notes |
| --- | --- | --- |
| 0010 (bad_alloc in the shader/program error path) | — | Not filed yet; see above |

## macOS root cause (2026-09-11): a stale GL error reported as std::bad_alloc

**Patch 0011 — the actual first-frame crash.** Found by symbolicating a RelWithDebInfo backtrace
rather than by reading code; the two preceding hypotheses (ES shader source, then the link-failure
reporting path) were both wrong, and each was disproved by a tester run.

The symbolicated frames:

```
mbgl::gl::UploadPass::createVertexBufferResource(...)      upload_pass.cpp:40
mbgl::gfx::UploadPass::createVertexBuffer<...>(...)        upload_pass.hpp:53
mbgl::RenderStaticData::upload(gfx::UploadPass&)           render_static_data.cpp:15
mbgl::Renderer::Impl::render(...)                          renderer_impl.cpp:248
```

`createVertexBufferResource` ends with:

```cpp
MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, size, data, ...));
if (glGetError()) {
    throw std::bad_alloc();
}
```

Two things combine badly. First, `MBGL_CHECK_ERROR` compiles to nothing under `NDEBUG`, so in a
release build the two `glGetError()` calls in `gl/upload_pass.cpp` are the **only** ones the GL
backend makes — nothing else ever empties the error queue. Second, mbgl reports *any* queued error
as `std::bad_alloc`. So an error raised at any earlier point survives until the first buffer
upload and is attributed to it.

The upload in question is `RenderStaticData::upload()`, which pushes the four-vertex tile quad -
**16 bytes**. There was never any memory pressure; `bad_alloc` was mbgl's way of saying "a GL error
happened", and it named the wrong cause from the very first crash report.

The queued error came from `Context::initializeExtensions()`, which calls
`glGetString(GL_EXTENSIONS)`. That was removed from `glGetString` in a 3.2+ core profile, where it
returns null and raises `GL_INVALID_ENUM`. mbgl already knows this — `hasAnisotropicFiltering()` in
`render_location_indicator_layer.cpp` performs the same query and deliberately consumes the error —
but `initializeExtensions()` never did. It only becomes fatal on a platform that forces a core
profile, which is why Windows and Linux were unaffected: their compatibility contexts answer the
query without error.

Note the interaction with patch 0009's context fix. Requesting a core profile was correct and
necessary, but it is also what made this query start raising `GL_INVALID_ENUM`. The original 2.1
crash was the same misreporting mechanism with a different source error, which is why both looked
identical as `std::bad_alloc` and why neither report ever mentioned memory.

The patch drains the queue immediately before each upload so the check reflects only that call,
consumes the deprecated query's error at its source, and logs the actual GL error code before
throwing so the next occurrence names itself.

**Not filed upstream yet.** Both halves are upstream-candidate and independent of WxLens: the
error-queue handling in `gl/upload_pass.cpp` is a correctness bug on any core-profile desktop GL
target, and `initializeExtensions()` should consume the error the same way the location-indicator
layer already does.

| WxLens patch / finding | Upstream | Notes |
| --- | --- | --- |
| 0011 (stale GL error reported as `bad_alloc`) | — | Not filed yet; see above |

## macOS, third site (2026-09-11): the same pattern in the texture pool

**Patch 0012.** With 0011 in place the crash moved rather than disappearing - which confirmed 0011
was right and incomplete. The new backtrace (symbolicated offline against the shipped `.dSYM` by
parsing its Mach-O symbol table, since no `atos` exists on the dev machine):

```
mbgl::gl::Texture2DPool::allocateGLMemory(...)        resource_pool.cpp:157
mbgl::gl::Context::createUniqueTexture(...)
mbgl::gl::Texture2D::allocateTexture() / ::create()
mbgl::gl::DynamicTexture::uploadDeferredImages(gfx::UploadPass&)
mbgl::GeometryTileRenderData::upload(gfx::UploadPass&)
mbgl::RenderTile::upload(gfx::UploadPass&)
mbgl::TileSourceRenderItem::upload(gfx::UploadPass&)
```

`Texture2DPool::allocateGLMemory()` carries the identical construct 0011 fixed: `glTexImage2D`
followed by `if (glGetError()) { throw std::bad_alloc(); }`. Same treatment - drain first, log the
real GL error code and the texture dimensions before throwing.

Note the progress this represents: the failure moved from `RenderStaticData::upload()`, the very
first static-geometry upload of the first frame, to *tile* upload. The renderer is now getting far
enough to process actual map tiles.

**The audit that should have come first.** Every `glGetError()` read in `src/mbgl` was enumerated
before writing this patch, rather than fixing sites one crash at a time:

| Site | Status |
| --- | --- |
| `gl/upload_pass.cpp` (x2) | fixed by 0011 |
| `gl/resource_pool.cpp` | fixed by 0012 |
| `gl/fence.cpp` | already drains in a loop |
| `platform/gl_functions.cpp` | debug-only (`MBGL_CHECK_ERROR` internals) |
| `renderer/layers/render_location_indicator_layer.cpp` | consumes its own error correctly |

Those three were the only sites with the misreporting pattern in the GL backend, so 0011+0012
should close the class rather than just the instance. The `mtl/` and `vulkan/` backends contain
similar `bad_alloc` throws but are not built here.

**Not filed upstream yet,** and belongs with 0011 as one report.

## Patch 0009 confirmed by experiment (2026-09-12)

Patch 0009 was written on a hypothesis and, when patch 0010 failed to change the crash, looked
like it might never have been needed. It was removed deliberately to find out, since ADR 0004
requires re-verifying every vendored patch on each submodule bump and a patch that earns nothing
should not be carried.

The experiment settled it. Without 0009, Apple's GLSL compiler emitted this 2810 times:

```
Shader failed to compile: ERROR: 0:1: '' : version '300' is not supported
                        : 0:1: '' : syntax error: #version
                        : 0:2: '' : #version required and missing.
                        : 0:78: '0' : syntax error: integers in layouts require GLSL 140 or later
```

That is the driver confirming the original reasoning outright: macOS cannot compile
`#version 300 es`, because Apple's OpenGL exposes no `GL_ARB_ES3_compatibility`. **0009 is
load-bearing and stays.**

Two things are worth keeping from how this played out:

- **A null result on one patch says nothing about another.** 0010 not changing the crash was taken
  as evidence against 0009, when the two address unrelated defects. 0009's necessity was only ever
  testable by removing it.
- **The failure is silent, not fatal.** mbgl catches the compile error, logs it and marks the
  program failed, so without 0009 the app runs and the basemap simply never draws. Nothing
  crashes. And before 0010, the failure could not even be reported - the error path allocated from
  an uninitialized length and threw `std::bad_alloc` before reaching `Log::Error`. 0010 is the
  reason this log line exists, which makes the two patches complementary rather than redundant.

### Custom-layer GL errors are inherited, not ours

The same run resolved the `GL error 1280` noise from `RadarSweepLayer`. With drain-on-entry
instrumentation in place the log showed **76 errors already queued on entry and 0 raised by the
layer itself** (74 at `render()`, one each at `initialize()` and `UploadSweep()`). The layer is
clean; it was reporting errors mbgl left in the queue. A future cleanup could drain in mbgl's
render pass, but nothing in WxLens needs changing.
