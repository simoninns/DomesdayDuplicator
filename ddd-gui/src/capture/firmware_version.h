/************************************************************************

    firmware_version.h

    What a device says about the build it is running
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ddd::capture {

// The FX3 firmware writes the commit it was built from into the USB product
// string it reports:
//
//     "Domesday Duplicator (a1b2c3d4)"
//
// A commit and no release version, which is the whole of what a device can
// usefully say about itself. The firmware and the gateware are installed
// together, from one signed bundle, built from one commit — so the two of them
// reporting the same hash is what "this device is consistent" means, and a
// difference between *those two* is worth mentioning. Which release that commit
// belongs to is in the bundle's signed manifest, which is verified before it is
// read; nothing needs the device to say it. The application is not part of that
// set and never was.
//
// This used to compare the device's commit against the application's, on the
// stated premise that one commit built all three. That premise has been false
// since the release streams were separated: the capture application releases
// under gui-v* tags and the firmware and gateware under fw-v*, so an
// application and a firmware from different commits is the ordinary state of a
// fully up-to-date Duplicator. The comparison fired on exactly that, which is
// the worst thing a warning can do — and update_gate.h already said why it was
// the wrong tool: "a commit identifies a build exactly and orders nothing".
//
// What carries the real compatibility weight is machine-to-machine and
// enforced at install time by the update gate: the firmware's protocol version
// in bcdDevice, and the gateware's register map version at register 0x01, both
// against the ranges this build was compiled knowing.

// Pull the commit hash out of a USB product string. Returns nothing if the
// string is not in the expected form, which includes the case of a device
// running firmware old enough to predate the embedded hash entirely.
//
//     "Domesday Duplicator (a1b2c3d4)"
//
// The commit is the last bracketed group, which is deliberate rather than
// incidental: a device that ever names anything before the bracket goes on
// being read correctly by an application built today.
std::optional<std::string> ParseFirmwareCommit(std::string_view product_string);

// Reduce a build stamp to the commit hash it names, or nothing if it does not
// name one.
//
// A stamp is not always a bare hash: a dirty working tree appends "-dirty",
// and a build outside a checkout reports "unknown". Both are handled here
// rather than at each comparison, so there is one place that decides what
// counts as a commit.
std::optional<std::string> NormaliseCommit(std::string_view version);

// Do two commit stamps name the same commit?
//
// Compared on their common prefix rather than as whole strings, because they
// are not always the same length: the firmware and the gateware ask git for
// eight characters, while a Nix build passes seven. Two builds of one commit
// must never be reported as differing — a warning that fires when nothing is
// wrong is worse than no warning, because it is dismissed unread.
//
// Anything that does not normalise to a commit compares equal to nothing,
// including itself.
//
// Still used, and legitimately: an update confirms afterwards that the device
// reports the commit the bundle said it would, and the firmware dialog says
// whether the device's two halves came from one build. Both compare a commit
// against a commit that is *supposed* to be the same one.
bool CommitsMatch(std::string_view first, std::string_view second);

// What a device's product string said about itself.
struct FirmwareIdentity {
  // The commit the firmware was built from, empty where the string named
  // none.
  std::string commit;

  bool NamesCommit() const { return !commit.empty(); }

  // Whether this is worth interrupting the user for.
  //
  // Only one case is: a device running this application's own firmware that
  // did not say which build it is. Nothing here compares the device against
  // the application, because the two come from different releases.
  bool ShouldWarn() const { return !NamesCommit(); }

  // The text of the warning, empty when there is nothing to warn about. Says
  // what is not known and what it usually means, and does not tell the user to
  // stop: capture works normally either way.
  std::string message;
};

// Read what a device's product string says about the firmware it is running.
FirmwareIdentity DescribeFirmware(std::string_view product_string);

}  // namespace ddd::capture
