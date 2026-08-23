cmake_minimum_required(VERSION 3.24)
project(nimbus-test CXX)

include(GoogleTest)

# Boost::timer/Boost::json: see app/CMakeLists.txt's comment - wxdata needs them but doesn't
# declare them itself, and find_package(Boost) doesn't create globally-visible imported targets.
find_package(Boost REQUIRED COMPONENTS timer json)
find_package(BZip2)
find_package(GTest)

# The wxdata-only slice of the legacy repo's GTest suite (docs/adr/0002), referenced directly from
# external/legacy-supercell-wx so it stays the single source of truth - not copied into Nimbus.
# Excludes every source/scwx/qt/* group, since those test scwx-qt, which Nimbus does not link.
set(LEGACY_TEST_DIR ${NIMBUS_DIR}/external/legacy-supercell-wx/test/source/scwx)

set(SRC_MAIN source/nimbus/wxdata_test_main.cpp)
set(SRC_AWIPS_TESTS ${LEGACY_TEST_DIR}/awips/coded_location.test.cpp
                    ${LEGACY_TEST_DIR}/awips/coded_time_motion_location.test.cpp
                    ${LEGACY_TEST_DIR}/awips/pvtec.test.cpp
                    ${LEGACY_TEST_DIR}/awips/text_product_file.test.cpp
                    ${LEGACY_TEST_DIR}/awips/ugc.test.cpp
                    ${LEGACY_TEST_DIR}/awips/wmo_header.test.cpp)
set(SRC_COMMON_TESTS ${LEGACY_TEST_DIR}/common/color_table.test.cpp
                     ${LEGACY_TEST_DIR}/common/products.test.cpp
                     ${LEGACY_TEST_DIR}/common/sites.test.cpp)
set(SRC_CONFIG_TESTS ${LEGACY_TEST_DIR}/config/ondas_config.test.cpp
                     ${LEGACY_TEST_DIR}/config/ondas_config_loader.test.cpp)
set(SRC_GR_TESTS ${LEGACY_TEST_DIR}/gr/placefile.test.cpp)
set(SRC_NETWORK_TESTS ${LEGACY_TEST_DIR}/network/dir_list.test.cpp
                      ${LEGACY_TEST_DIR}/network/ntp_client.test.cpp)
set(SRC_PROVIDER_TESTS ${LEGACY_TEST_DIR}/provider/aws_level2_data_provider.test.cpp
                       ${LEGACY_TEST_DIR}/provider/aws_level3_data_provider.test.cpp
                       ${LEGACY_TEST_DIR}/provider/http_level3_data_provider.test.cpp
                       ${LEGACY_TEST_DIR}/provider/iem_api_provider.test.cpp
                       ${LEGACY_TEST_DIR}/provider/nws_level3_behavior.test.cpp
                       ${LEGACY_TEST_DIR}/provider/nws_api_provider.test.cpp
                       ${LEGACY_TEST_DIR}/provider/ondas_level2_data_provider.test.cpp
                       ${LEGACY_TEST_DIR}/provider/ondas_level3_behavior.test.cpp
                       ${LEGACY_TEST_DIR}/provider/warnings_provider.test.cpp)
set(SRC_TYPES_TESTS ${LEGACY_TEST_DIR}/types/ondas_types.test.cpp)
set(SRC_UTIL_TESTS ${LEGACY_TEST_DIR}/util/float.test.cpp
                   ${LEGACY_TEST_DIR}/util/rangebuf.test.cpp
                   ${LEGACY_TEST_DIR}/util/streams.test.cpp
                   ${LEGACY_TEST_DIR}/util/strings.test.cpp
                   ${LEGACY_TEST_DIR}/util/vectorbuf.test.cpp)
set(SRC_WSR88D_TESTS ${LEGACY_TEST_DIR}/wsr88d/ar2v_file.test.cpp
                     ${LEGACY_TEST_DIR}/wsr88d/level3_file.test.cpp
                     ${LEGACY_TEST_DIR}/wsr88d/nexrad_file_factory.test.cpp)

set(CMAKE_FILES test.cmake)

