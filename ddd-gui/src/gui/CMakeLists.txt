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
    resources/ddd_gui_resources.qrc
    about_dialog.cpp
    about_text.cpp
    amplitude_panel.cpp
    analysis_cli.cpp
    analysis_dialog.cpp
    analysis_worker.cpp
    application_logger.cpp
    capture_controller.cpp
    capture_panel.cpp
    capture_failure_presenter.cpp
    capture_settings.cpp
    firmware_dialog.cpp
    firmware_text.cpp
    gain_choices.cpp
    log_message_model.cpp
    log_panel.cpp
    main_window.cpp
    settings_dialog.cpp
    spectrum_panel.cpp
    statistics_panel.cpp
    statistics_presenter.cpp
    theme_controller.cpp
    theme_manager.cpp
    update_page.cpp
    update_text.cpp
    update_worker.cpp
    waveform_panel.cpp
)

# AUTORCC compiles the .qrc listed above into the library. The resource is then
# registered by a static initialiser, which a linker may drop from a static
# library nothing references — see the Q_INIT_RESOURCE call in about_text.cpp,
# which is what makes the graphics reliably present.
set_target_properties(ddd_gui_lib PROPERTIES AUTOMOC ON AUTORCC ON)

target_include_directories(ddd_gui_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ddd_gui_lib
    PUBLIC
        ddd_capture
        ddd_analysis
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
