/************************************************************************

    raw_sink.cpp

    Writing a capture as uncompressed signed 16-bit
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "raw_sink.h"

#include <string>

#include "capture_format.h"
#include "sample_format.h"

namespace ddd::capture {
namespace {

// Samples per write to the file. The same figure the encoder uses, and for the
// same reason: large enough that the per-call overhead disappears, small enough
// that the scratch buffer stays cache friendly.
constexpr size_t kWriteChunkSamples = 65'536;

// Bytes per sample in the file. The same as on the wire, which is a coincidence
// worth naming rather than relying on: the wire word carries a 10-bit value in
// 16 bits, and the file carries that value scaled into a signed 16-bit sample.
constexpr size_t kFileBytesPerSample = 2;

}  // namespace

RawSink::RawSink() = default;

RawSink::~RawSink() { Finish(); }

bool RawSink::Open(const std::filesystem::path& file_path) {
  file_path_ = file_path;

  file_.open(file_path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file_.is_open()) {
    last_error_ = "RawSink::Open(): Failed to create the capture file";
    return false;
  }

  bytes_written_ = 0;
  samples_written_ = 0;
  finished_ = false;
  scratch_.resize(kWriteChunkSamples * kFileBytesPerSample);
  return true;
}

bool RawSink::Write(const uint8_t* wire_data, size_t sample_count) {
  if (!file_.is_open()) {
    last_error_ = "RawSink::Write(): The capture file is not open";
    return false;
  }

  // Byte by byte in and byte by byte out, so this is correct on a big-endian
  // host and makes no alignment assumption about the buffer it was handed —
  // the file is little-endian wherever it was written.
  size_t index = 0;
  size_t filled = 0;

  const auto flush = [this, &filled]() {
    if (filled == 0) {
      return true;
    }

    const size_t bytes = filled * kFileBytesPerSample;
    file_.write(reinterpret_cast<const char*>(scratch_.data()),
                static_cast<std::streamsize>(bytes));
    if (!file_.good()) {
      last_error_ = "RawSink::Write(): Failed to write to the capture file";
      return false;
    }

    bytes_written_ += bytes;
    samples_written_ += filled;
    filled = 0;
    return true;
  };

  for (; index < sample_count; ++index) {
    const uint8_t* const read_pointer = wire_data + (index * kBytesPerSample);
    const uint16_t ten_bit_value = static_cast<uint16_t>(
        static_cast<uint16_t>(read_pointer[0]) |
        static_cast<uint16_t>(static_cast<uint16_t>(read_pointer[1]) << 8));

    const auto sample = static_cast<uint16_t>(
        ToSigned16Bit(static_cast<int32_t>(ten_bit_value)));
    scratch_[filled * kFileBytesPerSample] = static_cast<uint8_t>(sample);
    scratch_[(filled * kFileBytesPerSample) + 1] =
        static_cast<uint8_t>(sample >> 8);
    ++filled;

    if (filled == kWriteChunkSamples && !flush()) {
      return false;
    }
  }

  return flush();
}

bool RawSink::Finish() {
  if (finished_ || !file_.is_open()) {
    return true;
  }

  finished_ = true;

  // Flushed and closed explicitly rather than left to the destructor, because
  // this is the call whose failure a user has to be told about: a capture that
  // could not be flushed is a file that is short, and the stream ending quietly
  // is how that goes unnoticed.
  file_.flush();
  const bool good = file_.good();
  file_.close();

  if (!good) {
    last_error_ = "RawSink::Finish(): Failed to flush the capture file";
    return false;
  }
  return true;
}

}  // namespace ddd::capture
