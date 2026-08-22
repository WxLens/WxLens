cmake_minimum_required(VERSION 3.24)
project(nimbus-app CXX)

find_package(Qt6 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(nimbus-app
    source/nimbus/main/main.cpp
    source/nimbus/log/logger.cpp
    source/nimbus/log/logger.hpp
)

target_include_directories(nimbus-app PRIVATE source)

qt_add_qml_module(nimbus-app
    URI Nimbus.App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
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
)

if (MSVC)
    target_compile_options(nimbus-app PRIVATE "/MP")
endif()
