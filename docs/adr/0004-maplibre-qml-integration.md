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
