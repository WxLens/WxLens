# Graph Report - WxLens  (2026-09-03)

## Corpus Check
- 116 files · ~131,484 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1918 nodes · 3065 edges · 159 communities (126 shown, 31 thin omitted)
- Extraction: 94% EXTRACTED · 6% INFERRED · 0% AMBIGUOUS · INFERRED: 189 edges (avg confidence: 0.83)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `19b520e1`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- overlay_manager.cpp
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
- shared_ptr
- RadarSweepProduct
- PolylineData
- TEST_F
- Level3RasterMetadata
- RadarSweepLayer::Impl
- TEST_F
- RadarProductStatus
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
- radar_sweep_product.cpp
- ThemeManager::Impl
- unit_format.cpp
- RadarSiteDataService::Impl
- GraphicOverlayPrimitive
- applyChannelValue
- logger
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
- "WxLens" name collision screening — 2026-09-03
- Project architecture
- roleNames
- map_object_store.cpp
- 3. New project structure
- palette_manager.cpp
- PaneController
- Phase 1 packaged acceptance record — 2026-08-30
- BuildLevel3GraphicOverlaySnapshot
- Level3RasterSnapshot
- Section
- WxLens performance baseline
- Q: Fix manual-test issues: full state-name radar search, site-change map recentering, and visible pane-local palette changes
- Q: OK, so what is the next step in this process?
- Level3ProductDescriptor
- polyline_layer.cpp
- MapObjectStore
- QObject
- radarSites
- QString
- MeasurementTest
- Q: Fix the Weather Overlays contrast problem.
- MapObjectScope
- SetArchiveTime
- Q: Why is WxLens velocity less vivid than RadarOmega and how should palette buttons be categorized without mixing radar fields?
- SourceProbeTest
- Q: Fix palette editor drags moving the map, make saved velocity use the new palette, and soften low-reflectivity blue noise.
- OverlayManager
- QString
- PaneGridModel::Impl
- TEST
- TEST
- Retest checklist
- setDefaultSourceKey
- Point
- Q: Could WxLens support two windows on separate monitors, with a 2x2 layout on one and 1x1 on the other, all panes linked, and how hard would that be?
- string
- contrast-audit.py
- OverlayManager::Impl
- QString
- FamilyMembers
- ui-drive.ps1
- QVariantMap
- overlay_manager.hpp
- roleNames
- Q: Address second manual UX feedback round and explain remaining acceptance checks
- refreshPlacefile
- TEST_F
- PolylineLayer::Impl
- SweepData
- PolylineVertex
- string
- pane_controller.hpp
- QByteArray
- QString
- QTimer
- QUrl
- QVariantList
- rgba8_pixel_t
- OverlayManagerTest
- ChangeOrigin
- optional
- PaneController
- QPointF
- QVariant
- QVariantList
- SyncChannel
- SyncGroupId
- time_point
- uint64_t
- vector
- Impl
- Q_INVOKABLE
- unique_ptr
- Entry
- MapObjectStore::Impl
- Level3GraphicOverlaySnapshot
- Common tasks
- QString
- main
- QStringList
- QVariantList
- QVariantMap
- Impl
- unique_ptr
- Q_ENUM

## God Nodes (most connected - your core abstractions)
1. `PaneController` - 80 edges
2. `PaletteManager` - 75 edges
3. `AppSettings` - 55 edges
4. `PaneController::Impl` - 54 edges
5. `TEST()` - 40 edges
6. `ThemeManager` - 36 edges
7. `PaletteModel` - 33 edges
8. `PaneGridModel` - 33 edges
9. `RadarSweepProduct::Impl` - 32 edges
10. `RadarSweepLayer::Impl` - 31 edges

## Surprising Connections (you probably didn't know these)
- `TEST()` --calls--> `bindSettings`  [INFERRED]
  test/source/wxlens/palettes/palette_manager.test.cpp → app/source/wxlens/palettes/palette_manager.hpp
- `TEST()` --calls--> `paletteNames`  [INFERRED]
  test/source/wxlens/palettes/palette_manager.test.cpp → app/source/wxlens/palettes/palette_manager.hpp
