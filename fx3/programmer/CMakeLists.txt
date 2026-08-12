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

# FX3 programmer tool.
# fx3-flashprog.c is separate from fx3-programmer.c so the path-resolution logic can be
# linked into a test without dragging in main() and libusb.
set(PROG_SOURCES
    src/fx3-programmer.c
    src/fx3-flashprog.c
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
    configs/70-domesday-duplicator.rules
    DESTINATION lib/udev/rules.d
)

# Tests. On by default when this is the top-level project, so `cmake -B build` gives a
# contributor the suite without having to know a flag; off when consumed as a subproject.
include(CTest)

if(BUILD_TESTING)
    # The tool is C, but GoogleTest is C++. Enable it only when tests are being built, so
    # a build without them still needs nothing but a C compiler.
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_subdirectory(tests)
endif()
