cmake_minimum_required(VERSION 3.24)

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             aws-sdk-cpp.cmake
             date.cmake
             hsluv-c.cmake
             maplibre-native-qt.cmake
             units.cmake)

# wxdata (linked below in the top-level CMakeLists.txt) needs these four unmodified.
include(aws-sdk-cpp.cmake)
include(date.cmake)
include(hsluv-c.cmake)
include(units.cmake)

# Nimbus's own map rendering dependency - see docs/adr/0004-maplibre-qml-integration.md.
include(maplibre-native-qt.cmake)
