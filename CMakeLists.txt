cmake_minimum_required(VERSION 3.24)
set(PROJECT_NAME nimbus)

include(tools/nimbus_config.cmake)

project(${PROJECT_NAME}
        VERSION      0.1.0
        DESCRIPTION  "Nimbus is a free, open source, modern weather radar viewer."
        HOMEPAGE_URL "https://github.com/Screen4639/nimbus"
        LANGUAGES    C CXX)

set(CMAKE_POLICY_DEFAULT_CMP0054 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0079 NEW)

nimbus_output_dirs_setup()

enable_testing()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set_property(DIRECTORY
             APPEND
             PROPERTY CMAKE_CONFIGURE_DEPENDS
             conanfile.py)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ALL_NO_LIB")

set(NIMBUS_DIR ${PROJECT_SOURCE_DIR})
set(NIMBUS_VERSION "0.1.0")

# wxdata reuse: docs/adr/0002-wxdata-reuse-strategy.md (Option A) - link the legacy Supercell Wx
# repo's Qt-free wxdata library unmodified. Only wxdata is added; the rest of the legacy repo
# (scwx-qt, its own external/) is not built.
add_subdirectory(external)
add_subdirectory(external/legacy-supercell-wx/wxdata ${CMAKE_BINARY_DIR}/wxdata)
add_subdirectory(app)
add_subdirectory(test)

set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER CMakePredefinedTargets)
