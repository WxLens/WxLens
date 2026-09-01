# Graph Report - WxLens  (2026-08-31)

## Corpus Check
- 100 files · ~95,591 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1663 nodes · 2630 edges · 98 communities (93 shown, 4 thin omitted)
- Extraction: 95% EXTRACTED · 5% INFERRED · 0% AMBIGUOUS · INFERRED: 136 edges (avg confidence: 0.83)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `8814ec3e`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- OverlayManager
- SavedPlaceManager
- MapObject
- PaletteModel
- RadarSiteDataService
- PaletteManager
- PaneGridModel
- MeasurementController
- Level3TextSnapshot
- ObjectToolController
- TEST_F
- PaneController::Impl
- refreshPlacefile
- settings_store.cpp
- PaneController
- TEST
- TEST_F
- pane_controller.cpp
- ThemeManager
- TEST_F
- Theme
- SweepData
- RadarSweepProduct
- AppSettings
- TEST_F
- Level3RasterMetadata
- RadarSweepLayer::Impl
- BuildLevel3RadialSnapshot
- logger
- RadarSweepProduct::Impl
- radar_sweep_layer.cpp
- ProductDescriptor
- QObject
- AppSettings::Impl
- app_settings.cpp
- TEST_F
- SnapTargetRegistry
- string
- RadarSweepLayerBinding
- WxLens AI Agent Instructions
- ConnectProductSignals
- AddPacket
- AppSettingsTest
- ADR 0004: MapLibre Native Qt QML integration — verified, and which module to use
- WxLens — Ground-Up Rewrite Roadmap
- Phase 1 packaged UX feedback — 2026-08-31
- BuildLevel3RasterSnapshot
- theme_manager.cpp
- TEST
- 4. Multi-pane camera synchronization & first-class map objects
- 7. Phase breakdown
- MapObjectStore
- radar_sweep_product.cpp
- ThemeManager::Impl
- unit_format.cpp
- MapObjectScope
- GraphicOverlayPrimitive
- applyChannelValue
- RadarSiteInfo
- TEST
- WxLensConan
- ADR 0001: Qt 6 / Qt Quick (QML) over a web-shell or Flutter UI
- ADR 0002: `wxdata` reuse via whole-repo submodule (Option A)
- ADR 0003: TOML for structured config/theme/palette storage
- Data source catalog
- SnapTargetRegistry::resolve
- Q: what is next
- GeometryRow
- Capability & interaction-taxonomy audit (Phase 0.5)
- 5. Palette/theming system design
- map_object_store.cpp
- CLAUDE.md
- Project architecture
- .PaneController
- PaneController
- 3. New project structure
- External dependencies
- MapObjectStore::Impl
- Phase 1 packaged acceptance record — 2026-08-30
- BuildLevel3GraphicOverlaySnapshot
- Level3RasterSnapshot
- Section
- WxLens performance baseline
- roleNames
- Q: OK, so what is the next step in this process?
- Level3RadialMetadata
- OverlayManager::Impl
- AppendWarnings
- overlay_manager.cpp
- PaneSyncTest
- TEST
- Level3GraphicOverlaySnapshot
- CartesianPackets
- Level3RadialSnapshot
- overlay_manager.hpp
- palette_defaults.hpp
- MapDetailGroup

## God Nodes (most connected - your core abstractions)
1. `PaneController` - 71 edges
2. `PaneController::Impl` - 54 edges
3. `PaletteManager` - 44 edges
4. `AppSettings` - 44 edges
5. `ThemeManager` - 36 edges
6. `PaletteModel` - 35 edges
7. `RadarSweepProduct::Impl` - 32 edges
8. `PaneGridModel` - 31 edges
9. `RadarSweepLayer::Impl` - 31 edges
10. `MapObjectStore` - 30 edges

## Surprising Connections (you probably didn't know these)
- `TEST()` --calls--> `addPlacefile`  [INFERRED]
  test/source/wxlens/overlays/overlay_manager.test.cpp → app/source/wxlens/overlays/overlay_manager.hpp
- `TEST()` --calls--> `DetectLevel3CartesianPacketFamily()`  [INFERRED]
  test/source/wxlens/products/level3_raster_product.test.cpp → app/source/wxlens/products/level3_raster_product.cpp
- `TEST()` --calls--> `BuildLevel3RasterSnapshot()`  [INFERRED]
  test/source/wxlens/products/level3_raster_product.test.cpp → app/source/wxlens/products/level3_raster_product.cpp
