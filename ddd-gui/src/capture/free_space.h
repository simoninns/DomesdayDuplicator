/************************************************************************

    free_space.h

    How much longer the disk will last
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <filesystem>

namespace ddd::capture {

// Free space matters here in a way it does not in most applications: a capture
// runs for the length of a disc side and cannot be resumed, so "there is
// 40 GB free" is not the question. The question is "will this last the next
// forty minutes", and that is a time, not a size — which is why every figure
// this header produces is expressed in seconds.

// What a FLAC capture is assumed to consume on disk.
//
// The wire rate is 80 MB/s and FLAC roughly halves it, so 40 MB/s is the
// working figure — the same estimate ld-decode's tooling uses for its own
// planning. It is deliberately an estimate and deliberately conservative in the
// direction that matters: real RF compresses better than this, so a capture
// that this predicts will fit almost always does.
inline constexpr double kEstimatedCaptureBytesPerSecond = 40.0e6;

struct FreeSpace {
  // False when the volume could not be interrogated at all — a directory that
  // does not exist yet, a permission error, a filesystem that does not report
  // it. A caller shows "unknown" rather than zero, because zero reads as "full"
  // and would be a lie that stops someone capturing.
  bool known = false;

  uint64_t bytes_available = 0;
};

// Space on the volume holding a directory. Never throws: a missing or
// unreadable path comes back as not known.
FreeSpace AvailableSpace(const std::filesystem::path& directory);

// How long a capture could run in a given number of bytes.
double CaptureSecondsRemaining(
    uint64_t bytes_available,
    double bytes_per_second = kEstimatedCaptureBytesPerSecond);

// And the reverse: what a capture of a given length is expected to need.
uint64_t CaptureBytesForSeconds(
    double seconds, double bytes_per_second = kEstimatedCaptureBytesPerSecond);

}  // namespace ddd::capture
