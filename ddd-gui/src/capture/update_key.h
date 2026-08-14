/************************************************************************

    update_key.h

    Which signatures this build accepts, and what each one proves
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "update_bundle.h"

namespace ddd::capture {

// There are two signing keys and they mean entirely different things.
//
// The **release** key's secret half is a CI secret and never appears in this
// repository. A release signature proves a bundle came from the project.
//
// The **development** key's secret half is committed to this repository on
// purpose (AGENTS.md §5.5) and is therefore public. A development signature
// proves a bundle is well formed and proves nothing whatever about where it
// came from. That is not a weakness in the scheme, it is the scheme: the
// development key *is* the unsigned-equivalent, made explicit and impossible
// to confuse with a release. An actually-unsigned format would have needed a
// second route through verification, and a second route through verification
// is where the bugs live.
//
// The acceptance rules that follow from that, and they are rules rather than
// defaults:
//
//   * a release build pins the release public key and accepts nothing else;
//   * accepting the development key requires an explicit, per-invocation
//     opt-in — `--dev-update-key`, or a debug build, which implies it;
//   * a development-signed bundle is bannered prominently as such in the
//     update interface, every time.
//
// The channel field in the manifest and the key that verified it must agree.
// A bundle claiming to be a release and signed with the development key is
// refused, because the claim and the proof disagree and the claim is the
// half an attacker writes.

// What this invocation is willing to accept.
struct UpdateKeyPolicy {
  // Accept a bundle signed with the committed development key.
  //
  // Off unless something asked for it. A debug build turns it on, because a
  // debug build is already a build nobody ships.
  bool accept_development_key = false;
};

// The policy a build starts with, before any command-line option.
UpdateKeyPolicy DefaultUpdateKeyPolicy();

// Whether this build has a release key pinned at all.
//
// False until the release pipeline exists to hold one, and that is worth
// asking about rather than discovering: a build with no release key can
// install nothing but development bundles, and the interface should say so
// rather than reporting every release bundle as unsigned.
bool HasReleaseUpdateKey();

// Open a bundle under these acceptance rules.
//
// Tries the release key first, then — only if the policy allows it — the
// development key. On success the manifest's channel has been checked
// against the key that actually verified it.
//
// Returns nothing, and sets `error` when it is not null, with a message
// written for whoever is looking at the screen.
std::optional<UpdateBundle> OpenUpdateBundleForPolicy(
    std::span<const uint8_t> archive, const UpdateKeyPolicy& policy,
    std::string* error);

// The development public key's text, exactly as tools/keys/development.pub
// carries it. Exposed for the tests and for the interface's "signed with the
// development key" banner; nothing else should need it.
std::string_view DevelopmentUpdateKeyText();

}  // namespace ddd::capture
