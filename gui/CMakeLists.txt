cmake_minimum_required(VERSION 3.16)
project(DomesdayDuplicator VERSION 1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emit compile_commands.json so clangd (and therefore any editor) can index the tree
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_subdirectory(src/DomesdayDuplicator)
add_subdirectory(src/dddconv)
add_subdirectory(src/dddutil)

# Tests. On by default when this is the top-level project, so `cmake -B build` gives a
# contributor the suite without having to know a flag; off when consumed as a subproject.
include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
