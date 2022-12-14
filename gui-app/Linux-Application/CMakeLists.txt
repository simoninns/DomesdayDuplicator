cmake_minimum_required(VERSION 3.13)
project(DomesdayDuplicator VERSION 1.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake_modules")

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Widgets SerialPort Multimedia)
find_package(LibUSB REQUIRED)

# For Qt < 5.15, emulate 5.15's Qt5CoreConfig.cmake, so we don't have to
# specify Qt5/Qt6 elsewhere.
if(QT_VERSION VERSION_LESS 5.15)
    function(qt_add_resources outfiles)
        qt5_add_resources("${outfiles}" ${ARGN})
        set("${outfiles}" "${${outfiles}}" PARENT_SCOPE)
    endfunction()
    foreach(library Core Gui SerialPort Widgets Multimedia)
        add_library(Qt::${library} INTERFACE IMPORTED)
        set_target_properties(Qt::${library} PROPERTIES
            INTERFACE_LINK_LIBRARIES "Qt5::${library}")
    endforeach()
endif()

add_subdirectory(DomesdayDuplicator)
add_subdirectory(dddconv)
add_subdirectory(dddutil)
