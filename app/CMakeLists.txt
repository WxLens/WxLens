cmake_minimum_required(VERSION 3.24)
project(wxlens-app CXX)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 OpenGL Network)
qt_standard_project_setup(REQUIRES 6.5)

# Public ingestion address only; never embed a Sentry API/auth token in the app.
set(WXLENS_SENTRY_DSN "https://8ccd3a6583abc6085ea5fa236535cd94@o4512056725602304.ingest.us.sentry.io/4512056733401088"
    CACHE STRING "Public Sentry crash reporting DSN (empty disables uploads)")
if (WXLENS_SENTRY_DSN AND NOT WXLENS_SENTRY_DSN MATCHES "^https://[0-9a-fA-F]+@[a-z0-9.-]+/[0-9]+$")
    message(FATAL_ERROR "WXLENS_SENTRY_DSN must be a public HTTPS DSN, without a password, query or fragment")
endif()
configure_file(source/wxlens/util/crash_reporting_config.hpp.in
    generated/wxlens/util/crash_reporting_config.hpp @ONLY)

# Boost::timer/Boost::json are used by wxdata (boost::timer::cpu_timer in
# aws_level2_chunks_data_provider.cpp, boost::json in util/json.hpp) but wxdata.cmake itself
# (external/, read-only) doesn't declare them, and wxdata is an OBJECT library so this only
# surfaces at final link time. find_package(Boost) also doesn't create globally-visible imported
# targets, so it must be called again here even though wxdata.cmake already called it.
find_package(Boost REQUIRED COMPONENTS timer json)
find_package(GeographicLib REQUIRED)
find_package(glm REQUIRED)
# toml++ backs the structured config store (docs/adr/0003-config-storage-format.md).
find_package(tomlplusplus REQUIRED)

# Split into a library plus a thin main() so wxlens-app-test can LINK these sources instead of
# recompiling them, and - the reason it happened now - so the QML module can live on a target a
# test binary is allowed to link against. An executable cannot be linked into another target, so
# while qt_add_qml_module sat on wxlens-app there was no way to exercise the QML in a test at all.
# test/test.cmake anticipated this ("the app should be split into a static library plus a thin
# main()"); ROADMAP slice 19 is where it came due.
qt_add_library(wxlens-app-lib STATIC
    source/wxlens/log/logger.cpp
    source/wxlens/log/logger.hpp
    source/wxlens/log/frame_timing.cpp
    source/wxlens/log/frame_timing.hpp
    source/wxlens/data/frame_cache.hpp
    source/wxlens/data/radar_site_data_service.cpp
    source/wxlens/data/radar_site_data_service.hpp
    source/wxlens/data/radar_site_database.cpp
    source/wxlens/data/radar_site_database.hpp
    source/wxlens/data/radar_site_marker_source.cpp
    source/wxlens/data/radar_site_marker_source.hpp
    source/wxlens/objects/map_object.hpp
    source/wxlens/objects/map_object_store.cpp
    source/wxlens/objects/map_object_store.hpp
    source/wxlens/objects/measurement_controller.cpp
    source/wxlens/objects/measurement_controller.hpp
    source/wxlens/objects/object_tool_controller.cpp
    source/wxlens/objects/object_tool_controller.hpp
    source/wxlens/objects/snap_target_registry.cpp
    source/wxlens/objects/snap_target_registry.hpp
    source/wxlens/objects/saved_place_manager.cpp
    source/wxlens/objects/saved_place_manager.hpp
    source/wxlens/overlays/overlay_manager.cpp
    source/wxlens/overlays/overlay_manager.hpp
    source/wxlens/panes/pane_controller.cpp
    source/wxlens/panes/pane_controller.hpp
    source/wxlens/panes/pane_grid_model.cpp
    source/wxlens/panes/pane_grid_model.hpp
    source/wxlens/panes/sync_types.hpp
    source/wxlens/palettes/palette_manager.cpp
    source/wxlens/palettes/palette_manager.hpp
    source/wxlens/palettes/palette_defaults.hpp
    source/wxlens/palettes/palette_model.cpp
    source/wxlens/palettes/palette_model.hpp
    source/wxlens/products/product_descriptor.hpp
    source/wxlens/products/level3_product_catalog.cpp
    source/wxlens/products/level3_product_catalog.hpp
    source/wxlens/products/level3_radial_product.cpp
    source/wxlens/products/level3_radial_product.hpp
    source/wxlens/products/level3_raster_product.cpp
    source/wxlens/products/level3_raster_product.hpp
    source/wxlens/products/level3_graphic_overlay.cpp
    source/wxlens/products/level3_graphic_overlay.hpp
    source/wxlens/products/level3_text_product.cpp
    source/wxlens/products/level3_text_product.hpp
    source/wxlens/products/radar_product_status.cpp
    source/wxlens/products/radar_product_status.hpp
    source/wxlens/products/radar_sweep_product.cpp
    source/wxlens/products/radar_sweep_product.hpp
    source/wxlens/render/radar_sweep_layer.cpp
    source/wxlens/render/radar_sweep_layer.hpp
    source/wxlens/settings/app_settings.cpp
    source/wxlens/settings/app_settings.hpp
    source/wxlens/settings/settings_store.cpp
    source/wxlens/settings/settings_store.hpp
    source/wxlens/theme/theme_manager.cpp
    source/wxlens/theme/theme_manager.hpp
    source/wxlens/util/crash_handler.cpp
    source/wxlens/util/crash_handler.hpp
    source/wxlens/util/crash_report.cpp
    source/wxlens/util/crash_report.hpp
    source/wxlens/util/crash_report_manager.cpp
    source/wxlens/util/crash_report_manager.hpp
    source/wxlens/util/geodesic.cpp
    source/wxlens/util/geodesic.hpp
    source/wxlens/util/radar_geometry.cpp
    source/wxlens/util/radar_geometry.hpp
    source/wxlens/util/unit_format.cpp
    source/wxlens/util/unit_format.hpp
)

