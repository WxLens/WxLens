# Graph Report - WxLens  (2026-09-02)

## Corpus Check
- 107 files · ~120,915 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1737 nodes · 2730 edges · 119 communities (115 shown, 3 thin omitted)
- Extraction: 95% EXTRACTED · 5% INFERRED · 0% AMBIGUOUS · INFERRED: 142 edges (avg confidence: 0.83)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `d7c8d672`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- OverlayManager
- SavedPlaceManager
- MapObject
- PaletteModel
- radar_site_data_service.cpp
- PaletteManager
- PaneGridModel
- MeasurementController
- Level3TextSnapshot
- ObjectToolController
- TEST_F
- PaneController::Impl
- AppSettings
- settings_store.cpp
- PaneController
- TEST
- TEST_F
- QVariantList
- ThemeManager
- TEST_F
- Theme
- SweepData
- RadarSweepProduct
- RadarProductStatus
- TEST_F
- Level3RasterMetadata
- RadarSweepLayer::Impl
- logger.cpp
- radar_product_status.cpp
- RadarSweepProduct::Impl
- radar_sweep_layer.cpp
- ProductDescriptor
- SnapTargetRegistry
- AppSettings::Impl
- app_settings.cpp
- TEST_F
- MeasurementController::Impl
- RadarSiteDataService
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
- PropagateChannel
- ComputeSweep
- ThemeManager::Impl
- unit_format.cpp
- RadarSiteDataService::Impl
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
- Level3RadialMetadata
- CLAUDE.md
- Project architecture
- roleNames
- map_object_store.cpp
- 3. New project structure
- External dependencies
- PaneController
- Phase 1 packaged acceptance record — 2026-08-30
- BuildLevel3GraphicOverlaySnapshot
- Level3RasterSnapshot
- Section
- WxLens performance baseline
- Q: Fix manual-test issues: full state-name radar search, site-change map recentering, and visible pane-local palette changes
- Q: OK, so what is the next step in this process?
- Level3ProductDescriptor
- string
- MapObjectStore
- QVariantList
- radarSites
- QString
- MeasurementTest
- Q: Fix the Weather Overlays contrast problem.
- MapObjectScope
- SetArchiveTime
- Q: Why is WxLens velocity less vivid than RadarOmega and how should palette buttons be categorized without mixing radar fields?
- SourceProbeTest
- Q: Fix palette editor drags moving the map, make saved velocity use the new palette, and soften low-reflectivity blue noise.
- Level3GraphicOverlaySnapshot
- QString
- PaneGridModel::Impl
- TEST
- GeodesicInverseResult
- Retest checklist
- setDefaultSourceKey
- Point
- Q: Could WxLens support two windows on separate monitors, with a 2x2 layout on one and 1x1 on the other, all panes linked, and how hard would that be?
- RadarBeamProbe
- TEST
- logger
- ToolbarAction
- MapDetailGroup
- QObject
- applyMapDetails
- MapObjectStore::Impl
- PaneSyncTest
- roleNames
- Q: Address second manual UX feedback round and explain remaining acceptance checks

## God Nodes (most connected - your core abstractions)
1. `PaneController` - 74 edges
2. `PaneController::Impl` - 54 edges
3. `AppSettings` - 54 edges
4. `PaletteManager` - 46 edges
5. `ThemeManager` - 36 edges
6. `PaletteModel` - 35 edges
7. `PaneGridModel` - 32 edges
8. `RadarSweepProduct::Impl` - 32 edges
9. `RadarSweepLayer::Impl` - 31 edges
10. `TEST_F()` - 31 edges

## Surprising Connections (you probably didn't know these)
- `TEST()` --calls--> `FindRadarSite()`  [INFERRED]
  test/source/wxlens/data/radar_site_database.test.cpp → app/source/wxlens/data/radar_site_database.cpp
- `TEST()` --calls--> `DetectLevel3CartesianPacketFamily()`  [INFERRED]
  test/source/wxlens/products/level3_raster_product.test.cpp → app/source/wxlens/products/level3_raster_product.cpp
