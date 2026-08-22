cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME nimbus-mln)

# See docs/adr/0004-maplibre-qml-integration.md: Nimbus uses the BSD-2-Clause "Quick" QML item
# (QMapLibre::Quick / QML type "MapLibre"), not the LGPL/GPL "Location" QtLocation plugin path
# and not the QWidgets path.
set(MLN_WITH_OPENGL ON)

# src/quick/plugins/CMakeLists.txt (the `import MapLibre` QML module registration target) uses
# CMAKE_SOURCE_DIR instead of CMAKE_CURRENT_SOURCE_DIR, which only resolves correctly when this
# library is the top-level CMake project - it breaks under add_subdirectory, which is how Nimbus
# (and every other vendored dependency here) consumes it. See
# docs/adr/0004-maplibre-qml-integration.md for the full diagnosis. Worked around by applying a
# tracked patch at configure time rather than hand-editing the vendored source, so external/ stays
# pristine in git; the patch is idempotent (checked via `git apply --check --reverse` first) so
# re-configuring is safe, but re-verify it still applies after every submodule bump.
find_package(Git REQUIRED)
set(MLN_QT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native-qt")
set(MLN_QT_CMAKE_SOURCE_DIR_PATCH
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/0004-mln-qt-plugins-cmake-source-dir.patch")
execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check --reverse "${MLN_QT_CMAKE_SOURCE_DIR_PATCH}"
    WORKING_DIRECTORY "${MLN_QT_SOURCE_DIR}"
    RESULT_VARIABLE MLN_QT_PATCH_ALREADY_APPLIED
    OUTPUT_QUIET ERROR_QUIET)
if (NOT MLN_QT_PATCH_ALREADY_APPLIED EQUAL 0)
    message(STATUS "Applying MapLibre Native Qt CMAKE_SOURCE_DIR patch (see ADR 0004)")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply "${MLN_QT_CMAKE_SOURCE_DIR_PATCH}"
        WORKING_DIRECTORY "${MLN_QT_SOURCE_DIR}"
        RESULT_VARIABLE MLN_QT_PATCH_RESULT)
    if (NOT MLN_QT_PATCH_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to apply MapLibre Native Qt CMake patch "
                             "(${MLN_QT_CMAKE_SOURCE_DIR_PATCH}) to ${MLN_QT_SOURCE_DIR} - "
                             "see docs/adr/0004-maplibre-qml-integration.md")
    endif()
else()
    message(STATUS "MapLibre Native Qt CMAKE_SOURCE_DIR patch already applied")
endif()

set(MLN_QT_WITH_QUICK_PLUGIN ON)
set(MLN_QT_WITH_LOCATION OFF)
set(MLN_QT_WITH_WIDGETS OFF)

add_subdirectory(maplibre-native-qt)

# src/quick/plugins/CMakeLists.txt sets LIBRARY_OUTPUT_DIRECTORY/RUNTIME_OUTPUT_DIRECTORY to a
# "MapLibre" subfolder (containing the plugin DLL, qmldir, and qmltypes together), but only as
# generic (non-per-config) properties - Nimbus's own tools/nimbus_config.cmake sets per-config
# CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE/etc. globally, and CMake applies those to every new
# target's per-config property *at creation time*, which then wins over a later generic-property
# override on a multi-config generator (confirmed: declarative_maplibre.dll actually lands in
# Release/lib, not .../MapLibre, despite that later set_target_properties call - $<TARGET_FILE_DIR>
# on the target reflects this and is NOT usable to locate the plugin's own qmldir/qmltypes).
# Capture the real, always-correct path directly instead.
set(NIMBUS_MLN_QT_QML_PLUGIN_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/maplibre-native-qt/src/quick/plugins/MapLibre"
    CACHE INTERNAL "Build output dir holding the MapLibre QML plugin's qmldir/dll/qmltypes")

set_target_properties(MLNQtCore PROPERTIES FOLDER mln)
set_target_properties(mbgl-core PROPERTIES FOLDER mln)

if (MSVC)
    target_compile_options(mbgl-core PRIVATE "/MP")
    target_compile_options(MLNQtCore PRIVATE "/MP")
endif()
