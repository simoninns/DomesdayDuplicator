/************************************************************************

    free_space.cpp

    How much longer the disk will last
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "free_space.h"

#include <system_error>

namespace ddd::capture {

FreeSpace AvailableSpace(const std::filesystem::path& directory) {
  FreeSpace result;
  if (directory.empty()) {
    return result;
  }

  // Asked explicitly, because the two platforms disagree about what a missing
  // directory means. POSIX statvfs fails on one, so space() reports an error;
  // Windows resolves the volume out of the path and never looks at the
  // directory at all, so space() succeeds and answers for the whole drive. The
  // header promises "not known" for a path that is not there, and that promise
  // has to hold on both.
  std::error_code error;
  if (!std::filesystem::is_directory(directory, error)) {
    return result;
  }

  const std::filesystem::space_info info =
      std::filesystem::space(directory, error);
  if (error) {
    return result;
  }

  // `available` rather than `free`. On a Unix filesystem some blocks are
  // reserved for root, and a capture running as an ordinary user cannot have
  // them — reporting `free` would promise several gigabytes that do not exist.
  if (info.available == static_cast<std::uintmax_t>(-1)) {
    return result;
  }

  result.known = true;
  result.bytes_available = static_cast<uint64_t>(info.available);
  return result;
}

double CaptureSecondsRemaining(uint64_t bytes_available,
                               double bytes_per_second) {
  if (bytes_per_second <= 0.0) {
    return 0.0;
  }
  return static_cast<double>(bytes_available) / bytes_per_second;
}

uint64_t CaptureBytesForSeconds(double seconds, double bytes_per_second) {
  if (seconds <= 0.0 || bytes_per_second <= 0.0) {
    return 0;
  }
  return static_cast<uint64_t>(seconds * bytes_per_second);
}

}  // namespace ddd::capture