- `TEST()` --calls--> `BuildLevel3RasterSnapshot()`  [INFERRED]
  test/source/wxlens/products/level3_raster_product.test.cpp → app/source/wxlens/products/level3_raster_product.cpp
- `TEST_F()` --calls--> `mapDetailVisible`  [INFERRED]
  test/source/wxlens/settings/app_settings.test.cpp → app/source/wxlens/settings/app_settings.hpp
- `TEST_F()` --calls--> `geometryRowVisible`  [INFERRED]
  test/source/wxlens/settings/app_settings.test.cpp → app/source/wxlens/settings/app_settings.hpp

## Import Cycles
- None detected.

## Communities (119 total, 3 thin omitted)

### Community 0 - "OverlayManager"
Cohesion: 0.05
Nodes (69): AppendWarnings(), ColorString(), Coordinates(), QByteArray, QObject, QString, QTimer, QUrl (+61 more)

### Community 1 - "SavedPlaceManager"
Cohesion: 0.07
Nodes (60): MapObjectStore, QObject, QString, QVariantList, SettingsStore, vector, Group, color (+52 more)

### Community 2 - "MapObject"
Cohesion: 0.11
Nodes (17): MapObject, color, colorOverride, id, label, latitudes, lifecycle, longitudes (+9 more)

### Community 3 - "PaletteModel"
Cohesion: 0.06
Nodes (59): QUrl, QByteArray, QColor, QHash, QModelIndex, QObject, QString, QUrl (+51 more)

### Community 4 - "radar_site_data_service.cpp"
Cohesion: 0.19
Nodes (13): QTimer, shared_ptr, string, time_point, uint64_t, Instance, LoadLatestLevel2Data, LoadLatestLevel3Data (+5 more)

### Community 5 - "PaletteManager"
Cohesion: 0.07
Nodes (58): PendingAction, QObject, QString, QStringList, QUrl, Entry, appliedText, factory (+50 more)

### Community 6 - "PaneGridModel"
Cohesion: 0.14
Nodes (19): QObject, Impl, QAbstractListModel, unique_ptr, PaneGridModel, activePane, activePaneChanged, activePaneIndex (+11 more)

### Community 7 - "MeasurementController"
Cohesion: 0.14
Nodes (19): QObject, QVariantMap, Impl, Q_ENUM, QObject, unique_ptr, MeasurementController, active (+11 more)

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
Nodes (39): shared_ptr, SyncGroupId, uint64_t, vector, attachLayers, PaneController::Impl, actualTime_, archiveTime_ (+31 more)

### Community 12 - "AppSettings"
Cohesion: 0.10
Nodes (20): AppSettings, centerMapOnSiteChangeChanged, controlBarDockedChanged, defaultObjectScopeChanged, distanceUnitsChanged, geometryRowsChanged, mapDetailsChanged, mapThemeChanged (+12 more)

### Community 13 - "settings_store.cpp"
Cohesion: 0.12
Nodes (30): Category, dirty, failedToParse, loaded, table, mutex, QString, DefaultConfigDirectory() (+22 more)

### Community 14 - "PaneController"
Cohesion: 0.07
Nodes (52): QPointF, QString, Impl, Q_INVOKABLE, QObject, unique_ptr, PaneController, bearing (+44 more)

### Community 15 - "TEST"
Cohesion: 0.16
Nodes (13): AltitudeRisesWithRangeAndWithTilt, BeamAltitudeMsl(), optional, ProbeRadarBeam(), BeamAltitudeMatchesTheFourThirdsEarthModel, CurvatureIsIncludedNotApproximatedAway, ProbeReportsRangeAndCompassAzimuth, ProbeSeparatesMslFromAboveRadarLevel (+5 more)

### Community 16 - "TEST_F"
Cohesion: 0.07
Nodes (29): ActivePaneTracksSelectionAndSurvivesLayoutShrink, CameraGroupHelperGroupsEveryCameraChannel, CenterOnCommandsTheViewAndPropagatesCameraChannels, ChannelsAreIndependent, ChannelsWithoutStateArePropagationNoOps, CopyCameraIsOneShotAcrossEveryCameraChannel, CopyChannelIsOneShotNotAPersistentLink, ElevationSelectionRejectsNoRealCutButRetainsRequestedCut (+21 more)

