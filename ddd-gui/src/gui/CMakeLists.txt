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
    auto_capture_controller.cpp
    application_logger.cpp
    capture_controller.cpp
    capture_naming_dialog.cpp
    capture_panel.cpp
    capture_failure_presenter.cpp
    capture_settings.cpp
    examine_dialog.cpp
    firmware_dialog.cpp
    firmware_text.cpp
    gain_choices.cpp
    guided_capture_dialog.cpp
    log_message_model.cpp
    log_panel.cpp
    main_window.cpp
    player_controller.cpp
    player_discovery.cpp
    player_panel.cpp
    player_remote_dialog.cpp
    player_settings.cpp
    player_text.cpp
    player_worker.cpp
    qt_serial_port.cpp
    serial_port_scanner.cpp
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

# ddd_player is the Qt-free player protocol; the QSerialPort backend that carries it, and
# the worker thread that drives it, belong here because this is where Qt lives.
target_link_libraries(ddd_gui_lib
    PUBLIC
        ddd_capture
        ddd_analysis
        ddd_player
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::SerialPort
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

# Desktop integration. Installed on Linux only: macOS gets this from the .app bundle's
# Info.plist and Windows from the MSI's shortcut, so installing freedesktop files there
# would just be litter.
#
# The Flatpak build reads these out of the install prefix rather than carrying its own
# copies, which is what stops the manifest and the application disagreeing about the
# application ID — there is one definition, DDD_APP_ID in the top-level CMakeLists, and
# both the filenames and the installed names are derived from it.
if(UNIX AND NOT APPLE)
    set(DDD_PACKAGING_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/../../packaging/assets")

    install(FILES "${DDD_PACKAGING_ASSETS}/${DDD_APP_ID}.desktop"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

    install(FILES "${DDD_PACKAGING_ASSETS}/${DDD_APP_ID}.metainfo.xml"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")

    # The same PNGs the application shows in its own About dialog, installed under the
    # icon name the .desktop file asks for. icon.ico beside them is the Windows installer's
    # copy of the same artwork, built from this set.
    foreach(icon_size 16 24 32 48 64 128 256)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon-${icon_size}.png"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${icon_size}x${icon_size}/apps"
                RENAME "${DDD_APP_ID}.png")
    endforeach()
endif()
