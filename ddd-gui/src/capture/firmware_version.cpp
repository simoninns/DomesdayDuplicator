/************************************************************************

    firmware_version.cpp

    Comparing the firmware's build against the application's
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "firmware_version.h"

#include <algorithm>
#include <cctype>

namespace ddd::capture {
namespace {

// The shortest prefix two stamps may be compared on.
//
// They are not always the same length. The firmware asks git for eight
// characters; the application's stamp comes from whichever build system
// produced it, and Nix supplies seven. Comparing full strings would report a
// mismatch between two builds of the same commit, which is the one thing this
// check must never do — a warning that fires when nothing is wrong is worse
// than no warning, because it is dismissed unread.
constexpr size_t kMinimumCommitLength = 7;

bool IsHexDigit(char character) {
  const unsigned char value = static_cast<unsigned char>(character);
  return std::isxdigit(value) != 0;
}

std::string ToLowerCase(std::string_view text) {
  std::string lowered(text);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return lowered;
}

}  // namespace

std::optional<std::string> ParseFirmwareCommit(
    std::string_view product_string) {
  const size_t open = product_string.rfind('(');
  if (open == std::string_view::npos) {
    return std::nullopt;
  }

  const size_t close = product_string.find(')', open);
  if (close == std::string_view::npos) {
    return std::nullopt;
  }

  // Through the same normalisation the application's own stamp goes through,
  // and that is not symmetry for its own sake: the firmware's CMake asks git
  // for the commit exactly as the application's does, so a firmware built from
  // a dirty tree reports "bb65470-dirty" in its product descriptor. Rejecting
  // that as unparseable would make every development device raise the "did not
  // report which firmware build it is running" warning.
  return NormaliseCommit(product_string.substr(open + 1, close - open - 1));
}

std::optional<std::string> NormaliseCommit(std::string_view version) {
  // A dirty build names the commit it started from. That is still the right
  // thing to compare against the firmware: the local edits are to the
  // application, not to the device.
  const size_t dirty = version.find("-dirty");
  if (dirty != std::string_view::npos) {
    version = version.substr(0, dirty);
  }

  if (version.size() < kMinimumCommitLength) {
    return std::nullopt;
  }

  if (!std::all_of(version.begin(), version.end(), IsHexDigit)) {
    return std::nullopt;
  }

  return ToLowerCase(version);
}

FirmwareVersionCheck CheckFirmwareVersion(
    std::string_view product_string, std::string_view application_version) {
  FirmwareVersionCheck check;

  const std::optional<std::string> application =
      NormaliseCommit(application_version);
  const std::optional<std::string> device = ParseFirmwareCommit(product_string);

  if (device.has_value()) {
    check.device_commit = *device;
  }
  if (application.has_value()) {
    check.application_commit = *application;
  }

  // Checked before the device, deliberately. A build that cannot name its own
  // commit has nothing to compare with, and saying "your firmware is unknown"
  // when the application is equally unknown would be an accusation it is not
  // entitled to make.
  if (!application.has_value()) {
    check.status = FirmwareVersionCheck::Status::kApplicationUnknown;
    return check;
  }

  if (!device.has_value()) {
    check.status = FirmwareVersionCheck::Status::kDeviceUnknown;
    check.message =
        "The device did not report which firmware build it is running. That "
        "usually means firmware older than the version check itself. Capture "
        "will work normally; updating the firmware is worth doing when "
        "convenient.";
    return check;
  }

  const size_t compared =
      std::min({device->size(), application->size(), size_t{8}});
  if (device->compare(0, compared, *application, 0, compared) == 0) {
    check.status = FirmwareVersionCheck::Status::kMatch;
    return check;
  }

  check.status = FirmwareVersionCheck::Status::kMismatch;
  check.message = "The device is running firmware built from commit " +
                  *device + ", and this application was built from commit " +
                  *application +
                  ". Every release builds both from the same commit, so these "
                  "differing means one of them was not updated. Capture will "
                  "work normally; if something behaves oddly, matching them up "
                  "is the first thing to try.";
  return check;
}

}  // namespace ddd::capture