### Community 17 - "QVariantList"
Cohesion: 0.33
Nodes (6): QVariantList, PaneController::coordinateAtOffset(), PaneController::coordinateForPixel(), elevationCuts, productCatalog, productOverlays

### Community 18 - "ThemeManager"
Cohesion: 0.08
Nodes (25): Impl, QObject, unique_ptr, ThemeManager, accent, border, control, controlActive (+17 more)

### Community 19 - "TEST_F"
Cohesion: 0.12
Nodes (17): BearingIsReportedAsACompassAngle, CommitExcludesTheLiveCursor, CommitPinsAMeasurementObject, CommittedMeasurementRespectsScope, DistanceFormattingShowsBothUnits, DistanceProducesDistanceAndBearing, DistanceStopsAtTwoVertices, GeodesicMatchesTheUnderlyingHelper (+9 more)

### Community 20 - "Theme"
Cohesion: 0.08
Nodes (26): QByteArray, QColor, ParseTheme(), Theme, accent, background, border, control (+18 more)

### Community 21 - "SweepData"
Cohesion: 0.11
Nodes (25): BuildColorTableLut(), BuildColorTableLutFromTable(), ColorTableLut, colors, maximum, minimum, QString, shared_ptr (+17 more)

### Community 22 - "RadarSweepProduct"
Cohesion: 0.13
Nodes (24): optional, QObject, string, time_point, Impl, Q_OBJECT, QObject, unique_ptr (+16 more)

### Community 23 - "RadarProductStatus"
Cohesion: 0.22
Nodes (10): QString, Impl, QObject, unique_ptr, RadarProductStatus, p, Q_PROPERTY, siteId (+2 more)

### Community 24 - "TEST_F"
Cohesion: 0.08
Nodes (25): AllPanesIsVisibleEverywhere, CurrentPaneOnlyIsVisibleOnlyInItsOwnPane, DrawingHelperPinsGeographicLineAndRejectsOnePoint, GeometryIsValidated, HitTestingWithoutAMapNeverHits, NullPaneIsNeverVisible, ObjectsForPaneFiltersByScope, PinnedObjectsAreStored (+17 more)

### Community 25 - "Level3RasterMetadata"
Cohesion: 0.09
Nodes (23): array, DataLevelCode, int16_t, optional, string, time_point, uint16_t, Level3RasterMetadata (+15 more)

### Community 26 - "RadarSweepLayer::Impl"
Cohesion: 0.09
Nodes (22): array, uint16_t, unique_ptr, RadarSweepLayer::Impl, binding_, colorTableMin_, colorTableScale_, gl_ (+14 more)

### Community 27 - "logger.cpp"
Cohesion: 0.36
Nodes (7): QString, shared_ptr, string, Create(), Initialize(), LogDirectory(), LogDirectoryPath()

### Community 28 - "radar_product_status.cpp"
Cohesion: 0.29
Nodes (8): QObject, string, RadarProductStatus::Impl, radarSite_, statusText_, SystemLocalZone(), TryLocateZone(), time_zone

### Community 29 - "RadarSweepProduct::Impl"
Cohesion: 0.10
Nodes (20): mutex, uint64_t, RadarSweepProduct::Impl, archiveTime_, colorTable_, colorTableLut_, data_, dataBlockType_ (+12 more)

### Community 30 - "radar_sweep_layer.cpp"
Cohesion: 0.19
Nodes (18): CheckGlError(), shared_ptr, Impl, unique_ptr, RadarSweepLayer, deinitialize, UploadColorTableLut, UploadSweep (+10 more)

### Community 31 - "ProductDescriptor"
Cohesion: 0.15
Nodes (11): DefaultPalette(), QString, ProductDescriptor, elevation, identity, identityKind, kind, palette (+3 more)

### Community 32 - "SnapTargetRegistry"
Cohesion: 0.25
Nodes (7): Q_INVOKABLE, Q_OBJECT, QObject, PaneController, SnapTargetRegistry, public, QVariantMap

