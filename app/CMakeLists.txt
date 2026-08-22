cmake_minimum_required(VERSION 3.24)
project(nimbus-app CXX)

find_package(Qt6 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.5)

# Boost::timer/Boost::json are used by wxdata (boost::timer::cpu_timer in
# aws_level2_chunks_data_provider.cpp, boost::json in util/json.hpp) but wxdata.cmake itself
# (external/, read-only) doesn't declare them, and wxdata is an OBJECT library so this only
# surfaces at final link time. find_package(Boost) also doesn't create globally-visible imported
# targets, so it must be called again here even though wxdata.cmake already called it.
find_package(Boost REQUIRED COMPONENTS timer json)

qt_add_executable(nimbus-app
    source/nimbus/main/main.cpp
    source/nimbus/log/logger.cpp
    source/nimbus/log/logger.hpp
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
        qml/Panes/PaneHost.qml
    RESOURCES
        res/branding/nimbus-icon.png
)

set_target_properties(nimbus-app PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

target_link_libraries(nimbus-app PRIVATE
    Qt6::Quick
    wxdata
    Boost::timer
    Boost::json
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
