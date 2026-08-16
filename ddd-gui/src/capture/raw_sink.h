/************************************************************************

    raw_sink.h

    Writing a capture as uncompressed signed 16-bit
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "sample_sink.h"

namespace ddd::capture {

// The capture-mode sink for anyone who would rather spend disk than CPU.
//
// The same samples a FLAC capture holds — signed 16-bit little-endian, the
// layout ld-decode calls the DdD 16-bit format — with nothing wrapped round
// them. Twice the file for none of the encoder.
//
// There is no header, so there is nowhere to put the provenance tags a FLAC
// capture carries and nowhere to record the rate it was written at. A decimated
// capture in this format is a bare stream of samples whose rate is known only
// because somebody wrote it down. That is the format's nature rather than an
// omission here, and it is the reason FLAC remains the default.
//
// Opening happens in Open() rather than in the constructor, for the reason
// FlacSink's does: a file that cannot be created is an ordinary condition a
// user causes by choosing a full disk, and reporting it needs a message rather
// than an exception.
//
// Thread-safety: as ISampleSink — the processing thread writes, and the
// progress counters are safe to read from elsewhere.
class RawSink : public ISampleSink {
 public:
  struct Options {
    // Keep every nth sample. See capture_format.h: 1 or 2, and anything else is
    // refused rather than rounded to something that would produce a file at a
    // rate nobody asked for.
    int decimation_factor = 1;
  };

  RawSink();
  ~RawSink() override;

  // Create the file. Returns false with the reason in LastError().
  bool Open(const std::filesystem::path& file_path, const Options& options);

  const char* Name() const override { return "s16"; }

  bool Write(const uint8_t* wire_data, size_t sample_count) override;
  bool Finish() override;

  uint64_t BytesWritten() const override { return bytes_written_; }
  uint64_t SamplesWritten() const override { return samples_written_; }

  const std::string& LastError() const override { return last_error_; }

  const std::filesystem::path& file_path() const { return file_path_; }

 private:
  std::ofstream file_;
  std::filesystem::path file_path_;
  std::string last_error_;

  // The converted samples on their way to the file. Sized once at Open() and
  // reused, never grown on the capture path.
  std::vector<uint8_t> scratch_;

  int decimation_factor_ = 1;

  // How far into the next buffer the next sample to keep lies. See
  // FlacWriter::WriteRawDeviceSamples: without it a buffer holding an odd
  // number of samples would shift the decimation phase at the seam.
  size_t decimation_offset_ = 0;

  bool finished_ = false;

  uint64_t bytes_written_ = 0;
  uint64_t samples_written_ = 0;
};

}  // namespace ddd::capture
