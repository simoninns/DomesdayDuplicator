/************************************************************************

    digest.h

    The one digest the update chain is built on
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ddd::capture {

// SHA-256, and only SHA-256.
//
// The device-update integrity chain is specified on a single digest computed
// once at build time and re-checked at every hand-off — bundle assembly, the
// download, the transfer to the FX3, and the readback from the flash the bytes
// were just written to. No link trusts the previous link's verification, which
// only works if every link is speaking about the same number. There is
// therefore deliberately no algorithm parameter anywhere in this interface: a
// second digest would be a second chain.
//
// SHA-256 rather than something faster or newer because it is the digest the
// surrounding tooling already agrees on. A maintainer can run sha256sum over a
// published payload and get the number the manifest claims, with nothing of
// ours involved, and that independent check is worth more than any property a
// different hash would offer.
//
// The implementation is vendored — see src/vendor/VENDOR.md. This header is one
// of the two places that knows which one, so replacing it is a change here and
// nowhere else.

// A SHA-256 digest: 32 bytes, in the order the algorithm produces them.
using Sha256Digest = std::array<uint8_t, 32>;

// The number of characters a digest occupies when written as hex.
inline constexpr size_t kSha256HexLength = 64;

// Digest a buffer that is already in memory.
Sha256Digest Sha256(std::span<const uint8_t> data);

// Digest a string's bytes. The manifest signature covers the manifest's exact
// bytes as they appear in the archive, so text is hashed as text and never
// re-serialised first.
Sha256Digest Sha256(std::string_view text);

// Digest data that arrives in pieces.
//
// The update flow never has a whole firmware image and its copy in memory at
// once if it can avoid it: the GUI streams a payload to the device while
// hashing it, and the firmware does the same on its side. Streaming is
// therefore the shape that matters and the one-shot function above is the
// convenience.
class Sha256Hasher {
 public:
  Sha256Hasher();

  // Not copyable or movable: the vendored context holds a pointer into this
  // object's own storage, so a copy would hash into the original's buffer. The
  // class exists for a lifetime measured in one function, so there is nothing
  // to gain by making that work.
  Sha256Hasher(const Sha256Hasher&) = delete;
  Sha256Hasher& operator=(const Sha256Hasher&) = delete;

  void Update(std::span<const uint8_t> data);

  // The digest of everything fed in so far. Calling this on a hasher that was
  // never fed gives the digest of the empty input, which is a defined value and
  // not an error.
  Sha256Digest Finish();

 private:
  // The vendored streaming context, held as raw storage so that its header does
  // not have to be included here. Sized and aligned in the implementation,
  // where the type is visible.
  alignas(8) std::array<uint8_t, 128> state_{};
  Sha256Digest digest_{};
};

// A digest as 64 lowercase hex characters — the form the manifest carries and
// the form sha256sum prints.
std::string ToHex(const Sha256Digest& digest);

// Read a digest back from hex, or nothing if the text is not exactly 64 hex
// characters.
//
// Strict on purpose: a manifest field that is the wrong length or carries a
// stray space is a manifest that was edited by hand or damaged in transit, and
// either way it must not be interpreted charitably. Both cases are the same
// answer — this bundle is not usable.
std::optional<Sha256Digest> ParseHexDigest(std::string_view hex);

}  // namespace ddd::capture