- `TEST()` --calls--> `activeName`  [INFERRED]
  test/source/wxlens/palettes/palette_manager.test.cpp → app/source/wxlens/palettes/palette_manager.hpp
- `TEST()` --calls--> `activeText`  [INFERRED]
  test/source/wxlens/palettes/palette_manager.test.cpp → app/source/wxlens/palettes/palette_manager.hpp
- `TEST()` --calls--> `paletteText`  [INFERRED]
  test/source/wxlens/palettes/palette_manager.test.cpp → app/source/wxlens/palettes/palette_manager.hpp

## Import Cycles
- None detected.

## Communities (159 total, 31 thin omitted)

### Community 0 - "overlay_manager.cpp"
Cohesion: 0.18
Nodes (17): AppendWarnings(), Coordinates(), shared_ptr, vector, FlattenPlacefile(), placefileItems, placefiles, refreshingWarnings (+9 more)

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
Cohesion: 0.08
Nodes (27): map, PaletteModel, PendingAction, Q_ENUM, Q_INVOKABLE, QObject, QString, PaletteManager (+19 more)

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
Nodes (38): PaneController, QObject, QVariantList, Impl, Q_ENUM, QObject, unique_ptr, ObjectToolController (+30 more)

### Community 10 - "TEST_F"
Cohesion: 0.07
Nodes (34): ArmShutdownWatchdog(), string, DumpAllThreadStacks(), ExceptionCodeName(), HandleException(), InstallCrashHandler(), WalkAndWriteStack(), WriteAll() (+26 more)

### Community 11 - "PaneController::Impl"
Cohesion: 0.05
Nodes (38): PaneController::Impl, actualTime_, archiveTime_, bearing_, catalogFailedConnection_, catalogLoadingConnection_, catalogReadyConnection_, centerLatitude_ (+30 more)

### Community 12 - "AppSettings"
Cohesion: 0.08
Nodes (33): AppSettings, centerMapOnSiteChangeChanged, configDirectory, controlBarDockedChanged, defaultObjectScopeChanged, defaultsReset, distanceUnitsChanged, geometryRows (+25 more)

### Community 13 - "settings_store.cpp"
Cohesion: 0.12
Nodes (30): Category, dirty, failedToParse, loaded, table, mutex, QString, DefaultConfigDirectory() (+22 more)

### Community 14 - "PaneController"
Cohesion: 0.08
Nodes (48): QString, QObject, PaneController, bearing, cameraChanged, cameraSynced, centerLatitude, centerLongitude (+40 more)

### Community 15 - "TEST"
Cohesion: 0.07
Nodes (27): AltitudeRisesWithRangeAndWithTilt, GeodesicDirect(), GeodesicInverse(), GeodesicInverseResult, azimuthDegrees, distanceMeters, BeamAltitudeMsl(), optional (+19 more)

### Community 16 - "TEST_F"
Cohesion: 0.06
Nodes (33): ActivePaneTracksSelectionAndSurvivesLayoutShrink, CameraGroupHelperGroupsEveryCameraChannel, CenterOnCommandsTheViewAndPropagatesCameraChannels, ChannelsAreIndependent, ChannelsWithoutStateArePropagationNoOps, CopyCameraIsOneShotAcrossEveryCameraChannel, CopyChannelIsOneShotNotAPersistentLink, ElevationSelectionRejectsNoRealCutButRetainsRequestedCut (+25 more)

### Community 17 - "QVariantList"
Cohesion: 0.33
Nodes (6): PaneController::coordinateAtOffset(), PaneController::coordinateForPixel(), elevationCuts, productCatalog, productOverlays, QVariantList

### Community 18 - "ThemeManager"
Cohesion: 0.08
Nodes (25): Impl, QObject, unique_ptr, ThemeManager, accent, border, control, controlActive (+17 more)

### Community 19 - "TEST_F"
Cohesion: 0.12
Nodes (17): BearingIsReportedAsACompassAngle, CommitExcludesTheLiveCursor, CommitPinsAMeasurementObject, CommittedMeasurementRespectsScope, DistanceFormattingShowsBothUnits, DistanceProducesDistanceAndBearing, DistanceStopsAtTwoVertices, GeodesicMatchesTheUnderlyingHelper (+9 more)

