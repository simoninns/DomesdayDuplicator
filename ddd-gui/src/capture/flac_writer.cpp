/************************************************************************

    flac_writer.cpp

    Native FLAC capture output
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "flac_writer.h"

#include <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>

#include <algorithm>
#include <atomic>
#include <thread>

#include "capture_format.h"
#include "sample_format.h"

namespace ddd::capture {
namespace {

// Samples per call to the encoder. Large enough that the per-call overhead
// disappears, small enough that the widened scratch buffer stays cache
// friendly.
constexpr size_t kEncodeChunkSamples = 65'536;

// ld-compress caps its own -j here, having observed throughput plateau once
// feeding the encoder becomes the bottleneck rather than the encoding itself.
// The same applies here, and the samples arrive on one thread.
constexpr unsigned int kMaximumEncoderThreads = 8;

// libFLAC documents its filenames as UTF-8 on every platform, including
// Windows, where it widens them itself. std::u8string is a distinct type in
// C++20, hence the copy.
std::string PathToUtf8(const std::filesystem::path& file_path) {
  const std::u8string utf8 = file_path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

}  // namespace

struct FlacWriter::Impl {
  FLAC__StreamEncoder* encoder = nullptr;
  FLAC__StreamMetadata* metadata = nullptr;
  bool encoder_initialised = false;
  bool finished = false;
  std::string last_error;

  // libFLAC takes one int32 per sample, so the device's 16-bit words are
  // widened here. Sized once at Open() and reused, never grown on the capture
  // path.
  std::vector<int32_t> scratch;

  std::atomic<size_t> bytes_written{0};
  std::atomic<size_t> samples_written{0};
  std::atomic<size_t> samples_encoded{0};

  void RecordEncoderError(const char* context) {
    const FLAC__StreamEncoderState state =
        FLAC__stream_encoder_get_state(encoder);
    last_error = std::string("FlacWriter::") + context +
                 "(): " + FLAC__StreamEncoderStateString[state];
  }

  // libFLAC calls this once per frame written. A member rather than a free
  // function because Impl is private to FlacWriter, and only its own members
  // can name it.
  static void ProgressCallback(const FLAC__StreamEncoder* /*encoder*/,
                               FLAC__uint64 bytes_written,
                               FLAC__uint64 samples_written,
                               uint32_t /*frames_written*/,
                               uint32_t /*total_frames_estimate*/,
                               void* client_data) {
    auto* const impl = static_cast<Impl*>(client_data);
    impl->bytes_written = static_cast<size_t>(bytes_written);

    // libFLAC's "samples written" is samples that have reached the file, which
    // is a different number from the samples handed to the encoder: with more
    // than one encoder thread a block can be in flight for some time. The gap
    // between the two is the only visible sign that the encoder rather than the
    // disk is what a struggling machine is waiting for.
    impl->samples_encoded = static_cast<size_t>(samples_written);
  }
};

FlacWriter::FlacWriter() : impl_(std::make_unique<Impl>()) {}

FlacWriter::~FlacWriter() {
  Finish();

  if (impl_->metadata != nullptr) {
    FLAC__metadata_object_delete(impl_->metadata);
    impl_->metadata = nullptr;
  }
  if (impl_->encoder != nullptr) {
    FLAC__stream_encoder_delete(impl_->encoder);
    impl_->encoder = nullptr;
  }
}

bool FlacWriter::SupportsMultithreading() {
  // FLAC__stream_encoder_set_num_threads and its result constants arrived
  // together in libFLAC 1.5.0. Testing for the constant rather than a version
  // macro stays honest against distributions that backport.
#ifdef FLAC__STREAM_ENCODER_SET_NUM_THREADS_OK
  return true;
#else
  return false;
#endif
}

