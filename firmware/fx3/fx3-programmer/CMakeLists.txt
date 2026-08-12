cmake_minimum_required(VERSION 3.10)
project(fx3-programmer C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# Include directories
include_directories(${LIBUSB_INCLUDE_DIRS})

# FX3 programmer tool
set(PROG_SOURCES
    src/fx3-programmer.c
)

# Create executable
add_executable(fx3-programmer ${PROG_SOURCES})
target_link_libraries(fx3-programmer ${LIBUSB_LIBRARIES})
target_compile_options(fx3-programmer PRIVATE -Wall -Wextra)

# Installation
install(TARGETS fx3-programmer
    RUNTIME DESTINATION bin
)

install(FILES 
    configs/88-cyusb.rules
    DESTINATION /etc/udev/rules.d
)
