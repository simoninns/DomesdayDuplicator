/************************************************************************

    test_stock_tar_bundle.cpp

    Reading a bundle the release tooling really produced
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "minisign_verify.h"
#include "stock_tar_bundle.h"
#include "update_bundle.h"
#include "update_fixtures.h"

namespace ddd::capture {
namespace {

using test::Checked;

MinisignPublicKey DevelopmentKey() {
  return Checked(ParseMinisignPublicKey(test::kDevelopmentPublicKey, nullptr));
}

// T2. The fixture is a real artefact — GNU tar's bytes, minisign's signature —
// so this is a comparison against something outside the project rather than a
// behavioural check against the project's own writer, which is what makes it a
// golden test rather than a unit one.

TEST(StockTarBundle, OpensABundleTheReleaseToolingProduced) {
  // The archive outlives the bundle deliberately: an opened bundle's payload
  // spans point into it rather than copying megabytes, which is the contract
  // update_bundle.h states.
  const std::vector<uint8_t> archive = test::StockTarBundle(1024);

  std::string error;
  const std::optional<UpdateBundle> opened =
      OpenUpdateBundle(archive, DevelopmentKey(), &error);
  ASSERT_TRUE(opened.has_value()) << error;

  const UpdateBundle bundle = Checked(opened);
  EXPECT_EQ(bundle.manifest.version, "1.4.0");
  EXPECT_EQ(bundle.manifest.commit, "0123abcd");
  EXPECT_EQ(bundle.manifest.channel, UpdateChannel::kDevelopment);
  EXPECT_EQ(bundle.trusted_comment, test::kTrustedComment);

  ASSERT_TRUE(bundle.manifest.firmware.has_value());
  EXPECT_EQ(Checked(bundle.manifest.firmware).file, "firmware.img");
  EXPECT_EQ(bundle.firmware.size(), test::kFirmwarePayload.size());
  EXPECT_TRUE(std::equal(bundle.firmware.begin(), bundle.firmware.end(),
                         test::kFirmwarePayload.begin(),
                         test::kFirmwarePayload.end()));

  ASSERT_TRUE(bundle.manifest.gateware.has_value());
  EXPECT_EQ(bundle.gateware.size(), test::kGatewarePayload.size());
}

// GNU tar pads to a 10,240-byte record by default, so the released file has
// thousands of zero bytes after the last payload. The reader stops at the first
// zero block, and this says so with the real padding rather than by inspection.
TEST(StockTarBundle, IgnoresTarsTrailingPadding) {
  const size_t content = test::StockTarBundle(0).size();
  ASSERT_LT(content, test::kStockTarBundleRecordBytes);

  const std::vector<uint8_t> archive =
      test::StockTarBundle(test::kStockTarBundleRecordBytes - content);
  ASSERT_EQ(archive.size(), test::kStockTarBundleRecordBytes);

  std::string error;
  EXPECT_TRUE(OpenUpdateBundle(archive, DevelopmentKey(), &error).has_value())
      << error;
}

// The archive this project's own writer produces and the one GNU tar produced
// carry the same entries with the same content, which is the property that lets
// the release be assembled by a shell script and read by the application.
TEST(StockTarBundle, AgreesWithThisProjectsOwnWriter) {
  UstarWriter writer;
  writer.AddFile(kManifestEntryName, test::Bytes(test::kManifestJson));
  writer.AddFile(kSignatureEntryName, test::Bytes(test::kManifestSignature));
  writer.AddFile("firmware.img", test::Bytes(test::kFirmwarePayload));
  writer.AddFile("gateware-app.rpd", test::Bytes(test::kGatewarePayload));

  const std::vector<uint8_t> our_archive = writer.Finish();
  const std::vector<uint8_t> their_archive = test::StockTarBundle(1024);

  std::string error;
  const std::vector<BundleEntry> ours =
      Checked(ReadUstarArchive(our_archive, &error));
  const std::vector<BundleEntry> theirs =
      Checked(ReadUstarArchive(their_archive, &error));

  ASSERT_EQ(ours.size(), theirs.size()) << error;
  for (size_t index = 0; index < ours.size(); ++index) {
    EXPECT_EQ(ours[index].name, theirs[index].name);
    EXPECT_TRUE(std::equal(ours[index].data.begin(), ours[index].data.end(),
                           theirs[index].data.begin(),
                           theirs[index].data.end()))
        << ours[index].name;
  }
}

}  // namespace
}  // namespace ddd::capture
