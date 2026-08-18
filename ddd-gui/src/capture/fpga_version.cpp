/************************************************************************

    fpga_version.cpp

    What the gateware reports about the build it came from
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "fpga_version.h"

#include "wire_protocol.h"

namespace ddd::capture {
namespace {

bool IsHexDigit(uint8_t character) {
  return (character >= '0' && character <= '9') ||
         (character >= 'a' && character <= 'f') ||
         (character >= 'A' && character <= 'F');
}

}  // namespace

bool FpgaVersion::MapVersionIsKnown() const {
  return present && map_version >= kRegisterMapVersionMinimum &&
         map_version <= kRegisterMapVersionMaximum;
}

bool FpgaVersion::IsRecoveryGateware() const {
  return MapVersionIsKnown() && image_role == kImageRoleFactory;
}

FpgaVersion ParseFpgaIdentity(const std::vector<uint8_t>& identity) {
  FpgaVersion version;

  if (identity.size() < kIdentityLength) {
    return version;
  }

  if (identity[kRegisterId] != kIdentityValue) {
    return version;
  }

  version.present = true;
  version.map_version = identity[kRegisterMapVersion];
  version.image_role = identity[kRegisterImageRole];

  const uint8_t flags = identity[kRegisterBuildFlags];
  version.dirty = (flags & kBuildFlagDirty) != 0;

  // The commit bit is positive logic, so every way of not knowing — gateware
  // built outside a checkout, a byte that arrived as zero — reads as no commit
  // rather than as a confident claim about commit 00000000.
  if ((flags & kBuildFlagCommit) == 0) {
    return version;
  }

  // The characters are null padded to eight, because the commit is seven
  // characters from one build system and eight from another. Anything that is
  // not a hex digit ends it, which covers the padding and also means a misread
  // link cannot put arbitrary bytes into a dialog.
  for (uint8_t index = 0; index < kCommitLength; ++index) {
    const uint8_t character = identity[kRegisterCommit + index];
    if (!IsHexDigit(character)) {
      break;
    }
    version.commit.push_back(static_cast<char>(character));
  }

  return version;
}

}  // namespace ddd::capture
