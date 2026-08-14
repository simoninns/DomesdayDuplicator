# ddd-update — the headless half of the device update path.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# A main() over ddd_capture and nothing else. It deliberately does not link Qt: that is
# the enforcement of the rule that the updater engine is Qt-free, in the same way
# ddd_capture_tests enforces it for the capture engine. If a Qt dependency ever creeps
# into the update path, this stops linking.

add_executable(ddd-update main.cpp)

target_link_libraries(ddd-update PRIVATE ddd_capture)

target_compile_options(ddd-update PRIVATE -Wall -Wextra)

include(GNUInstallDirs)
install(TARGETS ddd-update RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
