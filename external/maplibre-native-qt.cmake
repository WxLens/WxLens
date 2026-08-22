cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME nimbus-mln)

# See docs/adr/0004-maplibre-qml-integration.md: Nimbus uses the BSD-2-Clause "Quick" QML item
# (QMapLibre::Quick / QML type "MapLibre"), not the LGPL/GPL "Location" QtLocation plugin path
# and not the QWidgets path.
set(MLN_WITH_OPENGL ON)
# QML plugin target temporarily OFF: its CMakeLists.txt (src/quick/plugins/CMakeLists.txt) uses
# CMAKE_SOURCE_DIR instead of PROJECT_SOURCE_DIR/CMAKE_CURRENT_SOURCE_DIR, which only resolves
# correctly when this library is the top-level CMake project - it breaks under add_subdirectory
# (see docs/adr/0004-maplibre-qml-integration.md's "upstream CMake bug" note). MLNQtCore and
# MLNQtQuickPrivate (the actual QQuickItem C++ class) still build fine; only the `import MapLibre`
# QML module registration is affected. Re-enable once that's worked around.
set(MLN_QT_WITH_QUICK_PLUGIN OFF)
set(MLN_QT_WITH_LOCATION OFF)
set(MLN_QT_WITH_WIDGETS OFF)

add_subdirectory(maplibre-native-qt)

set_target_properties(MLNQtCore PROPERTIES FOLDER mln)
set_target_properties(mbgl-core PROPERTIES FOLDER mln)

if (MSVC)
    target_compile_options(mbgl-core PRIVATE "/MP")
    target_compile_options(MLNQtCore PRIVATE "/MP")
endif()
