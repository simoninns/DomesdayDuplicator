# The LaserDisc player protocol — Qt-free, and with no port in it.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Nothing here may link or include Qt, and nothing here opens a serial port: the whole
# protocol talks to the ISerialPort interface instead. That is what lets a connect, probe
# and command sequence be driven from a unit test in microseconds — with the player
# answering late, answering wrongly, answering at the wrong baud rate or going silent
# mid-command — on a machine with no player and no serial adapter attached.
#
# The Qt half (the QSerialPort backend, the worker thread and the controller) belongs in
# src/gui, where Qt already lives.

add_library(ddd_player STATIC
    command_encoder.cpp
    player_controls.cpp
    player_registry.cpp
    player_session.cpp
    response_parser.cpp
    user_code.cpp
)

target_include_directories(ddd_player PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(ddd_player PRIVATE -Wall -Wextra)
