cmake_minimum_required(VERSION 3.16)
project(dddutil VERSION 1.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Widgets)

qt_add_executable(dddutil WIN32 MACOSX_BUNDLE
    about.cpp about.h about.ui
    analysetestdata.cpp analysetestdata.h
    fileconverter.cpp fileconverter.h
    inputsample.cpp inputsample.h
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    progressdialog.cpp progressdialog.h progressdialog.ui
    sampledetails.cpp sampledetails.h
)
target_compile_definitions(dddutil PRIVATE
    QT_DEPRECATED_WARNINGS
)

target_link_libraries(dddutil PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
)

install(TARGETS dddutil
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
