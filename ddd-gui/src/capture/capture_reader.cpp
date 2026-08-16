/************************************************************************

    capture_reader.cpp

    Reading capture files back
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_reader.h"

#include <FLAC/stream_decoder.h>

#include <algorithm>
#include <deque>
#include <fstream>

#include "capture_format.h"
#include "sample_format.h"

namespace ddd::capture {
namespace {

// Bytes read per call for the uncompressed format
constexpr size_t kRawReadChunkBytes = size_t{2} * 65'536;

std::string PathToUtf8(const std::filesystem::path& file_path) {
  const std::u8string utf8 = file_path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

}  // namespace

struct CaptureReader::Impl {
  Format format = Format::kFlac;
  std::string last_error;
  std::optional<uint64_t> total_samples;
  std::vector<std::pair<std::string, std::string>> tags;

  // Uncompressed
  std::ifstream file;
  std::vector<uint8_t> read_buffer;

  // FLAC
  FLAC__StreamDecoder* decoder = nullptr;
  std::deque<uint16_t> decoded;
  bool decoder_end_of_stream = false;
  bool decoder_failed = false;

  // The three libFLAC decode callbacks. Members rather than free functions
  // because Impl is private to CaptureReader, and only its own members can name
  // it.
  static FLAC__StreamDecoderWriteStatus WriteCallback(
      const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame,
      const FLAC__int32* const buffer[], void* client_data) {
    Impl* impl = static_cast<Impl*>(client_data);

    // Mono is not an assumption to make quietly: a capture is mono by
    // definition, and reading channel 0 of a stereo file would silently halve
    // the analysed sample rate.
    if (frame->header.channels != kFlacChannels) {
      impl->last_error =
          "The FLAC stream is not mono, so it is not a Domesday Duplicator "
          "capture";
      impl->decoder_failed = true;
      return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    for (uint32_t i = 0; i < frame->header.blocksize; ++i) {
      // Back to the 10-bit domain the test pattern counts in. No rounding is
      // needed: every value the encoder wrote came from a 10-bit sample scaled
      // by 64.
      impl->decoded.push_back(
          static_cast<uint16_t>(ToTenBit(static_cast<int16_t>(buffer[0][i]))));
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
  }

  static void MetadataCallback(const FLAC__StreamDecoder* /*decoder*/,
                               const FLAC__StreamMetadata* metadata,
                               void* client_data) {
    Impl* impl = static_cast<Impl*>(client_data);

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO &&
        metadata->data.stream_info.total_samples != 0) {
      impl->total_samples = metadata->data.stream_info.total_samples;
    }

    if (metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
      const FLAC__StreamMetadata_VorbisComment& comment =
          metadata->data.vorbis_comment;
      for (uint32_t i = 0; i < comment.num_comments; ++i) {
        const FLAC__StreamMetadata_VorbisComment_Entry& entry =
            comment.comments[i];
        const std::string text(reinterpret_cast<const char*>(entry.entry),
                               entry.length);
        const size_t separator = text.find('=');
        if (separator == std::string::npos) {
          continue;
        }
        impl->tags.emplace_back(text.substr(0, separator),
                                text.substr(separator + 1));
      }
    }
  }

  static void ErrorCallback(const FLAC__StreamDecoder* /*decoder*/,
                            FLAC__StreamDecoderErrorStatus status,
                            void* client_data) {
    Impl* impl = static_cast<Impl*>(client_data);
    impl->last_error = std::string("FLAC decode error: ") +
                       FLAC__StreamDecoderErrorStatusString[status];
    impl->decoder_failed = true;
  }
};

CaptureReader::CaptureReader() : impl_(std::make_unique<Impl>()) {}

CaptureReader::~CaptureReader() {
  if (impl_->decoder != nullptr) {
    FLAC__stream_decoder_finish(impl_->decoder);
    FLAC__stream_decoder_delete(impl_->decoder);
    impl_->decoder = nullptr;
  }
}

std::optional<CaptureReader::Format> CaptureReader::FormatFromExtension(
    const std::filesystem::path& file_path) {
  const std::string extension = LowerCaseExtension(file_path);

  if (extension == kFlacExtension) {
    return Format::kFlac;
  }
  if (extension == kSigned16BitExtension ||
      extension == kLegacySigned16BitExtension) {
    return Format::kSigned16Bit;
  }
  return std::nullopt;
}

const char* CaptureReader::FormatName(Format format) {
  switch (format) {
    case Format::kFlac:
      return "FLAC (.flac)";
    case Format::kSigned16Bit:
      return "signed 16-bit (.s16, .raw)";
  }
  return "unknown";
}

