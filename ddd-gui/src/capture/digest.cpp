/************************************************************************

    digest.cpp

    The one digest the update chain is built on
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "digest.h"

#include <cstddef>
#include <new>

extern "C" {
#include "sha-256.h"
}

namespace ddd::capture {
namespace {

// The vendored context, placed in this object's own storage rather than
// allocated, so that hashing never touches the heap and the header stays free
// of the vendored declaration.
static_assert(sizeof(Sha_256) <= 128,
              "Sha256Hasher::state_ is too small for the vendored context");
static_assert(alignof(Sha_256) <= 8,
              "Sha256Hasher::state_ is not aligned enough for the vendored "
              "context");

Sha_256* Context(std::array<uint8_t, 128>& storage) {
  return std::launder(reinterpret_cast<Sha_256*>(storage.data()));
}

// One hex nibble, or 16 for a character that is not a hex digit. Written out
// rather than reached for through the locale-dependent library functions,
// which would accept a different set of characters under a different locale.
uint8_t HexNibble(char character) {
  if (character >= '0' && character <= '9') {
    return static_cast<uint8_t>(character - '0');
  }
  if (character >= 'a' && character <= 'f') {
    return static_cast<uint8_t>(character - 'a' + 10);
  }
  if (character >= 'A' && character <= 'F') {
    return static_cast<uint8_t>(character - 'A' + 10);
  }
  return 16;
}

}  // namespace

Sha256Digest Sha256(std::span<const uint8_t> data) {
  Sha256Digest digest{};
  calc_sha_256(digest.data(), data.data(), data.size());
  return digest;
}

Sha256Digest Sha256(std::string_view text) {
  return Sha256(std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(text.data()), text.size()));
}

Sha256Hasher::Sha256Hasher() {
  sha_256_init(new (state_.data()) Sha_256, digest_.data());
}

void Sha256Hasher::Update(std::span<const uint8_t> data) {
  sha_256_write(Context(state_), data.data(), data.size());
}

Sha256Digest Sha256Hasher::Finish() {
  sha_256_close(Context(state_));
  return digest_;
}

std::string ToHex(const Sha256Digest& digest) {
  static constexpr char kDigits[] = "0123456789abcdef";

  std::string hex;
  hex.reserve(kSha256HexLength);
  for (uint8_t byte : digest) {
    hex.push_back(kDigits[byte >> 4]);
    hex.push_back(kDigits[byte & 0x0F]);
  }
  return hex;
}

std::optional<Sha256Digest> ParseHexDigest(std::string_view hex) {
  if (hex.size() != kSha256HexLength) {
    return std::nullopt;
  }

  Sha256Digest digest{};
  for (size_t index = 0; index < digest.size(); ++index) {
    const uint8_t high = HexNibble(hex[index * 2]);
    const uint8_t low = HexNibble(hex[(index * 2) + 1]);
    if (high > 15 || low > 15) {
      return std::nullopt;
    }
    digest[index] = static_cast<uint8_t>((high << 4) | low);
  }
  return digest;
}

}  // namespace ddd::capture
