/************************************************************************

    capture_reader.h

    Reading capture files back
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ddd::capture {

// Yields 10-bit unsigned sample values from a capture file.
//
// The 10-bit domain is the common currency because that is what the device's
// test pattern counts in, so the ramp check in test_pattern_verifier.h is only
// meaningful there.
//
// The two formats this application writes, plus the old application's ".raw"
// spelling of the uncompressed one — see capture_format.h. Neither the packed
// 10-bit .lds nor the Ogg-encapsulated .ldf is read here; gui/ remains the tool
// for those.
//
// A decimated capture reads back as the samples it holds and nothing else. The
// rate a file was written at is in a FLAC header's label and in no part of an
// uncompressed file at all, and neither is something this reader reports:
// everything downstream of it counts samples.
//
// Thread-safety: none. One thread owns an instance for its lifetime.
class CaptureReader {
 public:
  enum class Format {
    kFlac,
    kSigned16Bit,
  };

  CaptureReader();
  ~CaptureReader();

  CaptureReader(const CaptureReader&) = delete;
  CaptureReader& operator=(const CaptureReader&) = delete;
  CaptureReader(CaptureReader&&) = delete;
  CaptureReader& operator=(CaptureReader&&) = delete;

  // Guess the format from the file name extension. Returns nothing for an
  // extension that is neither, so the caller can say so rather than guessing
  // wrong and reporting the resulting nonsense as data corruption.
  static std::optional<Format> FormatFromExtension(
      const std::filesystem::path& file_path);

  static const char* FormatName(Format format);

  bool Open(const std::filesystem::path& file_path, Format format,
            std::string& error_message);

  // Read up to max_samples 10-bit values into samples. Returns false on a read
  // or decode error. A short read is not an error: end_of_file says whether
  // there is more.
  bool Read(std::vector<uint16_t>& samples, size_t max_samples,
            bool& end_of_file);

  // Total samples in the file, where that is knowable — from the file size for
  // the uncompressed format, and from STREAMINFO for FLAC. A stream whose
  // header was never patched reports nothing, and callers show indeterminate
  // progress rather than a fabricated percentage.
  std::optional<uint64_t> TotalSamples() const;

  // Vorbis comments from the file, as name/value pairs, for FLAC inputs. Empty
  // for any other format or for a file that carries none. This is how a capture
  // says which build produced it once it has been separated from its metadata
  // sidecar.
  const std::vector<std::pair<std::string, std::string>>& Tags() const;

  const std::string& LastError() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ddd::capture
