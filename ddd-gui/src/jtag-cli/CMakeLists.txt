# ddd-jtag — the shell harness for the JTAG programming path.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# A main() over ddd_capture and nothing else, for the same reason ddd-update is: it
# deliberately does not link Qt, so the SVF player and the cable driver cannot grow a Qt
# dependency without this failing to link.
#
# Installed like ddd-update. Programming an FPGA's configuration flash is a manual act
# (AGENTS.md §4), and this is the tool that performs it — and the tool the provisioning
# bench measurements are taken with.

add_executable(ddd-jtag main.cpp)

target_link_libraries(ddd-jtag PRIVATE ddd_capture)

target_compile_options(ddd-jtag PRIVATE -Wall -Wextra)

include(GNUInstallDirs)
install(TARGETS ddd-jtag RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
