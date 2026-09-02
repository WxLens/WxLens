cmake_minimum_required(VERSION 3.24)
project(wxlens-test CXX)

include(GoogleTest)

# Boost::timer/Boost::json: see app/CMakeLists.txt's comment - wxdata needs them but doesn't
# declare them itself, and find_package(Boost) doesn't create globally-visible imported targets.
find_package(Boost REQUIRED COMPONENTS timer json)
find_package(BZip2)
find_package(GTest)

# The wxdata-only slice of the legacy repo's GTest suite (docs/adr/0002), referenced directly from
# external/legacy-supercell-wx so it stays the single source of truth - not copied into WxLens.
# Excludes every source/scwx/qt/* group, since those test scwx-qt, which WxLens does not link.
set(LEGACY_TEST_DIR ${WXLENS_DIR}/external/legacy-supercell-wx/test/source/scwx)

set(SRC_MAIN source/wxlens/wxdata_test_main.cpp)
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

add_executable(wxlens-wxdata-test ${SRC_MAIN}
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

target_include_directories(wxlens-wxdata-test PRIVATE ${GTest_INCLUDE_DIRS})

set_target_properties(wxlens-wxdata-test PROPERTIES CXX_STANDARD 20
                                                     CXX_STANDARD_REQUIRED ON
                                                     CXX_EXTENSIONS OFF)

if (MSVC)
    set_target_properties(wxlens-wxdata-test PROPERTIES LINK_FLAGS "/ignore:4099")
    target_compile_options(wxlens-wxdata-test PRIVATE -DNOMINMAX)
    target_compile_options(wxlens-wxdata-test PRIVATE "/MP")
endif()

if (LINUX)
    target_compile_definitions(wxlens-wxdata-test PRIVATE QT_NO_EMIT)
endif()

# Fixture data lives in the legacy repo's test/data submodule (docs/adr/0002) - not duplicated.
target_compile_definitions(wxlens-wxdata-test PRIVATE
    SCWX_TEST_DATA_DIR="${WXLENS_DIR}/external/legacy-supercell-wx/test/data")

target_link_libraries(wxlens-wxdata-test GTest::gtest
                                         wxdata
                                         Boost::timer
                                         Boost::json)

gtest_discover_tests(wxlens-wxdata-test)

# ---------------------------------------------------------------------------------------------
# wxlens-app-test: WxLens's own C++ model classes, tested independently of QML (docs/ROADMAP.md).
# Separate target from wxlens-wxdata-test because these need Qt, while the wxdata suite is
# deliberately Qt-free.
#
# The app's sources are compiled into this target rather than linked, since wxlens-app is an
# executable. If that source list grows much further, the app should be split into a static
# library plus a thin main() and both targets should link that instead.
find_package(Qt6 REQUIRED COMPONENTS Core Quick OpenGL Qml Network)
find_package(GeographicLib REQUIRED)
find_package(glm REQUIRED)
find_package(tomlplusplus REQUIRED)

set(WXLENS_APP_SRC ${WXLENS_DIR}/app/source/wxlens)

add_executable(wxlens-app-test
    source/wxlens/app_test_main.cpp
    source/wxlens/data/radar_site_database.test.cpp
    source/wxlens/objects/map_object_scope.test.cpp
    source/wxlens/objects/measurement.test.cpp
    source/wxlens/objects/saved_place_manager.test.cpp
    source/wxlens/overlays/overlay_manager.test.cpp
    source/wxlens/palettes/palette_model.test.cpp
    source/wxlens/palettes/palette_manager.test.cpp
    source/wxlens/products/level3_product_catalog.test.cpp
    source/wxlens/products/level3_radial_product.test.cpp
    source/wxlens/products/level3_raster_product.test.cpp
    source/wxlens/products/level3_graphic_overlay.test.cpp
    source/wxlens/products/level3_text_product.test.cpp
    source/wxlens/panes/pane_sync.test.cpp
    source/wxlens/panes/source_probe.test.cpp
    source/wxlens/settings/settings_store.test.cpp
    source/wxlens/settings/app_settings.test.cpp
    source/wxlens/theme/theme_manager.test.cpp
    source/wxlens/util/radar_geometry.test.cpp

    ${WXLENS_APP_SRC}/data/radar_site_data_service.cpp
    ${WXLENS_APP_SRC}/data/radar_site_database.cpp
    ${WXLENS_APP_SRC}/log/logger.cpp
    ${WXLENS_APP_SRC}/objects/map_object.hpp
    ${WXLENS_APP_SRC}/objects/map_object_store.cpp
    ${WXLENS_APP_SRC}/objects/measurement_controller.cpp
    ${WXLENS_APP_SRC}/objects/object_tool_controller.cpp
    ${WXLENS_APP_SRC}/objects/snap_target_registry.cpp
    ${WXLENS_APP_SRC}/objects/saved_place_manager.cpp
    ${WXLENS_APP_SRC}/overlays/overlay_manager.cpp
    ${WXLENS_APP_SRC}/panes/pane_controller.cpp
    ${WXLENS_APP_SRC}/panes/pane_grid_model.cpp
    ${WXLENS_APP_SRC}/panes/sync_types.hpp
    ${WXLENS_APP_SRC}/palettes/palette_manager.cpp
    ${WXLENS_APP_SRC}/palettes/palette_model.cpp
    ${WXLENS_APP_SRC}/products/radar_sweep_product.cpp
    ${WXLENS_APP_SRC}/products/level3_product_catalog.cpp
    ${WXLENS_APP_SRC}/products/level3_radial_product.cpp
    ${WXLENS_APP_SRC}/products/level3_raster_product.cpp
    ${WXLENS_APP_SRC}/products/level3_graphic_overlay.cpp
    ${WXLENS_APP_SRC}/products/level3_text_product.cpp
    ${WXLENS_APP_SRC}/render/radar_sweep_layer.cpp
    ${WXLENS_APP_SRC}/settings/app_settings.cpp
    ${WXLENS_APP_SRC}/settings/settings_store.cpp
    ${WXLENS_APP_SRC}/theme/theme_manager.cpp
    ${WXLENS_APP_SRC}/util/geodesic.cpp
    ${WXLENS_APP_SRC}/util/radar_geometry.cpp
    ${WXLENS_APP_SRC}/util/unit_format.cpp
    ${CMAKE_FILES})

# radar_sites.json reaches the app through qt_add_qml_module's resources, which wxlens-app-test
# does not get - and without it data::FindRadarSite returns nullopt for every site, so the whole
# site-metadata path (including the feet -> metres conversion §4.7's beam geometry depends on)
# would be untestable. Same resource path the app uses, so radar_site_database.cpp needs no
# test-only branch.
qt_add_resources(wxlens-app-test "wxlens-app-test-config"
    PREFIX "/qt/qml/WxLens/App/res/config"
    BASE "${WXLENS_DIR}/app/res/config"
    FILES "${WXLENS_DIR}/app/res/config/radar_sites.json")

qt_add_resources(wxlens-app-test "wxlens-app-test-palette"
    PREFIX "/qt/qml/WxLens/App/res/palettes/wct"
    BASE "${WXLENS_DIR}/external/legacy-supercell-wx/scwx-qt/res/palettes/wct"
    FILES "${WXLENS_DIR}/external/legacy-supercell-wx/scwx-qt/res/palettes/wct/DR.pal"
          "${WXLENS_DIR}/external/legacy-supercell-wx/scwx-qt/res/palettes/wct/DV.pal"
          "${WXLENS_DIR}/external/legacy-supercell-wx/scwx-qt/res/palettes/wct/SRV.pal")

qt_add_resources(wxlens-app-test "wxlens-app-test-themes"
    PREFIX "/qt/qml/WxLens/App/res/themes"
    BASE "${WXLENS_DIR}/app/res/themes"
    FILES "${WXLENS_DIR}/app/res/themes/operational-dark.toml"
          "${WXLENS_DIR}/app/res/themes/daylight.toml")

set_target_properties(wxlens-app-test PROPERTIES CXX_STANDARD 20
                                                 CXX_STANDARD_REQUIRED ON
                                                 CXX_EXTENSIONS OFF
                                                 AUTOMOC ON)

target_include_directories(wxlens-app-test PRIVATE ${GTest_INCLUDE_DIRS}
                                                   ${WXLENS_DIR}/app/source)
target_compile_definitions(wxlens-app-test PRIVATE
    SCWX_TEST_DATA_DIR="${WXLENS_DIR}/external/legacy-supercell-wx/test/data")

if (MSVC)
    set_target_properties(wxlens-app-test PROPERTIES LINK_FLAGS "/ignore:4099")
    # Same NOMINMAX requirement as wxlens-app - see app/CMakeLists.txt for the failure mode.
    target_compile_options(wxlens-app-test PRIVATE -DNOMINMAX)
    target_compile_options(wxlens-app-test PRIVATE "/MP")
endif()

target_link_libraries(wxlens-app-test GTest::gtest
                                      Qt6::Core
                                      Qt6::Qml
                                      Qt6::Quick
                                      Qt6::OpenGL
                                      Qt6::Network
                                      wxdata
                                      Boost::timer
                                      Boost::json
                                      GeographicLib::GeographicLib
                                      glm::glm-header-only
                                      tomlplusplus::tomlplusplus
                                      QMapLibre::Core)

gtest_discover_tests(wxlens-app-test)
