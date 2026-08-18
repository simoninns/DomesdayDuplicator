/************************************************************************

    boot_image.h

    Reading the FX3 boot image the boot ROM expects
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ddd::capture {

// The FX3 boot image format, from the host's side.
//
// `firmware.img` — the payload the update bundle carries and the file
// fx3-mkimage produces — is not a flat binary. It is a container: a four-byte
// header, then a run of sections each carrying a load address, then a
// termination record holding the program entry point, then a checksum over
// the section payloads. The device's boot ROM parses it when it boots from the
// EEPROM; the *host* parses it when it hands the same image to a device that
// has no working firmware, because the boot ROM's download command takes one
// section at a time and has to be told where each goes.
//
// So this file exists for exactly one caller — the recovery path — and it is
// pure so that caller's hardest part can be tested without a device. The
// format is AN76405 section 4.4, and fx3/mkimage/README.md is the project's
// own description of it; this is the reader for what that tool writes.

// One run of bytes and where the device is to put it.
//
// The bytes are described rather than copied: `offset` and `length` index the
// image the section was parsed from, which stays alive for as long as the
// parse result is used. A firmware image is a hundred kilobytes and the
// recovery path streams it straight back out again, so copying it a second
// time would buy nothing.
struct BootImageSection {
  uint32_t address = 0;
  size_t offset = 0;

  // Always a whole number of 32-bit words, because the format counts them.
  size_t length = 0;
};

// A parsed image.
struct BootImage {
  std::vector<BootImageSection> sections;

  // Where execution starts, from the termination record. Sent as a download
  // with no data stage, which is what makes the device run what it has just
  // been given.
  uint32_t entry_address = 0;

  // Every section's payload added up. The progress denominator, and the one
  // figure a caller wants that is not derivable in one line.
  size_t payload_bytes = 0;
};

// Parse an image and check everything about it that can be checked without a
// device.
//
// Returns nothing, and sets `problem` to a sentence, for anything that is not
// a well-formed executable FX3 image: a missing signature, an image type the
// boot ROM would refuse, a section running past the end of the file, or a
// checksum that does not match the payload. The checksum is verified here
// even though the bundle's SHA-256 has already vouched for these bytes,
// because the two checks answer different questions — the digest says the
// file arrived intact, and this says the file is an image at all.
//
// `problem` may be null.
std::optional<BootImage> ParseBootImage(std::span<const uint8_t> image,
                                        std::string* problem);

}  // namespace ddd::capture
