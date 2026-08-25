cmake_minimum_required(VERSION 3.24)
project(nimbus-app CXX)

find_package(Qt6 REQUIRED COMPONENTS Quick OpenGL)
qt_standard_project_setup(REQUIRES 6.5)

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

qt_add_executable(nimbus-app
    source/nimbus/main/main.cpp
    source/nimbus/log/logger.cpp
    source/nimbus/log/logger.hpp
    source/nimbus/data/radar_site_data_service.cpp
    source/nimbus/data/radar_site_data_service.hpp
    source/nimbus/data/radar_site_database.cpp
    source/nimbus/data/radar_site_database.hpp
    source/nimbus/objects/map_object.hpp
    source/nimbus/objects/map_object_store.cpp
    source/nimbus/objects/map_object_store.hpp
    source/nimbus/objects/measurement_controller.cpp
    source/nimbus/objects/measurement_controller.hpp
    source/nimbus/objects/object_tool_controller.cpp
    source/nimbus/objects/object_tool_controller.hpp
    source/nimbus/panes/pane_controller.cpp
    source/nimbus/panes/pane_controller.hpp
    source/nimbus/panes/pane_grid_model.cpp
    source/nimbus/panes/pane_grid_model.hpp
    source/nimbus/panes/sync_types.hpp
    source/nimbus/palettes/palette_manager.cpp
    source/nimbus/palettes/palette_manager.hpp
    source/nimbus/palettes/palette_model.cpp
    source/nimbus/palettes/palette_model.hpp
    source/nimbus/products/product_descriptor.hpp
    source/nimbus/products/radar_product_status.cpp
    source/nimbus/products/radar_product_status.hpp
    source/nimbus/products/radar_sweep_product.cpp
    source/nimbus/products/radar_sweep_product.hpp
    source/nimbus/render/radar_sweep_layer.cpp
    source/nimbus/render/radar_sweep_layer.hpp
    source/nimbus/settings/app_settings.cpp
    source/nimbus/settings/app_settings.hpp
    source/nimbus/settings/settings_store.cpp
    source/nimbus/settings/settings_store.hpp
    source/nimbus/util/crash_handler.cpp
    source/nimbus/util/crash_handler.hpp
    source/nimbus/util/geodesic.cpp
    source/nimbus/util/geodesic.hpp
    source/nimbus/util/radar_geometry.cpp
    source/nimbus/util/radar_geometry.hpp
    source/nimbus/util/unit_format.cpp
    source/nimbus/util/unit_format.hpp
    $<$<PLATFORM_ID:Windows>:res/branding/app.rc>
)

target_include_directories(nimbus-app PRIVATE source)

qt_add_qml_module(nimbus-app
    URI Nimbus.App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Chrome/TopBar.qml
        qml/Chrome/SideRail.qml
        qml/Dialogs/SettingsChoice.qml
        qml/Dialogs/SettingsDialog.qml
        qml/Dialogs/PaletteDialog.qml
        qml/Controls/ColorPicker.qml
        qml/Panes/MapObjectsLayer.qml
        qml/Panes/MeasurementLayer.qml
        qml/Panes/RadarGeometryPanel.qml
        qml/Panes/PaneGrid.qml
        qml/Panes/PaneHost.qml
    RESOURCES
        res/branding/nimbus-icon.png
        res/config/radar_sites.json
        res/gl/radar.vert
        res/gl/radar.frag
)

# NOAA Weather and Climate Toolkit defaults, carried forward unmodified with the attribution in
# ACKNOWLEDGEMENTS.md. Referencing the vendored copies as resource inputs keeps external/ pristine
# and prevents Nimbus's copies drifting from the reused legacy dependency.
set(NIMBUS_WCT_PALETTE_DIR
    "${CMAKE_SOURCE_DIR}/external/legacy-supercell-wx/scwx-qt/res/palettes/wct")
qt_add_resources(nimbus-app "nimbus-wct-palettes"
    PREFIX "/qt/qml/Nimbus/App/res/palettes/wct"
    BASE "${NIMBUS_WCT_PALETTE_DIR}"
    FILES
        "${NIMBUS_WCT_PALETTE_DIR}/CC.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/Default16.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/DOD_DSD.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/DR.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/DV.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/ET.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/HC.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/KDP.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/KDP2.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/OHP.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/SRV.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/STP.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/SW.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/VIL.pal"
        "${NIMBUS_WCT_PALETTE_DIR}/ZDR.pal")