### Community 20 - "Theme"
Cohesion: 0.08
Nodes (26): QByteArray, QColor, ParseTheme(), Theme, accent, background, border, control (+18 more)

### Community 21 - "shared_ptr"
Cohesion: 0.28
Nodes (9): BuildColorTableLut(), BuildColorTableLutFromTable(), QString, shared_ptr, color_table_lut, OnLevelTwoDataLoaded, sweep_data, Ar2vFile (+1 more)

### Community 22 - "RadarSweepProduct"
Cohesion: 0.16
Nodes (16): optional, QObject, time_point, Impl, Q_OBJECT, QObject, unique_ptr, RadarSweepProduct (+8 more)

### Community 23 - "PolylineData"
Cohesion: 0.24
Nodes (10): vector, PolylineData, vertices, Impl, shared_ptr, shared_ptr, PolylineLayerBinding, data (+2 more)

### Community 24 - "TEST_F"
Cohesion: 0.08
Nodes (25): AllPanesIsVisibleEverywhere, CurrentPaneOnlyIsVisibleOnlyInItsOwnPane, DrawingHelperPinsGeographicLineAndRejectsOnePoint, GeometryIsValidated, HitTestingWithoutAMapNeverHits, NullPaneIsNeverVisible, ObjectsForPaneFiltersByScope, PinnedObjectsAreStored (+17 more)

### Community 25 - "Level3RasterMetadata"
Cohesion: 0.09
Nodes (23): array, DataLevelCode, int16_t, optional, string, time_point, uint16_t, Level3RasterMetadata (+15 more)

### Community 26 - "RadarSweepLayer::Impl"
Cohesion: 0.10
Nodes (20): array, uint16_t, unique_ptr, RadarSweepLayer::Impl, binding_, colorTableMin_, colorTableScale_, gl_ (+12 more)

### Community 27 - "TEST_F"
Cohesion: 0.14
Nodes (18): AppliedEditToAFamilyDefaultReachesPanesWithoutAnOverride, ApplyingAVelocityPaletteNeverRecoloursReflectivity, ColorTableLut, ExplicitPaneOverrideSurvivesAFamilyDefaultChange, PaletteSyncChannelRebuildsTheReceivingPanesLut, PaneGridModel, ReflectivityAndVelocityPanesRenderFromTheirOwnFamilies, ResetAllRestoresFactoryRenderingAndFamilyDefaults (+10 more)

### Community 28 - "RadarProductStatus"
Cohesion: 0.14
Nodes (18): QObject, QString, string, Impl, QObject, unique_ptr, RadarProductStatus, RadarProductStatus::Impl (+10 more)

### Community 29 - "RadarSweepProduct::Impl"
Cohesion: 0.10
Nodes (20): mutex, uint64_t, RadarSweepProduct::Impl, archiveTime_, colorTable_, colorTableLut_, data_, dataBlockType_ (+12 more)

### Community 30 - "radar_sweep_layer.cpp"
Cohesion: 0.24
Nodes (15): CheckGlError(), shared_ptr, Impl, unique_ptr, RadarSweepLayer, deinitialize, UploadColorTableLut, UploadSweep (+7 more)

### Community 31 - "ProductDescriptor"
Cohesion: 0.18
Nodes (10): QString, ProductDescriptor, elevation, identity, identityKind, kind, palette, product (+2 more)

### Community 32 - "SnapTargetRegistry"
Cohesion: 0.25
Nodes (7): Q_INVOKABLE, Q_OBJECT, QObject, PaneController, SnapTargetRegistry, public, QVariantMap

### Community 33 - "AppSettings::Impl"
Cohesion: 0.10
Nodes (19): AppSettings::Impl, centerMapOnSiteChange_, controlBarDocked_, defaultObjectScope_, distanceUnits_, geometryRowVisible_, Load, mapDetailsPreset_ (+11 more)

### Community 34 - "app_settings.cpp"
Cohesion: 0.11
Nodes (19): QVariantMap, centerMapOnSiteChange, mapDetailVisibility, preferredMeasurementTool, resetToolbarActions, setCenterMapOnSiteChange, setControlBarDocked, setDefaultObjectScope (+11 more)

