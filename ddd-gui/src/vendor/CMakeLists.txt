# The vendored cryptography — see VENDOR.md.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# A target of its own, rather than four more files in ddd_capture, so that the quality
# gates that apply to project code cannot apply to code the project must not edit
# (AGENTS.md §3):
#
#   * no -Wall -Wextra — a warning here is not ours to fix, and the next upstream refresh
#     would reintroduce it anyway;
#   * no clang-tidy, set explicitly rather than left to inherit;
#   * excluded from the clang-format glob in the top-level CMakeLists.txt.
#
# Everything the engine sees of this is the two wrappers in src/capture: digest.h for the
# hash and minisign_verify.h for the signature. Nothing else includes these headers, so
# swapping an implementation later is a change to two files.

add_library(ddd_vendor_crypto STATIC
    sha-256.c
    monocypher.c
    monocypher-ed25519.c
)

target_include_directories(ddd_vendor_crypto PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# SYSTEM on the consuming side would be the usual way to silence warnings from headers;
# these are compiled here rather than included widely, so turning the gate off at the
# target is both simpler and complete.
set_target_properties(ddd_vendor_crypto PROPERTIES
    C_CLANG_TIDY ""
    POSITION_INDEPENDENT_CODE ON
)
