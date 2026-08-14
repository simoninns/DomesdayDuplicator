/************************************************************************

    minisign_verify.cpp

    Checking that a bundle came from the project and not from somebody else
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "minisign_verify.h"

#include <algorithm>
#include <cstddef>
#include <vector>

extern "C" {
#include "monocypher-ed25519.h"
#include "monocypher.h"
}

namespace ddd::capture {
namespace {

constexpr std::string_view kUntrustedCommentPrefix = "untrusted comment: ";
constexpr std::string_view kTrustedCommentPrefix = "trusted comment: ";

// Decoded sizes. A public key line is algorithm, key id and key; a signature
// line is algorithm, key id and signature.
constexpr size_t kPublicKeyBytes = 2 + 8 + 32;
constexpr size_t kSignatureBytes = 2 + 8 + 64;
constexpr size_t kGlobalSignatureBytes = 64;

bool Fail(std::string* error, std::string message) {
  if (error != nullptr) {
    *error = std::move(message);
  }
  return false;
}

// Standard base64 with padding, which is what minisign writes. Rejects
// whitespace inside the data, a wrong length, and any character outside the
// alphabet — a signature line is machine-written and exactly one thing.
std::optional<std::vector<uint8_t>> DecodeBase64(std::string_view text) {
  static constexpr std::string_view kAlphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  if (text.empty() || (text.size() % 4) != 0) {
    return std::nullopt;
  }

  size_t padding = 0;
  while (padding < 2 && text.size() > padding &&
         text[text.size() - 1 - padding] == '=') {
    ++padding;
  }

  std::vector<uint8_t> decoded;
  decoded.reserve((text.size() / 4) * 3);

  uint32_t accumulator = 0;
  int bits = 0;
  for (size_t index = 0; index + padding < text.size(); ++index) {
    const size_t symbol = kAlphabet.find(text[index]);
    if (symbol == std::string_view::npos) {
      return std::nullopt;
    }

    accumulator = (accumulator << 6) | static_cast<uint32_t>(symbol);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      decoded.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xFF));
    }
  }

  // Whatever is left over must be zero: base64 that encodes stray bits in its
  // final symbol has more than one representation, and accepting both would
  // mean two byte strings decoding from one line.
  if (bits >= 6 || (accumulator & ((1U << bits) - 1)) != 0) {
    return std::nullopt;
  }
  return decoded;
}

// Split into lines on '\n', discarding a trailing '\r' so a file that has been
// through a Windows editor still verifies. A trailing empty line is dropped.
std::vector<std::string_view> SplitLines(std::string_view text) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t end = text.find('\n', start);
    std::string_view line = end == std::string_view::npos
                                ? text.substr(start)
                                : text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    lines.push_back(line);
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }

  while (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }
  return lines;
}

std::optional<MinisignAlgorithm> ReadAlgorithm(const uint8_t* bytes) {
  if (bytes[0] == 'E' && bytes[1] == 'd') {
    return MinisignAlgorithm::kLegacy;
  }
  if (bytes[0] == 'E' && bytes[1] == 'D') {
    return MinisignAlgorithm::kPrehashed;
  }
  return std::nullopt;
}

}  // namespace

std::optional<MinisignPublicKey> ParseMinisignPublicKey(std::string_view text,
                                                        std::string* error) {
  const std::vector<std::string_view> lines = SplitLines(text);
  if (lines.size() != 2) {
    Fail(error, "a public key is a comment line and a data line");
    return std::nullopt;
  }
  if (!lines[0].starts_with(kUntrustedCommentPrefix)) {
    Fail(error, "the first line is not an untrusted comment");
    return std::nullopt;
  }

  const std::optional<std::vector<uint8_t>> decoded = DecodeBase64(lines[1]);
  if (!decoded || decoded->size() != kPublicKeyBytes) {
    Fail(error, "the key line is not base64 of the right length");
    return std::nullopt;
  }

  // Only the legacy tag appears in a public key: it names the signature scheme,
  // Ed25519, and not whether a given signature was prehashed.
  if ((*decoded)[0] != 'E' || (*decoded)[1] != 'd') {
    Fail(error, "the key is not an Ed25519 key");
    return std::nullopt;
  }

  MinisignPublicKey key;
  std::copy_n(decoded->begin() + 2, key.key_id.size(), key.key_id.begin());
  std::copy_n(decoded->begin() + 10, key.key.size(), key.key.begin());
  return key;
}

std::optional<MinisignSignature> ParseMinisignSignature(std::string_view text,
                                                        std::string* error) {
  const std::vector<std::string_view> lines = SplitLines(text);
  if (lines.size() != 4) {
    Fail(error,
         "a signature is a comment line, a signature line, a trusted comment "
         "line and a global signature line");
    return std::nullopt;
  }
  if (!lines[0].starts_with(kUntrustedCommentPrefix)) {
    Fail(error, "the first line is not an untrusted comment");
    return std::nullopt;
  }
  if (!lines[2].starts_with(kTrustedCommentPrefix)) {
    Fail(error, "the third line is not a trusted comment");
    return std::nullopt;
  }

  const std::optional<std::vector<uint8_t>> decoded = DecodeBase64(lines[1]);
  if (!decoded || decoded->size() != kSignatureBytes) {
    Fail(error, "the signature line is not base64 of the right length");
    return std::nullopt;
  }

  const std::optional<MinisignAlgorithm> algorithm =
      ReadAlgorithm(decoded->data());
  if (!algorithm) {
    Fail(error, "unknown signature algorithm");
    return std::nullopt;
  }

  const std::optional<std::vector<uint8_t>> global = DecodeBase64(lines[3]);
  if (!global || global->size() != kGlobalSignatureBytes) {
    Fail(error, "the global signature line is not base64 of the right length");
    return std::nullopt;
  }

  MinisignSignature signature;
  signature.algorithm = *algorithm;
  std::copy_n(decoded->begin() + 2, signature.key_id.size(),
              signature.key_id.begin());
  std::copy_n(decoded->begin() + 10, signature.signature.size(),
              signature.signature.begin());
  signature.trusted_comment =
      std::string(lines[2].substr(kTrustedCommentPrefix.size()));
  std::copy_n(global->begin(), signature.global_signature.size(),
              signature.global_signature.begin());
  return signature;
}

bool VerifyMinisign(std::span<const uint8_t> message,
                    const MinisignSignature& signature,
                    const MinisignPublicKey& public_key, std::string* error) {
  if (signature.key_id != public_key.key_id) {
    return Fail(error, "the signature was made by a different key");
  }

  // The message signature. Prehashed mode signs a BLAKE2b-512 of the file
  // rather than the file, so hash first and sign the hash — this is minisign's
  // own scheme and not Ed25519ph, which is a different construction.
  int result = 0;
  if (signature.algorithm == MinisignAlgorithm::kPrehashed) {
    std::array<uint8_t, 64> hash{};
    crypto_blake2b(hash.data(), hash.size(), message.data(), message.size());
    result =
        crypto_ed25519_check(signature.signature.data(), public_key.key.data(),
                             hash.data(), hash.size());
  } else {
    result =
        crypto_ed25519_check(signature.signature.data(), public_key.key.data(),
                             message.data(), message.size());
  }

  if (result != 0) {
    return Fail(error, "the signature does not match the signed data");
  }

  // The global signature, over the message signature followed by the trusted
  // comment. Without this check the trusted comment would be attacker-editable
  // text presented to a user as though the project had written it.
  std::vector<uint8_t> covered;
  covered.reserve(signature.signature.size() +
                  signature.trusted_comment.size());
  covered.insert(covered.end(), signature.signature.begin(),
                 signature.signature.end());
  covered.insert(covered.end(), signature.trusted_comment.begin(),
                 signature.trusted_comment.end());

  if (crypto_ed25519_check(signature.global_signature.data(),
                           public_key.key.data(), covered.data(),
                           covered.size()) != 0) {
    return Fail(error, "the trusted comment is not covered by the signature");
  }

  if (error != nullptr) {
    error->clear();
  }
  return true;
}

}  // namespace ddd::capture
