cmake_minimum_required(VERSION 3.16)
project(DomesdayDuplicator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake_modules")

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Widgets SerialPort)
find_package(LibUSB REQUIRED)

qt_add_executable(DomesdayDuplicator MACOSX_BUNDLE
    aboutdialog.cpp aboutdialog.h aboutdialog.ui
    advancednamingdialog.cpp advancednamingdialog.h advancednamingdialog.ui
    automaticcapturedialog.cpp automaticcapturedialog.h automaticcapturedialog.ui
    configuration.cpp configuration.h
    configurationdialog.cpp configurationdialog.h configurationdialog.ui
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    playercommunication.cpp playercommunication.h
    playercontrol.cpp playercontrol.h
    playerremotedialog.cpp playerremotedialog.h playerremotedialog.ui
    usbcapture.cpp usbcapture.h
    usbdevice.cpp usbdevice.h
)
target_compile_definitions(DomesdayDuplicator PRIVATE
    QT_DEPRECATED_WARNINGS
)

target_link_libraries(DomesdayDuplicator PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::SerialPort
    ${LibUSB_LIBRARIES}
)

# Resources:
set_source_files_properties("Graphics/Domesday Duplicator logo colour 250px.png"
    PROPERTIES QT_RESOURCE_ALIAS "Domesday Duplicator logo"
)
set(resources_resource_files
    "Graphics/Domesday Duplicator logo colour 150px.png"
    "Graphics/Domesday Duplicator logo colour 250px.png"
)

qt_add_resources(DomesdayDuplicator "resources"
    PREFIX
        "/graphics/logo"
    FILES
        ${resources_resource_files}
)
set(resources1_resource_files
    "Graphics/ApplicationIcon/DomesdayDuplicator_128x128.png"
    "Graphics/ApplicationIcon/DomesdayDuplicator_16x16.png"
    "Graphics/ApplicationIcon/DomesdayDuplicator_24x24.png"
    "Graphics/ApplicationIcon/DomesdayDuplicator_256x256.png"
    "Graphics/ApplicationIcon/DomesdayDuplicator_300x300.png"
    "Graphics/ApplicationIcon/DomesdayDuplicator_32x32.png"
    "Graphics/ApplicationIcon/DomesdayDuplicator_48x48.png"
    "Graphics/ApplicationIcon/DomesdayDuplicator_64x64.png"
)

qt_add_resources(DomesdayDuplicator "resources1"
    PREFIX
        "/graphics/ApplicationIcon"
    FILES
        ${resources1_resource_files}
)

target_include_directories(DomesdayDuplicator PRIVATE
    ${LibUSB_INCLUDE_DIRS}
)


if(WIN32)
    target_compile_definitions(DomesdayDuplicator PRIVATE
        NOMINMAX
        QUSB_LIBRARY
    )

    target_link_libraries(DomesdayDuplicator PRIVATE
        # TODO: Does this depend on compiler?
        AdvAPI32
    )
endif()

install(TARGETS DomesdayDuplicator
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
