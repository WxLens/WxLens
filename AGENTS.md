# Nimbus AI Agent Instructions

Nimbus is a from-scratch, C++20/Qt 6 Quick (QML) rewrite of Supercell Wx: a free, open source
weather radar viewer. **`docs/ROADMAP.md` is the architectural source of truth** — read it before
making any non-trivial change, especially §0 (ground rules) and §0.1/§0.2 (locked principles and
agent execution rules). This file is a fast-lookup companion, not a replacement for it.

## Project architecture

### `wxdata` (reused, unmodified) + `app/` (new)
- **`external/legacy-supercell-wx/wxdata/`** — the Qt-free NEXRAD Level 2/3 parsing, `.pal` color
  table, AWIPS text-product, and network-provider library from Supercell Wx, linked in unmodified
  via `add_subdirectory` (see `docs/adr/0002-wxdata-reuse-strategy.md`). **Never edit files under
  `external/`** — that's the read-only reference/reused-dependency tree. If `wxdata` needs a
  change, it belongs upstream in the legacy repo first, then the submodule pin advances.
- **`app/`** — the new Qt Quick application. `app/qml/` is presentation-only; `app/source/nimbus/`
  is C++20 owning all state/business logic, namespace `nimbus`. See `docs/ROADMAP.md` §3.2 for the
  full intended directory layout (`data/`, `products/`, `render/`, `panes/`, `objects/`, `theme/`,
  `settings/`, `log/`) — most of these are still empty pending Phase 1 slices (§7).

**Critical:** QML never contains business logic — it binds to `Q_PROPERTY`/`Q_INVOKABLE`/signals
on C++ objects. Never add Qt dependencies to `wxdata` (it's read-only anyway, but the rule
matters if a change is ever made upstream).

