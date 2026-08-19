/************************************************************************

    version.cpp

    Build provenance for the capture application
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "version.h"

#include <string>

// Defined on this one translation unit by CMake, so changing either stamp
// rebuilds a single object file rather than the whole application.
#ifndef DDD_VERSION
#define DDD_VERSION "unknown"
#endif

#ifndef DDD_COMMIT
#define DDD_COMMIT "unknown"
#endif

namespace ddd::capture {
namespace {

constexpr std::string_view kUnknown = "unknown";

}  // namespace

std::string_view Version() { return DDD_VERSION; }

std::string_view Commit() { return DDD_COMMIT; }

std::string BuildStamp() {
  const std::string_view version = Version();
  const std::string_view commit = Commit();

  const bool has_version = !version.empty() && version != kUnknown;
  const bool has_commit = !commit.empty() && commit != kUnknown;

  if (has_version && has_commit) {
    return std::string(version) + " (" + std::string(commit) + ")";
  }
  if (has_version) {
    return std::string(version);
  }
  if (has_commit) {
    return std::string(commit);
  }
  return std::string(kUnknown);
}

}  // namespace ddd::capture