### Community 35 - "TEST_F"
Cohesion: 0.10
Nodes (20): controlBarDocked, distanceUnits, mapDetailsPreset, mapTheme, measurementGesture, snapStrength, snapTolerancePixels, ChangesNotifyExactlyOnce (+12 more)

### Community 36 - "MeasurementController::Impl"
Cohesion: 0.18
Nodes (12): QVariantList, vector, MeasurementController::Impl, activePaneId_, cursor_, hasCursor_, mode_, points_ (+4 more)

### Community 37 - "RadarSiteDataService"
Cohesion: 0.12
Nodes (16): Impl, Q_OBJECT, unique_ptr, RadarSiteDataService, LevelThreeCatalogFailed, LevelThreeCatalogLoading, LevelThreeCatalogReady, LevelThreeDataLoadedForRequest (+8 more)

### Community 38 - "RadarSweepLayerBinding"
Cohesion: 0.16
Nodes (13): shared_ptr, sweep_snapshot, SweepSnapshot, colorTableLut, sweep, Impl, mutex, shared_ptr (+5 more)

### Community 39 - "WxLens AI Agent Instructions"
Cohesion: 0.15
Nodes (13): Build system, Code style & conventions, Conan + CMake workflow, External dependencies, graphify, License discipline, Qt 6.11.1 requirement, Resources (+5 more)

### Community 40 - "ConnectProductSignals"
Cohesion: 0.15
Nodes (20): CompatiblePalettes(), QObject, QStringList, DefaultPalette(), attachLayers, availableProducts, compatiblePaletteNames, homeLatitude (+12 more)

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
Cohesion: 0.09
Nodes (21): Definition of ready for the next user test, Direct pane targeting and radar-site selection, Measurement tool preferences and radar-value reader, Minimal and customizable persistent chrome, P0 — correctness and interaction defects, P0 — Palette synchronization has no user interface, P1 — Accessible names and roles are absent almost everywhere, P1 — Bottom control bar is unreachable at increased text scaling (+13 more)

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

### Community 52 - "radar_sweep_product.cpp"
Cohesion: 0.26
Nodes (12): ComputeCoordinates(), ComputeSweep(), vector, IsRadarDataIncomplete(), NormalizeAngle(), elevation_cuts, is_archive, site_latitude (+4 more)

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
Nodes (8): applyChannelValue, channelValue, level3Product, setSyncGroup, syncGroup, ChangeOrigin, QVariant, SyncChannel

### Community 58 - "logger"
Cohesion: 0.06
Nodes (28): AltitudeIsConvertedFromTheListsFeet, AltitudesAreSaneAsMetresAcrossTheExtremes, optional, string, vector, FindRadarSite(), RadarSiteInfo, altitudeMslMeters (+20 more)

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

### Community 71 - ""WxLens" name collision screening — 2026-09-03"
Cohesion: 0.18
Nodes (7): Directory map, WxLens — Repo Map, Decision, Findings, Sources, What was searched, 2026-09-03, "WxLens" name collision screening — 2026-09-03

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

### Community 76 - "palette_manager.cpp"
Cohesion: 0.19
Nodes (18): PendingAction, QUrl, FamilyLabel(), activeIsFactoryPalette, BeginAction, ClearPendingAction, completePendingSave, ExecutePendingAction (+10 more)

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

### Community 87 - "polyline_layer.cpp"
Cohesion: 0.24
Nodes (11): CheckGlError(), Impl, unique_ptr, PolylineLayer, deinitialize, Upload, initialize, p (+3 more)

### Community 88 - "MapObjectStore"
Cohesion: 0.13
Nodes (18): QModelIndex, QVariant, Impl, Q_INVOKABLE, QAbstractListModel, unique_ptr, MapObjectStore, data (+10 more)

### Community 89 - "QObject"
Cohesion: 0.14
Nodes (9): QObject, PaneController, PaneController, MapObjectStore, SettingsStore, QStringList, QVariantList, PaneController (+1 more)

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
Cohesion: 0.40
Nodes (5): SetArchiveTime, selectArchiveTime, selectLive, optional, time_point

### Community 96 - "Q: Why is WxLens velocity less vivid than RadarOmega and how should palette buttons be categorized without mixing radar fields?"
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: Why is WxLens velocity less vivid than RadarOmega and how should palette buttons be categorized without mixing radar fields?, Source Nodes

