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
