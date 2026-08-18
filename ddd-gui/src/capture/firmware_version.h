/************************************************************************

    firmware_version.h

    Comparing the firmware's build against the application's
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ddd::capture {

// Every release builds the application, the FX3 firmware and the FPGA gateware
// from one commit (AGENTS.md §9), and the firmware writes its own commit hash
// into the USB product string it reports:
//
//     "Domesday Duplicator (a1b2c3d4)"
//
// So the application can tell, on connect, whether the device in front of it is
// running the firmware it was released alongside. A mismatch usually means a
// device that was never updated, and it is worth saying so, because the
// symptoms of an old firmware are the symptoms of everything else: a capture
// that stops, a sequence error, a device that will not open.
//
// It is a warning and never an error. Old firmware is not known to be broken —
// it is merely not the firmware this build was tested with — and the user in
// front of a working capture is better served by a note than by a refusal. The
// application must monitor and capture normally whatever this says.

// Pull the commit hash out of a USB product string. Returns nothing if the
// string is not in the expected form, which includes the case of a device
// running firmware old enough to predate the embedded hash entirely.
std::optional<std::string> ParseFirmwareCommit(std::string_view product_string);

// Reduce a version stamp to the commit hash it names, or nothing if it does not
// name one.
//
// The application's own stamp is not always a bare hash: a dirty working tree
// appends "-dirty", and a build outside a checkout reports "unknown". Both are
// handled here rather than at the comparison, so there is one place that
// decides what counts as a commit.
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
bool CommitsMatch(std::string_view first, std::string_view second);

// What the comparison concluded.
struct FirmwareVersionCheck {
  enum class Status {
    // Both sides name a commit and they agree.
    kMatch,

    // Both sides name a commit and they differ. The one case that is genuinely
    // worth telling a user about.
    kMismatch,

    // The device's product string carried no commit — an old firmware, or a
    // descriptor that could not be read.
    kDeviceUnknown,

    // This build cannot name its own commit, so it is in no position to judge
    // the firmware's. Warning here would fire on every developer build and
    // teach the user to dismiss the dialog without reading it, which would cost
    // the warning its only value.
    kApplicationUnknown,
  };

  Status status = Status::kApplicationUnknown;

  // The hashes as compared, empty where none was available
  std::string device_commit;
  std::string application_commit;

  // Whether this is worth interrupting the user for
  bool ShouldWarn() const {
    return status == Status::kMismatch || status == Status::kDeviceUnknown;
  }

  // The text of the warning, empty when there is nothing to warn about. Says
  // what differs and what it means, and does not tell the user to stop.
  std::string message;
};

// Compare a device's product string against this build's version stamp.
FirmwareVersionCheck CheckFirmwareVersion(std::string_view product_string,
                                          std::string_view application_version);

}  // namespace ddd::capture
