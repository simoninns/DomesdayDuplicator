/************************************************************************

    update_gate.h

    Whether this bundle may be installed on this device by this build
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "device_updater.h"
#include "update_manifest.h"
#include "usb_device_info.h"

namespace ddd::capture {

// The install-time compatibility gate.
//
// A bundle that has been opened has been proved to come from the holder of
// the key and to carry the bytes that key signed for. It has *not* been
// proved installable on the device in front of the user, and those are
// different questions with different answers: the first is about the file
// and the second is about this device, this application and this moment.
//
// The rule the gate exists for: **a user cannot use this application to
// flash the device past this application's own understanding.** A bundle
// whose firmware would speak a protocol this build does not know disables
// the install and says "update the application first", so the ordering users
// must follow — application, then device — is the only ordering the
// interface permits rather than something the release notes ask for.
//
// Downgrades are permitted deliberately: installing an older release is a
// legitimate thing to want, and the archive of release bundles is where one
// comes from. They pass through this same gate, so a downgrade below what
// this build supports is refused for exactly the reason an overreaching
// upgrade is.
//
// Nothing here compares commit strings. A commit identifies a build exactly
// and orders nothing: "a1b2c3d4" is neither newer nor older than "e5f6a7b8",
// and code that appeared to compare them would be comparing text.

// Why an install is refused, or why it needs saying out loud.
enum class UpdateGateVerdict {
  // Install away.
  kAllowed,

  // The bundle is fine and the device is fine, but this application is too
  // old to be the one doing it. The interface offers the application update
  // instead; the install button stays disabled.
  kApplicationTooOld,

  // Something about the bundle or the device makes this install wrong.
  kIncompatible,
};

struct UpdateGateResult {
  UpdateGateVerdict verdict = UpdateGateVerdict::kAllowed;

  // One line per reason, written for a user. Empty when allowed.
  std::vector<std::string> reasons;

  bool allowed() const { return verdict == UpdateGateVerdict::kAllowed; }
};

// What the gate is being asked about.
struct UpdateGateInput {
  // This build's version stamp, as capture::Version() reports it.
  //
  // May be a commit hash or "unknown" for a developer build, in which case
  // the minimum-application-version check cannot be made at all — and is
  // then *not* made, rather than being made charitably. A build that cannot
  // say how old it is is a build that has to say so.
  std::string application_version;

  // The device as it reports itself now.
  DeviceIdentity device;

  // Whether a device is attached at all. False is a refusal with a reason
  // rather than a special case for the caller to remember.
  bool device_attached = false;

  // Which software the device is running.
  //
  // A device in a recovery personality has no identity to report — no product
  // string, no protocol version, no gateware — so the checks that compare
  // against those have nothing to compare and correctly stay quiet. What this
  // adds is the one check that only applies there: a device with no working
  // firmware cannot be repaired by a bundle that carries none.
  //
  // A device running the legacy firmware is refused outright, and it is the
  // only refusal here that no bundle can satisfy: that firmware predates the
  // update protocol, so there is nothing on the device to receive an update.
  DevicePersonality device_personality = DevicePersonality::kApplication;
};

// Decide whether this bundle may be installed.
UpdateGateResult CheckUpdateGate(const UpdateManifest& manifest,
                                 const UpdateGateInput& input);

// Whether a device's advertised protocol version is one this build speaks.
//
// Zero — firmware predating the field — is neither supported nor
// unsupported: it is unknown, and the caller treats it as old firmware that
// this build may update but must not make claims about.
bool ProtocolVersionIsSupported(int version);

// The same for the gateware's register map version.
bool RegisterMapVersionIsSupported(int version);

}  // namespace ddd::capture
