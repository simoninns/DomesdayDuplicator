/************************************************************************

    test_minisign_verify.cpp

    Verifying signatures a different program produced
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>

#include "minisign_verify.h"
#include "update_fixtures.h"

namespace ddd::capture {
namespace {

using test::Bytes;
using test::Checked;

MinisignPublicKey DevelopmentKey() {
  std::string error;
  const std::optional<MinisignPublicKey> key =
      ParseMinisignPublicKey(test::kDevelopmentPublicKey, &error);
  EXPECT_TRUE(key.has_value()) << error;
  return key.value_or(MinisignPublicKey{});
}

MinisignSignature Signature(std::string_view text) {
  std::string error;
  const std::optional<MinisignSignature> signature =
      ParseMinisignSignature(text, &error);
  EXPECT_TRUE(signature.has_value()) << error;
  return signature.value_or(MinisignSignature{});
}

// Everything below is checked against signatures minisign 0.12 produced, not
// against signatures this code produced. A verifier tested only against its own
// output has proved that it agrees with itself.

TEST(MinisignVerify, VerifiesASignatureMinisignMade) {
  std::string error;
  EXPECT_TRUE(VerifyMinisign(Bytes(test::kManifestJson),
                             Signature(test::kManifestSignature),
                             DevelopmentKey(), &error))
      << error;
}

TEST(MinisignVerify, VerifiesThePrehashedForm) {
  std::string error;
  EXPECT_TRUE(VerifyMinisign(Bytes(test::kManifestJson),
                             Signature(test::kManifestSignaturePrehashed),
                             DevelopmentKey(), &error))
      << error;
}

TEST(MinisignVerify, ExposesTheTrustedComment) {
  EXPECT_EQ(Signature(test::kManifestSignature).trusted_comment,
            test::kTrustedComment);
}

TEST(MinisignVerify, RefusesAManifestWithOneByteChanged) {
  std::string tampered(test::kManifestJson);
  const size_t position = tampered.find("1.4.0");
  ASSERT_NE(position, std::string::npos);
  tampered[position + 2] = '5';

  std::string error;
  EXPECT_FALSE(VerifyMinisign(Bytes(tampered),
                              Signature(test::kManifestSignature),
                              DevelopmentKey(), &error));
  EXPECT_FALSE(error.empty());
}

// The trusted comment is the one part of a signature file a caller may show to
// a user, and it is only worth that because the second signature covers it.
TEST(MinisignVerify, RefusesAnEditedTrustedComment) {
  std::string edited(test::kManifestSignature);
  const size_t position = edited.find("channel development");
  ASSERT_NE(position, std::string::npos);
  edited.replace(position, std::string_view("channel development").size(),
                 "channel release....");

  std::string error;
  EXPECT_FALSE(VerifyMinisign(Bytes(test::kManifestJson), Signature(edited),
                              DevelopmentKey(), &error));
  EXPECT_FALSE(error.empty());
}

TEST(MinisignVerify, RefusesASignatureFromAnotherKey) {
  std::string error;
  const std::optional<MinisignPublicKey> other =
      ParseMinisignPublicKey(test::kOtherPublicKey, &error);
  ASSERT_TRUE(other.has_value()) << error;

  EXPECT_FALSE(VerifyMinisign(Bytes(test::kManifestJson),
                              Signature(test::kManifestSignature),
                              Checked(other), &error));
  EXPECT_FALSE(error.empty());
}

TEST(MinisignVerify, RefusesAMalformedPublicKey) {
  std::string error;
  EXPECT_FALSE(ParseMinisignPublicKey("", &error).has_value());
  EXPECT_FALSE(
      ParseMinisignPublicKey("RWR82Ay8IQPniaM+g2JAeVDIBxTGinceXiVzrjUfHL9Ki3MT"
                             "2lj7S3QM\n",
                             &error)
          .has_value());
  EXPECT_FALSE(ParseMinisignPublicKey(
                   "untrusted comment: x\nnot base64 at all!\n", &error)
                   .has_value());
  EXPECT_FALSE(
      ParseMinisignPublicKey("untrusted comment: x\nRWR82Ay8IQPniaM=\n", &error)
          .has_value());
}

TEST(MinisignVerify, RefusesAMalformedSignature) {
  std::string error;
  EXPECT_FALSE(ParseMinisignSignature("", &error).has_value());

  // Three lines: the trusted comment and its signature are not optional.
  std::string truncated(test::kManifestSignature);
  truncated.resize(truncated.rfind('\n', truncated.size() - 2) + 1);
  EXPECT_FALSE(ParseMinisignSignature(truncated, &error).has_value());

  // An unknown algorithm tag. "Rd" is not "Ed" or "ED", and the two-character
  // tag sits at the head of the base64 payload.
  std::string wrong_algorithm(test::kManifestSignature);
  const size_t line = wrong_algorithm.find('\n') + 1;
  wrong_algorithm[line] = 'S';
  EXPECT_FALSE(ParseMinisignSignature(wrong_algorithm, &error).has_value());
}

// A file that went through an editor on Windows still verifies: the line
// endings are not part of what was signed.
TEST(MinisignVerify, ToleratesCarriageReturns) {
  std::string with_crlf;
  for (char character : test::kManifestSignature) {
    if (character == '\n') {
      with_crlf.push_back('\r');
    }
    with_crlf.push_back(character);
  }

  std::string error;
  EXPECT_TRUE(VerifyMinisign(Bytes(test::kManifestJson), Signature(with_crlf),
                             DevelopmentKey(), &error))
      << error;
}

}  // namespace
}  // namespace ddd::capture