### Community 97 - "SourceProbeTest"
Cohesion: 0.22
Nodes (8): ProbeAlwaysReportsWhatWasAsked, PaneController, testing::Test, SourceProbeTest, model_, TEST_F(), UnavailableProbeCarriesNoBeamFiguresAtAll, UnboundSourceIsUnavailableWithAReason

### Community 98 - "Q: Fix palette editor drags moving the map, make saved velocity use the new palette, and soften low-reflectivity blue noise."
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: Fix palette editor drags moving the map, make saved velocity use the new palette, and soften low-reflectivity blue noise., Source Nodes

### Community 99 - "OverlayManager"
Cohesion: 0.15
Nodes (14): Impl, unique_ptr, OverlayManager, p, placefilesChanged, placefilesVisibleChanged, Q_PROPERTY, refreshingWarningsChanged (+6 more)

### Community 100 - "QString"
Cohesion: 0.29
Nodes (3): QString, BundledPaletteName(), QString

### Community 101 - "PaneGridModel::Impl"
Cohesion: 0.20
Nodes (10): unique_ptr, vector, PaneGridModel::Impl, activePaneIndex_, defaultSourceKey_, gridHeight_, gridWidth_, nextPaneId_ (+2 more)

### Community 102 - "TEST"
Cohesion: 0.20
Nodes (7): BuildLevel3ProductCatalog(), string, vector, Level3ProductCatalog, LevelTwoAndLevelThreeIdentitiesCannotCollide, MapsAvailableAwipsIdsToCanonicalCategories, TEST()

### Community 103 - "TEST"
Cohesion: 0.11
Nodes (18): PaletteModel, activeDraftApplied, confirmationRequired, editor, ApplyingAPaletteMakesItItsFamilysDefault, ConfirmationDoesNotAllowPendingActionToBeReplaced, DraftAppliedStateDistinguishesTheTwoCloseWarnings, EditsRemainDraftUntilApplied (+10 more)

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

### Community 108 - "string"
Cohesion: 0.27
Nodes (4): string, optional, vector, Level3File

### Community 109 - "contrast-audit.py"
Cohesion: 0.36
Nodes (8): composite(), luminance(), main(), parse(), ratio(), WCAG 2.1 contrast audit for the theme-token pairs the dialogs actually use. Run…, #AARRGGBB tokens (elevated_surface) are composited onto the surface they sit on., rgb()

### Community 110 - "OverlayManager::Impl"
Cohesion: 0.14
Nodes (14): QObject, OverlayManager::Impl, network_, placefileItems_, placefiles_, placefilesVisible_, refreshingWarnings_, self_ (+6 more)

### Community 111 - "QString"
Cohesion: 0.18
Nodes (17): QObject, QString, Activate, activeName, activeText, CanonicalUnits, familyDefaultsChanged, FamilyOf (+9 more)

### Community 112 - "FamilyMembers"
Cohesion: 0.23
Nodes (15): SettingsStore, applyActive, bindSettings, PaletteManager::compatibleFamilies(), familyDefault, familyDefaultNames, FamilyMembers, KnownFamilies (+7 more)

### Community 115 - "overlay_manager.hpp"
Cohesion: 0.50
Nodes (3): QObject, QNetworkAccessManager, QNetworkReply

### Community 117 - "roleNames"
Cohesion: 0.67
Nodes (3): QByteArray, QHash, roleNames

### Community 118 - "Q: Address second manual UX feedback round and explain remaining acceptance checks"
Cohesion: 0.40
Nodes (4): Answer, Outcome, Q: Address second manual UX feedback round and explain remaining acceptance checks, Source Nodes

### Community 119 - "refreshPlacefile"
Cohesion: 0.19
Nodes (12): addPlacefile, importWarningFile, refreshPlacefile, refreshWarnings, PlacefileRecord, error, items, loading (+4 more)

### Community 120 - "TEST_F"
Cohesion: 0.16
Nodes (11): placefilesVisible, warningsVisible, LoadsWxdataPlacefileVectors, QTemporaryDir, OverlayManagerTest, directory_, settings_, TEST_F() (+3 more)

