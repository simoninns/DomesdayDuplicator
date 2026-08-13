/************************************************************************

    capturereader.cpp

    Domesday Duplicator - reading capture files back (P7-23)

    This file is part of the Domesday Duplicator.
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capturereader.h"

#include "captureformat.h"
#include "samplecodec.h"

#include <FLAC/stream_decoder.h>

#include <algorithm>
#include <cstdio>
#include <deque>
#include <fstream>

namespace
{
// Bytes read per call for the two uncompressed formats. A multiple of both group sizes so
// no group is ever split across two reads.
constexpr size_t rawReadChunkBytes = 5 * 4 * 65536;

std::string PathToUtf8(const std::filesystem::path &filePath)
{
    const std::u8string utf8 = filePath.u8string();
    return std::string(utf8.begin(), utf8.end());
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
struct CaptureReader::Impl
{
    Format format = Format::FlacOgg;
    std::string lastError;
    std::optional<uint64_t> totalSamples;

    // Uncompressed formats
    std::ifstream file;
    std::vector<uint8_t> readBuffer;

    // FLAC
    FLAC__StreamDecoder *decoder = nullptr;
    std::deque<uint16_t> decoded;
    bool decoderEndOfStream = false;
    bool decoderFailed = false;

    // The three libFLAC decode callbacks. Members rather than free functions because Impl
    // is private to CaptureReader, and only its own members can name it.
    static FLAC__StreamDecoderWriteStatus WriteCallback(const FLAC__StreamDecoder * /*decoder*/,
                                                        const FLAC__Frame *frame, const FLAC__int32 *const buffer[],
                                                        void *clientData)
    {
        Impl *impl = static_cast<Impl *>(clientData);

        // Mono is not an assumption to make quietly: an .ldf is mono by definition, and
        // reading channel 0 of a stereo file would silently halve the analysed sample rate.
        if (frame->header.channels != 1)
        {
            impl->lastError = "The FLAC stream is not mono, so it is not a Domesday Duplicator capture";
            impl->decoderFailed = true;
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }

        for (uint32_t i = 0; i < frame->header.blocksize; ++i)
        {
            // Back to the 10-bit domain the test pattern counts in. No rounding is needed:
            // every value the encoder wrote came from a 10-bit sample scaled by 64.
            impl->decoded.push_back(static_cast<uint16_t>(SampleCodec::toTenBit(static_cast<int16_t>(buffer[0][i]))));
        }

        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

    static void MetadataCallback(const FLAC__StreamDecoder * /*decoder*/, const FLAC__StreamMetadata *metadata,
                                 void *clientData)
    {
        Impl *impl = static_cast<Impl *>(clientData);

        if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO && metadata->data.stream_info.total_samples != 0)
        {
            impl->totalSamples = metadata->data.stream_info.total_samples;
        }
    }

    static void ErrorCallback(const FLAC__StreamDecoder * /*decoder*/, FLAC__StreamDecoderErrorStatus status,
                              void *clientData)
    {
        Impl *impl = static_cast<Impl *>(clientData);
        impl->lastError = std::string("FLAC decode error: ") + FLAC__StreamDecoderErrorStatusString[status];
        impl->decoderFailed = true;
    }
};

//----------------------------------------------------------------------------------------------------------------------
CaptureReader::CaptureReader() : impl(std::make_unique<Impl>())
{
}

//----------------------------------------------------------------------------------------------------------------------
CaptureReader::~CaptureReader()
{
    if (impl->decoder != nullptr)
    {
        FLAC__stream_decoder_finish(impl->decoder);
        FLAC__stream_decoder_delete(impl->decoder);
        impl->decoder = nullptr;
    }
}