add_executable(nimbus-wxdata-test ${SRC_MAIN}
                                  ${SRC_AWIPS_TESTS}
                                  ${SRC_COMMON_TESTS}
                                  ${SRC_CONFIG_TESTS}
                                  ${SRC_GR_TESTS}
                                  ${SRC_NETWORK_TESTS}
                                  ${SRC_PROVIDER_TESTS}
                                  ${SRC_TYPES_TESTS}
                                  ${SRC_UTIL_TESTS}
                                  ${SRC_WSR88D_TESTS}
                                  ${CMAKE_FILES})

target_include_directories(nimbus-wxdata-test PRIVATE ${GTest_INCLUDE_DIRS})

set_target_properties(nimbus-wxdata-test PROPERTIES CXX_STANDARD 20
                                                     CXX_STANDARD_REQUIRED ON
                                                     CXX_EXTENSIONS OFF)

if (MSVC)
    set_target_properties(nimbus-wxdata-test PROPERTIES LINK_FLAGS "/ignore:4099")
    target_compile_options(nimbus-wxdata-test PRIVATE -DNOMINMAX)
    target_compile_options(nimbus-wxdata-test PRIVATE "/MP")
endif()

if (LINUX)
    target_compile_definitions(nimbus-wxdata-test PRIVATE QT_NO_EMIT)
endif()

# Fixture data lives in the legacy repo's test/data submodule (docs/adr/0002) - not duplicated.
target_compile_definitions(nimbus-wxdata-test PRIVATE
    SCWX_TEST_DATA_DIR="${NIMBUS_DIR}/external/legacy-supercell-wx/test/data")

target_link_libraries(nimbus-wxdata-test GTest::gtest
                                         wxdata
                                         Boost::timer
                                         Boost::json)

gtest_discover_tests(nimbus-wxdata-test)

# ---------------------------------------------------------------------------------------------
# nimbus-app-test: Nimbus's own C++ model classes, tested independently of QML (docs/ROADMAP.md).
# Separate target from nimbus-wxdata-test because these need Qt, while the wxdata suite is
# deliberately Qt-free.
#
# The app's sources are compiled into this target rather than linked, since nimbus-app is an
# executable. If that source list grows much further, the app should be split into a static
# library plus a thin main() and both targets should link that instead.
find_package(Qt6 REQUIRED COMPONENTS Core Quick OpenGL Qml)
find_package(GeographicLib REQUIRED)
find_package(glm REQUIRED)

set(NIMBUS_APP_SRC ${NIMBUS_DIR}/app/source/nimbus)

add_executable(nimbus-app-test
    source/nimbus/app_test_main.cpp
    source/nimbus/panes/pane_sync.test.cpp

    ${NIMBUS_APP_SRC}/data/radar_site_data_service.cpp
    ${NIMBUS_APP_SRC}/data/radar_site_database.cpp
    ${NIMBUS_APP_SRC}/log/logger.cpp
    ${NIMBUS_APP_SRC}/panes/pane_controller.cpp
    ${NIMBUS_APP_SRC}/panes/pane_grid_model.cpp
    ${NIMBUS_APP_SRC}/panes/sync_types.hpp
    ${NIMBUS_APP_SRC}/products/radar_sweep_product.cpp
    ${NIMBUS_APP_SRC}/render/radar_sweep_layer.cpp
    ${NIMBUS_APP_SRC}/util/geodesic.cpp
    ${CMAKE_FILES})

set_target_properties(nimbus-app-test PROPERTIES CXX_STANDARD 20
                                                 CXX_STANDARD_REQUIRED ON
                                                 CXX_EXTENSIONS OFF
                                                 AUTOMOC ON)

target_include_directories(nimbus-app-test PRIVATE ${GTest_INCLUDE_DIRS}
                                                   ${NIMBUS_DIR}/app/source)

if (MSVC)
    set_target_properties(nimbus-app-test PROPERTIES LINK_FLAGS "/ignore:4099")
    # Same NOMINMAX requirement as nimbus-app - see app/CMakeLists.txt for the failure mode.
    target_compile_options(nimbus-app-test PRIVATE -DNOMINMAX)
    target_compile_options(nimbus-app-test PRIVATE "/MP")
endif()

target_link_libraries(nimbus-app-test GTest::gtest
                                      Qt6::Core
                                      Qt6::Qml
                                      Qt6::Quick
                                      Qt6::OpenGL
                                      wxdata
                                      Boost::timer
                                      Boost::json
                                      GeographicLib::GeographicLib
                                      glm::glm-header-only
                                      QMapLibre::Core)

gtest_discover_tests(nimbus-app-test)