- `TEST_F()` --calls--> `mapTheme`  [INFERRED]
  test/source/wxlens/settings/app_settings.test.cpp → app/source/wxlens/settings/app_settings.hpp
- `TEST_F()` --calls--> `controlBarDocked`  [INFERRED]
  test/source/wxlens/settings/app_settings.test.cpp → app/source/wxlens/settings/app_settings.hpp

## Import Cycles
- None detected.

## Communities (98 total, 4 thin omitted)

### Community 0 - "OverlayManager"
Cohesion: 0.11
Nodes (24): QVariantList, Impl, QObject, unique_ptr, OverlayManager, p, placefileItems, placefiles (+16 more)

### Community 1 - "SavedPlaceManager"
Cohesion: 0.07
Nodes (60): MapObjectStore, QObject, QString, QVariantList, SettingsStore, vector, Group, color (+52 more)

### Community 2 - "MapObject"
Cohesion: 0.11
Nodes (17): MapObject, color, colorOverride, id, label, latitudes, lifecycle, longitudes (+9 more)

### Community 3 - "PaletteModel"
Cohesion: 0.05
Nodes (60): QUrl, QByteArray, QColor, QHash, QModelIndex, QObject, QString, QUrl (+52 more)

### Community 4 - "RadarSiteDataService"
Cohesion: 0.05
Nodes (53): Level3File, mutex, QTimer, shared_ptr, string, time_point, uint64_t, vector (+45 more)

### Community 5 - "PaletteManager"
Cohesion: 0.07
Nodes (56): PendingAction, QObject, QString, QStringList, QUrl, Entry, factory, factoryText (+48 more)

### Community 6 - "PaneGridModel"
Cohesion: 0.05
Nodes (53): ChangeOrigin, PaneController, QByteArray, QHash, QModelIndex, QObject, QString, QVariant (+45 more)

### Community 7 - "MeasurementController"
Cohesion: 0.08
Nodes (45): PaneController, QObject, QString, QVariantList, QVariantMap, vector, Impl, Q_ENUM (+37 more)

### Community 8 - "Level3TextSnapshot"
Cohesion: 0.06
Nodes (40): AddGraphicPages(), AddTabularPages(), BuildLevel3TextSnapshot(), BuildTextProductSnapshot(), Level3File, optional, shared_ptr, int16_t (+32 more)

### Community 9 - "ObjectToolController"
Cohesion: 0.08
Nodes (37): PaneController, QObject, QVariantList, Impl, Q_ENUM, QObject, unique_ptr, ObjectToolController (+29 more)

### Community 10 - "TEST_F"
Cohesion: 0.07
Nodes (34): ArmShutdownWatchdog(), string, DumpAllThreadStacks(), ExceptionCodeName(), HandleException(), InstallCrashHandler(), WalkAndWriteStack(), WriteAll() (+26 more)

### Community 11 - "PaneController::Impl"
Cohesion: 0.05
Nodes (37): shared_ptr, SyncGroupId, uint64_t, vector, PaneController::Impl, actualTime_, archiveTime_, bearing_ (+29 more)

### Community 12 - "refreshPlacefile"
Cohesion: 0.18
Nodes (15): ColorString(), QByteArray, QString, QUrl, rgba8_pixel_t, FlattenPlacefile(), addPlacefile, importWarningFile (+7 more)

### Community 13 - "settings_store.cpp"
Cohesion: 0.12
Nodes (30): Category, dirty, failedToParse, loaded, table, mutex, QString, DefaultConfigDirectory() (+22 more)

### Community 14 - "PaneController"
Cohesion: 0.08
Nodes (33): QString, Impl, Q_INVOKABLE, QObject, unique_ptr, PaneController, cameraChanged, cameraSynced (+25 more)

### Community 15 - "TEST"
Cohesion: 0.07
Nodes (27): AltitudeRisesWithRangeAndWithTilt, GeodesicDirect(), GeodesicInverse(), GeodesicInverseResult, azimuthDegrees, distanceMeters, BeamAltitudeMsl(), optional (+19 more)

### Community 16 - "TEST_F"
Cohesion: 0.08
Nodes (24): ActivePaneTracksSelectionAndSurvivesLayoutShrink, CameraGroupHelperGroupsEveryCameraChannel, ChannelsAreIndependent, ChannelsWithoutStateArePropagationNoOps, CopyCameraIsOneShotAcrossEveryCameraChannel, CopyChannelIsOneShotNotAPersistentLink, ElevationSelectionRejectsNoRealCutButRetainsRequestedCut, GridProvidesIndependentPanes (+16 more)

