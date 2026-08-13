/************************************************************************

    flac_writer.h

    Native FLAC capture output
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ddd::capture {

// Writes a capture as mono 16-bit native FLAC, with the sample rate stamped
// rather than measured (see capture_format.h).
//
// Why libFLAC in-process rather than piping to the flac binary, which is what
// ld-decode's ld-compress does: the three installers this application ships as
// would each have to bundle, locate and version-check an external executable,
// and a subprocess on the capture path is one more thing that can die forty
// minutes into a capture. libFLAC is BSD-licensed, so linking it into a GPLv3
// application is fine.
//
// The libFLAC types are anonymous struct typedefs and cannot be forward
// declared, so the encoder state lives behind a pimpl rather than leaking
// FLAC's headers into everything that captures.
//
// Thread-safety: none. One thread owns an instance for its lifetime — in a
// capture that is the processing thread. The two byte counters are atomic only
// so that a monitoring thread can read progress without a lock; every mutating
// call must come from the owning thread.
class FlacWriter {
 public:
  struct Tag {
    std::string name;
    std::string value;
  };

  struct Options {
    // 0-8, as flac's -0 .. -8. The same default ld-compress uses, and for the
    // same reason: a capture is an archival copy that will be stored and copied
    // for years, and the encoding cost is paid once.
    //
    // It is affordable because libFLAC 1.5.0 encodes on several threads.
    // Measured on a 16-core machine against a noisy 2 MHz tone, one second of
    // capture at a time:
    //
    //     level 0/1   34.2 MB   42.8% of raw   0.12 s to encode
    //     level 5     24.0 MB   30.0%          0.11 s
    //     level 8     23.7 MB   29.7%          0.10 s
    //
    // The encode cost is flat across the range because it is spread over the
    // cores, so the higher levels are very nearly free — and the file is 30%
    // smaller than at level 1, which over a disc side is tens of gigabytes. The
    // whole pipeline sustains the device's 80 MB/s at level 8 with the ring
    // never going deeper than one buffer of 128; the soak test measures it.
    //
    // On an older libFLAC the encode is single-threaded
    // (SupportsMultithreading() says which), and level 8 may then not keep up.
    // That shows up as kBufferOverflow, whose guidance names lowering this
    // setting as the first remedy.
    int compression_level = 8;

    // 0 asks for one thread per core, capped at kMaximumEncoderThreads.
    // Multithreaded encoding needs libFLAC 1.5.0 or later; on anything older
    // this is silently a single-threaded encode, which is why the level default
    // is conservative.
    unsigned int threads = 0;

    // Written into the STREAMINFO sample-rate field. Not a measurement.
    uint32_t sample_rate_label = 40'000;

    // Vorbis comments, so a capture separated from its metadata sidecar can
    // still say which build produced it.
    std::vector<Tag> tags;
  };

  FlacWriter();
  ~FlacWriter();

  FlacWriter(const FlacWriter&) = delete;
  FlacWriter& operator=(const FlacWriter&) = delete;
  FlacWriter(FlacWriter&&) = delete;
  FlacWriter& operator=(FlacWriter&&) = delete;

  // Open the output file and configure the encoder. Returns false and fills
  // error_message on failure.
  bool Open(const std::filesystem::path& file_path, const Options& options,
            std::string& error_message);

  // Encode sample_count samples of raw device data.
  //
  // The input is the device's own layout — 16-bit little-endian words each
  // holding a 10-bit unsigned sample — because that is what sits in the disk
  // buffer, and copying it into an intermediate form first would be a memcpy of
  // 80 MB/s for nothing.
  bool WriteRawDeviceSamples(const uint8_t* device_data, size_t sample_count);

  // Flush the encoder and close the file. Safe to call twice; the destructor
  // calls it.
  //
  // Not a formality: this writes the final partial frame and patches the stream
  // header, so a capture whose encoder was never finished loses its tail and
  // reports the wrong length.
  bool Finish();

  // Bytes on disk so far, from the encoder's own progress callback. With a
  // compressor in the path the file size no longer follows from the sample
  // count, so this is the only honest answer.
  size_t BytesWritten() const;

  // Samples handed to the encoder so far
  size_t SamplesWritten() const;

  // Samples handed over but not yet in the file.
  //
  // The encoder holds work in flight, and with more than one encoder thread it
  // holds rather a lot of it. A backlog that sits at a steady figure is the
  // encoder keeping pace; one that climbs is the encoder falling behind, and
  // that is a different remedy from a slow disk — a lower compression level
  // rather than a faster drive. Without this the two look identical from the
  // outside, because both end as a buffer queue that will not come down.
  size_t SamplesPending() const;

  const std::string& LastError() const;

  // Whether the libFLAC this was built against can encode on more than one
  // thread. False means one core is doing all of it, which the application says
  // out loud rather than leaving as an unexplained shortfall.
  static bool SupportsMultithreading();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ddd::capture
