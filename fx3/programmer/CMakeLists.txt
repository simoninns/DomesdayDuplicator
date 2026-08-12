cmake_minimum_required(VERSION 3.10)
project(fx3-programmer C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Emit compile_commands.json so clangd (and therefore any editor) can index the tree
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# Include directories
include_directories(${LIBUSB_INCLUDE_DIRS})

# FX3 programmer tool
set(PROG_SOURCES
    src/fx3-programmer.c
)

# Cypress secondary loader, needed for permanent (EEPROM/SPI) programming
set(FLASHPROG_IMAGE cyfxflashprog.img)
set(FLASHPROG_DATADIR "${CMAKE_INSTALL_DATADIR}/domesday-duplicator")

# Create executable
add_executable(fx3-programmer ${PROG_SOURCES})
target_link_libraries(fx3-programmer ${LIBUSB_LIBRARIES})
target_compile_options(fx3-programmer PRIVATE -Wall -Wextra)

# Tell the binary where its secondary loader will be installed, so it can find it from
# any working directory rather than only from the build tree
target_compile_definitions(fx3-programmer PRIVATE
    FLASHPROG_INSTALL_PATH="${CMAKE_INSTALL_FULL_DATADIR}/domesday-duplicator/${FLASHPROG_IMAGE}"
)

# Installation
install(TARGETS fx3-programmer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${FLASHPROG_IMAGE}
    DESTINATION ${FLASHPROG_DATADIR}
)

# lib/udev/rules.d, not /etc/udev/rules.d: an absolute DESTINATION escapes the install
# prefix, which breaks staged and sandboxed installs. Distribution packagers should copy
# or symlink from $PREFIX/lib/udev/rules.d into /etc/udev/rules.d — see README.md.
install(FILES
    configs/88-cyusb.rules
    DESTINATION lib/udev/rules.d
)