### Community 17 - "pane_controller.cpp"
Cohesion: 0.09
Nodes (26): QPointF, QVariantList, QVariantMap, applyMapDetails, bearing, centerLatitude, centerLongitude, PaneController::coordinateAtOffset() (+18 more)

### Community 18 - "ThemeManager"
Cohesion: 0.08
Nodes (25): Impl, QObject, unique_ptr, ThemeManager, accent, border, control, controlActive (+17 more)

### Community 19 - "TEST_F"
Cohesion: 0.08
Nodes (23): BearingIsReportedAsACompassAngle, CommitExcludesTheLiveCursor, CommitPinsAMeasurementObject, CommittedMeasurementRespectsScope, DistanceFormattingShowsBothUnits, DistanceProducesDistanceAndBearing, DistanceStopsAtTwoVertices, GeodesicMatchesTheUnderlyingHelper (+15 more)

### Community 20 - "Theme"
Cohesion: 0.08
Nodes (26): QByteArray, QColor, ParseTheme(), Theme, accent, background, border, control (+18 more)

### Community 21 - "SweepData"
Cohesion: 0.12
Nodes (23): BuildColorTableLut(), BuildColorTableLutFromTable(), ColorTableLut, colors, maximum, minimum, QString, shared_ptr (+15 more)

### Community 22 - "RadarSweepProduct"
Cohesion: 0.17
Nodes (15): optional, QObject, time_point, Impl, Q_OBJECT, QObject, unique_ptr, RadarSweepProduct (+7 more)

### Community 23 - "AppSettings"
Cohesion: 0.12
Nodes (17): AppSettings, controlBarDockedChanged, defaultObjectScopeChanged, distanceUnitsChanged, geometryRowsChanged, mapDetailsChanged, mapThemeChanged, measurementGestureChanged (+9 more)

### Community 24 - "TEST_F"
Cohesion: 0.08
Nodes (25): AllPanesIsVisibleEverywhere, CurrentPaneOnlyIsVisibleOnlyInItsOwnPane, DrawingHelperPinsGeographicLineAndRejectsOnePoint, GeometryIsValidated, HitTestingWithoutAMapNeverHits, NullPaneIsNeverVisible, ObjectsForPaneFiltersByScope, PinnedObjectsAreStored (+17 more)

### Community 25 - "Level3RasterMetadata"
Cohesion: 0.09
Nodes (23): array, DataLevelCode, int16_t, optional, string, time_point, uint16_t, Level3RasterMetadata (+15 more)

### Community 26 - "RadarSweepLayer::Impl"
Cohesion: 0.09
Nodes (23): array, uint16_t, unique_ptr, RadarSweepLayer::Impl, binding_, colorTableMin_, colorTableScale_, gl_ (+15 more)

### Community 27 - "BuildLevel3RadialSnapshot"
Cohesion: 0.12
Nodes (19): AppendVertex(), BuildLevel3RadialSnapshot(), Level3File, optional, ProductSymbologyBlock, shared_ptr, uint8_t, FindRadialPacket() (+11 more)

### Community 28 - "logger"
Cohesion: 0.08
Nodes (26): QString, shared_ptr, string, Create(), Initialize(), LogDirectory(), LogDirectoryPath(), QObject (+18 more)

### Community 29 - "RadarSweepProduct::Impl"
Cohesion: 0.10
Nodes (20): mutex, uint64_t, RadarSweepProduct::Impl, archiveTime_, colorTable_, colorTableLut_, data_, dataBlockType_ (+12 more)

### Community 30 - "radar_sweep_layer.cpp"
Cohesion: 0.20
Nodes (17): CheckGlError(), shared_ptr, Impl, unique_ptr, RadarSweepLayer, deinitialize, UploadColorTableLut, UploadSweep (+9 more)

### Community 31 - "ProductDescriptor"
Cohesion: 0.17
Nodes (10): QString, ProductDescriptor, elevation, identity, identityKind, kind, palette, product (+2 more)

### Community 32 - "QObject"
Cohesion: 0.17
Nodes (8): QObject, QString, PaneController, PaneController, MapObjectStore, SettingsStore, QVariantList, SettingsStore