### Community 121 - "PolylineLayer::Impl"
Cohesion: 0.15
Nodes (13): unique_ptr, PolylineLayer::Impl, binding_, gl_, lastUploaded_, numVertices_, shaderProgram_, uMVPMatrixLocation_ (+5 more)

### Community 122 - "SweepData"
Cohesion: 0.13
Nodes (16): ColorTableLut, colors, maximum, minimum, rgba8_pixel_t, string, uint16_t, uint8_t (+8 more)

### Community 123 - "PolylineVertex"
Cohesion: 0.29
Nodes (7): PolylineVertex, a, b, g, latitude, longitude, r

### Community 124 - "string"
Cohesion: 0.50
Nodes (5): string, Level2ProductForDescription(), LoadBundledColorTable(), ProductBlockType(), Level2Product

### Community 125 - "pane_controller.hpp"
Cohesion: 0.22
Nodes (8): shared_ptr, applyMapDetails, layerBinding, pixelForCoordinate, PaneController::probeSourceAt(), QVariantMap, RadarSweepLayerBinding, QPointF

### Community 147 - "Entry"
Cohesion: 0.25
Nodes (8): Entry, appliedText, factory, factoryText, family, source, units, workingText

### Community 148 - "MapObjectStore::Impl"
Cohesion: 0.33
Nodes (6): vector, MapObjectStore::Impl, nextId_, objects_, revision_, Objects

### Community 149 - "Level3GraphicOverlaySnapshot"
Cohesion: 0.33
Nodes (6): time_point, uint16_t, Level3GraphicOverlaySnapshot, primitives, productTime, unsupportedPacketCodes

### Community 150 - "Common tasks"
Cohesion: 0.40
Nodes (5): Adding a new Data Layer Provider (Phase 2/3), Adding a new overlay draw primitive, Adding new NEXRAD product support, Common tasks, Custom map layers must trigger their own repaints

### Community 151 - "QString"
Cohesion: 0.50
Nodes (4): ColorString(), statusText, QString, rgba8_pixel_t

## Knowledge Gaps
- **620 isolated node(s):** `Q_PROPERTY`, `paletteNamesChanged`, `activePaletteChanged`, `confirmationRequiredChanged`, `activeDraftAppliedChanged` (+615 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1084 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **31 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Work-memory lessons

**Preferred sources** — corroborated by past sessions; start here.
- `PaletteManager` (4× useful, score=3.855070895) _(code changed — re-verify)_
- `Phase 1 completion and release-readiness gates` (3× useful, score=2.853436771) _(code changed — re-verify)_
- `Remaining Phase 1 Level 3 sub-slices` (2× useful, score=1.865008417) _(code changed — re-verify)_

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `PaletteManager` connect `PaletteManager` to `app_settings.cpp`, `TEST`, `palette_manager.cpp`, `QString`, `FamilyMembers`, `Entry`, `TEST_F`?**
  _High betweenness centrality (0.070) - this node is a cross-community bridge._
- **Why does `PaneController::Impl` connect `PaneController::Impl` to `overlay_manager.cpp`, `ConnectProductSignals`, `PaneController`, `QVariantList`, `applyChannelValue`, `pane_controller.hpp`, `SetArchiveTime`?**
  _High betweenness centrality (0.061) - this node is a cross-community bridge._
- **Why does `PaneGridModel` connect `PaneGridModel` to `SourceProbeTest`, `setDefaultSourceKey`, `roleNames`, `TEST_F`, `PropagateChannel`, `main`, `QObject`, `radarSites`, `MeasurementTest`, `TEST_F`?**
  _High betweenness centrality (0.045) - this node is a cross-community bridge._
- **Are the 21 inferred relationships involving `TEST()` (e.g. with `activeDraftApplied` and `activeName`) actually correct?**
  _`TEST()` has 21 INFERRED edges - model-reasoned connections that need verification._
- **What connects `Q_PROPERTY`, `paletteNamesChanged`, `activePaletteChanged` to the rest of the system?**
  _620 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `SavedPlaceManager` be split into smaller, more focused modules?**
  _Cohesion score 0.06875 - nodes in this community are weakly interconnected._
- **Should `MapObject` be split into smaller, more focused modules?**
  _Cohesion score 0.1111111111111111 - nodes in this community are weakly interconnected._