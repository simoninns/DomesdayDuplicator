/************************************************************************

    version.h

    Build provenance for the capture application
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <string_view>

namespace ddd::capture {

// The commit this binary was built from, or "unknown" outside a git checkout
// when the build was not given one. It lives in the engine rather than the Qt
// layer because every front end has to be able to report it — the release gate
// rejects an artefact that cannot name its own source (AGENTS.md §9).
std::string_view Version();

}  // namespace ddd::capture
