/************************************************************************

    test_update_bundle.cpp

    The archive layer, and what it refuses
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "boot_image_fixture.h"
#include "digest.h"
#include "minisign_verify.h"
#include "update_bundle.h"
#include "update_fixtures.h"

namespace ddd::capture {
namespace {

using test::Bytes;
using test::ByteVector;
using test::Checked;

// Write text into a tar header field. The blocks below start zeroed, so a
// field shorter than its width is already NUL-padded and only its characters
// need writing.
void Put(std::vector<uint8_t>& block, size_t offset, std::string_view text) {
  std::copy(text.begin(), text.end(),
            block.begin() + static_cast<ptrdiff_t>(offset));
}

// Recompute a header block's checksum after editing it, so that a test of what
// the reader refuses is testing the rule it means to and not the checksum
// catching the edit first.
void Reseal(std::vector<uint8_t>& block) {
  std::fill_n(block.begin() + 148, 8, static_cast<uint8_t>(' '));

  uint32_t sum = 0;
  for (size_t index = 0; index < 512; ++index) {
    sum += block[index];
  }
  for (size_t index = 6; index > 0; --index) {
    block[147 + index] = static_cast<uint8_t>('0' + (sum & 7));
    sum >>= 3;
  }
  block[154] = '\0';
  block[155] = ' ';
}

MinisignPublicKey DevelopmentKey() {
  const std::optional<MinisignPublicKey> key =
      ParseMinisignPublicKey(test::kDevelopmentPublicKey, nullptr);
  EXPECT_TRUE(key.has_value());
  return key.value_or(MinisignPublicKey{});
}

// The fixture bundle: the signed manifest, its signature and both payloads, in
// the order the format fixes.
std::vector<uint8_t> MakeBundle() {
  UstarWriter writer;
  writer.AddFile(kManifestEntryName, Bytes(test::kManifestJson));
  writer.AddFile(kSignatureEntryName, Bytes(test::kManifestSignature));
  writer.AddFile("firmware.img", Bytes(test::kFirmwarePayload));
  writer.AddFile("gateware-app.rpd", Bytes(test::kGatewarePayload));
  return writer.Finish();
}

// A provisioning set: firmware beside the JTAG vectors, signed by the same key
// and read by the same reader. One format, whatever it carries.
std::vector<uint8_t> MakeProvisioningBundle() {
  const std::vector<uint8_t> firmware = test::MakeBootImage();

  UstarWriter writer;
  writer.AddFile(kManifestEntryName, Bytes(test::kProvisioningManifestJson));
  writer.AddFile(kSignatureEntryName,
                 Bytes(test::kProvisioningManifestSignature));
  writer.AddFile(kFirmwareEntryName, firmware);
  writer.AddFile(kProvisioningEntryName, Bytes(test::kProvisioningPayload));
  writer.AddFile(kFactoryGatewareEntryName,
                 Bytes(test::kFactoryGatewarePayload));
  return writer.Finish();
}

std::string ErrorFrom(const std::vector<uint8_t>& archive) {
  std::string error;
  EXPECT_FALSE(OpenUpdateBundle(archive, DevelopmentKey(), &error).has_value());
  return error;
}

// --- the archive layer -------------------------------------------------

TEST(UstarArchive, RoundTripsEntriesThroughTheWriterAndReader) {
  const std::vector<uint8_t> short_payload = ByteVector("one");

  // 512 bytes exactly and 513 bytes: the two cases where the block padding is
  // most easily got wrong.
  std::vector<uint8_t> exact_block(512);
  std::vector<uint8_t> over_block(513);
  for (size_t index = 0; index < over_block.size(); ++index) {
    over_block[index] = static_cast<uint8_t>(index);
    if (index < exact_block.size()) {
      exact_block[index] = static_cast<uint8_t>(255 - index);
    }
  }

  UstarWriter writer;
  writer.AddFile("short", short_payload);
  writer.AddFile("empty", {});
  writer.AddFile("exact-block", exact_block);
  writer.AddFile("over-block", over_block);

  const std::vector<uint8_t> archive = writer.Finish();
  ASSERT_EQ(archive.size() % 512, 0u);

  std::string error;
  const std::vector<BundleEntry> entries =
      Checked(ReadUstarArchive(archive, &error));
  ASSERT_EQ(entries.size(), 4u) << error;

  EXPECT_EQ(entries[0].name, "short");
  EXPECT_TRUE(std::equal(entries[0].data.begin(), entries[0].data.end(),
                         short_payload.begin(), short_payload.end()));
  EXPECT_EQ(entries[1].name, "empty");
  EXPECT_TRUE(entries[1].data.empty());
  EXPECT_EQ(entries[2].data.size(), exact_block.size());
  EXPECT_TRUE(std::equal(entries[3].data.begin(), entries[3].data.end(),
                         over_block.begin(), over_block.end()));
}

// The writer's output has to be readable by stock tar, and the reader has to
// cope with what stock tar writes. The first half of that is checked by the
// update-bundle flake check, which runs real tar over a real bundle; this is
// the second half — a header written the way GNU tar writes one, with the
// old-style "ustar  " magic and a trailing space after the checksum, neither of
// which this writer produces.
TEST(UstarArchive, ReadsAHeaderInTheOtherConventions) {
  std::vector<uint8_t> archive(size_t{512} * 3, 0);

  Put(archive, 0, "notes.txt");
  Put(archive, 100, "0000644");       // mode
  Put(archive, 108, "0000000");       // uid
  Put(archive, 116, "0000000");       // gid
  Put(archive, 124, "00000000005 ");  // size, space-terminated
  Put(archive, 136, "14746230515 ");  // mtime, space-terminated
  archive[156] = '0';                 // a regular file
  Put(archive, 257, "ustar  ");       // GNU's magic and version
  Put(archive, 265, "root");          // owner name
  Put(archive, 297, "root");          // group name
  Reseal(archive);

  Put(archive, 512, "hello");

  std::string error;
  const std::vector<BundleEntry> entries =
      Checked(ReadUstarArchive(archive, &error));
  ASSERT_EQ(entries.size(), 1u) << error;
  EXPECT_EQ(entries[0].name, "notes.txt");
  EXPECT_EQ(entries[0].data.size(), 5u);
}

TEST(UstarArchive, RefusesAnythingThatIsNotAFlatArchiveOfRegularFiles) {
  UstarWriter writer;
  writer.AddFile("firmware.img", Bytes(test::kFirmwarePayload));
  const std::vector<uint8_t> good = writer.Finish();

  // A directory entry, resealed so the checksum is not what refuses it.
  std::vector<uint8_t> directory = good;
  directory[156] = '5';
  Reseal(directory);
  std::string error;
  EXPECT_FALSE(ReadUstarArchive(directory, &error).has_value());

  // A name with a path in it, likewise.
  std::vector<uint8_t> nested = good;
  Put(nested, 0, "sub/firmware");
  Reseal(nested);
  EXPECT_FALSE(ReadUstarArchive(nested, &error).has_value());

  // A corrupted header, which the checksum catches.
  std::vector<uint8_t> corrupt = good;
  corrupt[10] = 'X';
  EXPECT_FALSE(ReadUstarArchive(corrupt, &error).has_value());

  // A truncated archive: the header claims more data than is there.
  std::vector<uint8_t> truncated = good;
  truncated.resize(512);
  EXPECT_FALSE(ReadUstarArchive(truncated, &error).has_value());

  // Not a whole number of blocks.
  std::vector<uint8_t> ragged = good;
  ragged.pop_back();
  EXPECT_FALSE(ReadUstarArchive(ragged, &error).has_value());

  EXPECT_FALSE(ReadUstarArchive({}, &error).has_value());
}

// Two entries of one name is the oldest trick in the archive-format book:
// whichever the verifier reads, the extractor reads the other.
TEST(UstarArchive, RefusesTwoEntriesOfOneName) {
  UstarWriter writer;
  writer.AddFile("firmware.img", Bytes(test::kFirmwarePayload));
  writer.AddFile("firmware.img", Bytes(test::kGatewarePayload));

  std::string error;
  EXPECT_FALSE(ReadUstarArchive(writer.Finish(), &error).has_value());
}

// --- the bundle ---------------------------------------------------------

TEST(UpdateBundle, OpensASignedBundle) {
  // Named, not a temporary: an opened bundle's payload spans point into the
  // archive rather than copying it, which is the contract update_bundle.h
  // states.
  const std::vector<uint8_t> archive = MakeBundle();

  std::string error;
  const std::optional<UpdateBundle> opened =
      OpenUpdateBundle(archive, DevelopmentKey(), &error);
  ASSERT_TRUE(opened.has_value()) << error;

  const UpdateBundle bundle = Checked(opened);
  EXPECT_EQ(bundle.manifest.version, "1.4.0");
  EXPECT_EQ(bundle.manifest.channel, UpdateChannel::kDevelopment);
  EXPECT_EQ(bundle.trusted_comment, test::kTrustedComment);
  EXPECT_EQ(bundle.firmware.size(), test::kFirmwarePayload.size());
  EXPECT_EQ(bundle.gateware.size(), test::kGatewarePayload.size());
  EXPECT_EQ(Sha256(bundle.firmware), Checked(bundle.manifest.firmware).sha256);
}

TEST(UpdateBundle, OpensASignedProvisioningSet) {
  const std::vector<uint8_t> archive = MakeProvisioningBundle();

  std::string error;
  const std::optional<UpdateBundle> opened =
      OpenUpdateBundle(archive, DevelopmentKey(), &error);
  ASSERT_TRUE(opened.has_value()) << error;

  const UpdateBundle bundle = Checked(opened);
  ASSERT_TRUE(bundle.manifest.provisioning.has_value());
  EXPECT_EQ(bundle.provisioning.size(), test::kProvisioningPayload.size());

  // Checked against the manifest's digest like every other payload, which is
  // the whole reason a new component kind costs nothing in trust: it arrives
  // through the same four checks the format already specifies.
  EXPECT_EQ(Sha256(bundle.provisioning),
            Checked(bundle.manifest.provisioning).sha256);
}

TEST(UpdateBundle, RefusesAProvisioningSetWhoseVectorsHaveBeenChanged) {
  std::string vectors(test::kProvisioningPayload);
  vectors[vectors.size() - 2] = '7';

  const std::vector<uint8_t> firmware = test::MakeBootImage();

  UstarWriter writer;
  writer.AddFile(kManifestEntryName, Bytes(test::kProvisioningManifestJson));
  writer.AddFile(kSignatureEntryName,
                 Bytes(test::kProvisioningManifestSignature));
  writer.AddFile(kFirmwareEntryName, firmware);
  writer.AddFile(kProvisioningEntryName, Bytes(vectors));

  const std::string error = ErrorFrom(writer.Finish());
  EXPECT_NE(error.find("digest"), std::string::npos) << error;
}

TEST(UpdateBundle, RefusesABundleSignedByAKeyItDoesNotHold) {
  const std::optional<MinisignPublicKey> other =
      ParseMinisignPublicKey(test::kOtherPublicKey, nullptr);
  ASSERT_TRUE(other.has_value());

  std::string error;
  EXPECT_FALSE(
      OpenUpdateBundle(MakeBundle(), Checked(other), &error).has_value());
  EXPECT_FALSE(error.empty());
}

// The manifest carries the digests, so tampering with it is caught by the
// signature and tampering with a payload is caught by the digest. Both are
// checked, because a chain that only had one of them would be a chain with a
// link missing.
TEST(UpdateBundle, RefusesATamperedManifest) {
  std::string manifest(test::kManifestJson);
  const size_t position = manifest.find("Test bundle");
  ASSERT_NE(position, std::string::npos);
  manifest[position] = 'B';

  UstarWriter writer;
  writer.AddFile(kManifestEntryName, Bytes(manifest));
  writer.AddFile(kSignatureEntryName, Bytes(test::kManifestSignature));
  writer.AddFile("firmware.img", Bytes(test::kFirmwarePayload));
  writer.AddFile("gateware-app.rpd", Bytes(test::kGatewarePayload));

  EXPECT_NE(ErrorFrom(writer.Finish()).find("not signed"), std::string::npos);
}

TEST(UpdateBundle, RefusesATamperedPayload) {
  std::string firmware(test::kFirmwarePayload);
  firmware[0] = 'f';

  UstarWriter writer;
  writer.AddFile(kManifestEntryName, Bytes(test::kManifestJson));
  writer.AddFile(kSignatureEntryName, Bytes(test::kManifestSignature));
  writer.AddFile("firmware.img", Bytes(firmware));
  writer.AddFile("gateware-app.rpd", Bytes(test::kGatewarePayload));

  EXPECT_NE(ErrorFrom(writer.Finish()).find("digest"), std::string::npos);
}

// A payload of the wrong length is caught before its digest is computed, so a
// truncated download is reported as what it is.
TEST(UpdateBundle, RefusesAPayloadOfTheWrongLength) {
  UstarWriter writer;
  writer.AddFile(kManifestEntryName, Bytes(test::kManifestJson));
  writer.AddFile(kSignatureEntryName, Bytes(test::kManifestSignature));
  writer.AddFile("firmware.img", Bytes(test::kFirmwarePayload.substr(0, 20)));
  writer.AddFile("gateware-app.rpd", Bytes(test::kGatewarePayload));

  EXPECT_NE(ErrorFrom(writer.Finish()).find("length"), std::string::npos);
}

TEST(UpdateBundle, RefusesABundleMissingAPayloadTheManifestNames) {
  UstarWriter writer;
  writer.AddFile(kManifestEntryName, Bytes(test::kManifestJson));
  writer.AddFile(kSignatureEntryName, Bytes(test::kManifestSignature));
  writer.AddFile("firmware.img", Bytes(test::kFirmwarePayload));

  EXPECT_NE(ErrorFrom(writer.Finish()).find("does not contain"),
            std::string::npos);
}

TEST(UpdateBundle, RefusesABundleWithNoSignature) {
  UstarWriter writer;
  writer.AddFile(kManifestEntryName, Bytes(test::kManifestJson));
  writer.AddFile("firmware.img", Bytes(test::kFirmwarePayload));
  writer.AddFile("gateware-app.rpd", Bytes(test::kGatewarePayload));

  EXPECT_NE(ErrorFrom(writer.Finish()).find("no manifest.minisig"),
            std::string::npos);
}

// The manifest must be *first*, not merely present: a reader that searched for
// it would verify one entry while an extractor that took the first match used
// another.
TEST(UpdateBundle, RefusesABundleWhoseManifestIsNotTheFirstEntry) {
  UstarWriter writer;
  writer.AddFile("firmware.img", Bytes(test::kFirmwarePayload));
  writer.AddFile(kManifestEntryName, Bytes(test::kManifestJson));
  writer.AddFile(kSignatureEntryName, Bytes(test::kManifestSignature));
  writer.AddFile("gateware-app.rpd", Bytes(test::kGatewarePayload));

  EXPECT_NE(ErrorFrom(writer.Finish()).find("first entry"), std::string::npos);
}

TEST(UpdateBundle, RefusesSomethingThatIsNotAnArchiveAtAll) {
  EXPECT_FALSE(ErrorFrom(ByteVector("this is not a tar file")).empty());
}

}  // namespace
}  // namespace ddd::capture
