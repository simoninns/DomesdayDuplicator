/************************************************************************

    update_fixtures.h

    A real signed bundle, for the update tooling tests
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// Fixtures for the update bundle tests.
//
// The manifest below and its two signatures were produced by minisign 0.12
// using tools/keys/development.key, and every one of them verifies with a stock
// `minisign -V`. That is the point of holding them here as literals rather than
// signing something inside the test: a verifier checked only against signatures
// it produced itself has proved that it agrees with itself. These were produced
// by the same program the release pipeline runs, so they check the thing that
// actually matters — that this code and minisign mean the same thing by a
// signature.
//
// Regenerating them, if the fixture ever needs to change:
//
//   minisign -Sm manifest.json -s tools/keys/development.key
//            -x manifest.minisig -t "<trusted comment>"
//
// and for the prehashed form, the same with -SH. Both forms are kept because
// minisign produces either depending on its -H flag, and a verifier that only
// understood one would fail on a bundle signed by a maintainer who typed the
// other.

namespace ddd::capture::test {

// The payloads the manifest describes. Text rather than plausible binary: a
// test that fails should say which byte differs, and these show up in a
// diff.
inline constexpr std::string_view kFirmwarePayload =
    "FX3 firmware payload, for the tests only.\n";
inline constexpr std::string_view kGatewarePayload =
    "FPGA application gateware payload, for the tests only.\n";

// The manifest, byte for byte as it was signed. Any change to this text —
// including a change to its whitespace — invalidates both signatures below.
inline constexpr std::string_view kManifestJson =
    R"({
  "manifest_version": 1,
  "channel": "development",
  "version": "1.4.0",
  "commit": "0123abcd",
  "created": "2026-08-14T09:15:00Z",
  "release_notes": "Test bundle for the update tooling unit tests.",
  "components": {
    "firmware": {
      "file": "firmware.img",
      "length": 42,
      "sha256": "c8422f817b69fa687531b26f4f190b2c4a97fe7b6791850ff76dc10a2d52b2d9",
      "identity": "0123abcd",
      "interface_version": 1
    },
    "gateware": {
      "file": "gateware-app.rpd",
      "length": 55,
      "sha256": "73af12c40cb9f02c03d84162217a841544fb5cb7d2dda9fdd00fff8e9d117c2a",
      "identity": "0123abcd",
      "interface_version": 2
    }
  },
  "compatibility": {
    "minimum_application_version": "1.4.0",
    "minimum_register_map_version": 2,
    "epcs_layout_version": 1
  }
}
)";

// tools/keys/development.pub, verbatim.
inline constexpr std::string_view kDevelopmentPublicKey =
    "untrusted comment: Domesday Duplicator development signing key — public "
    "half. Proves format, never authenticity.\n"
    "RWR82Ay8IQPniaM+g2JAeVDIBxTGinceXiVzrjUfHL9Ki3MT2lj7S3QM\n";

// A different key, for the "signed by somebody else" case. Generated the same
// way and used for nothing else.
inline constexpr std::string_view kOtherPublicKey =
    "untrusted comment: a key this project does not know\n"
    "RWRnCgNtmnHT20UqS1DlUhWPYmbIHI1smBK4JQ+6oLaVrR2KZHz7aPqt\n";

// The signature over kManifestJson, in minisign's default mode: Ed25519 over
// the message itself.
inline constexpr std::string_view kManifestSignature =
    "untrusted comment: Domesday Duplicator update bundle 1.4.0\n"
    "RUR82Ay8IQPniVVScfppj5cEXlQZPJkBH75pPcjgq7gPM4P9323IUetg0V2EYmqm9w7ZbXRM5"
    "Eu+YIN5dLijpOCb2DZwchepJgw=\n"
    "trusted comment: domesday-duplicator-update-1.4.0.dddfw version 1.4.0 "
    "channel development\n"
    "PBZV5xkDITJX97EylwqtedT+z3Xu+FFJtXJXq1dDAKgCYLnv/ggdodvUWyCe8rcAFyPyvGUAe"
    "Pe737G+dQ/JBg==\n";

// The same manifest signed with minisign -H: Ed25519 over a BLAKE2b-512 hash
// of the message.
inline constexpr std::string_view kManifestSignaturePrehashed =
    "untrusted comment: prehashed signature from the development key\n"
    "RUR82Ay8IQPniVVScfppj5cEXlQZPJkBH75pPcjgq7gPM4P9323IUetg0V2EYmqm9w7ZbXRM5"
    "Eu+YIN5dLijpOCb2DZwchepJgw=\n"
    "trusted comment: prehashed form of the same manifest\n"
    "6LcEyrc5MNcobWqft9tCiqH8+Twui1McUjQRs/LJm4JLaLTQj8o5mEK1C1X5AtcfD2WN/bU6Y"
    "lUfoU3ncFgLBQ==\n";

// The trusted comment the release-mode signature carries, which a caller is
// entitled to show a user because the second signature covers it.
inline constexpr std::string_view kTrustedComment =
    "domesday-duplicator-update-1.4.0.dddfw version 1.4.0 channel development";

// The static analysis cannot follow gtest's ASSERT_TRUE into the assertion it
// makes, so a dereference after one still reads to it as unchecked. Taking the
// value with value_or keeps the assertion above meaningful and the analysis
// satisfied without scattering suppressions through these files — the same
// device tests/analysis/test_amplitude_history.cpp uses, for the same reason.
template <typename T>
T Checked(const std::optional<T>& value) {
  return value.value_or(T{});
}

inline std::span<const uint8_t> Bytes(std::string_view text) {
  return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

inline std::vector<uint8_t> ByteVector(std::string_view text) {
  const std::span<const uint8_t> bytes = Bytes(text);
  return {bytes.begin(), bytes.end()};
}

}  // namespace ddd::capture::test
