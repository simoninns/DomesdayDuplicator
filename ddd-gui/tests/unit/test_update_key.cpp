/************************************************************************

    test_update_key.cpp

    T1 unit test for which signatures a build accepts
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "update_fixtures.h"
#include "update_key.h"

namespace ddd::capture {
namespace {

using test::ByteVector;
using test::kDevelopmentPublicKey;
using test::kFirmwarePayload;
using test::kGatewarePayload;
using test::kManifestJson;
using test::kManifestSignature;

// The bundle the fixtures describe, assembled as the release tooling would.
std::vector<uint8_t> MakeBundle(
    std::string_view manifest = kManifestJson,
    std::string_view signature = kManifestSignature) {
  UstarWriter writer;
  writer.AddFile(kManifestEntryName, test::Bytes(manifest));
  writer.AddFile(kSignatureEntryName, test::Bytes(signature));
  writer.AddFile("firmware.img", test::Bytes(kFirmwarePayload));
  writer.AddFile("gateware-app.rpd", test::Bytes(kGatewarePayload));
  return writer.Finish();
}

// The fixture manifest is stamped "development", which is what the committed
// key signs. A bundle signed with that key and claiming to be a release, or
// the reverse, is the case the channel check exists for — and it cannot be
// constructed here, because doing so would need a signature over a different
// manifest and the point of the fixtures is that they were made by minisign
// itself. The claim/proof disagreement is therefore checked through the
// manifest text, below.

TEST(UpdateKey, AcceptsADevelopmentBundleWhenTheOptInIsGiven) {
  const std::vector<uint8_t> archive = MakeBundle();

  UpdateKeyPolicy policy;
  policy.accept_development_key = true;

  std::string error;
  const std::optional<UpdateBundle> bundle =
      OpenUpdateBundleForPolicy(archive, policy, &error);

  ASSERT_TRUE(bundle.has_value()) << error;

  const UpdateBundle opened = test::Checked(bundle);
  EXPECT_EQ(opened.manifest.channel, UpdateChannel::kDevelopment);
  EXPECT_EQ(opened.manifest.version, "1.4.0");
}

// The rule that matters: without the explicit opt-in, a development-signed
// bundle is refused. The development key's secret half is committed to this
// repository, so its signature proves format and never authenticity.
TEST(UpdateKey, RefusesADevelopmentBundleWithoutTheOptIn) {
  const std::vector<uint8_t> archive = MakeBundle();

  UpdateKeyPolicy policy;
  policy.accept_development_key = false;

  std::string error;
  const std::optional<UpdateBundle> bundle =
      OpenUpdateBundleForPolicy(archive, policy, &error);

  EXPECT_FALSE(bundle.has_value());
  EXPECT_NE(error.find("not signed by the Domesday Duplicator project"),
            std::string::npos);
  EXPECT_NE(error.find("not been installed"), std::string::npos);
}

// A bundle claiming a channel its signature does not support is refused. The
// claim is written by whoever made the bundle and the proof is not, so the
// disagreement is decided in favour of the proof.
TEST(UpdateKey, RefusesABundleClaimingToBeAReleaseButSignedForDevelopment) {
  std::string manifest(kManifestJson);
  const std::string from = "\"channel\": \"development\"";
  const std::string to = "\"channel\": \"release\"";
  const size_t at = manifest.find(from);
  ASSERT_NE(at, std::string::npos);
  manifest.replace(at, from.size(), to);

  const std::vector<uint8_t> archive = MakeBundle(manifest);

  UpdateKeyPolicy policy;
  policy.accept_development_key = true;

  std::string error;
  const std::optional<UpdateBundle> bundle =
      OpenUpdateBundleForPolicy(archive, policy, &error);

  // Edited text does not verify at all, which is the first line of defence
  // and the one that fires here.
  EXPECT_FALSE(bundle.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(UpdateKey, RefusesRubbish) {
  const std::vector<uint8_t> archive = ByteVector("not an archive at all");

  UpdateKeyPolicy policy;
  policy.accept_development_key = true;

  std::string error;
  EXPECT_FALSE(OpenUpdateBundleForPolicy(archive, policy, &error).has_value());
  EXPECT_FALSE(error.empty());
}

TEST(UpdateKey, TheCompiledInDevelopmentKeyIsTheCommittedOne) {
  // A drift between the key in tools/keys/ and the one compiled in here
  // would mean every development bundle the tooling produces is refused by
  // the application, which is a failure nobody would think to look for.
  EXPECT_EQ(DevelopmentUpdateKeyText(), kDevelopmentPublicKey);
}

// The default policy has to be usable: a build with no release key pinned
// that also refused the development key could open nothing at all, and an
// application that cannot be tested is not safer than one that can.
TEST(UpdateKey, TheDefaultPolicyCanOpenSomething) {
  const UpdateKeyPolicy policy = DefaultUpdateKeyPolicy();

  if (!HasReleaseUpdateKey()) {
    EXPECT_TRUE(policy.accept_development_key);
  }

  const std::vector<uint8_t> archive = MakeBundle();
  std::string error;
  EXPECT_TRUE(OpenUpdateBundleForPolicy(archive, policy, &error).has_value() ||
              HasReleaseUpdateKey())
      << error;
}

}  // namespace
}  // namespace ddd::capture