bool FlacWriter::Open(const std::filesystem::path& file_path,
                      const Options& options, std::string& error_message) {
  if (impl_->encoder_initialised) {
    error_message = "FlacWriter::Open(): The encoder is already open";
    return false;
  }

  impl_->encoder = FLAC__stream_encoder_new();
  if (impl_->encoder == nullptr) {
    error_message = "FlacWriter::Open(): Failed to allocate a FLAC encoder";
    return false;
  }

  bool ok = true;
  ok = ok && FLAC__stream_encoder_set_channels(impl_->encoder, kFlacChannels);
  ok = ok && FLAC__stream_encoder_set_bits_per_sample(impl_->encoder,
                                                      kFlacBitsPerSample);
  ok = ok && FLAC__stream_encoder_set_sample_rate(impl_->encoder,
                                                  options.sample_rate_label);
  ok = ok && FLAC__stream_encoder_set_compression_level(
                 impl_->encoder, static_cast<uint32_t>(std::clamp(
                                     options.compression_level, 0, 8)));

  // Verification re-decodes every frame as it is written and compares. That is
  // the wrong trade on a real-time path — it roughly doubles the cost of the
  // thing most likely to run out of CPU — and the integrity of the result is
  // checked instead by decoding the finished file, which costs nothing during
  // the capture itself.
  ok = ok && FLAC__stream_encoder_set_verify(impl_->encoder, false);

  // A capture has no known length, so the estimate is zero rather than a guess.
  ok = ok && FLAC__stream_encoder_set_total_samples_estimate(impl_->encoder, 0);

  if (!ok) {
    error_message = "FlacWriter::Open(): Failed to configure the FLAC encoder";
    return false;
  }

#ifdef FLAC__STREAM_ENCODER_SET_NUM_THREADS_OK
  {
    unsigned int threads = options.threads;
    if (threads == 0) {
      const unsigned int cores =
          std::max(1U, std::thread::hardware_concurrency());
      threads = std::min(cores, kMaximumEncoderThreads);
    }

    // Not fatal if it is refused. A single-threaded encode is slower, not
    // wrong, and the capture is better attempted than declined — the overflow
    // detection on the capture path is what catches a machine that then cannot
    // keep up.
    if (FLAC__stream_encoder_set_num_threads(
            impl_->encoder, std::min(threads, kMaximumEncoderThreads)) !=
        FLAC__STREAM_ENCODER_SET_NUM_THREADS_OK) {
      impl_->last_error =
          "The FLAC encoder refused the requested thread count; encoding "
          "single-threaded";
    }
  }
#endif

  // Provenance, so a capture separated from its metadata sidecar still names
  // its origin. Unknown comments are ignored by every decoder, so this cannot
  // break ld-decode.
  if (!options.tags.empty()) {
    impl_->metadata =
        FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
    if (impl_->metadata == nullptr) {
      error_message =
          "FlacWriter::Open(): Failed to allocate the Vorbis comment block";
      return false;
    }

    for (const Tag& tag : options.tags) {
      FLAC__StreamMetadata_VorbisComment_Entry entry{};
      if (!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(
              &entry, tag.name.c_str(), tag.value.c_str())) {
        error_message =
            "FlacWriter::Open(): Failed to build a Vorbis comment entry";
        return false;
      }

      // copy = false hands ownership of the entry to the metadata object, so
      // the allocation above is not leaked.
      if (!FLAC__metadata_object_vorbiscomment_append_comment(impl_->metadata,
                                                              entry, false)) {
        error_message =
            "FlacWriter::Open(): Failed to append a Vorbis comment entry";
        return false;
      }
    }

    FLAC__StreamMetadata* blocks[] = {impl_->metadata};
    if (!FLAC__stream_encoder_set_metadata(impl_->encoder, blocks, 1)) {
      error_message =
          "FlacWriter::Open(): Failed to attach metadata to the FLAC encoder";
      return false;
    }
  }

  // init_file, not init_ogg_file. That one call is the whole difference between
  // this and the .ldf the old application writes.
  const std::string utf8_path = PathToUtf8(file_path);
  const FLAC__StreamEncoderInitStatus init_status =
      FLAC__stream_encoder_init_file(impl_->encoder, utf8_path.c_str(),
                                     &Impl::ProgressCallback, impl_.get());
  if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
    error_message =
        std::string(
            "FlacWriter::Open(): Failed to open the FLAC output file: ") +
        FLAC__StreamEncoderInitStatusString[init_status];
    return false;
  }

  impl_->scratch.resize(kEncodeChunkSamples);
  impl_->bytes_written = 0;
  impl_->samples_written = 0;
  impl_->samples_encoded = 0;
  impl_->encoder_initialised = true;
  impl_->finished = false;
  return true;
}

bool FlacWriter::WriteRawDeviceSamples(const uint8_t* device_data,
                                       size_t sample_count) {
  if (!impl_->encoder_initialised) {
    impl_->last_error =
        "FlacWriter::WriteRawDeviceSamples(): The encoder is not open";
    return false;
  }

  size_t remaining = sample_count;
  const uint8_t* read_pointer = device_data;

  while (remaining > 0) {
    const size_t chunk = std::min(remaining, kEncodeChunkSamples);

    // Widen the device's 16-bit words into the int32 buffer libFLAC wants,
    // applying the bias and scale ld-decode calls the DdD 16-bit format.
    // Reading the two bytes individually rather than casting to uint16_t* keeps
    // this correct on a big-endian host and free of alignment assumptions about
    // the buffer it was handed.
    for (size_t i = 0; i < chunk; ++i) {
      const uint16_t ten_bit_value = static_cast<uint16_t>(
          static_cast<uint16_t>(read_pointer[0]) |
          static_cast<uint16_t>(static_cast<uint16_t>(read_pointer[1]) << 8));
      impl_->scratch[i] = ToSigned16Bit(static_cast<int32_t>(ten_bit_value));
      read_pointer += kBytesPerSample;
    }

    if (!FLAC__stream_encoder_process_interleaved(
            impl_->encoder, impl_->scratch.data(),
            static_cast<uint32_t>(chunk))) {
      impl_->RecordEncoderError("WriteRawDeviceSamples");
      return false;
    }

    impl_->samples_written += chunk;
    remaining -= chunk;
  }

  return true;
}

bool FlacWriter::Finish() {
  if (!impl_->encoder_initialised || impl_->finished) {
    return true;
  }

  impl_->finished = true;
  if (!FLAC__stream_encoder_finish(impl_->encoder)) {
    impl_->RecordEncoderError("Finish");
    return false;
  }
  return true;
}

size_t FlacWriter::BytesWritten() const { return impl_->bytes_written.load(); }

size_t FlacWriter::SamplesWritten() const {
  return impl_->samples_written.load();
}

size_t FlacWriter::SamplesPending() const {
  const size_t handed_in = impl_->samples_written.load();
  const size_t committed = impl_->samples_encoded.load();

  // The two counters are read separately and the encoder is running while they
  // are, so the committed figure can briefly be the newer of the two. Clamped
  // rather than subtracted blindly: an unsigned wrap here would show a backlog
  // of eighteen quintillion samples, which is a far more alarming display than
  // the momentary skew it came from.
  return (handed_in > committed) ? (handed_in - committed) : 0;
}

const std::string& FlacWriter::LastError() const { return impl_->last_error; }

}  // namespace ddd::capture
