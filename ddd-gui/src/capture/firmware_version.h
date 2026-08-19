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
// The firmware and the gateware are a matched pair — one commit, one signed
// bundle, one fw-v* tag — so a difference between *those two* is worth
// mentioning. The application is not part of that set and never was.
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
// Two forms are accepted, because two exist in the field:
//
//     "Domesday Duplicator (a1b2c3d4)"           any build
//     "Domesday Duplicator 1.5.0 (a1b2c3d4)"     a build from an fw-v* tag
//
// The commit is in brackets at the end of both, which is why the release
// version was put in front of them rather than inside: a host that only knows
// the older shape goes on reading the newer one.
std::optional<std::string> ParseFirmwareCommit(std::string_view product_string);

// Pull the release version out of a USB product string, where it names one.
//
// Nothing for the older form, which is the honest answer: a firmware that was
// not built from a tag belongs to no release, and there is nothing to report.
// Nothing, too, for anything between the name and the bracket that is not a
// dotted numeric version — a string this code does not understand must not be
// presented to a user as a version.
std::optional<std::string> ParseFirmwareRelease(
    std::string_view product_string);

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
