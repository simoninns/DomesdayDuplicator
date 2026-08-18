/************************************************************************

    update_bundle.cpp

    Reading and writing the .dddfw update bundle
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_bundle.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "digest.h"

namespace ddd::capture {
namespace {

// The tar block, and where everything lives in it. POSIX.1-1988 ustar; the
// field offsets are the format and are not going to change, so they are written
// out here rather than described by a packed struct whose layout the compiler
// would be entitled to have opinions about.
constexpr size_t kBlockSize = 512;
constexpr size_t kNameOffset = 0;
constexpr size_t kNameSize = 100;
constexpr size_t kModeOffset = 100;
constexpr size_t kUidOffset = 108;
constexpr size_t kGidOffset = 116;
constexpr size_t kSizeOffset = 124;
constexpr size_t kSizeSize = 12;
constexpr size_t kMtimeOffset = 136;
constexpr size_t kChecksumOffset = 148;
constexpr size_t kChecksumSize = 8;
constexpr size_t kTypeFlagOffset = 156;
constexpr size_t kLinkNameOffset = 157;
constexpr size_t kMagicOffset = 257;
constexpr size_t kVersionOffset = 263;
constexpr size_t kPrefixOffset = 345;
constexpr size_t kPrefixSize = 155;

// A payload no bundle will legitimately exceed. The largest thing a bundle
// carries is a gateware image under a megabyte, and the FX3 firmware is smaller
// still; sixty-four megabytes is far past any of that and far short of a length
// field that could make a reader allocate its way into trouble.
constexpr uint64_t kMaximumEntryBytes = 64ULL * 1024 * 1024;

bool Fail(std::string* error, std::string message) {
  if (error != nullptr) {
    *error = std::move(message);
  }
  return false;
}

// Read an octal field. Tar pads these with leading zeros and terminates with a
// NUL, a space, or both; some writers use trailing spaces instead. Anything
// that is not an octal digit inside that convention is refused.
std::optional<uint64_t> ReadOctal(const uint8_t* field, size_t size) {
  size_t index = 0;
  while (index < size && (field[index] == ' ' || field[index] == '\0')) {
    ++index;
  }

  bool any_digits = false;
  uint64_t value = 0;
  for (; index < size; ++index) {
    const uint8_t character = field[index];
    if (character == '\0' || character == ' ') {
      break;
    }
    if (character < '0' || character > '7') {
      return std::nullopt;
    }
    // Twenty-one octal digits would overflow; the fields are twelve wide, so
    // this cannot happen, and the check is here so that it still cannot happen
    // if a future field is wider.
    if (value > (UINT64_MAX >> 3)) {
      return std::nullopt;
    }
    value = (value << 3) | static_cast<uint64_t>(character - '0');
    any_digits = true;
  }

  // Everything after the terminator must be padding.
  for (; index < size; ++index) {
    if (field[index] != '\0' && field[index] != ' ') {
      return std::nullopt;
    }
  }

  if (!any_digits) {
    return std::nullopt;
  }
  return value;
}

// The header checksum: the unsigned sum of every byte of the block with the
// checksum field itself read as spaces.
uint32_t HeaderChecksum(const uint8_t* block) {
  uint32_t sum = 0;
  for (size_t index = 0; index < kBlockSize; ++index) {
    const bool in_checksum_field =
        index >= kChecksumOffset && index < kChecksumOffset + kChecksumSize;
    sum += in_checksum_field ? static_cast<uint32_t>(' ')
                             : static_cast<uint32_t>(block[index]);
  }
  return sum;
}

// A NUL-terminated (or field-filling) string out of a fixed-width field.
std::string ReadField(const uint8_t* field, size_t size) {
  size_t length = 0;
  while (length < size && field[length] != '\0') {
    ++length;
  }
  return std::string(reinterpret_cast<const char*>(field), length);
}

bool IsZeroBlock(const uint8_t* block) {
  for (size_t index = 0; index < kBlockSize; ++index) {
    if (block[index] != 0) {
      return false;
    }
  }
  return true;
}

// Write a NUL-terminated octal number, right-aligned and zero-padded, in the
// way tar writes its numeric fields: `size - 1` digits then a NUL.
void WriteOctal(uint64_t value, uint8_t* field, size_t size) {
  for (size_t index = size - 1; index > 0; --index) {
    field[index - 1] = static_cast<uint8_t>('0' + (value & 7));
    value >>= 3;
  }
  field[size - 1] = '\0';
}

void WriteField(std::string_view text, uint8_t* field, size_t size) {
  const size_t length = std::min(text.size(), size);
  if (length != 0) {
    std::memcpy(field, text.data(), length);
  }
}

// Find one entry by name.
const BundleEntry* FindEntry(const std::vector<BundleEntry>& entries,
                             std::string_view name) {
  for (const BundleEntry& entry : entries) {
    if (entry.name == name) {
      return &entry;
    }
  }
  return nullptr;
}

// Check one declared component against the archive.
bool CheckComponent(const std::vector<BundleEntry>& entries,
                    const UpdateComponent& component, std::string_view label,
                    std::span<const uint8_t>* payload, std::string* error) {
  const BundleEntry* entry = FindEntry(entries, component.file);
  if (entry == nullptr) {
    return Fail(error, "the manifest names a " + std::string(label) +
                           " payload the bundle does not contain");
  }

  if (entry->data.size() != component.length) {
    return Fail(error, "the " + std::string(label) +
                           " payload is not the length the manifest states");
  }

  if (Sha256(entry->data) != component.sha256) {
    return Fail(error, "the " + std::string(label) +
                           " payload does not match the digest the manifest "
                           "states");
  }

  *payload = entry->data;
  return true;
}

}  // namespace

std::optional<std::vector<BundleEntry>> ReadUstarArchive(
    std::span<const uint8_t> archive, std::string* error) {
  if (archive.empty() || (archive.size() % kBlockSize) != 0) {
    Fail(error,
         "the archive is empty or not a whole number of 512-byte "
         "blocks");
    return std::nullopt;
  }

  std::vector<BundleEntry> entries;
  size_t offset = 0;

  while (offset + kBlockSize <= archive.size()) {
    const uint8_t* block = archive.data() + offset;

    // Two zero blocks end an archive. One is enough to stop on: everything
    // after it is padding to the writer's block factor, and a reader that kept
    // going would be reading whatever a writer left in the tail.
    if (IsZeroBlock(block)) {
      break;
    }

    const std::optional<uint64_t> stored_checksum =
        ReadOctal(block + kChecksumOffset, kChecksumSize);
    if (!stored_checksum || *stored_checksum != HeaderChecksum(block)) {
      Fail(error, "a header block has a bad checksum");
      return std::nullopt;
    }

    if (std::memcmp(block + kMagicOffset, "ustar", 5) != 0) {
      Fail(error, "a header block is not in ustar format");
      return std::nullopt;
    }

    const uint8_t type_flag = block[kTypeFlagOffset];
    if (type_flag != '0' && type_flag != '\0') {
      Fail(error, "the archive contains something that is not a regular file");
      return std::nullopt;
    }

    if (block[kPrefixOffset] != '\0') {
      Fail(error, "the archive contains a path prefix");
      return std::nullopt;
    }

    const std::string name = ReadField(block + kNameOffset, kNameSize);
    if (name.empty() || name.find('/') != std::string::npos) {
      Fail(error, "the archive contains an entry that is not a bare filename");
      return std::nullopt;
    }
    if (FindEntry(entries, name) != nullptr) {
      // Two entries of one name is the oldest trick in the archive-format
      // book: whichever the verifier reads, the extractor reads the other.
      Fail(error, "the archive contains two entries with the same name");
      return std::nullopt;
    }

    const std::optional<uint64_t> size =
        ReadOctal(block + kSizeOffset, kSizeSize);
    if (!size) {
      Fail(error, "a header block has a malformed size");
      return std::nullopt;
    }
    if (*size > kMaximumEntryBytes) {
      Fail(error, "the archive contains an implausibly large entry");
      return std::nullopt;
    }

    const size_t data_offset = offset + kBlockSize;
    const size_t padded =
        ((static_cast<size_t>(*size) + kBlockSize - 1) / kBlockSize) *
        kBlockSize;
    if (data_offset + padded > archive.size()) {
      Fail(error, "an entry runs past the end of the archive");
      return std::nullopt;
    }

    entries.push_back(BundleEntry{
        name, archive.subspan(data_offset, static_cast<size_t>(*size))});
    offset = data_offset + padded;
  }

  if (entries.empty()) {
    Fail(error, "the archive contains no entries");
    return std::nullopt;
  }
  return entries;
}

void UstarWriter::AddFile(std::string_view name,
                          std::span<const uint8_t> data) {
  const size_t header_offset = blocks_.size();
  blocks_.resize(header_offset + kBlockSize, 0);
  uint8_t* header = blocks_.data() + header_offset;

  WriteField(name, header + kNameOffset, kNameSize);

  // Fixed metadata. A bundle is content addressed — every payload is covered
  // by a digest in the manifest — so the ownership and timestamps in the
  // archive carry no information, and fixing them is what lets two assemblies
  // of the same inputs produce the same bytes.
  WriteOctal(0644, header + kModeOffset, 8);
  WriteOctal(0, header + kUidOffset, 8);
  WriteOctal(0, header + kGidOffset, 8);
  WriteOctal(data.size(), header + kSizeOffset, kSizeSize);
  WriteOctal(0, header + kMtimeOffset, 12);
  header[kTypeFlagOffset] = '0';
  WriteField("ustar", header + kMagicOffset, 6);
  WriteField("00", header + kVersionOffset, 2);

  // The link name, owner and group names, device numbers and path prefix are
  // all left as the zeros the block was created with. Empty is what they mean
  // here, and writing an owner name would be writing whoever assembled the
  // bundle into an artefact that is supposed to depend only on its inputs.

  // The checksum is computed with its own field read as spaces, so it has to
  // be filled with spaces before the sum and overwritten after it.
  std::fill_n(header + kChecksumOffset, kChecksumSize,
              static_cast<uint8_t>(' '));
  const uint32_t checksum = HeaderChecksum(header);

  // Six octal digits, a NUL, then a space — the layout GNU tar writes and every
  // reader accepts.
  WriteOctal(checksum, header + kChecksumOffset, 7);
  header[kChecksumOffset + 7] = ' ';

  blocks_.insert(blocks_.end(), data.begin(), data.end());
  const size_t remainder = data.size() % kBlockSize;
  if (remainder != 0) {
    blocks_.resize(blocks_.size() + (kBlockSize - remainder), 0);
  }
}

std::vector<uint8_t> UstarWriter::Finish() const {
  std::vector<uint8_t> archive = blocks_;
  archive.resize(archive.size() + (2 * kBlockSize), 0);
  return archive;
}

std::optional<UpdateBundle> OpenUpdateBundle(std::span<const uint8_t> archive,
                                             const MinisignPublicKey& key,
                                             std::string* error) {
  const std::optional<std::vector<BundleEntry>> entries =
      ReadUstarArchive(archive, error);
  if (!entries) {
    return std::nullopt;
  }

  // The manifest must be first. Not merely present: a reader that searched for
  // it would verify one entry while an extractor that took the first match
  // used another.
  if (entries->front().name != kManifestEntryName) {
    Fail(error, "the first entry in the bundle is not " +
                    std::string(kManifestEntryName));
    return std::nullopt;
  }
  const BundleEntry& manifest_entry = entries->front();

  const BundleEntry* signature_entry = FindEntry(*entries, kSignatureEntryName);
  if (signature_entry == nullptr) {
    Fail(error, "the bundle carries no " + std::string(kSignatureEntryName));
    return std::nullopt;
  }

  std::string reason;
  const std::optional<MinisignSignature> signature = ParseMinisignSignature(
      std::string_view(
          reinterpret_cast<const char*>(signature_entry->data.data()),
          signature_entry->data.size()),
      &reason);
  if (!signature) {
    Fail(error, "the bundle's signature is malformed: " + reason);
    return std::nullopt;
  }

  // Authenticity before interpretation: nothing below this line reads the
  // manifest's content, so a bundle from somebody else is refused before any
  // of what it says has been believed.
  if (!VerifyMinisign(manifest_entry.data, *signature, key, &reason)) {
    Fail(error,
         "the bundle is not signed by a key this build accepts: " + reason);
    return std::nullopt;
  }

  std::vector<std::string> manifest_errors;
  const std::optional<UpdateManifest> manifest = ParseUpdateManifest(
      std::string_view(
          reinterpret_cast<const char*>(manifest_entry.data.data()),
          manifest_entry.data.size()),
      &manifest_errors);
  if (!manifest) {
    Fail(error, manifest_errors.empty()
                    ? "the bundle's manifest could not be read"
                    : "the bundle's manifest could not be read: " +
                          manifest_errors.front());
    return std::nullopt;
  }

  UpdateBundle bundle;
  bundle.manifest = *manifest;
  bundle.trusted_comment = signature->trusted_comment;

  if (manifest->firmware &&
      !CheckComponent(*entries, *manifest->firmware, "firmware",
                      &bundle.firmware, error)) {
    return std::nullopt;
  }
  if (manifest->gateware &&
      !CheckComponent(*entries, *manifest->gateware, "gateware",
                      &bundle.gateware, error)) {
    return std::nullopt;
  }
  if (manifest->provisioning &&
      !CheckComponent(*entries, *manifest->provisioning,
                      "provisioning "
                      "gateware",
                      &bundle.provisioning, error)) {
    return std::nullopt;
  }
  if (manifest->factory_gateware &&
      !CheckComponent(*entries, *manifest->factory_gateware, "factory gateware",
                      &bundle.factory_gateware, error)) {
    return std::nullopt;
  }

  if (error != nullptr) {
    error->clear();
  }
  return bundle;
}

}  // namespace ddd::capture
