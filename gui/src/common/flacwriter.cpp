/************************************************************************

    flacwriter.cpp

    Domesday Duplicator - Ogg FLAC capture output (P7-21)

    This file is part of the Domesday Duplicator.
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "flacwriter.h"

#include "samplecodec.h"

#include <FLAC/metadata.h>
#include <FLAC/stream_encoder.h>

#include <algorithm>
#include <atomic>
#include <thread>

namespace
{
// Samples per call to the encoder. Large enough that the per-call overhead disappears,
// small enough that the widened scratch buffer stays cache-friendly.
constexpr size_t encodeChunkSamples = 65536;

// ld-compress caps its own -j here, having observed throughput plateau once feeding the
// encoder becomes the bottleneck rather than the encoding itself. The same applies here,
// and the samples arrive on one thread.
constexpr unsigned int maxThreads = 8;

std::string PathToUtf8(const std::filesystem::path &filePath)
{
    // libFLAC documents its filenames as UTF-8 on every platform, including Windows, where
    // it widens them itself. std::u8string is a distinct type in C++20, hence the copy.
    const std::u8string utf8 = filePath.u8string();
    return std::string(utf8.begin(), utf8.end());
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
struct FlacWriter::Impl
{
    FLAC__StreamEncoder *encoder = nullptr;
    FLAC__StreamMetadata *metadata = nullptr;
    bool encoderInitialised = false;
    bool finished = false;
    std::string lastError;

    // libFLAC takes one int32 per sample, so the device's 16-bit words are widened here.
    // Sized once at Open() and reused, never grown on the capture path.
    std::vector<int32_t> scratch;

    std::atomic<size_t> bytesWritten{ 0 };
    std::atomic<size_t> samplesWritten{ 0 };

    void RecordEncoderError(const char *context)
    {
        const FLAC__StreamEncoderState state = FLAC__stream_encoder_get_state(encoder);
        lastError = std::string("FlacWriter::") + context + "(): " + FLAC__StreamEncoderStateString[state];
    }

    // libFLAC calls this once per frame written. A member rather than a free function
    // because Impl is private to FlacWriter, and only its own members can name it.
    static void ProgressCallback(const FLAC__StreamEncoder * /*encoder*/, FLAC__uint64 bytesWritten,
                                 FLAC__uint64 /*samplesWritten*/, uint32_t /*framesWritten*/,
                                 uint32_t /*totalFramesEstimate*/, void *clientData)
    {
        static_cast<Impl *>(clientData)->bytesWritten = static_cast<size_t>(bytesWritten);
    }
};

//----------------------------------------------------------------------------------------------------------------------
FlacWriter::FlacWriter() : impl(std::make_unique<Impl>())
{
}

//----------------------------------------------------------------------------------------------------------------------
FlacWriter::~FlacWriter()
{
    Finish();

    if (impl->metadata != nullptr)
    {
        FLAC__metadata_object_delete(impl->metadata);
        impl->metadata = nullptr;
    }
    if (impl->encoder != nullptr)
    {
        FLAC__stream_encoder_delete(impl->encoder);
        impl->encoder = nullptr;
    }
}

//----------------------------------------------------------------------------------------------------------------------
bool FlacWriter::SupportsMultithreading()
{
    // FLAC__stream_encoder_set_num_threads and its result constants arrived together in
    // libFLAC 1.5.0. Testing for the constant rather than a version macro stays honest
    // against distributions that backport.
#ifdef FLAC__STREAM_ENCODER_SET_NUM_THREADS_OK
    return true;
#else
    return false;
#endif
}