qt_add_executable(wxlens-app
    source/wxlens/main/main.cpp
    $<$<PLATFORM_ID:Windows>:res/branding/app.rc>
)

target_include_directories(wxlens-app-lib PUBLIC source "${CMAKE_CURRENT_BINARY_DIR}/generated")

qt_add_qml_module(wxlens-app-lib
    URI WxLens.App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/CrashReporter.qml
        qml/Chrome/TopBar.qml
        qml/Chrome/BottomControlBar.qml
        qml/Chrome/TimeControls.qml
        qml/Dialogs/SettingsChoice.qml
        qml/Dialogs/SettingsDialog.qml
        qml/Dialogs/PaletteDialog.qml
        qml/Dialogs/SavedPlacesDialog.qml
        qml/Dialogs/OverlaysDialog.qml
        qml/Dialogs/HelpDialog.qml
        qml/Dialogs/CrashReportDialog.qml
        qml/Controls/ColorPicker.qml
        qml/Controls/DialogCloseButton.qml
        qml/Controls/WxButton.qml
        qml/Controls/WxMenuItem.qml
        qml/Panes/MapObjectsLayer.qml
        qml/Panes/MeasurementLayer.qml
        qml/Panes/RadarGeometryPanel.qml
        qml/Panes/PaneGrid.qml
        qml/Panes/PaneHost.qml
        qml/Panes/ProductBrowser.qml
        qml/Panes/SitePicker.qml
        qml/Panes/Level3ProductLayer.qml
        qml/Panes/WeatherOverlaysLayer.qml
        qml/Panes/RadarSiteLayer.qml
    RESOURCES
        res/branding/wxlens-icon.png
        res/config/radar_sites.json
        res/themes/operational-dark.toml
        res/themes/daylight.toml
        res/gl/radar.vert
        res/gl/radar.frag
)

# NOAA Weather and Climate Toolkit defaults, carried forward unmodified with the attribution in
# ACKNOWLEDGEMENTS.md. Referencing the vendored copies as resource inputs keeps external/ pristine
# and prevents WxLens's copies drifting from the reused legacy dependency.
set(WXLENS_WCT_PALETTE_DIR
    "${CMAKE_SOURCE_DIR}/external/legacy-supercell-wx/scwx-qt/res/palettes/wct")