### Community 33 - "AppSettings::Impl"
Cohesion: 0.11
Nodes (18): AppSettings::Impl, centerMapOnSiteChange_, controlBarDocked_, defaultObjectScope_, distanceUnits_, geometryRowVisible_, Load, mapDetailsPreset_ (+10 more)

### Community 34 - "app_settings.cpp"
Cohesion: 0.11
Nodes (26): centerMapOnSiteChange, configDirectory, geometryRows, geometryRowVisible, hasSection, mapDetailGroups, mapDetailVisibility, mapDetailVisible (+18 more)

### Community 35 - "TEST_F"
Cohesion: 0.08
Nodes (23): main(), setScopeKind, controlBarDocked, defaultObjectScope, distanceUnits, mapDetailsPreset, mapTheme, measurementGesture (+15 more)

### Community 36 - "MeasurementController::Impl"
Cohesion: 0.18
Nodes (12): QVariantList, vector, MeasurementController::Impl, activePaneId_, cursor_, hasCursor_, mode_, points_ (+4 more)

### Community 37 - "RadarSiteDataService"
Cohesion: 0.12
Nodes (16): Impl, Q_OBJECT, unique_ptr, RadarSiteDataService, LevelThreeCatalogFailed, LevelThreeCatalogLoading, LevelThreeCatalogReady, LevelThreeDataLoadedForRequest (+8 more)

### Community 38 - "RadarSweepLayerBinding"
Cohesion: 0.18
Nodes (12): shared_ptr, sweep_snapshot, SweepSnapshot, colorTableLut, sweep, Impl, mutex, shared_ptr (+4 more)

### Community 39 - "WxLens AI Agent Instructions"
Cohesion: 0.14
Nodes (14): Adding a new Data Layer Provider (Phase 2/3), Adding a new overlay draw primitive, Adding new NEXRAD product support, Build system, Code style & conventions, Common tasks, Conan + CMake workflow, Custom map layers must trigger their own repaints (+6 more)

### Community 40 - "ConnectProductSignals"
Cohesion: 0.18
Nodes (17): CompatiblePalettes(), QObject, QStringList, availableProducts, compatiblePaletteNames, effectivePaletteName, homeLatitude, homeLongitude (+9 more)

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
Cohesion: 0.19
Nodes (14): AppendVertex(), BuildLevel3RasterSnapshot(), CartesianPackets, family, raster, Level3File, optional, ProductSymbologyBlock (+6 more)

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

### Community 51 - "PropagateChannel"
Cohesion: 0.33
Nodes (6): ChangeOrigin, PaneController, SyncChannel, copyCamera, copyChannel, PropagateChannel

### Community 52 - "ComputeSweep"
Cohesion: 0.33
Nodes (9): ComputeCoordinates(), ComputeSweep(), vector, IsRadarDataIncomplete(), NormalizeAngle(), elevation_cuts, DataBlockType, degrees (+1 more)

### Community 53 - "ThemeManager::Impl"
Cohesion: 0.28
Nodes (6): QObject, SettingsStore, ThemeManager::Impl, active_, themes_, QMap

### Community 54 - "unit_format.cpp"
Cohesion: 0.39
Nodes (8): QString, DecimalsFor(), FormatAltitude(), FormatBearing(), FormatGroundDistance(), GetDistanceUnitPreference(), SetDistanceUnitPreference(), DistanceUnitPreference

### Community 55 - "RadarSiteDataService::Impl"
Cohesion: 0.13
Nodes (15): Level3File, mutex, RadarSiteDataService::Impl, catalogLoadInProgress_, level2Provider_, level3Cache_, level3Catalog_, level3Mutex_ (+7 more)

### Community 56 - "GraphicOverlayPrimitive"
Cohesion: 0.14
Nodes (14): GeographicPoint, latitude, longitude, GraphicOverlayPrimitive, forecast, geometry, kind, label (+6 more)

### Community 57 - "applyChannelValue"
Cohesion: 0.36
Nodes (8): ChangeOrigin, QVariant, SyncChannel, applyChannelValue, channelValue, level3Product, setSyncGroup, syncGroup

