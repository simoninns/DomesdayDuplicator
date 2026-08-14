/************************************************************************

    update_bundle.h

    Reading and writing the .dddfw update bundle
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "minisign_verify.h"
#include "update_manifest.h"

namespace ddd::capture {

// The update bundle: one file carrying everything a device may write to itself.
//
//     domesday-duplicator-update-<version>.dddfw   uncompressed ustar archive
//       manifest.json      always the first entry
//       manifest.minisig   detached Ed25519 signature over manifest.json
//       firmware.img       FX3 image, when the bundle carries firmware
//       gateware-app.rpd   raw EPCS byte stream, when it carries gateware
//
// Uncompressed tar because the two properties that matter are that stock `tar`
// can list and extract it, and that reading it takes a couple of hundred lines
// of code with no dependency. Both payloads are already compressed by the
// toolchains that produced them, so a compressed container would trade those
// properties for nothing.
//
// The reader below is the *only* thing in the application that touches a file a
// user may have downloaded from anywhere, so it is written to be read end to
// end: no seeking, no allocation proportional to a field in the input, and
// every length checked against the buffer before it is used.

// One entry in the archive. `data` points into the caller's buffer, which must
// outlive the entry: a bundle is megabytes and copying every payload to hand
// back a self-contained object would double the peak memory of an update for no
// benefit.
struct BundleEntry {
  std::string name;
  std::span<const uint8_t> data;
};

// Read a ustar archive. Returns nothing, and sets `error` when it is not null,
// for anything that is not a flat archive of regular files.
//
// Deliberately narrow: directories, symlinks, hard links, device nodes, long-
// name extensions, non-empty path prefixes and any name containing a path
// separator are all refused rather than interpreted. A bundle is four files in
// one directory, and every feature accepted beyond that is a feature an
// attacker gets to use.
std::optional<std::vector<BundleEntry>> ReadUstarArchive(
    std::span<const uint8_t> archive, std::string* error);

// Build a ustar archive.
//
// Nothing in the application writes a bundle — assembly is
// tools/make-update-bundle.sh, so that the released artefact is made by stock
// tools from the tagged commit. This exists so the reader can be tested against
// an archive it did not produce the ideas for, and so tests can construct the
// malformed and tampered cases that a shell script cannot conveniently make.
class UstarWriter {
 public:
  // Append a regular file. Names are written as given and must be short enough
  // for the 100-byte name field; there is no prefix splitting, because bundle
  // entries are bare filenames.
  void AddFile(std::string_view name, std::span<const uint8_t> data);

  // The complete archive, including the two zero blocks that end it.
  std::vector<uint8_t> Finish() const;

 private:
  std::vector<uint8_t> blocks_;
};

// A bundle that has been opened, verified and understood.
struct UpdateBundle {
  UpdateManifest manifest;

  // The signature's trusted comment, which is covered by the signature and so
  // may be shown to a user. Whatever the release pipeline chose to put there —
  // in practice the bundle's filename and version.
  std::string trusted_comment;

  // The payload bytes, pointing into the archive buffer the caller supplied.
  // Present exactly when the corresponding component is present in the
  // manifest.
  std::span<const uint8_t> firmware;
  std::span<const uint8_t> gateware;
};

// Open a bundle: check that it is what it claims to be, and refuse it
// otherwise.
//
// The order of the checks is the point of this function, and it is the order
// the "Update bundle format" documentation page specifies:
//
//   1. the archive parses, and `manifest.json` is its first entry — first so
//      that a bundle cannot hide a second manifest behind the one that gets
//      signed;
//   2. `manifest.minisig` verifies against `public_key` over the manifest's
//      exact bytes. Nothing has been interpreted yet, so an unauthentic bundle
//      is rejected before any of its content has been believed;
//   3. only then is the manifest parsed and its schema version checked;
//   4. every component the manifest declares is present in the archive, is
//      exactly the length the manifest states, and hashes to the digest the
//      manifest states.
//
// A bundle that passes has been proved to come from the holder of the key and
// to carry the bytes that key signed for. It has *not* been proved installable
// on the device in front of the user — that is the compatibility gate, which
// needs the device and belongs to the caller.
std::optional<UpdateBundle> OpenUpdateBundle(std::span<const uint8_t> archive,
                                             const MinisignPublicKey& key,
                                             std::string* error);

// The entry names the format fixes. Producers must use these, and the reader
// looks for nothing else.
inline constexpr std::string_view kManifestEntryName = "manifest.json";
inline constexpr std::string_view kSignatureEntryName = "manifest.minisig";

// The extension a bundle carries, for the file dialog's filter and for the
// release pipeline's asset name.
inline constexpr std::string_view kUpdateBundleExtension = ".dddfw";

}  // namespace ddd::capture
