/************************************************************************

    version.cpp

    Build provenance for the capture application
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "version.h"

#include <string_view>

#ifndef DDD_COMMIT
#define DDD_COMMIT "unknown"
#endif

namespace ddd::capture {

std::string_view Commit() { return DDD_COMMIT; }

}  // namespace ddd::capture