qt_add_resources(wxlens-app-lib "wxlens-wct-palettes"
    PREFIX "/qt/qml/WxLens/App/res/palettes/wct"
    BASE "${WXLENS_WCT_PALETTE_DIR}"
    FILES
        "${WXLENS_WCT_PALETTE_DIR}/CC.pal"
        "${WXLENS_WCT_PALETTE_DIR}/Default16.pal"
        "${WXLENS_WCT_PALETTE_DIR}/DOD_DSD.pal"
        "${WXLENS_WCT_PALETTE_DIR}/ET.pal"
        "${WXLENS_WCT_PALETTE_DIR}/HC.pal"
        "${WXLENS_WCT_PALETTE_DIR}/KDP.pal"
        "${WXLENS_WCT_PALETTE_DIR}/KDP2.pal"
        "${WXLENS_WCT_PALETTE_DIR}/OHP.pal"
        "${WXLENS_WCT_PALETTE_DIR}/SRV.pal"
        "${WXLENS_WCT_PALETTE_DIR}/STP.pal"
        "${WXLENS_WCT_PALETTE_DIR}/SW.pal"
        "${WXLENS_WCT_PALETTE_DIR}/VIL.pal"
        "${WXLENS_WCT_PALETTE_DIR}/ZDR.pal")

# WxLens's reflectivity and velocity defaults use application-owned ramps tuned for legibility
# over the dark map. Keep these visual assets outside the read-only dependency.
qt_add_resources(wxlens-app-lib "wxlens-app-palettes"
    PREFIX "/qt/qml/WxLens/App/res/palettes/wct"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/res/palettes/wct"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/res/palettes/wct/DR.pal"
        "${CMAKE_CURRENT_SOURCE_DIR}/res/palettes/wct/DV.pal")

