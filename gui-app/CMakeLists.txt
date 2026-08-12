cmake_minimum_required(VERSION 3.16)
project(DomesdayDuplicator VERSION 1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/tools/cmake_modules")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets SerialPort)
find_package(LibUSB REQUIRED)

# Use qt_standard_project_setup if available (Qt 6.3+), otherwise manual setup
if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    include(GNUInstallDirs)
endif()

add_subdirectory(tools/DomesdayDuplicator)
add_subdirectory(tools/dddconv)
add_subdirectory(tools/dddutil)
