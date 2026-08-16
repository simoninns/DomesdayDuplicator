/************************************************************************

    flac_sink.cpp

    Writing a capture to a FLAC file
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "flac_sink.h"

namespace ddd::capture {

FlacSink::FlacSink() : writer_(std::make_unique<FlacWriter>()) {}

FlacSink::~FlacSink() = default;

bool FlacSink::Open(const std::filesystem::path& file_path,
                    const FlacWriter::Options& options, int decimation_factor) {
  file_path_ = file_path;
  decimation_factor_ = decimation_factor;
  return writer_->Open(file_path, options, last_error_);
}

bool FlacSink::Write(const uint8_t* wire_data, size_t sample_count) {
  if (!writer_->WriteRawDeviceSamples(wire_data, sample_count,
                                      decimation_factor_)) {
    last_error_ = writer_->LastError();
    return false;
  }
  return true;
}

bool FlacSink::Finish() {
  if (!writer_->Finish()) {
    last_error_ = writer_->LastError();
    return false;
  }
  return true;
}

uint64_t FlacSink::BytesWritten() const { return writer_->BytesWritten(); }

uint64_t FlacSink::SamplesWritten() const { return writer_->SamplesWritten(); }

uint64_t FlacSink::SamplesPending() const { return writer_->SamplesPending(); }

}  // namespace ddd::capture
