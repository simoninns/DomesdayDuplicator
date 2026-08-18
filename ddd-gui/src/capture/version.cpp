/************************************************************************

    version.cpp

    Build provenance for the capture application
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "version.h"

// Defined on this one translation unit by CMake, so changing the version stamp
// rebuilds a single object file rather than the whole application.
#ifndef DDD_VERSION
#define DDD_VERSION "unknown"
#endif

namespace ddd::capture {

std::string_view Version() { return DDD_VERSION; }

}  // namespace ddd::capture