### Community 33 - "AppSettings::Impl"
Cohesion: 0.13
Nodes (15): AppSettings::Impl, controlBarDocked_, defaultObjectScope_, distanceUnits_, geometryRowVisible_, Load, mapDetailsPreset_, mapDetailVisible_ (+7 more)

### Community 34 - "app_settings.cpp"
Cohesion: 0.13
Nodes (22): configDirectory, controlBarDocked, geometryRows, geometryRowVisible, hasSection, mapDetailGroups, mapDetailsPreset, mapDetailVisibility (+14 more)

### Community 35 - "TEST_F"
Cohesion: 0.11
Nodes (18): defaultObjectScope, distanceUnits, measurementGesture, snapStrength, snapTolerancePixels, ChangesNotifyExactlyOnce, ChangesPersistAcrossAReload, EveryGeometryRowDefaultsVisible (+10 more)

### Community 36 - "SnapTargetRegistry"
Cohesion: 0.25
Nodes (7): Q_INVOKABLE, Q_OBJECT, QObject, PaneController, SnapTargetRegistry, public, QVariantMap

### Community 37 - "string"
Cohesion: 0.21
Nodes (7): string, optional, vector, Level3File, array, Level3File, Level3File

### Community 38 - "RadarSweepLayerBinding"
Cohesion: 0.18
Nodes (12): shared_ptr, sweep_snapshot, SweepSnapshot, colorTableLut, sweep, Impl, mutex, shared_ptr (+4 more)

### Community 39 - "WxLens AI Agent Instructions"
Cohesion: 0.14
Nodes (14): Adding a new Data Layer Provider (Phase 2/3), Adding a new overlay draw primitive, Adding new NEXRAD product support, Build system, Code style & conventions, Common tasks, Conan + CMake workflow, Custom map layers must trigger their own repaints (+6 more)

### Community 40 - "ConnectProductSignals"
Cohesion: 0.16
Nodes (17): optional, PaneController, QStringList, time_point, DefaultPalette(), attachLayers, availableProducts, ConnectProductSignals (+9 more)

### Community 41 - "AddPacket"
Cohesion: 0.27
Nodes (10): AddLinkedVector(), AddPacket(), GraphicOverlayKind, int16_t, shared_ptr, string, Offset(), Point() (+2 more)

### Community 42 - "AppSettingsTest"
Cohesion: 0.16
Nodes (10): AppSettingsTest, settings_, store_, tempDir_, QString, QTemporaryDir, QVariantMap, SettingsStore (+2 more)

### Community 43 - "ADR 0004: MapLibre Native Qt QML integration — verified, and which module to use"
Cohesion: 0.17
Nodes (11): ADR 0004: MapLibre Native Qt QML integration — verified, and which module to use, Consequences, Context, Decision, Follow-up finding (still Phase 0, same session): QML plugin wiring needs its own work, Resolution (Phase 1 slice 1, 2026-08-22): option 1, tracked patch applied at configure time, Second follow-up finding (same session): an actual upstream CMake bug blocks the QML plugin target under `add_subdirectory`, Slice 10 follow-up (2026-08-25): the QML style setter does not reload a live map (+3 more)

