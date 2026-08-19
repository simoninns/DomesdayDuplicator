cmake_minimum_required(VERSION 3.16)
project(fx3-mkimage C)

# Emit compile_commands.json so clangd (and therefore any editor) can index the tree
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# Commit stamp, same shape as the firmware and the GUI: callers building
# outside a git checkout pass -DDDD_COMMIT=<hash>, and only when they have not
# does the build ask git. See AGENTS.md section 9.
if(NOT DEFINED DDD_COMMIT OR DDD_COMMIT STREQUAL "")
    execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE DDD_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(NOT DDD_COMMIT)
    set(DDD_COMMIT "unknown")
endif()

message(STATUS "fx3-mkimage commit: ${DDD_COMMIT}")

# The pure half — ELF parsing and image construction — is a library so the test
# suite can link it without main().
add_library(fx3-bootimage STATIC src/fx3-bootimage.c)
target_include_directories(fx3-bootimage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(fx3-bootimage PRIVATE -Wall -Wextra)

add_executable(fx3-mkimage src/main.c)
target_link_libraries(fx3-mkimage PRIVATE fx3-bootimage)
target_compile_options(fx3-mkimage PRIVATE -Wall -Wextra)
target_compile_definitions(fx3-mkimage PRIVATE FX3_MKIMAGE_VERSION="${DDD_COMMIT}")

install(TARGETS fx3-mkimage RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Tests. On by default when this is the top-level project, matching the other
# components; off when consumed as a subproject.
include(CTest)

if(BUILD_TESTING)
    # The tool is C; only the GoogleTest suite needs a C++ compiler, so the
    # requirement is declared here rather than in project() above.
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_subdirectory(tests)
endif()
