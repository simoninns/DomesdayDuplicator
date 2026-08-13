# The Qt layer, and the application built on it.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Everything except main() lives in a static library so the test binaries can link it. A
# window, a model or a controller compiled only into an executable is a thing no test can
# reach.

add_library(ddd_gui_lib STATIC
    about_text.cpp
    application_logger.cpp
    log_message_model.cpp
    log_panel.cpp
    main_window.cpp
    panel_placeholder.cpp
    theme_controller.cpp
    theme_manager.cpp
)

set_target_properties(ddd_gui_lib PROPERTIES AUTOMOC ON)

target_include_directories(ddd_gui_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ddd_gui_lib
    PUBLIC
        ddd_capture
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_options(ddd_gui_lib PRIVATE -Wall -Wextra)

add_executable(ddd-gui main.cpp)

set_target_properties(ddd-gui PROPERTIES AUTOMOC ON)

target_link_libraries(ddd-gui PRIVATE ddd_gui_lib)

target_compile_options(ddd-gui PRIVATE -Wall -Wextra)

# WIN32_EXECUTABLE keeps a console window from opening behind the application on Windows;
# MACOSX_BUNDLE is what makes the macOS build an .app rather than a bare binary.
set_target_properties(ddd-gui PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

include(GNUInstallDirs)
install(TARGETS ddd-gui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