### Community 44 - "WxLens — Ground-Up Rewrite Roadmap"
Cohesion: 0.17
Nodes (12): 0.1 Locked architectural principles, 0.2 Agent execution rules, 0. Ground rules for agents executing this roadmap, 1. Tech stack decision, 2. Repo map of the CURRENT repo (reference source — read-only), 6. Data source strategy, 8. Feature backlog (captured now, mostly low/no priority — don't build speculatively), 9. Open questions for the user / other planning agents (+4 more)

### Community 45 - "Phase 1 packaged UX feedback — 2026-08-31"
Cohesion: 0.13
Nodes (14): Definition of ready for the next user test, Direct pane targeting and radar-site selection, Measurement tool preferences and radar-value reader, Minimal and customizable persistent chrome, P0 — correctness and interaction defects, P1 — usability and accessibility blockers, P2 — progressive disclosure and expert efficiency, Pane and top-bar information density (+6 more)

### Community 46 - "BuildLevel3RasterSnapshot"
Cohesion: 0.29
Nodes (9): AppendVertex(), BuildLevel3RasterSnapshot(), Level3File, optional, ProductSymbologyBlock, uint8_t, DetectLevel3CartesianPacketFamily(), FindCartesianPacket() (+1 more)

### Community 47 - "theme_manager.cpp"
Cohesion: 0.42
Nodes (8): QString, LocalPath(), ReadTheme(), availableThemesChanged, exportActiveTheme, importTheme, setActiveTheme, themesDirectory

### Community 48 - "TEST"
Cohesion: 0.20
Nodes (9): QStringList, activeTheme, availableThemes, background, dark, BundlesDarkAndLightAndLiveSwitches, SelectionPersistsAndUnknownNameIsRejected, ShareableThemeRoundTripsAndMalformedThemeIsSafe (+1 more)

### Community 49 - "4. Multi-pane camera synchronization & first-class map objects"
Cohesion: 0.20
Nodes (10): 4.1 Per-channel synchronization model (not a single link boolean), 4.2 Feedback-loop prevention, 4.3 Unified first-class map objects (markers, measurements, drawings, range rings, annotations, storm tracks), 4.4 Measurement as a reusable interaction framework, 4.5 Quick sync/object controls in pane chrome (not buried in Settings), 4.6 Pane/data-service architecture underneath the sync model, 4.7 Radar geometry & beam-height interrogation, 4.8 Acceptance criteria for this architecture (+2 more)

### Community 50 - "7. Phase breakdown"
Cohesion: 0.20
Nodes (10): 7. Phase breakdown, Phase 0.5 — Capability & interaction-taxonomy audit, Phase 0 — Project scaffolding, wxdata integration, repo map/docs, Phase 1 completion and release-readiness gates, Phase 1 — Single-site modernized radar UI (near-term, executable phase), Phase 2 — Multi-site mesh/mosaic radar, Phase 3 — Additional data layers + velocity improvements, Phase 4 — 3D storm structure rendering (+2 more)

### Community 51 - "MapObjectStore"
Cohesion: 0.13
Nodes (18): QModelIndex, QVariant, Impl, Q_INVOKABLE, QAbstractListModel, unique_ptr, MapObjectStore, data (+10 more)

### Community 52 - "radar_sweep_product.cpp"
Cohesion: 0.18
Nodes (18): ComputeCoordinates(), ComputeSweep(), string, vector, IsRadarDataIncomplete(), Level2ProductForDescription(), LoadBundledColorTable(), NormalizeAngle() (+10 more)

### Community 53 - "ThemeManager::Impl"
Cohesion: 0.28
Nodes (6): QObject, SettingsStore, ThemeManager::Impl, active_, themes_, QMap

### Community 54 - "unit_format.cpp"
Cohesion: 0.39
Nodes (8): QString, DecimalsFor(), FormatAltitude(), FormatBearing(), FormatGroundDistance(), GetDistanceUnitPreference(), SetDistanceUnitPreference(), DistanceUnitPreference

### Community 55 - "MapObjectScope"
Cohesion: 0.25
Nodes (8): MapObjectScopeKind, SyncChannel, SyncGroupId, MapObjectScope, channel, kind, originGroupId, originPaneId

### Community 56 - "GraphicOverlayPrimitive"
Cohesion: 0.14
Nodes (14): GeographicPoint, latitude, longitude, GraphicOverlayPrimitive, forecast, geometry, kind, label (+6 more)

### Community 57 - "applyChannelValue"
Cohesion: 0.36
Nodes (8): ChangeOrigin, QVariant, SyncChannel, applyChannelValue, channelValue, level3Product, setSyncGroup, syncGroup

### Community 58 - "RadarSiteInfo"
Cohesion: 0.10
Nodes (20): AltitudeIsConvertedFromTheListsFeet, AltitudesAreSaneAsMetresAcrossTheExtremes, optional, string, vector, FindRadarSite(), RadarSiteInfo, altitudeMslMeters (+12 more)

### Community 59 - "TEST"
Cohesion: 0.29
Nodes (7): ConvertsClassicRasterIntoGeographicRendererContract, DetectsArchivedPrecipitationArrayWithoutFabricatingGeometry, Level3RasterProduct, RejectsRadialFamily, path, Fixture(), TEST()

### Community 61 - "ADR 0001: Qt 6 / Qt Quick (QML) over a web-shell or Flutter UI"
Cohesion: 0.29
Nodes (6): ADR 0001: Qt 6 / Qt Quick (QML) over a web-shell or Flutter UI, Consequences, Context, Decision, Rationale, Status

### Community 62 - "ADR 0002: `wxdata` reuse via whole-repo submodule (Option A)"
Cohesion: 0.29
Nodes (6): ADR 0002: `wxdata` reuse via whole-repo submodule (Option A), Consequences, Context, Decision, Follow-up finding (same session): pin `external/units` to the legacy repo's exact commit, Status

### Community 63 - "ADR 0003: TOML for structured config/theme/palette storage"
Cohesion: 0.29
Nodes (6): ADR 0003: TOML for structured config/theme/palette storage, Consequences, Context, Decision, Rationale, Status

### Community 64 - "Data source catalog"
Cohesion: 0.29
Nodes (6): Base map (vector tiles for the map surface itself, not a weather data layer), Data source catalog, Not yet integrated / no provider exists anywhere in the codebase, Phase 1 (implemented via `wxdata`, already working), Phase 2 — multi-site mesh/mosaic, Phase 3 — additional data layers

### Community 65 - "SnapTargetRegistry::resolve"
Cohesion: 0.33
Nodes (5): PaneController, QObject, QVariantMap, SnapTargetRegistry::resolve(), SnapTargetRegistry::SnapTargetRegistry()

### Community 66 - "Q: what is next"
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: what is next, Source Nodes

### Community 67 - "GeometryRow"
Cohesion: 0.33
Nodes (6): GeometryRow, dimWhen, id, label, note, valueKey

### Community 68 - "Capability & interaction-taxonomy audit (Phase 0.5)"
Cohesion: 0.33
Nodes (5): Capability & interaction-taxonomy audit (Phase 0.5), Capability matrix, Product taxonomy, Sources, Tool/interaction taxonomy

### Community 69 - "5. Palette/theming system design"
Cohesion: 0.33
Nodes (6): 5.1 Radar/data-layer color palettes (`.pal` files) — preserve format, rewrite editor, 5.2 App chrome theme (new capability — closes the identified gap), 5.3 UI personas & progressive disclosure, 5.4 Primary control surface placement — bottom, not the left rail, 5.5 Iconography — layout glyphs, not numerals, 5. Palette/theming system design

### Community 70 - "map_object_store.cpp"
Cohesion: 0.26
Nodes (10): QObject, QPointF, DistanceBetween(), DistanceToSegment(), Clear, clearObjects, refreshFormatting, setObjectScope (+2 more)

### Community 72 - "Project architecture"
Cohesion: 0.40
Nodes (5): Data Source → Data Product → Visualization Layer → View, Namespace convention, Per-channel synchronization, not a link boolean, Project architecture, `wxdata` (reused, unmodified) + `app/` (new)

### Community 73 - ".PaneController"
Cohesion: 0.29
Nodes (6): QObject, effectivePaletteName, homeLatitude, homeLongitude, ApplyPalette, setPaletteName

### Community 74 - "PaneController"
Cohesion: 0.36
Nodes (11): ApplyOrigin(), PaneController, QString, QVariantList, Add, addLine, addMarker, addRangeRing (+3 more)

### Community 75 - "3. New project structure"
Cohesion: 0.40
Nodes (5): 3.1 `wxdata` reuse mechanics, 3.2 Proposed directory layout (new repo), 3.3 Agent-legibility documents (Phase 0 deliverable), 3.4 Audit-friendly logging (Phase 0 deliverable), 3. New project structure

### Community 76 - "External dependencies"
Cohesion: 0.50
Nodes (4): External dependencies, License discipline, Vendored-dependency patches (`external/patches/`), Vendored submodules (`external/`)

### Community 77 - "MapObjectStore::Impl"
Cohesion: 0.33
Nodes (6): vector, MapObjectStore::Impl, nextId_, objects_, revision_, Objects

### Community 78 - "Phase 1 packaged acceptance record — 2026-08-30"
Cohesion: 0.33
Nodes (5): Build and automated checks, Next acceptance action, Packaged visual checks, Phase 1 packaged acceptance record — 2026-08-30, Preliminary performance evidence

### Community 79 - "BuildLevel3GraphicOverlaySnapshot"
Cohesion: 0.25
Nodes (7): BuildLevel3GraphicOverlaySnapshot(), Level3File, optional, ConvertsRealStormTrackingFixture, Level3GraphicOverlay, RejectsRadialProduct, TEST()

### Community 80 - "Level3RasterSnapshot"
Cohesion: 0.50
Nodes (4): shared_ptr, Level3RasterSnapshot, metadata, sweep

### Community 81 - "Section"
Cohesion: 0.50
Nodes (4): Section, id, summary, title

### Community 82 - "WxLens performance baseline"
Cohesion: 0.40
Nodes (4): 2026-08-29 status, 2026-08-30 preliminary packaged run, Required capture procedure, WxLens performance baseline

### Community 83 - "roleNames"
Cohesion: 0.67
Nodes (3): QByteArray, QHash, roleNames

### Community 85 - "Q: OK, so what is the next step in this process?"
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: OK, so what is the next step in this process?, Source Nodes

### Community 86 - "Level3RadialMetadata"
Cohesion: 0.11
Nodes (18): DataLevelCode, int16_t, optional, string, time_point, uint16_t, Level3RadialMetadata, dataLevelCoded (+10 more)

### Community 87 - "OverlayManager::Impl"
Cohesion: 0.15
Nodes (12): QTimer, OverlayManager::Impl, network_, placefileItems_, placefiles_, placefilesVisible_, refreshingWarnings_, self_ (+4 more)

### Community 88 - "AppendWarnings"
Cohesion: 0.22
Nodes (9): main(), setScopeKind, AppendWarnings(), Coordinates(), shared_ptr, vector, refreshWarnings, Coordinate (+1 more)

### Community 89 - "overlay_manager.cpp"
Cohesion: 0.22
Nodes (9): QObject, OverlayManager::OverlayManager(), refreshingWarnings, removePlacefile, statusText, WarningColor(), Phenomenon, Placefile (+1 more)

### Community 90 - "PaneSyncTest"
Cohesion: 0.20
Nodes (5): PaneController, PaneController, testing::Test, PaneSyncTest, model_

### Community 91 - "TEST"
Cohesion: 0.22
Nodes (7): BuildLevel3ProductCatalog(), string, vector, Level3ProductCatalog, LevelTwoAndLevelThreeIdentitiesCannotCollide, MapsAvailableAwipsIdsToCanonicalCategories, TEST()

### Community 92 - "Level3GraphicOverlaySnapshot"
Cohesion: 0.33
Nodes (6): time_point, uint16_t, Level3GraphicOverlaySnapshot, primitives, productTime, unsupportedPacketCodes

### Community 93 - "CartesianPackets"
Cohesion: 0.40
Nodes (5): CartesianPackets, family, raster, shared_ptr, RasterDataPacket

### Community 94 - "Level3RadialSnapshot"
Cohesion: 0.50
Nodes (4): shared_ptr, Level3RadialSnapshot, metadata, sweep

### Community 97 - "MapDetailGroup"
Cohesion: 0.67
Nodes (3): MapDetailGroup, id, label

## Knowledge Gaps
- **554 isolated node(s):** `radarSite_`, `level2Provider_`, `level3Mutex_`, `level3Providers_`, `level3Cache_` (+549 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 949 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **4 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `map_` connect `PaletteManager` to `QObject`, `AppSettings::Impl`, `app_settings.cpp`, `RadarSiteDataService`, `ConnectProductSignals`, `PaneController::Impl`, `settings_store.cpp`, `pane_controller.cpp`, `radar_sweep_product.cpp`, `RadarSiteInfo`?**
  _High betweenness centrality (0.079) - this node is a cross-community bridge._
- **Why does `PaneController::Impl` connect `PaneController::Impl` to `overlay_manager.cpp`, `RadarSiteDataService`, `PaletteManager`, `RadarSweepLayerBinding`, `ConnectProductSignals`, `.PaneController`, `PaneController`, `pane_controller.cpp`, `RadarSweepProduct`, `applyChannelValue`, `ProductDescriptor`?**
  _High betweenness centrality (0.065) - this node is a cross-community bridge._
- **Why does `PaletteManager` connect `PaletteManager` to `PaletteModel`?**
  _High betweenness centrality (0.061) - this node is a cross-community bridge._
- **What connects `radarSite_`, `level2Provider_`, `level3Mutex_` to the rest of the system?**
  _554 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `OverlayManager` be split into smaller, more focused modules?**
  _Cohesion score 0.10869565217391304 - nodes in this community are weakly interconnected._
- **Should `SavedPlaceManager` be split into smaller, more focused modules?**
  _Cohesion score 0.06875 - nodes in this community are weakly interconnected._
- **Should `MapObject` be split into smaller, more focused modules?**
  _Cohesion score 0.1111111111111111 - nodes in this community are weakly interconnected._