### Community 58 - "RadarSiteInfo"
Cohesion: 0.14
Nodes (14): optional, string, vector, FindRadarSite(), RadarSiteInfo, altitudeMslMeters, country, id (+6 more)

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

### Community 70 - "Level3RadialMetadata"
Cohesion: 0.05
Nodes (43): AppendVertex(), BuildLevel3RadialSnapshot(), Level3File, optional, ProductSymbologyBlock, shared_ptr, uint8_t, FindRadialPacket() (+35 more)

### Community 72 - "Project architecture"
Cohesion: 0.40
Nodes (5): Data Source → Data Product → Visualization Layer → View, Namespace convention, Per-channel synchronization, not a link boolean, Project architecture, `wxdata` (reused, unmodified) + `app/` (new)

### Community 73 - "roleNames"
Cohesion: 0.67
Nodes (3): QByteArray, QHash, roleNames

### Community 74 - "map_object_store.cpp"
Cohesion: 0.26
Nodes (10): QObject, QPointF, DistanceBetween(), DistanceToSegment(), Clear, clearObjects, refreshFormatting, setObjectScope (+2 more)

### Community 75 - "3. New project structure"
Cohesion: 0.40
Nodes (5): 3.1 `wxdata` reuse mechanics, 3.2 Proposed directory layout (new repo), 3.3 Agent-legibility documents (Phase 0 deliverable), 3.4 Audit-friendly logging (Phase 0 deliverable), 3. New project structure

### Community 76 - "External dependencies"
Cohesion: 0.50
Nodes (4): External dependencies, License discipline, Vendored-dependency patches (`external/patches/`), Vendored submodules (`external/`)

### Community 77 - "PaneController"
Cohesion: 0.36
Nodes (11): ApplyOrigin(), PaneController, QString, QVariantList, Add, addLine, addMarker, addRangeRing (+3 more)

### Community 78 - "Phase 1 packaged acceptance record — 2026-08-30"
Cohesion: 0.33
Nodes (5): Build and automated checks, Next acceptance action, Packaged visual checks, Phase 1 packaged acceptance record — 2026-08-30, Preliminary performance evidence

### Community 79 - "BuildLevel3GraphicOverlaySnapshot"
Cohesion: 0.25
Nodes (7): BuildLevel3GraphicOverlaySnapshot(), Level3File, optional, ConvertsRealStormTrackingFixture, Level3GraphicOverlay, RejectsRadialProduct, TEST()

### Community 80 - "Level3RasterSnapshot"
Cohesion: 0.33
Nodes (5): shared_ptr, Level3File, Level3RasterSnapshot, metadata, sweep

### Community 81 - "Section"
Cohesion: 0.50
Nodes (4): Section, id, summary, title

### Community 82 - "WxLens performance baseline"
Cohesion: 0.40
Nodes (4): 2026-08-29 status, 2026-08-30 preliminary packaged run, Required capture procedure, WxLens performance baseline

### Community 83 - "Q: Fix manual-test issues: full state-name radar search, site-change map recentering, and visible pane-local palette changes"
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: Fix manual-test issues: full state-name radar search, site-change map recentering, and visible pane-local palette changes, Source Nodes

### Community 85 - "Q: OK, so what is the next step in this process?"
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: OK, so what is the next step in this process?, Source Nodes

### Community 86 - "Level3ProductDescriptor"
Cohesion: 0.20
Nodes (9): vector, level3_catalog, QString, Level3ProductDescriptor, awipsId, categoryDescription, categoryId, description (+1 more)

### Community 87 - "string"
Cohesion: 0.27
Nodes (4): string, optional, vector, Level3File

### Community 88 - "MapObjectStore"
Cohesion: 0.13
Nodes (18): QModelIndex, QVariant, Impl, Q_INVOKABLE, QAbstractListModel, unique_ptr, MapObjectStore, data (+10 more)

### Community 89 - "QVariantList"
Cohesion: 0.20
Nodes (6): PaneController, MapObjectStore, SettingsStore, QVariantList, PaneController, SettingsStore