set_target_properties(wxlens-app-lib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

set_target_properties(wxlens-app PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

target_link_libraries(wxlens-app-lib PUBLIC
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::OpenGL
    Qt6::Network
    wxdata
    Boost::timer
    Boost::json
    GeographicLib::GeographicLib
    glm::glm-header-only
    tomlplusplus::tomlplusplus
    QMapLibre::Core
    # DbgHelp backs wxlens::util::InstallCrashHandler's symbolized stack walk. It ships with
    # Windows itself, so this adds a link dependency, not a redistributable.
    $<$<PLATFORM_ID:Windows>:dbghelp>
)

# Both halves are required, and the second one is easy to miss. qt_add_qml_module on a STATIC
# library splits the module in two: the library holds the C++, while a generated companion target
# (<target>plugin) carries the type registration and the compiled QML/resources. Linking only
# wxlens-app-lib builds and links cleanly, then fails at runtime with
# `No module named "WxLens.App" found` - confirmed by an offscreen launch during this slice, where
# every C++ subsystem logged a normal startup and only the QML engine failed. Link the plugin
# explicitly rather than relying on qt_add_executable's finalization to infer it.
target_link_libraries(wxlens-app PRIVATE wxlens-app-lib wxlens-app-libplugin)

# declarative_maplibre (docs/adr/0004-maplibre-qml-integration.md) is a Qt QML plugin - a
# MODULE_LIBRARY loaded by the QML engine at runtime, not something an app links against at
# compile time (target_link_libraries on it is a CMake error: "may not be linked into another
# target"). add_dependencies just orders the build so it exists before the POST_BUILD copy step
# below runs. QT_QML_IMPORT_PATH helps build-time QML tooling (qmllint, IDEs) find it; the copy
# step places it at <exe-dir>/qml/MapLibre so it resolves at runtime the same way Qt's own QML
# modules already do (QQmlEngine's default import path list includes "<app-dir>/qml" - confirmed
# during Phase 0, no qt.conf needed). WXLENS_MLN_QT_QML_PLUGIN_DIR (set in the read-only
# external/maplibre-native-qt.cmake), not $<TARGET_FILE_DIR:declarative_maplibre>, is the correct
# source - see that file's comment for why the generator-expression path is wrong on this
# multi-config generator.
add_dependencies(wxlens-app declarative_maplibre)

set_target_properties(wxlens-app-lib PROPERTIES
    QT_QML_IMPORT_PATH "${WXLENS_MLN_QT_QML_PLUGIN_DIR}/.."
)

set(WXLENS_QML_DEPLOY_DIR "$<TARGET_FILE_DIR:wxlens-app>/qml")
if (APPLE)
    set_target_properties(wxlens-app PROPERTIES
        OUTPUT_NAME WxLens
        MACOSX_BUNDLE_BUNDLE_NAME WxLens
        MACOSX_BUNDLE_GUI_IDENTIFIER org.wxlens.WxLens
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${WXLENS_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${WXLENS_VERSION}")
    set(WXLENS_QML_DEPLOY_DIR "$<TARGET_BUNDLE_DIR:wxlens-app>/Contents/Resources/qml")
endif()

# The plugin's qmldir/qmltypes (written by configure_file, unaffected by the output-dir bug
# above) live in WXLENS_MLN_QT_QML_PLUGIN_DIR, but its actual .dll doesn't - that got redirected
# by the same per-config-property bug to wherever CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE points
# (confirmed: Release/lib, not .../MapLibre). $<TARGET_FILE:...> always finds the real artifact
# regardless of which directory it landed in, so copy that explicitly alongside the qmldir/types.
add_custom_command(TARGET wxlens-app
                   POST_BUILD
                   COMMAND "${CMAKE_COMMAND}" -E copy_directory
                       "${WXLENS_MLN_QT_QML_PLUGIN_DIR}"
                       "${WXLENS_QML_DEPLOY_DIR}/MapLibre"
                   COMMAND "${CMAKE_COMMAND}" -E copy
                       "$<TARGET_FILE:declarative_maplibre>"
                       "${WXLENS_QML_DEPLOY_DIR}/MapLibre/"
                   COMMENT "Deploying MapLibre QML plugin next to wxlens-app...")

if (MSVC)
    target_compile_options(wxlens-app-lib PUBLIC "/MP")
    # wxdata.cmake sets this PRIVATE on wxdata itself (external/, read-only) so it doesn't
    # propagate to consumers - matches test/test.cmake's identical fix. Without it, an unguarded
    # <windows.h> in wxlens-app's own compilation (pulled in transitively, e.g. via
    # scwx/util/time.hpp -> date/tz.h) leaks min/max macros into external/units's templated
    # operator==, producing a cascade of unrelated-looking syntax errors in units/core.h -
    # confirmed while adding Phase 1 slice 2's radar_product_status.cpp, the first wxlens-app
    # source to pull in scwx::util::TimeString.
    target_compile_options(wxlens-app-lib PUBLIC -DNOMINMAX)
endif()

if (LINUX)
    # GCC's libstdc++ <execution> parallel-algorithm backend pulls in oneTBB's profiling.h, which
    # declares methods literally named emit() - Qt's emit macro (active once any Qt header has
    # been included earlier in the same translation unit, which is every wxlens-app .cpp) expands
    # that to nothing, breaking the declaration ("expected unqualified-id before ')' token").
    # test/test.cmake already carries this exact fix for wxlens-wxdata-test; it just never got
    # applied here or to wxlens-app-test. Safe unconditionally: wxlens-app has zero bare `emit`
    # calls (Q_EMIT throughout), so disabling the macro changes nothing it relies on.
    target_compile_definitions(wxlens-app-lib PUBLIC QT_NO_EMIT)
endif()

if (WIN32)
    # Qt DLLs (including the QtQuick.Window/QtQml QML plugins Main.qml needs) aren't found next
    # to the exe otherwise, since C:/Qt/.../bin isn't normally on PATH - confirmed by an actual
    # run during Phase 0 failing to load QML with "module QtQuick.Window is not installed" until
    # this ran. --qmldir scans our own QML sources so windeployqt deploys exactly the QML modules
    # actually imported, not a guess.
    add_custom_command(TARGET wxlens-app
                       POST_BUILD
                       COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                           --no-translations
                           --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                           $<TARGET_FILE:wxlens-app>
                       COMMENT "Running windeployqt for wxlens-app...")
endif()
