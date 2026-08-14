/************************************************************************

    boot_image_fixture.h

    Building FX3 boot images for the tests that read them
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ddd::capture::test {

// A writer for the format boot_image.h reads, used to make the inputs the
// parser is tested against.
//
// Deliberately a second implementation rather than a call into anything the
// parser shares: a reader tested only against a writer that shares its
// assumptions has proved the two agree, not that either is right. This one is
// written from the same public description the parser was — AN76405 section
// 4.4, and fx3/mkimage/README.md's table of it — and its output is checked
// against the worked example the application note itself publishes.
class BootImageBuilder {
 public:
  // One section: bytes, and where they load. `bytes` must be a whole number
  // of 32-bit words, because the format counts them.
  void AddSection(uint32_t address, const std::vector<uint8_t>& bytes) {
    sections_.push_back({address, bytes});
  }

  void SetEntryAddress(uint32_t address) { entry_ = address; }

  // The control byte, so a test can build the non-executable case.
  void SetImageControl(uint8_t control) { control_ = control; }

  // The type byte, so a test can build an image the boot ROM would refuse.
  void SetImageType(uint8_t type) { type_ = type; }

  // Break the checksum, so a test can build a damaged image.
  void SetChecksumOverride(uint32_t checksum) { checksum_override_ = checksum; }

  std::vector<uint8_t> Build() const {
    std::vector<uint8_t> image{'C', 'Y', control_, type_};
    uint32_t checksum = 0;

    for (const Section& section : sections_) {
      Append32(image, static_cast<uint32_t>(section.bytes.size() / 4));
      Append32(image, section.address);
      image.insert(image.end(), section.bytes.begin(), section.bytes.end());

      for (size_t offset = 0; offset + 3 < section.bytes.size(); offset += 4) {
        checksum += static_cast<uint32_t>(section.bytes[offset]) |
                    (static_cast<uint32_t>(section.bytes[offset + 1]) << 8) |
                    (static_cast<uint32_t>(section.bytes[offset + 2]) << 16) |
                    (static_cast<uint32_t>(section.bytes[offset + 3]) << 24);
      }
    }

    // The termination record carries the entry point in its address field.
    Append32(image, 0);
    Append32(image, entry_);
    Append32(image,
             checksum_override_.has_value() ? *checksum_override_ : checksum);
    return image;
  }

 private:
  struct Section {
    uint32_t address = 0;
    std::vector<uint8_t> bytes;
  };

  static void Append32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  }

  std::vector<Section> sections_;
  uint32_t entry_ = 0x40078000;
  uint8_t control_ = 0x1C;
  uint8_t type_ = 0xB0;
  std::optional<uint32_t> checksum_override_;
};

// A plausible small image: two sections at the addresses the FX3's code and
// data actually live at, and an entry point in the first of them.
inline std::vector<uint8_t> MakeBootImage() {
  BootImageBuilder builder;
  builder.AddSection(0x40003000, std::vector<uint8_t>(256, 0xA5));
  builder.AddSection(0x40008000, std::vector<uint8_t>(64, 0x5A));
  builder.SetEntryAddress(0x40003000);
  return builder.Build();
}

}  // namespace ddd::capture::test
