/************************************************************************

    sample_sink.h

    Where samples go
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ddd::capture {

// Somewhere validated sample data is written.
//
// Monitor mode and capture mode differ by which of these is attached and by
// nothing else. The device streams either way, the ring runs either way, the
// validation and metrics run either way — the only difference is whether the
// bytes reach a file. That is what makes starting a capture from a monitoring
// session instantaneous and, more usefully, what makes monitoring exercise the
// exact code path a capture will use.
//
// Write() receives the device's own wire layout: 16-bit little-endian words
// with the sequence markers already stripped. Converting to something else
// before this point would be a copy of 80 MB/s that most sinks do not want —
// the FLAC encoder widens the words itself as it reads them.
//
// Thread-safety: an implementation is used by the processing thread only. The
// progress counters may be read from elsewhere, so they are the
// implementation's job to make safe, and nothing else is.
class ISampleSink {
 public:
  ISampleSink() = default;
  virtual ~ISampleSink() = default;

  ISampleSink(const ISampleSink&) = delete;
  ISampleSink& operator=(const ISampleSink&) = delete;
  ISampleSink(ISampleSink&&) = delete;
  ISampleSink& operator=(ISampleSink&&) = delete;

  // A name for logs ("null", "flac", ...)
  virtual const char* Name() const = 0;

  // Whether anything reaches storage through this sink.
  //
  // The one bit that separates monitoring from capturing, asked of the sink
  // rather than inferred from its name so that a test double which really does
  // keep the data can say so.
  virtual bool StoresData() const { return true; }

  // Write one buffer. Returns false on failure, and LastError() then says why.
  //
  // This is on the deadline: at 80 MB/s a 2 MB buffer arrives every 26 ms, and
  // anything this does for longer than the ring is deep loses samples.
  virtual bool Write(const uint8_t* wire_data, size_t sample_count) = 0;

  // Flush and close. Called once, when the sink is detached or the capture
  // ends. Returns false on failure — for a FLAC file this is where the stream
  // header is patched, so a failure here means a file that is short and lies
  // about its length.
  virtual bool Finish() = 0;

  // Bytes committed to storage so far. Zero for a sink that stores nothing.
  //
  // Not derivable from the sample count once a compressor is involved, which is
  // why sinks report it rather than the orchestrator calculating it.
  virtual uint64_t BytesWritten() const = 0;

  // Samples accepted so far
  virtual uint64_t SamplesWritten() const = 0;

  // Samples accepted but not yet committed to storage.
  //
  // Zero for a sink with nothing in flight, which is why this has a default
  // rather than being pure: most sinks write straight through and have nothing
  // to say. A compressing sink does, and the figure separates "the disk cannot
  // keep up" from "the encoder cannot keep up" — two problems with different
  // remedies that look the same from the ring's point of view.
  virtual uint64_t SamplesPending() const { return 0; }

  virtual const std::string& LastError() const = 0;
};

// The monitor-mode sink: counts what goes past and discards it.
//
// Not a placeholder. Monitoring is a mode a user may sit in for a long time
// while adjusting a player, and it has to cost as close to nothing as a sink
// can — so this does no work at all beyond the counters the statistics panel
// reads.
class NullSink : public ISampleSink {
 public:
  const char* Name() const override { return "null"; }

  bool StoresData() const override { return false; }

  bool Write(const uint8_t* wire_data, size_t sample_count) override;
  bool Finish() override { return true; }

  uint64_t BytesWritten() const override { return 0; }
  uint64_t SamplesWritten() const override { return samples_written_; }

  const std::string& LastError() const override { return no_error_; }

 private:
  uint64_t samples_written_ = 0;
  std::string no_error_;
};

}  // namespace ddd::capture
