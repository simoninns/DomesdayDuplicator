# The capture engine — Qt-free by rule.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Nothing here may link or include Qt. That is what lets the engine be unit tested without
# a display, and what will let a headless command-line capture tool reuse it unchanged.

add_library(ddd_capture STATIC
    capture_format.cpp
    capture_pipeline.cpp
    capture_reader.cpp
    disk_buffer_ring.cpp
    flac_sink.cpp
    flac_writer.cpp
    logger.cpp
    memory_lock.cpp
    monitor_tap.cpp
    sample_metrics.cpp
    sample_sink.cpp
    sequence_validator.cpp
    synthetic_source.cpp
    test_pattern_verifier.cpp
    thread_priority.cpp
    transfer_result.cpp
    version.cpp
)

target_include_directories(ddd_capture PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(ddd_capture PRIVATE -Wall -Wextra)

target_include_directories(ddd_capture SYSTEM PUBLIC ${FLAC_INCLUDE_DIRS})
target_link_libraries(ddd_capture PUBLIC ${FLAC_LIBRARIES} Threads::Threads)

# Only this target sees the version stamp, so a new commit rebuilds one object file rather
# than every Qt translation unit in the application.
target_compile_definitions(ddd_capture PRIVATE DDD_VERSION="${DDD_VERSION}")