bool CaptureReader::Open(const std::filesystem::path& file_path, Format format,
                         std::string& error_message) {
  impl_->format = format;

  if (format == Format::kFlac) {
    impl_->decoder = FLAC__stream_decoder_new();
    if (impl_->decoder == nullptr) {
      error_message = "Failed to allocate a FLAC decoder";
      return false;
    }

    FLAC__stream_decoder_set_md5_checking(impl_->decoder, false);

    // STREAMINFO is delivered by default; the comment block has to be asked
    // for, and without it a capture's provenance tags are unreadable.
    FLAC__stream_decoder_set_metadata_respond(
        impl_->decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);

    const std::string utf8_path = PathToUtf8(file_path);
    const FLAC__StreamDecoderInitStatus init_status =
        FLAC__stream_decoder_init_file(
            impl_->decoder, utf8_path.c_str(), &Impl::WriteCallback,
            &Impl::MetadataCallback, &Impl::ErrorCallback, impl_.get());
    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
      error_message = std::string("Failed to open the FLAC file: ") +
                      FLAC__StreamDecoderInitStatusString[init_status];
      return false;
    }

    // Reading the metadata up front is what fills in the total sample count and
    // the tags, and it is also the first point at which a file that is not
    // really a FLAC stream says so.
    if (!FLAC__stream_decoder_process_until_end_of_metadata(impl_->decoder) ||
        impl_->decoder_failed) {
      error_message = impl_->last_error.empty()
                          ? "The file is not a readable FLAC stream"
                          : impl_->last_error;
      return false;
    }

    return true;
  }

  impl_->file.open(file_path, std::ios::in | std::ios::binary);
  if (!impl_->file.is_open()) {
    error_message = "Failed to open the capture file";
    return false;
  }

  std::error_code size_error;
  const uintmax_t file_size = std::filesystem::file_size(file_path, size_error);
  if (!size_error) {
    impl_->total_samples = static_cast<uint64_t>(file_size) / kBytesPerSample;
  }

  impl_->read_buffer.resize(kRawReadChunkBytes);
  return true;
}

bool CaptureReader::Read(std::vector<uint16_t>& samples, size_t max_samples,
                         bool& end_of_file) {
  samples.clear();
  end_of_file = false;

  if (impl_->format == Format::kFlac) {
    // Decode frame by frame until there is enough buffered or the stream ends.
    // The decoder hands over whole frames through the write callback, so the
    // surplus stays queued for the next call rather than being decoded twice.
    while (impl_->decoded.size() < max_samples &&
           !impl_->decoder_end_of_stream) {
      if (!FLAC__stream_decoder_process_single(impl_->decoder) ||
          impl_->decoder_failed) {
        if (impl_->last_error.empty()) {
          impl_->last_error = "The FLAC stream could not be decoded";
        }
        return false;
      }

      if (FLAC__stream_decoder_get_state(impl_->decoder) ==
          FLAC__STREAM_DECODER_END_OF_STREAM) {
        impl_->decoder_end_of_stream = true;
      }
    }

    const size_t take = std::min(max_samples, impl_->decoded.size());
    samples.assign(impl_->decoded.begin(),
                   impl_->decoded.begin() + static_cast<std::ptrdiff_t>(take));
    impl_->decoded.erase(
        impl_->decoded.begin(),
        impl_->decoded.begin() + static_cast<std::ptrdiff_t>(take));
    end_of_file = impl_->decoder_end_of_stream && impl_->decoded.empty();
    return true;
  }

  const size_t samples_wanted =
      std::min(max_samples, impl_->read_buffer.size() / kBytesPerSample);
  impl_->file.read(
      reinterpret_cast<char*>(impl_->read_buffer.data()),
      static_cast<std::streamsize>(samples_wanted * kBytesPerSample));
  const size_t bytes_read = static_cast<size_t>(impl_->file.gcount());
  if (bytes_read == 0) {
    end_of_file = true;
    return true;
  }

  const size_t samples_read = bytes_read / kBytesPerSample;
  samples.resize(samples_read);
  for (size_t i = 0; i < samples_read; ++i) {
    const int16_t value = static_cast<int16_t>(
        static_cast<uint16_t>(impl_->read_buffer[i * kBytesPerSample]) |
        static_cast<uint16_t>(
            static_cast<uint16_t>(impl_->read_buffer[(i * kBytesPerSample) + 1])
            << 8));
    samples[i] = static_cast<uint16_t>(ToTenBit(value));
  }

  end_of_file = bytes_read < (samples_wanted * kBytesPerSample);
  return true;
}

std::optional<uint64_t> CaptureReader::TotalSamples() const {
  return impl_->total_samples;
}

const std::vector<std::pair<std::string, std::string>>& CaptureReader::Tags()
    const {
  return impl_->tags;
}

const std::string& CaptureReader::LastError() const {
  return impl_->last_error;
}

}  // namespace ddd::capture
