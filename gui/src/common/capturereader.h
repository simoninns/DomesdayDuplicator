/************************************************************************

    capturereader.h

    Domesday Duplicator - reading capture files back (P7-23)

    Yields 10-bit unsigned sample values from any of the three capture formats:

        .ldf   Ogg FLAC, what the application writes as of P7-21
        .lds   packed 10-bit, what it wrote before that
        .raw   uncompressed signed 16-bit

    The .lds path is not legacy support out of politeness — years of captures exist in that
    format, and the writer going away does not make them unreadable. The 10-bit domain is
    the common currency because that is what the FPGA's test pattern counts in, so the ramp
    check in testdataanalyser.h only means anything there.

    Qt-free, like the rest of src/common, so the tests can drive it directly.

    This file is part of the Domesday Duplicator.
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#ifndef CAPTUREREADER_H
#define CAPTUREREADER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class CaptureReader
{
public:
    enum class Format
    {
        FlacOgg,
        Packed10Bit,
        Signed16Bit,
    };

public:
    CaptureReader();
    ~CaptureReader();

    CaptureReader(const CaptureReader &) = delete;
    CaptureReader &operator=(const CaptureReader &) = delete;

    // Guess the format from the file name extension. Returns nothing for an extension that
    // is not one of the three, so the caller can say so rather than guessing wrong and
    // reporting the resulting nonsense as data corruption.
    static std::optional<Format> FormatFromExtension(const std::filesystem::path &filePath);

    static const char *FormatName(Format format);

    bool Open(const std::filesystem::path &filePath, Format format, std::string &errorMessage);

    // Read up to maxSamples 10-bit values into samples. Returns false on a read or decode
    // error. A short read is not an error: endOfFile says whether there is more.
    bool Read(std::vector<uint16_t> &samples, size_t maxSamples, bool &endOfFile);

    // Total samples in the file, where that is knowable — from the file size for the two
    // uncompressed formats, and from STREAMINFO for FLAC. A streamed FLAC whose header was
    // never patched reports nothing, and callers show indeterminate progress rather than a
    // fabricated percentage.
    std::optional<uint64_t> GetTotalSamples() const;

    const std::string &GetLastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // CAPTUREREADER_H
