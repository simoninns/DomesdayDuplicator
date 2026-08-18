/************************************************************************

    boot_image.cpp

    Reading the FX3 boot image the boot ROM expects
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "boot_image.h"

namespace ddd::capture {
namespace {

// The four-byte header: two signature bytes, the control byte and the type.
constexpr size_t kHeaderBytes = 4;

// A section record: length in 32-bit words, then load address.
constexpr size_t kSectionHeaderBytes = 8;

// The termination record is a section header whose length is zero, and the
// checksum follows it.
constexpr size_t kChecksumBytes = 4;

constexpr uint8_t kSignatureFirst = 'C';
constexpr uint8_t kSignatureSecond = 'Y';

// Bit 0 of the control byte is set for an image that is *not* executable
// code. The boot ROM will not run one, so neither will this.
constexpr uint8_t kControlNotExecutable = 0x01;

// The only image type this project produces or accepts: normal firmware with
// a checksum.
constexpr uint8_t kImageTypeNormalFirmware = 0xB0;

// A bound on how many sections a file may declare, so that a corrupt length
// field cannot have this loop for a very long time before failing. A firmware
// image is a handful of segments split at 64 KiB; a thousand is far beyond
// anything real and far below anything expensive.
constexpr size_t kMaximumSections = 1024;

uint32_t Read32(std::span<const uint8_t> image, size_t offset) {
  return static_cast<uint32_t>(image[offset]) |
         (static_cast<uint32_t>(image[offset + 1]) << 8) |
         (static_cast<uint32_t>(image[offset + 2]) << 16) |
         (static_cast<uint32_t>(image[offset + 3]) << 24);
}

bool Fail(std::string* problem, std::string text) {
  if (problem != nullptr) {
    *problem = std::move(text);
  }
  return false;
}

}  // namespace

std::optional<BootImage> ParseBootImage(std::span<const uint8_t> image,
                                        std::string* problem) {
  if (problem != nullptr) {
    problem->clear();
  }

  // The smallest legal image is a header, a termination record and a
  // checksum. It would load nothing, but it would parse, and rejecting it
  // here rather than at the first read keeps every read below in bounds.
  if (image.size() < kHeaderBytes + kSectionHeaderBytes + kChecksumBytes) {
    Fail(problem, "That file is too short to be firmware for this device.");
    return std::nullopt;
  }

  if (image[0] != kSignatureFirst || image[1] != kSignatureSecond) {
    Fail(problem, "That file is not firmware for this device.");
    return std::nullopt;
  }

  if ((image[2] & kControlNotExecutable) != 0) {
    Fail(problem,
         "That firmware image does not contain a program the device "
         "can run.");
    return std::nullopt;
  }

  if (image[3] != kImageTypeNormalFirmware) {
    Fail(problem, "That firmware image is of a kind this device cannot boot.");
    return std::nullopt;
  }

  BootImage parsed;
  uint32_t checksum = 0;
  size_t position = kHeaderBytes;

  for (;;) {
    if (image.size() - position < kSectionHeaderBytes) {
      Fail(problem,
           "That firmware image ends part way through. It may not have "
           "downloaded completely.");
      return std::nullopt;
    }

    const uint32_t words = Read32(image, position);
    const uint32_t address = Read32(image, position + 4);
    position += kSectionHeaderBytes;

    // A zero-length section is the termination record, and its address field
    // is the program entry point rather than a place to put anything.
    if (words == 0) {
      parsed.entry_address = address;
      break;
    }

    if (parsed.sections.size() >= kMaximumSections) {
      Fail(problem,
           "That firmware image is not laid out the way this device's "
           "firmware is.");
      return std::nullopt;
    }

    // Multiplied in a width that cannot wrap: `words` is a 32-bit field read
    // straight out of the file, and a hostile one times four in 32 bits would
    // produce a small number that then passed the bounds check below.
    const size_t payload = static_cast<size_t>(words) * 4;
    if (payload > image.size() - position) {
      Fail(problem,
           "That firmware image ends part way through. It may not have "
           "downloaded completely.");
      return std::nullopt;
    }

    for (size_t offset = 0; offset < payload; offset += 4) {
      checksum += Read32(image, position + offset);
    }

    BootImageSection section;
    section.address = address;
    section.offset = position;
    section.length = payload;
    parsed.sections.push_back(section);
    parsed.payload_bytes += payload;

    position += payload;
  }

  if (image.size() - position != kChecksumBytes) {
    // Either there is no checksum, or there are bytes after it. Both mean the
    // file is not what fx3-mkimage wrote, and neither is worth guessing at:
    // this image is about to be executed by a device that cannot say no.
    Fail(problem,
         "That firmware image is not laid out the way this device's "
         "firmware is.");
    return std::nullopt;
  }

  if (Read32(image, position) != checksum) {
    Fail(problem,
         "That firmware image failed its own checksum, so it is damaged. "
         "Download it again.");
    return std::nullopt;
  }

  if (parsed.sections.empty()) {
    Fail(problem, "That firmware image contains no program to load.");
    return std::nullopt;
  }

  return parsed;
}

}  // namespace ddd::capture
