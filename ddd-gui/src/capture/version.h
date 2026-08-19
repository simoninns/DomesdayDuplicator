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

// The commit this build was made from, or "unknown" outside a git checkout
// when the build was not given one.
//
// One stamp, not two. There was briefly a release version beside this, on the
// reasoning that a version orders and a commit does not — but ordering the
// application against a device turned out to be the wrong question entirely
// (see firmware_version.h), and the device cannot carry a release version in
// any case. What is left is the stamp that *identifies*, and all three parts
// of a Duplicator now carry the same kind of one: the application here, the
// FX3 in its USB product string, the FPGA in its identity register. A bug
// report quotes three hashes and they can be set beside one another.
//
// This is what capture metadata records and what an update compares against a
// bundle's declared payload identity — always against a commit that is
// supposed to be the same one, never as a judgement of age.
//
// It lives in the engine rather than in the Qt layer because every front end
// has to be able to report it: the release gate rejects an artefact that
// cannot name its own source (AGENTS.md §9), and "unknown" is what it fails.
std::string_view Commit();

}  // namespace ddd::capture