### Data Source → Data Product → Visualization Layer → View
The core architectural pattern (docs/ROADMAP.md §0.1 principle #4, §4.6): a pane hosts a **View**;
a View binds to a **Visualization Layer** (a renderer registered per product kind — radar today);
a Visualization Layer consumes a **Data Product** sourced from a **Data Source** service. Never
give a pane radar-specific fields directly (no `PaneController::radarSite` as a fundamental
model) — radar is the first implementation of this pipeline, not a special case baked into it.

### Per-channel synchronization, not a link boolean
Panes synchronize per-property (`Location`, `Zoom`, `Bearing`, `Product`, `Palette`, etc. — full
list in §4.1), never via one global linked flag. See §4.1-§4.2 before touching any sync code.

### Namespace convention
`namespace nimbus { namespace X { ... } }`, nested, with closing comments. Fully qualified
namespaces in headers; no `using namespace` in headers.

## Build system

### Conan + CMake workflow
Conan 2 manages most dependencies; CMake integrates it via the `cmake-conan` provider
(`external/cmake-conan`). A few `wxdata` dependencies are vendored git submodules instead (AWS SDK
for C++, HowardHinnant/date, nholthaus/units, hsluv-c) because `wxdata` needs them built from
source unmodified — see `docs/adr/0002-wxdata-reuse-strategy.md`. MapLibre Native Qt (the map
renderer) is also a vendored submodule, for the same "needs custom build, not on ConanCenter"
reason — see `docs/adr/0004-maplibre-qml-integration.md`.

**Setup script usage (Windows):**
```powershell
.\tools\setup-windows-vs2026-release.bat [BUILD_DIR]
```

**Manual CMake configuration:**
```bash
conan config install ./tools/conan/profiles/nimbus-windows_vs2026_x64 -tf profiles
mkdir build && cd build
conan install ../ --remote conancenter --build missing \
    --profile:all nimbus-windows_vs2026_x64 --settings:all build_type=Release \
    --output-folder ./conan/
cmake ../ -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=../external/cmake-conan/conan_provider.cmake \
    -DCONAN_HOST_PROFILE=nimbus-windows_vs2026_x64 \
    -DCONAN_BUILD_PROFILE=nimbus-windows_vs2026_x64 \
    -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
cmake --build . --target nimbus-app
```

CMake Presets are in `CMakePresets.json` (`windows-vs2026-x64-release`/`-debug`,
`linux-gcc14-release`/`-debug` — the Linux preset is a documented template, not yet verified on a
real Linux box from this repo).

### Qt 6.11.1 requirement
Qt is installed separately (not via Conan), at `C:/Qt/6.11.1/msvc2022_64` on this dev machine.
Needs at least the `Quick`/`QuickControls2`/`ShaderTools` modules — all present in the local
install already (confirmed during Phase 0).

## External dependencies

### Vendored submodules (`external/`)
- `legacy-supercell-wx/` — the whole Supercell Wx repo, shallow submodule; only its `wxdata/` is
  built. See ADR 0002.
- `aws-sdk-cpp/`, `date/`, `units/`, `hsluv-c/` — `wxdata`'s non-Conan dependencies, vendored
  because `wxdata` links them from source. See ADR 0002 for exactly which `wxdata` files need
  which.
- `maplibre-native-qt/` (+ its nested `vendor/maplibre-native`) — the map rendering engine. Use its
  `src/quick` (`QMapLibre::Quick`, QML type `MapLibre`, BSD-2-Clause) integration, **not**
  `src/location` (QtLocation plugin, LGPL/GPL) or `src/widgets`. Needs real bug/missing-API
  fixes applied as tracked patches (see below) to build and work correctly under Nimbus's
  `add_subdirectory` consumption — see ADR 0004, including its "Slice 3 findings" section.
- `cmake-conan/` — the Conan 2 CMake provider glue.

### Vendored-dependency patches (`external/patches/`)
`external/maplibre-native-qt` needed real fixes/additions this repo can't make upstream directly
(a build-breaking CMake bug, a missing custom-layer API, a black-screen rendering bug, and a
lost-signal race that strands every map after the first — see ADR 0004). Rather than hand-editing
the vendored source, each fix is a tracked `.patch` file under
`external/patches/`, applied idempotently at CMake configure time by
`nimbus_apply_mln_qt_patch()` in `external/maplibre-native-qt.cmake` (checks
`git apply --check --reverse` first, so re-configuring is safe and `external/` stays pristine in
git). If a future submodule bump breaks one of these patches, that function's `FATAL_ERROR`
will say so at configure time — reconcile the patch against the new upstream source, don't just
delete it. Follow this same pattern for any other vendored-dependency fix that isn't a Nimbus-side
bug.

### License discipline
MIT project license. No GPL dependencies. LGPL only as dynamically-linked shared libraries. Any
new dependency needs a license check against this rule first (see `docs/ROADMAP.md` §0).

## Testing

`test/test.cmake` builds `nimbus-wxdata-test`: the wxdata-only slice of the legacy repo's GTest
suite (the `scwx::qt::*` test groups are excluded since Nimbus doesn't link `scwx-qt`), referencing
`.test.cpp` files directly from `external/legacy-supercell-wx/test/source/scwx/` rather than
duplicating them. Fixture data comes from `external/legacy-supercell-wx/test/data` (a nested
submodule of the legacy repo). Run via:
```bash
cmake --build . --target nimbus-wxdata-test
ctest --output-on-failure
```
As `app/source/nimbus/` grows in Phase 1, its own tests belong under `test/source/nimbus/`,
mirroring the directory tree, per `docs/ROADMAP.md`'s "test the C++ models independently of QML"
rule.

## Code style & conventions

- **clang-format**: `.clang-format`/`.clang-tidy`/`.clang-format-ignore` are copied from the legacy
  repo verbatim (same style rules); apply before commits.
- **Google C++ Style Guide** for naming/structure.
- **Pimpl idiom** for classes with non-trivial private state, matching the legacy codebase.
- No `using namespace` in headers. No GPL dependencies. Prefer `Q_EMIT` over the bare `emit`
  keyword if any dependency in the include chain `#define`s `emit` to something else (verify
  whether this repo's dependency set actually needs it — the legacy repo's reason was a specific
  conflicting library; recheck before assuming it still applies here).

## Common tasks

### Adding a new Data Layer Provider (Phase 2/3)
Follow the per-source-key-singleton + shared-cache pattern established by `RadarSiteDataService`
(`docs/ROADMAP.md` §4.6) — one service instance per source key (e.g. per radar site, or per
mosaic region), shared across every pane displaying that source.

### Custom map layers must trigger their own repaints
A `QMapLibre::CustomLayerHostInterface` layer draws only when mbgl renders a frame, and mbgl does
**not** repaint just because a custom layer registered or its data changed. Whenever a layer's
underlying product publishes new data, something must call `QMapLibre::Map::triggerRepaint()` —
see `RadarLayerController::attachRadarSweep`. Skipping this produces content that stays invisible
until an unrelated pan/scroll forces a frame, which reads as "the data never loaded."

### Adding a new overlay draw primitive
Port the *behavior* of the matching class in
`external/legacy-supercell-wx/scwx-qt/source/scwx/qt/gl/draw/` to a Qt RHI custom render node
under `app/source/nimbus/render/` — don't port the code line-for-line (the rendering backend is
different), and don't add the primitive as a MapLibre custom layer without checking whether it
belongs in the unified `MapObjectsLayer` instead (`docs/ROADMAP.md` §4.3).

### Adding new NEXRAD product support
This is a `wxdata` change, not a Nimbus one — it belongs upstream in the legacy repo
(`external/legacy-supercell-wx/wxdata/source/scwx/wsr88d/rpg/level3_message_factory.cpp` for
ICD-defined Level 3 products), then the submodule pin advances. Nimbus's own work is wiring the
new product into `app/source/nimbus/products/` and a renderer under `app/source/nimbus/render/`.

## Updating dependencies
Modify `conanfile.py`'s `requires` tuple for Conan packages; advance the relevant submodule pin
(`git submodule update --remote external/<name>`) for vendored ones. Check the license discipline
rule above before adding anything new.

## Resources
- `docs/ROADMAP.md` — full architecture, phase breakdown, open questions.
- `docs/adr/` — one-way-door decisions (Qt Quick vs. web shell, wxdata reuse strategy, config
  format, MapLibre QML integration).
- `docs/capability-matrix.md` — Phase 0.5 capability/product/tool taxonomy (once written).
