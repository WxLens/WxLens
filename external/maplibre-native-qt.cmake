cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME wxlens-mln)

# See docs/adr/0004-maplibre-qml-integration.md: WxLens uses the BSD-2-Clause "Quick" QML item
# (QMapLibre::Quick / QML type "MapLibre"), not the LGPL/GPL "Location" QtLocation plugin path
# and not the QWidgets path.
set(MLN_WITH_OPENGL ON)

# Vendored MapLibre Native Qt needs small fixes/additions WxLens can't make upstream directly.
# Rather than hand-editing the vendored source (external/ must stay pristine in git - see
# docs/adr/0004-maplibre-qml-integration.md), each fix is a tracked patch under external/patches/,
# applied idempotently at configure time (git apply --check --reverse tests whether it's already
# applied first, so re-configuring is safe) - re-verify each still applies after a submodule bump.
find_package(Git REQUIRED)
set(MLN_QT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/maplibre-native-qt")

set(MLN_QT_PATCHES
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/0004-mln-qt-plugins-cmake-source-dir.patch"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/0005-mln-qt-expose-map-object.patch"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/0006-mln-qt-no-clear-in-renderable-bind.patch"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/0007-mln-qt-connect-map-signals-before-style-load.patch"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/0008-mln-qt-reload-style-from-qml.patch")

# The final patch is the completion marker for this ordered series. Later fixes intentionally
# touch lines introduced by earlier patches, so testing patch 0005's reverse in isolation stops
# being valid after 0007 is present. A clean checkout cannot contain 0008 without this driver
# having successfully applied 0004-0007 first.
list(GET MLN_QT_PATCHES -1 MLN_QT_FINAL_PATCH)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check --reverse "${MLN_QT_FINAL_PATCH}"
    WORKING_DIRECTORY "${MLN_QT_SOURCE_DIR}"
    RESULT_VARIABLE mlnPatchesAlreadyApplied
    OUTPUT_QUIET ERROR_QUIET)
if (mlnPatchesAlreadyApplied EQUAL 0)
    message(STATUS "MapLibre Native Qt patch series already applied (ADR 0004)")
else()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --check ${MLN_QT_PATCHES}
        WORKING_DIRECTORY "${MLN_QT_SOURCE_DIR}"
        RESULT_VARIABLE mlnPatchesApplicable
        OUTPUT_QUIET ERROR_QUIET)
    if (NOT mlnPatchesApplicable EQUAL 0)
        message(FATAL_ERROR "MapLibre Native Qt patch series is neither cleanly applied nor "
                            "applicable to ${MLN_QT_SOURCE_DIR} - see ADR 0004")
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply ${MLN_QT_PATCHES}
        WORKING_DIRECTORY "${MLN_QT_SOURCE_DIR}"
        RESULT_VARIABLE mlnPatchResult)
    if (NOT mlnPatchResult EQUAL 0)
        message(FATAL_ERROR "Failed to apply MapLibre Native Qt patch series - see ADR 0004")
    endif()
endif()

# `import MapLibre` QML module registration target uses CMAKE_SOURCE_DIR instead of
# CMAKE_CURRENT_SOURCE_DIR, which only resolves correctly when this library is the top-level
# CMake project - breaks under add_subdirectory, which is how WxLens (and every other vendored
# dependency here) consumes it. See ADR 0004's "upstream CMake bug" note for the full diagnosis.

# MapQuickItem has no public way to reach the underlying core Map, which WxLens's radar renderer
# (Phase 1 slice 3) needs to register itself via Map::addCustomLayer. Adds mapLibreMap() and a
# mapReady() signal - see ADR 0004's slice 3 resolution note and the patch file itself.

# The Qt OpenGL backend's renderable bind() unconditionally cleared the framebuffer to opaque
# black. bind() is also invoked MID-FRAME by mbgl's DrawableCustomLayerHostTweaker after every
# style CustomLayer renders, so with any custom layer present the entire already-drawn map got
# wiped each frame (observed: whole map black once WxLens registered even a no-op custom layer;
# base map fine without it). mbgl's own gl::RenderPass constructor performs the proper pass-start
# clear right after bind() anyway, so the removed clear was redundant there and only ever
# destructive. See ADR 0004's slice 3 resolution note.

# MapQuickItem connects the core Map's needsRendering/mapChanged signals in updatePaintNode(), i.e.
# when the scene-graph node is first created - but the Map is created and its style load started
# earlier, in MapQuickItemPrivate::initialize(). A style that resolves fast enough (notably a
# second map reusing the first one's cached style) finishes loading in that window, so
# MapChangeDidFinishLoadingStyle is emitted with nothing connected and is lost outright:
# m_styleLoaded stays false, syncStyleChanges() never runs, and styleLoaded() never fires. Found in
# Phase 1 slice 4 - growing the pane grid from 1x1 to 2x2 left every pane after the first without
# its radar layer forever. Moves both connections to immediately after the Map is constructed.

# MapQuickItem::setStyle() only updates the string used during initialize(); changing the QML
# `style` property after the map exists therefore does nothing. Theme-driven basemap changes need
# the setter to forward the new URL to the live core Map, whose normal mapChanged/styleLoaded
# signals then rebuild WxLens's custom layers. Found during the live-review follow-up to slice 10.

set(MLN_QT_WITH_QUICK_PLUGIN ON)
set(MLN_QT_WITH_LOCATION OFF)
set(MLN_QT_WITH_WIDGETS OFF)

add_subdirectory(maplibre-native-qt)

# src/quick/plugins/CMakeLists.txt sets LIBRARY_OUTPUT_DIRECTORY/RUNTIME_OUTPUT_DIRECTORY to a
# "MapLibre" subfolder (containing the plugin DLL, qmldir, and qmltypes together), but only as
# generic (non-per-config) properties - WxLens's own tools/wxlens_config.cmake sets per-config
# CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE/etc. globally, and CMake applies those to every new
# target's per-config property *at creation time*, which then wins over a later generic-property
# override on a multi-config generator (confirmed: declarative_maplibre.dll actually lands in
# Release/lib, not .../MapLibre, despite that later set_target_properties call - $<TARGET_FILE_DIR>
# on the target reflects this and is NOT usable to locate the plugin's own qmldir/qmltypes).
# Capture the real, always-correct path directly instead.
set(WXLENS_MLN_QT_QML_PLUGIN_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/maplibre-native-qt/src/quick/plugins/MapLibre"
    CACHE INTERNAL "Build output dir holding the MapLibre QML plugin's qmldir/dll/qmltypes")

set_target_properties(MLNQtCore PROPERTIES FOLDER mln)
set_target_properties(mbgl-core PROPERTIES FOLDER mln)

if (MSVC)
    target_compile_options(mbgl-core PRIVATE "/MP")
    target_compile_options(MLNQtCore PRIVATE "/MP")
endif()
