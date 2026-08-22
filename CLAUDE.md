# Nimbus — Repo Map

See [AGENTS.md](AGENTS.md) for full architecture, build workflow, and conventions, and
[docs/ROADMAP.md](docs/ROADMAP.md) for the complete multi-phase architectural plan. This file is
just a directory map.

## Directory map

```
nimbus/
├── AGENTS.md                     # primary agent guide
├── CLAUDE.md                     # this file
├── docs/
│   ├── ROADMAP.md                # architectural source of truth, phase breakdown
│   ├── adr/                      # one-way-door decisions, one file per decision
│   ├── capability-matrix.md      # Phase 0.5: current/AWIPS/planned capability comparison
│   └── data-sources.md           # living catalog of external data sources by phase
├── conanfile.py                  # Conan 2 dependency manifest
├── CMakeLists.txt / CMakePresets.json
├── external/                     # vendored dependencies — READ-ONLY, never edit in place
│   ├── legacy-supercell-wx/      # whole Supercell Wx repo; only wxdata/ is built (ADR 0002)
│   ├── aws-sdk-cpp/              # wxdata's S3 provider dependency
│   ├── date/                     # wxdata's timezone/calendar fallback (conditional)
│   ├── units/                    # wxdata's compile-time units library
│   ├── hsluv-c/                  # wxdata's color_table/gr color-space dependency
│   ├── maplibre-native-qt/       # map renderer (+ nested vendor/maplibre-native core, ADR 0004)
│   └── cmake-conan/              # Conan 2 CMake integration
├── app/                          # the Qt Quick application
│   ├── qml/
│   │   ├── Main.qml
│   │   ├── Panes/                # (Phase 1) pane grid, per-pane map host, sync UI
│   │   ├── Chrome/                # (Phase 1) top bar, side rail, docks
│   │   ├── Dialogs/               # (Phase 1) settings, palette editor, about
│   │   ├── Controls/               # (Phase 1) shared custom Qt Quick Controls style
│   │   └── Theme/                 # (Phase 1) theme singleton + built-in themes
│   ├── source/nimbus/             # C++20 backend, namespace nimbus
│   │   ├── main/                  # entry point (implemented)
│   │   ├── data/                  # (Phase 1) Data Source layer
│   │   ├── provider/              # (Phase 2/3) satellite/sounding/model/mosaic providers
│   │   ├── products/              # (Phase 1) Data Product layer
│   │   ├── render/                # (Phase 1) Visualization Layer - ported gl/draw/
│   │   ├── panes/                 # (Phase 1) View layer - pane grid + sync state
│   │   ├── objects/                # (Phase 1) unified MapObject store
│   │   ├── theme/                 # (Phase 1) ThemeManager
│   │   ├── settings/               # (Phase 1) TOML-backed settings (ADR 0003)
│   │   ├── log/                    # thin wrapper over wxdata's util::Logger
│   │   └── util/
│   ├── res/                        # icons, fonts, bundled .pal files
│   └── CMakeLists.txt
├── test/                          # GTest, mirrors app/source/nimbus/ tree once it has code
│   ├── CMakeLists.txt / test.cmake # currently: nimbus-wxdata-test (reused wxdata test slice)
│   └── source/nimbus/
└── tools/                         # setup scripts, conan profiles
```

Directories marked `(Phase N)` above are scaffolded but empty as of Phase 0 — see
`docs/ROADMAP.md` §7 for what each phase adds.