### Community 90 - "radarSites"
Cohesion: 0.47
Nodes (6): CountryName(), QString, QVariantList, string, radarSites, RegionName()

### Community 91 - "QString"
Cohesion: 0.31
Nodes (9): PaneController, QString, addPoint, beginDrag, commit, formatDistance, readout, readoutForPoints (+1 more)

### Community 92 - "MeasurementTest"
Cohesion: 0.20
Nodes (6): MapObjectStore, PaneController, testing::Test, MeasurementTest, measurement_, model_

### Community 93 - "Q: Fix the Weather Overlays contrast problem."
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: Fix the Weather Overlays contrast problem., Source Nodes

### Community 94 - "MapObjectScope"
Cohesion: 0.25
Nodes (8): MapObjectScopeKind, SyncChannel, SyncGroupId, MapObjectScope, channel, kind, originGroupId, originPaneId

### Community 95 - "SetArchiveTime"
Cohesion: 0.33
Nodes (6): optional, PaneController, time_point, SetArchiveTime, selectArchiveTime, selectLive

### Community 96 - "Q: Why is WxLens velocity less vivid than RadarOmega and how should palette buttons be categorized without mixing radar fields?"
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: Why is WxLens velocity less vivid than RadarOmega and how should palette buttons be categorized without mixing radar fields?, Source Nodes

### Community 97 - "SourceProbeTest"
Cohesion: 0.22
Nodes (8): ProbeAlwaysReportsWhatWasAsked, PaneController, testing::Test, SourceProbeTest, model_, TEST_F(), UnavailableProbeCarriesNoBeamFiguresAtAll, UnboundSourceIsUnavailableWithAReason

### Community 98 - "Q: Fix palette editor drags moving the map, make saved velocity use the new palette, and soften low-reflectivity blue noise."
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: Fix palette editor drags moving the map, make saved velocity use the new palette, and soften low-reflectivity blue noise., Source Nodes

### Community 99 - "Level3GraphicOverlaySnapshot"
Cohesion: 0.33
Nodes (6): time_point, uint16_t, Level3GraphicOverlaySnapshot, primitives, productTime, unsupportedPacketCodes

### Community 100 - "QString"
Cohesion: 0.25
Nodes (3): QString, BundledPaletteName(), QString

### Community 101 - "PaneGridModel::Impl"
Cohesion: 0.20
Nodes (10): unique_ptr, vector, PaneGridModel::Impl, activePaneIndex_, defaultSourceKey_, gridHeight_, gridWidth_, nextPaneId_ (+2 more)

### Community 102 - "TEST"
Cohesion: 0.22
Nodes (7): BuildLevel3ProductCatalog(), string, vector, Level3ProductCatalog, LevelTwoAndLevelThreeIdentitiesCannotCollide, MapsAvailableAwipsIdsToCanonicalCategories, TEST()

### Community 103 - "GeodesicInverseResult"
Cohesion: 0.25
Nodes (6): GeodesicDirect(), GeodesicInverse(), GeodesicInverseResult, azimuthDegrees, distanceMeters, pair

### Community 104 - "Retest checklist"
Cohesion: 0.25
Nodes (7): Fixes included, Palette isolation, Product-browser wheel isolation, Retest checklist, Verification performed, Weather Overlays, WxLens critical-defects retest build

### Community 105 - "setDefaultSourceKey"
Cohesion: 0.33
Nodes (6): QModelIndex, QVariant, data, rowCount, setDefaultSourceKey, setGridSize

### Community 106 - "Point"
Cohesion: 0.40
Nodes (5): Point, latitude, longitude, snapKind, snapLabel

### Community 107 - "Q: Could WxLens support two windows on separate monitors, with a 2x2 layout on one and 1x1 on the other, all panes linked, and how hard would that be?"
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: Could WxLens support two windows on separate monitors, with a 2x2 layout on one and 1x1 on the other, all panes linked, and how hard would that be?, Source Nodes

