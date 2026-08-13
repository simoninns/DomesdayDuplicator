/************************************************************************

    captureformat.h

    Domesday Duplicator - what the capture application writes

    One place for the facts every other file needs to agree on: the formats, their
    extensions, and the sample-rate label that goes into a FLAC header.

    This file is part of the Domesday Duplicator.
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#ifndef CAPTUREFORMAT_H
#define CAPTUREFORMAT_H

#include <cstdint>

namespace CaptureFormats
{

// What the application writes. Packed 10-bit (.lds) was removed in P7-22 — it is still
// *read* (see capturereader.h), because the existing captures do not disappear when the
// writer does.
enum class Format
{
    // Ogg FLAC, mono, 16-bit signed. Byte-compatible with ld-decode's .ldf, so the decode
    // toolchain reads it with no conversion step. The default.
    FlacOgg,

    // Uncompressed signed 16-bit, the same samples the FLAC path encodes. The fallback for
    // a machine that cannot sustain the encoder, and the reason the compressor being on the
    // capture path is a recoverable situation rather than a dead end.
    Signed16Bit,
};

// The real sampling rate of the device
inline constexpr uint32_t sampleRateHz = 40000000;

// FLAC's sample-rate field cannot hold 40 MHz — the format tops out at 655350 Hz — so .ldf
// stamps 40000 and everything downstream treats it as a label rather than a rate. This
// matches lddecode/compress.py's SAMPLE_RATE, and a different value here would produce a
// file ld-decode reads at the wrong speed.
inline constexpr uint32_t flacSampleRateLabel = 40000;

// A 4:1 decimated capture is a real 10 Msps stream, so it says so. Same convention: the
// label is the rate in kHz-scaled form that 40 Msps maps to.
inline constexpr uint32_t flacSampleRateLabelDecimated = 10000;

// File name extensions, without the dot
inline constexpr const char *flacExtension = "ldf";
inline constexpr const char *signed16BitExtension = "raw";
inline constexpr const char *legacyPackedExtension = "lds";

} // namespace CaptureFormats

#endif // CAPTUREFORMAT_H
