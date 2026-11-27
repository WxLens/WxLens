cmake_minimum_required(VERSION 3.24)

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             aws-sdk-cpp.cmake
             date.cmake
             hsluv-c.cmake
             legacy-supercell-wx.cmake
             maplibre-native-qt.cmake
             patch-series.cmake
             units.cmake)

# wxlens_apply_patch_series(), used by the wxdata and MapLibre patch steps below.
include(patch-series.cmake)

# Tracked wxdata patches, applied before the top-level CMakeLists.txt adds wxdata (ADR 0002).
include(legacy-supercell-wx.cmake)

# wxdata (linked below in the top-level CMakeLists.txt) needs these four unmodified.
include(aws-sdk-cpp.cmake)
include(date.cmake)
include(hsluv-c.cmake)
include(units.cmake)

# WxLens's own map rendering dependency - see docs/adr/0004-maplibre-qml-integration.md.
include(maplibre-native-qt.cmake)