//----------------------------------------------------------------------------------------------------------------------
bool FlacWriter::Open(const std::filesystem::path &filePath, const Options &options, std::string &errorMessage)
{
    if (impl->encoderInitialised)
    {
        errorMessage = "FlacWriter::Open(): The encoder is already open";
        return false;
    }

    impl->encoder = FLAC__stream_encoder_new();
    if (impl->encoder == nullptr)
    {
        errorMessage = "FlacWriter::Open(): Failed to allocate a FLAC encoder";
        return false;
    }

    // The stream description. These have to match lddecode/compress.py's flac invocation
    // exactly, or the result is a FLAC file that is not an .ldf.
    bool ok = true;
    ok = ok && FLAC__stream_encoder_set_channels(impl->encoder, 1);
    ok = ok && FLAC__stream_encoder_set_bits_per_sample(impl->encoder, 16);
    ok = ok && FLAC__stream_encoder_set_sample_rate(impl->encoder, options.sampleRateLabel);
    ok = ok && FLAC__stream_encoder_set_compression_level(
                   impl->encoder, static_cast<uint32_t>(std::clamp(options.compressionLevel, 0, 8)));

    // Verification re-decodes every frame as it is written and compares. That is the wrong
    // trade on a real-time path — it roughly doubles the cost of the thing most likely to
    // run out of CPU — and the integrity of the result is checked instead by decoding the
    // finished file, which costs nothing during the capture itself.
    ok = ok && FLAC__stream_encoder_set_verify(impl->encoder, false);

    // A capture has no known length, so the estimate is zero rather than a guess.
    ok = ok && FLAC__stream_encoder_set_total_samples_estimate(impl->encoder, 0);

    if (!ok)
    {
        errorMessage = "FlacWriter::Open(): Failed to configure the FLAC encoder";
        return false;
    }

#ifdef FLAC__STREAM_ENCODER_SET_NUM_THREADS_OK
    {
        unsigned int threads = options.threads;
        if (threads == 0)
        {
            const unsigned int cores = std::max(1u, std::thread::hardware_concurrency());
            threads = std::min(cores, maxThreads);
        }

        // Not fatal if it is refused. A single-threaded encode is slower, not wrong, and the
        // capture is better attempted than declined — the buffer overrun detection on the
        // capture path is what catches a machine that then cannot keep up.
        if (FLAC__stream_encoder_set_num_threads(impl->encoder, std::min(threads, maxThreads)) !=
            FLAC__STREAM_ENCODER_SET_NUM_THREADS_OK)
        {
            impl->lastError = "The FLAC encoder refused the requested thread count; encoding single-threaded";
        }
    }
#endif

    // Provenance, so a capture separated from its .json sidecar still names its origin
    // (P7-25). Unknown comments are ignored by every decoder, so this cannot break
    // ld-decode.
    if (!options.tags.empty())
    {
        impl->metadata = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
        if (impl->metadata == nullptr)
        {
            errorMessage = "FlacWriter::Open(): Failed to allocate the Vorbis comment block";
            return false;
        }

        for (const Tag &tag : options.tags)
        {
            FLAC__StreamMetadata_VorbisComment_Entry entry{};
            if (!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, tag.name.c_str(),
                                                                                tag.value.c_str()))
            {
                errorMessage = "FlacWriter::Open(): Failed to build a Vorbis comment entry";
                return false;
            }

            // copy = false hands ownership of the entry to the metadata object, so the
            // allocation above is not leaked.
            if (!FLAC__metadata_object_vorbiscomment_append_comment(impl->metadata, entry, false))
            {
                errorMessage = "FlacWriter::Open(): Failed to append a Vorbis comment entry";
                return false;
            }
        }

        FLAC__StreamMetadata *blocks[] = { impl->metadata };
        if (!FLAC__stream_encoder_set_metadata(impl->encoder, blocks, 1))
        {
            errorMessage = "FlacWriter::Open(): Failed to attach metadata to the FLAC encoder";
            return false;
        }
    }

    const std::string utf8Path = PathToUtf8(filePath);
    const FLAC__StreamEncoderInitStatus initStatus =
        FLAC__stream_encoder_init_ogg_file(impl->encoder, utf8Path.c_str(), &Impl::ProgressCallback, impl.get());
    if (initStatus != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
    {
        errorMessage = std::string("FlacWriter::Open(): Failed to open the FLAC output file: ") +
                       FLAC__StreamEncoderInitStatusString[initStatus];
        return false;
    }

    impl->scratch.resize(encodeChunkSamples);
    impl->bytesWritten = 0;
    impl->samplesWritten = 0;
    impl->encoderInitialised = true;
    impl->finished = false;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FlacWriter::WriteRawDeviceSamples(const uint8_t *deviceData, size_t sampleCount, size_t stride)
{
    if (!impl->encoderInitialised)
    {
        impl->lastError = "FlacWriter::WriteRawDeviceSamples(): The encoder is not open";
        return false;
    }
    if (stride == 0)
    {
        impl->lastError = "FlacWriter::WriteRawDeviceSamples(): A stride of zero was requested";
        return false;
    }

    size_t remaining = sampleCount / stride;
    const uint8_t *readPointer = deviceData;

    while (remaining > 0)
    {
        const size_t chunk = std::min(remaining, encodeChunkSamples);

        // Widen the device's 16-bit words into the int32 buffer libFLAC wants, applying the
        // bias and scale ld-decode calls the DdD 16-bit format. Reading the two bytes
        // individually rather than casting to uint16_t* keeps this correct on a big-endian
        // host and free of alignment assumptions about the USB buffer.
        for (size_t i = 0; i < chunk; ++i)
        {
            const uint16_t tenBitValue =
                static_cast<uint16_t>(readPointer[0]) | static_cast<uint16_t>(static_cast<uint16_t>(readPointer[1]) << 8);
            impl->scratch[i] = SampleCodec::toSixteenBit(static_cast<int32_t>(tenBitValue));
            readPointer += 2 * stride;
        }

        if (!FLAC__stream_encoder_process_interleaved(impl->encoder, impl->scratch.data(), static_cast<uint32_t>(chunk)))
        {
            impl->RecordEncoderError("WriteRawDeviceSamples");
            return false;
        }

        impl->samplesWritten += chunk;
        remaining -= chunk;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool FlacWriter::Finish()
{
    if (!impl->encoderInitialised || impl->finished)
    {
        return true;
    }

    impl->finished = true;
    if (!FLAC__stream_encoder_finish(impl->encoder))
    {
        impl->RecordEncoderError("Finish");
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
size_t FlacWriter::GetBytesWritten() const
{
    return impl->bytesWritten.load();
}

//----------------------------------------------------------------------------------------------------------------------
size_t FlacWriter::GetSamplesWritten() const
{
    return impl->samplesWritten.load();
}

//----------------------------------------------------------------------------------------------------------------------
const std::string &FlacWriter::GetLastError() const
{
    return impl->lastError;
}
