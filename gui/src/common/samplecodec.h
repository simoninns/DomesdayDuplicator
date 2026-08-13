/************************************************************************

    samplecodec.h

    Domesday Duplicator - the 10-bit packed / 16-bit signed sample codec

    Format
    ------

    Four consecutive 10-bit samples pack into five bytes, most significant bit first:

        byte 0: 0000 0000    sample 0, bits 9..2
        byte 1: 0011 1111    sample 0, bits 1..0 | sample 1, bits 9..4
        byte 2: 1111 2222    sample 1, bits 3..0 | sample 2, bits 9..6
        byte 3: 2222 3333    sample 2, bits 5..0 | sample 3, bits 9..8
        byte 4: 3333 3333    sample 3, bits 7..0

    The unpacked form is signed 16-bit, scaled and biased so that the 10-bit unsigned range
    0..1023 maps onto the signed 16-bit range: unpacked = (packed - 512) * 64.

    That expression is not a local convention. ld-decode's lddecode/lds.py calls it "the DdD
    16-bit format" and unpacks .lds into exactly it before handing the samples to flac, which
    is why the .ldf this application now writes (see flacwriter.h) needs no format
    negotiation with the decode toolchain: the sample values were always the same, only the
    container differed.

    Status after P7-22
    ------------------

    The capture application no longer *writes* packed 10-bit data, so unpackGroup() is the
    half with production callers — capturereader.cpp uses it to read the .lds files years of
    captures already exist in. packGroup() is retained deliberately: it is the exact inverse
    the round-trip property test in tests/test_samplecodec.cpp asserts against, and that
    round-trip is the strongest evidence the unpacker is right. Removing it would leave the
    reader covered by golden vectors alone.

    This file is part of the Domesday Duplicator.
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#ifndef SAMPLECODEC_H
#define SAMPLECODEC_H

#include <cstdint>

namespace SampleCodec
{

// One packed group is 4 samples in 5 bytes
inline constexpr int samplesPerGroup = 4;
inline constexpr int bytesPerGroup = 5;

// The 10-bit sample value that maps to signed-16-bit zero
inline constexpr int32_t zeroOffset = 512;

// Scale factor between the 10-bit and 16-bit representations
inline constexpr int32_t scale = 64;

// Convert one 16-bit signed sample to its 10-bit unsigned representation.
inline constexpr int32_t toTenBit(int16_t sample)
{
    return (sample / scale) + zeroOffset;
}

// Convert one 10-bit unsigned sample to its 16-bit signed representation.
inline constexpr int16_t toSixteenBit(int32_t sample)
{
    return static_cast<int16_t>((sample - zeroOffset) * scale);
}

// Pack four 16-bit signed samples into five bytes.
inline void packGroup(const int16_t *input, uint8_t *output)
{
    const int32_t word0 = toTenBit(input[0]);
    const int32_t word1 = toTenBit(input[1]);
    const int32_t word2 = toTenBit(input[2]);
    const int32_t word3 = toTenBit(input[3]);

    output[0] = static_cast<uint8_t>((word0 & 0x03FC) >> 2);
    output[1] = static_cast<uint8_t>(((word0 & 0x0003) << 6) + ((word1 & 0x03F0) >> 4));
    output[2] = static_cast<uint8_t>(((word1 & 0x000F) << 4) + ((word2 & 0x03C0) >> 6));
    output[3] = static_cast<uint8_t>(((word2 & 0x003F) << 2) + ((word3 & 0x0300) >> 8));
    output[4] = static_cast<uint8_t>(word3 & 0x00FF);
}

// Unpack five bytes into four 16-bit signed samples.
inline void unpackGroup(const uint8_t *input, int16_t *output)
{
    const int32_t byte0 = input[0];
    const int32_t byte1 = input[1];
    const int32_t byte2 = input[2];
    const int32_t byte3 = input[3];
    const int32_t byte4 = input[4];

    // Multiplication rather than left-shift, to keep the arithmetic free of the implicit
    // conversions that a shift on a promoted signed char introduces
    const int32_t word0 = ((byte0 & 0xFF) * 4) + ((byte1 & 0xC0) >> 6);
    const int32_t word1 = ((byte1 & 0x3F) * 16) + ((byte2 & 0xF0) >> 4);
    const int32_t word2 = ((byte2 & 0x0F) * 64) + ((byte3 & 0xFC) >> 2);
    const int32_t word3 = ((byte3 & 0x03) * 256) + (byte4 & 0xFF);

    output[0] = toSixteenBit(word0);
    output[1] = toSixteenBit(word1);
    output[2] = toSixteenBit(word2);
    output[3] = toSixteenBit(word3);
}

// Unpack five bytes into four 10-bit unsigned samples.
//
// The reader wants the raw 10-bit values rather than the scaled 16-bit ones: the test
// pattern the FPGA generates is a 10-bit counter, so the ramp check in testdataanalyser.h
// is only meaningful in that domain.
inline void unpackGroupTenBit(const uint8_t *input, uint16_t *output)
{
    const int32_t byte0 = input[0];
    const int32_t byte1 = input[1];
    const int32_t byte2 = input[2];
    const int32_t byte3 = input[3];
    const int32_t byte4 = input[4];

    output[0] = static_cast<uint16_t>(((byte0 & 0xFF) * 4) + ((byte1 & 0xC0) >> 6));
    output[1] = static_cast<uint16_t>(((byte1 & 0x3F) * 16) + ((byte2 & 0xF0) >> 4));
    output[2] = static_cast<uint16_t>(((byte2 & 0x0F) * 64) + ((byte3 & 0xFC) >> 2));
    output[3] = static_cast<uint16_t>(((byte3 & 0x03) * 256) + (byte4 & 0xFF));
}

} // namespace SampleCodec

#endif // SAMPLECODEC_H
