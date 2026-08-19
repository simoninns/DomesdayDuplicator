/************************************************************************

    version.h

    Build provenance for the capture application
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <string>
#include <string_view>

namespace ddd::capture {

// Two stamps, because a build has two things worth saying about itself and
// they answer different questions.
//
// They live in the engine rather than in the Qt layer because every front end
// has to be able to report them — the release gate rejects an artefact that
// cannot name its own source (AGENTS.md §9).

// The release this build is, as a dotted numeric version — "1.2.0" — or
// "unknown" for a build that is not a numbered release.
//
// This is the stamp that *orders*, and the only one that may be compared with
// another. The update gate refuses a bundle that demands a newer application
// than this, and it can only do that against a version: a commit identifies a
// build exactly and orders nothing.
//
// "unknown" is the honest answer for a developer build and for CI builds from
// an untagged commit, and the gate then declines to make the comparison rather
// than making it charitably.
std::string_view Version();

// The commit this build was made from, or "unknown" outside a git checkout
// when the build was not given one.
//
// This is the stamp that *identifies*. It is what a bug report needs, what
// capture metadata records, and what an update compares against a bundle's
// declared payload identity — always against a commit that is supposed to be
// the same one, never as a judgement of age.
std::string_view Commit();

// The two together, as a build says who it is: "1.2.0 (a1b2c3d4)".
//
// Falls back to the commit alone where there is no release version, which is
// what a developer build should show — "unknown (a1b2c3d4)" would put a word
// on screen that means nothing to the person reading it. Only a build that can
// name neither reports "unknown", and that is the state the release gate fails
// an artefact for.
std::string BuildStamp();

}  // namespace ddd::capture