set_target_properties(nimbus-app PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

target_link_libraries(nimbus-app PRIVATE
    Qt6::Quick
    Qt6::OpenGL
    wxdata
    Boost::timer
    Boost::json
    GeographicLib::GeographicLib
    glm::glm-header-only
    tomlplusplus::tomlplusplus
    QMapLibre::Core
    # DbgHelp backs nimbus::util::InstallCrashHandler's symbolized stack walk. It ships with
    # Windows itself, so this adds a link dependency, not a redistributable.
    $<$<PLATFORM_ID:Windows>:dbghelp>
)

# declarative_maplibre (docs/adr/0004-maplibre-qml-integration.md) is a Qt QML plugin - a
# MODULE_LIBRARY loaded by the QML engine at runtime, not something an app links against at
# compile time (target_link_libraries on it is a CMake error: "may not be linked into another
# target"). add_dependencies just orders the build so it exists before the POST_BUILD copy step
# below runs. QT_QML_IMPORT_PATH helps build-time QML tooling (qmllint, IDEs) find it; the copy
# step places it at <exe-dir>/qml/MapLibre so it resolves at runtime the same way Qt's own QML
# modules already do (QQmlEngine's default import path list includes "<app-dir>/qml" - confirmed
# during Phase 0, no qt.conf needed). NIMBUS_MLN_QT_QML_PLUGIN_DIR (set in
# external/maplibre-native-qt.cmake), not $<TARGET_FILE_DIR:declarative_maplibre>, is the correct
# source - see that file's comment for why the generator-expression path is wrong on this
# multi-config generator.
add_dependencies(nimbus-app declarative_maplibre)

set_target_properties(nimbus-app PROPERTIES
    QT_QML_IMPORT_PATH "${NIMBUS_MLN_QT_QML_PLUGIN_DIR}/.."
)

# The plugin's qmldir/qmltypes (written by configure_file, unaffected by the output-dir bug
# above) live in NIMBUS_MLN_QT_QML_PLUGIN_DIR, but its actual .dll doesn't - that got redirected
# by the same per-config-property bug to wherever CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE points
# (confirmed: Release/lib, not .../MapLibre). $<TARGET_FILE:...> always finds the real artifact
# regardless of which directory it landed in, so copy that explicitly alongside the qmldir/types.
add_custom_command(TARGET nimbus-app
                   POST_BUILD
                   COMMAND "${CMAKE_COMMAND}" -E copy_directory
                       "${NIMBUS_MLN_QT_QML_PLUGIN_DIR}"
                       "$<TARGET_FILE_DIR:nimbus-app>/qml/MapLibre"
                   COMMAND "${CMAKE_COMMAND}" -E copy
                       "$<TARGET_FILE:declarative_maplibre>"
                       "$<TARGET_FILE_DIR:nimbus-app>/qml/MapLibre/"
                   COMMENT "Deploying MapLibre QML plugin next to nimbus-app...")

if (MSVC)
    target_compile_options(nimbus-app PRIVATE "/MP")
    # wxdata.cmake sets this PRIVATE on wxdata itself (external/, read-only) so it doesn't
    # propagate to consumers - matches test/test.cmake's identical fix. Without it, an unguarded
    # <windows.h> in nimbus-app's own compilation (pulled in transitively, e.g. via
    # scwx/util/time.hpp -> date/tz.h) leaks min/max macros into external/units's templated
    # operator==, producing a cascade of unrelated-looking syntax errors in units/core.h -
    # confirmed while adding Phase 1 slice 2's radar_product_status.cpp, the first nimbus-app
    # source to pull in scwx::util::TimeString.
    target_compile_options(nimbus-app PRIVATE -DNOMINMAX)
endif()

if (WIN32)
    # Qt DLLs (including the QtQuick.Window/QtQml QML plugins Main.qml needs) aren't found next
    # to the exe otherwise, since C:/Qt/.../bin isn't normally on PATH - confirmed by an actual
    # run during Phase 0 failing to load QML with "module QtQuick.Window is not installed" until
    # this ran. --qmldir scans our own QML sources so windeployqt deploys exactly the QML modules
    # actually imported, not a guess.
    add_custom_command(TARGET nimbus-app
                       POST_BUILD
                       COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                           --no-translations
                           --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                           $<TARGET_FILE:nimbus-app>
                       COMMENT "Running windeployqt for nimbus-app...")
endif()
