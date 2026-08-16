/************************************************************************

    flac_sink.h

    Writing a capture to a FLAC file
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "flac_writer.h"
#include "sample_sink.h"

namespace ddd::capture {

// The capture-mode sink: everything that arrives is encoded into a FLAC file.
//
// A thin adapter over FlacWriter, and thin on purpose — the writer is used by
// tests and by offline tooling without the pipeline anywhere near it, so the
// pipeline's view of it is a separate, small thing rather than the writer
// growing a second personality.
//
// Opening happens in the constructor's companion Open() rather than in the
// constructor itself, because a file that cannot be created is an ordinary
// condition a user causes by choosing a full disk, and reporting it needs a
// message rather than an exception.
//
// Thread-safety: as ISampleSink — the processing thread writes, and the
// progress counters are safe to read from elsewhere.
class FlacSink : public ISampleSink {
 public:
  FlacSink();
  ~FlacSink() override;

  // Create the file and configure the encoder. Returns false with the reason in
  // LastError().
  //
  // decimation_factor is what the writer is asked for on every buffer. It is
  // held here rather than in FlacWriter::Options because it is a property of
  // this capture rather than of the encoder, and because the writer's other
  // users pass it per call.
  bool Open(const std::filesystem::path& file_path,
            const FlacWriter::Options& options, int decimation_factor = 1);

  const char* Name() const override { return "flac"; }

  bool Write(const uint8_t* wire_data, size_t sample_count) override;
  bool Finish() override;

  uint64_t BytesWritten() const override;
  uint64_t SamplesWritten() const override;
  uint64_t SamplesPending() const override;

  const std::string& LastError() const override { return last_error_; }

  const std::filesystem::path& file_path() const { return file_path_; }

 private:
  std::unique_ptr<FlacWriter> writer_;
  std::filesystem::path file_path_;
  std::string last_error_;
  int decimation_factor_ = 1;
};

}  // namespace ddd::capture
