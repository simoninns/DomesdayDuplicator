/************************************************************************

    test_boot_image.cpp

    Reading the FX3 boot image the boot ROM expects
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "boot_image.h"
#include "boot_image_fixture.h"
#include "update_fixtures.h"

namespace ddd::capture {
namespace {

using test::BootImageBuilder;

// This parser decides what bytes get executed by a device that cannot say no.
// Every malformed case below is therefore a refusal rather than a
// best-effort read, and each one is checked for producing a sentence as well
// as for producing nothing — a refusal a user cannot act on is half a
// refusal.

TEST(BootImageTest, AWellFormedImageParsesIntoItsSections) {
  BootImageBuilder builder;
  builder.AddSection(0x40003000, std::vector<uint8_t>(256, 0xA5));
  builder.AddSection(0x40008000, std::vector<uint8_t>(64, 0x5A));
  builder.SetEntryAddress(0x40003004);

  const std::vector<uint8_t> image = builder.Build();

  std::string problem = "not cleared";
  const std::optional<BootImage> parsed = ParseBootImage(image, &problem);

  ASSERT_TRUE(parsed.has_value()) << problem;
  EXPECT_TRUE(problem.empty());

  const BootImage image_sections = test::Checked(parsed);
  ASSERT_EQ(image_sections.sections.size(), 2u);
  EXPECT_EQ(image_sections.sections[0].address, 0x40003000u);
  EXPECT_EQ(image_sections.sections[0].length, 256u);
  EXPECT_EQ(image_sections.sections[1].address, 0x40008000u);
  EXPECT_EQ(image_sections.sections[1].length, 64u);

  EXPECT_EQ(image_sections.entry_address, 0x40003004u);
  EXPECT_EQ(image_sections.payload_bytes, 320u);
}

// The sections describe the caller's buffer rather than copying it, so the
// offsets have to land on the right bytes and not merely on the right
// lengths.
TEST(BootImageTest, SectionOffsetsPointAtTheRightBytes) {
  BootImageBuilder builder;
  builder.AddSection(0x40003000, std::vector<uint8_t>(8, 0x11));
  builder.AddSection(0x40004000, std::vector<uint8_t>(8, 0x22));

  const std::vector<uint8_t> image = builder.Build();
  const std::optional<BootImage> parsed = ParseBootImage(image, nullptr);
  ASSERT_TRUE(parsed.has_value());

  const BootImage sections = test::Checked(parsed);
  for (size_t index = 0; index < 8; ++index) {
    EXPECT_EQ(image[sections.sections[0].offset + index], 0x11);
    EXPECT_EQ(image[sections.sections[1].offset + index], 0x22);
  }
}

TEST(BootImageTest, AFileWithoutTheSignatureIsRefused) {
  std::vector<uint8_t> image = test::MakeBootImage();
  image[0] = 'X';

  std::string problem;
  EXPECT_FALSE(ParseBootImage(image, &problem).has_value());
  EXPECT_FALSE(problem.empty());
}

TEST(BootImageTest, AnImageThatIsNotExecutableCodeIsRefused) {
  BootImageBuilder builder;
  builder.AddSection(0x40003000, std::vector<uint8_t>(16, 0x00));
  // Bit 0 set means the image does not contain executable code.
  builder.SetImageControl(0x1D);

  std::string problem;
  EXPECT_FALSE(ParseBootImage(builder.Build(), &problem).has_value());
  EXPECT_FALSE(problem.empty());
}

TEST(BootImageTest, AnImageTypeTheBootRomWouldRefuseIsRefusedHere) {
  BootImageBuilder builder;
  builder.AddSection(0x40003000, std::vector<uint8_t>(16, 0x00));
  builder.SetImageType(0xB2);

  std::string problem;
  EXPECT_FALSE(ParseBootImage(builder.Build(), &problem).has_value());
  EXPECT_FALSE(problem.empty());
}

// The bundle's SHA-256 has already vouched for these bytes by the time this
// runs, and the checksum is verified anyway: the digest says the file arrived
// intact and this says the file is an image at all. A bundle assembled with
// the wrong payload passes the first and fails this.
TEST(BootImageTest, AnImageWhoseChecksumDoesNotMatchIsRefused) {
  BootImageBuilder builder;
  builder.AddSection(0x40003000, std::vector<uint8_t>(64, 0xFF));
  builder.SetChecksumOverride(0);

  std::string problem;
  EXPECT_FALSE(ParseBootImage(builder.Build(), &problem).has_value());
  EXPECT_NE(problem.find("damaged"), std::string::npos)
      << "a damaged image should say so: " << problem;
}

TEST(BootImageTest, ATruncatedImageIsRefused) {
  const std::vector<uint8_t> whole = test::MakeBootImage();

  // Cut it a section header short of the end, so the walk runs out of buffer
  // rather than reading a wrong-but-present length.
  const std::vector<uint8_t> cut(whole.begin(), whole.end() - 20);

  std::string problem;
  EXPECT_FALSE(ParseBootImage(cut, &problem).has_value());
  EXPECT_FALSE(problem.empty());
}

TEST(BootImageTest, AnEmptyFileIsRefused) {
  std::string problem;
  EXPECT_FALSE(ParseBootImage({}, &problem).has_value());
  EXPECT_FALSE(problem.empty());
}

// Trailing bytes are refused rather than ignored. The image is about to be
// executed by a device that cannot check it, so a file that is not exactly
// what fx3-mkimage wrote is not guessed at.
TEST(BootImageTest, BytesAfterTheChecksumAreRefused) {
  std::vector<uint8_t> image = test::MakeBootImage();
  image.push_back(0x00);

  std::string problem;
  EXPECT_FALSE(ParseBootImage(image, &problem).has_value());
  EXPECT_FALSE(problem.empty());
}

TEST(BootImageTest, AnImageWithNoSectionsIsRefused) {
  BootImageBuilder builder;
  builder.SetEntryAddress(0x40003000);

  std::string problem;
  EXPECT_FALSE(ParseBootImage(builder.Build(), &problem).has_value());
  EXPECT_FALSE(problem.empty());
}

// A length field is multiplied by four to get a byte count, and the
// multiplication is done in a width that cannot wrap. Without that, a hostile
// 0x40000000 becomes zero and the section is accepted as empty.
TEST(BootImageTest, ASectionLengthThatWouldWrapIsRefused) {
  std::vector<uint8_t> image{'C', 'Y', 0x1C, 0xB0};

  // dLength = 0x40000000 words, which is 2^32 bytes.
  image.insert(image.end(), {0x00, 0x00, 0x00, 0x40});
  // dAddress
  image.insert(image.end(), {0x00, 0x30, 0x00, 0x40});
  // Nothing like enough payload to follow it.
  image.insert(image.end(), 16, 0x00);

  std::string problem;
  EXPECT_FALSE(ParseBootImage(image, &problem).has_value());
  EXPECT_FALSE(problem.empty());
}

}  // namespace
}  // namespace ddd::capture