//----------------------------------------------------------------------------------------------------------------------
std::optional<CaptureReader::Format> CaptureReader::FormatFromExtension(const std::filesystem::path &filePath)
{
    std::string extension = filePath.extension().string();
    if (!extension.empty() && extension.front() == '.')
    {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (extension == CaptureFormats::flacExtension)
    {
        return Format::FlacOgg;
    }
    if (extension == CaptureFormats::legacyPackedExtension)
    {
        return Format::Packed10Bit;
    }
    if (extension == CaptureFormats::signed16BitExtension)
    {
        return Format::Signed16Bit;
    }
    return std::nullopt;
}

//----------------------------------------------------------------------------------------------------------------------
const char *CaptureReader::FormatName(Format format)
{
    switch (format)
    {
    case Format::FlacOgg:
        return "Ogg FLAC (.ldf)";
    case Format::Packed10Bit:
        return "packed 10-bit (.lds)";
    case Format::Signed16Bit:
        return "signed 16-bit (.raw)";
    }
    return "unknown";
}

//----------------------------------------------------------------------------------------------------------------------
bool CaptureReader::Open(const std::filesystem::path &filePath, Format format, std::string &errorMessage)
{
    impl->format = format;

    if (format == Format::FlacOgg)
    {
        impl->decoder = FLAC__stream_decoder_new();
        if (impl->decoder == nullptr)
        {
            errorMessage = "Failed to allocate a FLAC decoder";
            return false;
        }

        FLAC__stream_decoder_set_md5_checking(impl->decoder, false);

        const std::string utf8Path = PathToUtf8(filePath);
        const FLAC__StreamDecoderInitStatus initStatus =
            FLAC__stream_decoder_init_ogg_file(impl->decoder, utf8Path.c_str(), &Impl::WriteCallback,
                                               &Impl::MetadataCallback, &Impl::ErrorCallback, impl.get());
        if (initStatus != FLAC__STREAM_DECODER_INIT_STATUS_OK)
        {
            errorMessage = std::string("Failed to open the FLAC file: ") + FLAC__StreamDecoderInitStatusString[initStatus];
            return false;
        }

        // Reading the metadata up front is what fills in the total sample count, and it is
        // also the first point at which a file that is not really an Ogg FLAC stream says so.
        if (!FLAC__stream_decoder_process_until_end_of_metadata(impl->decoder) || impl->decoderFailed)
        {
            errorMessage = impl->lastError.empty() ? "The file is not a readable Ogg FLAC stream" : impl->lastError;
            return false;
        }

        return true;
    }

    impl->file.open(filePath, std::ios::in | std::ios::binary);
    if (!impl->file.is_open())
    {
        errorMessage = "Failed to open the capture file";
        return false;
    }

    std::error_code sizeError;
    const uintmax_t fileSize = std::filesystem::file_size(filePath, sizeError);
    if (!sizeError)
    {
        impl->totalSamples = (format == Format::Packed10Bit)
                                 ? (static_cast<uint64_t>(fileSize) / SampleCodec::bytesPerGroup) * SampleCodec::samplesPerGroup
                                 : static_cast<uint64_t>(fileSize) / 2;
    }

    impl->readBuffer.resize(rawReadChunkBytes);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
bool CaptureReader::Read(std::vector<uint16_t> &samples, size_t maxSamples, bool &endOfFile)
{
    samples.clear();
    endOfFile = false;

    if (impl->format == Format::FlacOgg)
    {
        // Decode frame by frame until there is enough buffered or the stream ends. The
        // decoder hands over whole frames through the write callback, so the surplus stays
        // queued for the next call rather than being decoded twice.
        while (impl->decoded.size() < maxSamples && !impl->decoderEndOfStream)
        {
            if (!FLAC__stream_decoder_process_single(impl->decoder) || impl->decoderFailed)
            {
                if (impl->lastError.empty())
                {
                    impl->lastError = "The FLAC stream could not be decoded";
                }
                return false;
            }

            if (FLAC__stream_decoder_get_state(impl->decoder) == FLAC__STREAM_DECODER_END_OF_STREAM)
            {
                impl->decoderEndOfStream = true;
            }
        }

        const size_t take = std::min(maxSamples, impl->decoded.size());
        samples.assign(impl->decoded.begin(), impl->decoded.begin() + static_cast<std::ptrdiff_t>(take));
        impl->decoded.erase(impl->decoded.begin(), impl->decoded.begin() + static_cast<std::ptrdiff_t>(take));
        endOfFile = impl->decoderEndOfStream && impl->decoded.empty();
        return true;
    }

    if (impl->format == Format::Packed10Bit)
    {
        const size_t groupsWanted = std::min(maxSamples / SampleCodec::samplesPerGroup,
                                             impl->readBuffer.size() / SampleCodec::bytesPerGroup);
        const size_t bytesWanted = groupsWanted * SampleCodec::bytesPerGroup;

        impl->file.read(reinterpret_cast<char *>(impl->readBuffer.data()), static_cast<std::streamsize>(bytesWanted));
        const size_t bytesRead = static_cast<size_t>(impl->file.gcount());
        if (bytesRead == 0)
        {
            endOfFile = true;
            return true;
        }

        // A trailing partial group is not decodable — five bytes are needed for four
        // samples — so it is dropped, and the count says how many samples that cost.
        const size_t groupsRead = bytesRead / SampleCodec::bytesPerGroup;
        samples.resize(groupsRead * SampleCodec::samplesPerGroup);
        for (size_t group = 0; group < groupsRead; ++group)
        {
            SampleCodec::unpackGroupTenBit(impl->readBuffer.data() + (group * SampleCodec::bytesPerGroup),
                                           samples.data() + (group * SampleCodec::samplesPerGroup));
        }

        endOfFile = bytesRead < bytesWanted;
        return true;
    }

    // Signed 16-bit
    const size_t samplesWanted = std::min(maxSamples, impl->readBuffer.size() / 2);
    impl->file.read(reinterpret_cast<char *>(impl->readBuffer.data()), static_cast<std::streamsize>(samplesWanted * 2));
    const size_t bytesRead = static_cast<size_t>(impl->file.gcount());
    if (bytesRead == 0)
    {
        endOfFile = true;
        return true;
    }

    const size_t samplesRead = bytesRead / 2;
    samples.resize(samplesRead);
    for (size_t i = 0; i < samplesRead; ++i)
    {
        const int16_t value = static_cast<int16_t>(static_cast<uint16_t>(impl->readBuffer[i * 2]) |
                                                   static_cast<uint16_t>(static_cast<uint16_t>(impl->readBuffer[(i * 2) + 1]) << 8));
        samples[i] = static_cast<uint16_t>(SampleCodec::toTenBit(value));
    }

    endOfFile = bytesRead < (samplesWanted * 2);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
std::optional<uint64_t> CaptureReader::GetTotalSamples() const
{
    return impl->totalSamples;
}

//----------------------------------------------------------------------------------------------------------------------
const std::string &CaptureReader::GetLastError() const
{
    return impl->lastError;
}
