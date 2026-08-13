/************************************************************************

    flacwriter.h

    Domesday Duplicator - Ogg FLAC capture output (P7-21)

    Writes the .ldf ld-decode already consumes: mono, 16-bit signed, Ogg-encapsulated FLAC,
    with the sample-rate field stamped rather than measured (see captureformat.h).

    Why libFLAC in-process rather than piping to the flac binary, which is what
    ld-decode's ld-compress does: the three installers this application now ships as would
    each have to bundle, locate and version-check an external executable, and a subprocess
    on the capture path is one more thing that can die forty minutes into a capture. The
    library is BSD-licensed and so is fine to link into a GPLv3 application.

    The libFLAC types are anonymous struct typedefs and cannot be forward-declared, so the
    encoder state lives behind a pimpl rather than leaking FLAC's headers into everything
    that captures. This class is also deliberately Qt-free, so the tests can drive it
    without a QApplication.

    This file is part of the Domesday Duplicator.
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#ifndef FLACWRITER_H
#define FLACWRITER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class FlacWriter
{
public:
    struct Tag
    {
        std::string name;
        std::string value;
    };

    struct Options
    {
        // 0-8, as flac's -0 .. -8. ld-compress defaults to 8 because it post-processes a
        // finished file with no deadline; this runs while 40 million samples a second are
        // arriving, so the default here is low and the setting exists to be raised only as
        // far as a given machine's measurements allow.
        int compressionLevel = 1;

        // 0 asks for one thread per core, capped at 8. Multithreaded encoding needs libFLAC
        // 1.5.0 or later; on anything older this is silently a single-threaded encode,
        // which is why the level default is conservative.
        unsigned int threads = 0;

        // Written into the STREAMINFO sample-rate field. Not a measurement — see
        // captureformat.h.
        uint32_t sampleRateLabel = 40000;

        // Vorbis comments, so a capture that has been separated from its .json sidecar can
        // still say which build produced it (P7-25).
        std::vector<Tag> tags;
    };

public:
    FlacWriter();
    ~FlacWriter();

    FlacWriter(const FlacWriter &) = delete;
    FlacWriter &operator=(const FlacWriter &) = delete;

    // Open the output file and configure the encoder. Returns false and fills errorMessage
    // on failure; nothing is left on disk in that case.
    bool Open(const std::filesystem::path &filePath, const Options &options, std::string &errorMessage);

    // Encode sampleCount samples of raw device data.
    //
    // The input is the device's own layout — 16-bit little-endian words each holding a
    // 10-bit unsigned sample — because that is what sits in the disk buffer, and copying it
    // into an intermediate form first would be a memcpy of 80 MB/s for nothing. stride
    // selects every nth sample, which is how 4:1 decimation for CD RF reaches this path
    // without a second code route (P7-22).
    bool WriteRawDeviceSamples(const uint8_t *deviceData, size_t sampleCount, size_t stride = 1);

    // Flush the encoder and close the file. Safe to call twice; the destructor calls it.
    bool Finish();

    // Bytes on disk so far, from the encoder's own progress callback. The UI needs this
    // because with a compressor in the path the file size no longer follows from the sample
    // count.
    size_t GetBytesWritten() const;

    // Samples handed to the encoder so far
    size_t GetSamplesWritten() const;

    const std::string &GetLastError() const;

    // Whether the libFLAC this was built against can encode on more than one thread. False
    // means one core is doing all of it, which the capture dialog says out loud rather than
    // leaving as an unexplained shortfall.
    static bool SupportsMultithreading();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // FLACWRITER_H
