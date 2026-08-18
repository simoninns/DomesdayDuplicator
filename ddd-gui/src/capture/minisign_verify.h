/************************************************************************

    minisign_verify.h

    Checking that a bundle came from the project and not from somebody else
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

// Verification of minisign detached signatures — the authenticity half of the
// update chain.
//
// A release bundle carries `manifest.json` and `manifest.minisig`, an Ed25519
// signature over the manifest's exact bytes. The manifest in turn carries the
// SHA-256 of every payload, so signing that one small file covers the whole
// bundle without anything having to sign a file that would contain its own
// signature, and the offline file-picker path and the online download path
// verify by identical means.
//
// Why minisign rather than a format of our own: the signature can be produced
// and checked by a stock `minisign` binary, so a maintainer can verify a
// published bundle without this application, and this application can be
// checked against a signature it did not produce. A bespoke container would
// have made both of those impossible and bought nothing.
//
// This is a verifier only. Nothing in the application signs, and there is no
// code here that could: signing is `minisign` itself, invoked by
// tools/make-update-bundle.sh at build time with a key the application never
// sees.
//
// The format, for the reader who does not want to go and look it up. A public
// key file is two lines — an untrusted comment, then base64 of the two-byte
// algorithm, an eight-byte key identifier and the 32-byte key. A signature file
// is four — an untrusted comment; base64 of the algorithm, key identifier and
// the 64-byte signature; a *trusted* comment; then base64 of a second signature
// covering the first signature concatenated with that trusted comment. The
// second signature is what makes the trusted comment trustworthy, and checking
// it is not optional here.

struct MinisignPublicKey {
  // Identifies which key signed a file. Not a security property — it is a hint
  // that stops a verifier reporting "bad signature" when the honest answer is
  // "signed by a different key" — but a mismatch is still refused, because a
  // signature that does not even claim to come from this key has nothing to
  // say about the file.
  std::array<uint8_t, 8> key_id{};

  std::array<uint8_t, 32> key{};
};

struct MinisignSignature {
  std::array<uint8_t, 8> key_id{};
  std::array<uint8_t, 64> signature{};

  // The signed comment line, without its "trusted comment: " prefix. The
  // release pipeline puts the bundle's filename and version here, and because
  // it is covered by the global signature it is text a caller may quote back to
  // a user. Nothing else in a signature file is.
  std::string trusted_comment;

  std::array<uint8_t, 64> global_signature{};
};

// Parse a `.pub` file's text. Returns nothing, and sets `error` when it is not
// null, for anything that is not a well-formed minisign public key.
std::optional<MinisignPublicKey> ParseMinisignPublicKey(std::string_view text,
                                                        std::string* error);

// Parse a `.minisig` file's text.
std::optional<MinisignSignature> ParseMinisignSignature(std::string_view text,
                                                        std::string* error);

// Does this signature, made by this key, cover exactly these bytes?
//
// Both signatures in the file are checked — the one over the message and the
// one over the trusted comment — and the key identifiers must agree. Any
// failure is false with a reason in `error`; there is no partial success and no
// "signature valid but comment unverified" state for a caller to misread.
bool VerifyMinisign(std::span<const uint8_t> message,
                    const MinisignSignature& signature,
                    const MinisignPublicKey& public_key, std::string* error);

}  // namespace ddd::capture
