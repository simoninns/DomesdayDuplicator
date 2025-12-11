cmake_minimum_required(VERSION 3.10)
project(elf2img C)

# Build the elf2img utility as a host tool
add_executable(elf2img elf2img.c)

# Set compiler flags
target_compile_options(elf2img PRIVATE -Wall)

# Install the binary
install(TARGETS elf2img RUNTIME DESTINATION bin)
