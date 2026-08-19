/************************************************************************

    firmware_version.cpp

    What a device says about the build it is running
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

// A dotted numeric version and nothing else: "1", "1.5", "1.5.0".
//
// Strict on purpose. Anything else between the name and the bracket is a word
// this code does not understand, and showing it to a user as a version would
// be presenting an unparsed fragment of a device's descriptor as a fact.
bool IsDottedVersion(std::string_view text) {
  if (text.empty() || text.front() == '.' || text.back() == '.') {
    return false;
  }

  bool previous_was_dot = false;
  for (const char character : text) {
    if (character == '.') {
      if (previous_was_dot) {
        return false;
      }
      previous_was_dot = true;
      continue;
    }
    if (character < '0' || character > '9') {
      return false;
    }
    previous_was_dot = false;
  }
  return true;
}

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

std::optional<std::string> ParseFirmwareRelease(
    std::string_view product_string) {
  const size_t open = product_string.rfind('(');
  if (open == std::string_view::npos || open == 0) {
    return std::nullopt;
  }

  // Between the fixed name and the bracket, which is where the release version
  // sits when there is one. The name itself is not matched on: this has to
  // keep working if the product string is ever renamed, and what identifies
  // the field is its position rather than what precedes it.
  std::string_view middle = product_string.substr(0, open);
  while (!middle.empty() && middle.back() == ' ') {
    middle.remove_suffix(1);
  }

  const size_t space = middle.rfind(' ');
  if (space == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view candidate = middle.substr(space + 1);

  if (!IsDottedVersion(candidate)) {
    return std::nullopt;
  }
  return std::string(candidate);
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

bool CommitsMatch(std::string_view first, std::string_view second) {
  const std::optional<std::string> left = NormaliseCommit(first);
  const std::optional<std::string> right = NormaliseCommit(second);

  if (!left.has_value() || !right.has_value()) {
    return false;
  }

  const size_t compared = std::min({left->size(), right->size(), size_t{8}});
  return left->compare(0, compared, *right, 0, compared) == 0;
}

FirmwareIdentity DescribeFirmware(std::string_view product_string) {
  FirmwareIdentity identity;

  const std::optional<std::string> commit = ParseFirmwareCommit(product_string);
  if (commit.has_value()) {
    identity.commit = *commit;
    return identity;
  }

  // Two causes, and the message names both because the more likely one is not
  // the one this warning was originally written for.
  //
  // A device wearing this application's own identifiers has always stamped its
  // commit — the identifiers and the stamp arrived together — so firmware too
  // old to carry one is nearly a museum piece. What does happen, and happens
  // to almost everybody once, is that the product string could not be read at
  // all: it is a string descriptor, reading it means opening the device, and
  // on Linux an unopenable device is the missing udev rule that the very next
  // thing they try will also fail on.
  identity.message =
      "The device did not report which firmware build it is running. On Linux "
      "that usually means the udev rules are not installed, so the device "
      "cannot be opened to be asked; otherwise it means firmware older than "
      "the version stamp itself. Monitoring will show which of the two it is.";
  return identity;
}

}  // namespace ddd::capture