### Community 108 - "RadarBeamProbe"
Cohesion: 0.25
Nodes (8): RadarBeamProbe, azimuthDegrees, beamCenterAboveRadarMeters, beamCenterAltitudeMslMeters, elevationAngleDegrees, elevationAngleKnown, rangeMeters, siteAltitudeMslMeters

### Community 109 - "TEST"
Cohesion: 0.29
Nodes (6): AltitudeIsConvertedFromTheListsFeet, AltitudesAreSaneAsMetresAcrossTheExtremes, KnownSiteResolves, RadarSiteDatabaseTest, TEST(), UnknownSiteResolvesToNothing

### Community 111 - "ToolbarAction"
Cohesion: 0.67
Nodes (3): ToolbarAction, id, label

### Community 112 - "MapDetailGroup"
Cohesion: 0.67
Nodes (3): MapDetailGroup, id, label

### Community 113 - "QObject"
Cohesion: 0.33
Nodes (4): QObject, PaneController, QStringList, SettingsStore

### Community 114 - "applyMapDetails"
Cohesion: 0.67
Nodes (3): QVariantMap, applyMapDetails, PaneController::probeSourceAt()

### Community 115 - "MapObjectStore::Impl"
Cohesion: 0.33
Nodes (6): vector, MapObjectStore::Impl, nextId_, objects_, revision_, Objects

### Community 116 - "PaneSyncTest"
Cohesion: 0.25
Nodes (4): PaneController, testing::Test, PaneSyncTest, model_

### Community 117 - "roleNames"
Cohesion: 0.67
Nodes (3): QByteArray, QHash, roleNames

### Community 118 - "Q: Address second manual UX feedback round and explain remaining acceptance checks"
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: Address second manual UX feedback round and explain remaining acceptance checks, Source Nodes

## Knowledge Gaps
- **587 isolated node(s):** `radarSite_`, `level2Provider_`, `level3Mutex_`, `level3Providers_`, `level3Cache_` (+582 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 996 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Work-memory lessons

**Preferred sources** — corroborated by past sessions; start here.
- `PaletteManager` (4× useful, score=3.903533413)
- `PaneGridModel` (2× useful, score=1.955415112)
- `ColorTableLut` (2× useful, score=1.93325599)
- `Phase 1 completion and release-readiness gates` (2× useful, score=1.888453642)
- `Remaining Phase 1 Level 3 sub-slices` (2× useful, score=1.888453642)

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `map_` connect `PaneController::Impl` to `AppSettings::Impl`, `app_settings.cpp`, `radar_site_data_service.cpp`, `PaletteManager`, `QString`, `settings_store.cpp`, `PaneController`, `QObject`, `RadarSweepProduct`, `RadarSiteInfo`?**
  _High betweenness centrality (0.073) - this node is a cross-community bridge._
- **Why does `PaneController::Impl` connect `PaneController::Impl` to `OverlayManager`, `RadarSiteDataService`, `RadarSweepLayerBinding`, `ConnectProductSignals`, `PaneController`, `QVariantList`, `Level3ProductDescriptor`, `RadarSweepProduct`, `applyChannelValue`, `ProductDescriptor`, `SetArchiveTime`?**
  _High betweenness centrality (0.063) - this node is a cross-community bridge._
- **Why does `PaneGridModel` connect `PaneGridModel` to `SourceProbeTest`, `roleNames`, `setDefaultSourceKey`, `PropagateChannel`, `PaneSyncTest`, `TEST_F`, `QVariantList`, `radarSites`, `MeasurementTest`?**
  _High betweenness centrality (0.058) - this node is a cross-community bridge._
- **What connects `radarSite_`, `level2Provider_`, `level3Mutex_` to the rest of the system?**
  _587 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `OverlayManager` be split into smaller, more focused modules?**
  _Cohesion score 0.05009009009009009 - nodes in this community are weakly interconnected._
- **Should `SavedPlaceManager` be split into smaller, more focused modules?**
  _Cohesion score 0.06875 - nodes in this community are weakly interconnected._
- **Should `MapObject` be split into smaller, more focused modules?**
  _Cohesion score 0.1111111111111111 - nodes in this community are weakly interconnected._