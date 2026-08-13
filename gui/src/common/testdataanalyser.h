/************************************************************************

    testdataanalyser.h

    Domesday Duplicator - test-pattern integrity check (P7-19)

    Step 4 of the capture-integrity procedure in TESTING.md. With the FPGA's test-pattern
    generator running, every sample the device produces is the previous one plus one,
    wrapping at the end of the sequence, so any break in that ramp is a sample the capture
    path lost or corrupted. That makes this the one check that covers the whole chain —
    ADC, gateware, FX3, USB, buffering and the file writer — with a pass/fail a bench
    session can act on.

    This lived in dddutil until P7-12 removed that application. It is a pure function of a
    stream of 10-bit sample values here, with no Qt and no file handling, so the same code
    backs the menu item, the --analyse-test-data command line mode and the unit tests.

    Sequence length is discovered rather than assumed: older gateware ramps 0..1023 and
    newer ramps 0..1020, and a check that hard-coded either would report the other as
    corrupt.

    This file is part of the Domesday Duplicator.
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#ifndef TESTDATAANALYSER_H
#define TESTDATAANALYSER_H

#include <cstddef>
#include <cstdint>

class TestDataAnalyser
{
public:
    struct Result
    {
        // False once a break has been seen. Everything below describes that break.
        bool passed = true;

        // Samples consumed before the break, which is the offset of the bad sample
        uint64_t samplesChecked = 0;
        uint16_t expectedValue = 0;
        uint16_t actualValue = 0;

        // The ramp length that was detected, or 0 if the stream ended before it wrapped.
        // A capture too short to wrap is not a failure, but it is worth reporting, since a
        // pass over 900 samples proves much less than a pass over a disc.
        uint16_t sequenceLength = 0;
    };

public:
    // Feed the next block of 10-bit sample values. Returns false once the ramp has broken;
    // further calls are ignored, so a caller can stop at its own convenience.
    bool Feed(const uint16_t *samples, size_t count);

    bool HasFailed() const { return !result.passed; }
    const Result &GetResult() const { return result; }

private:
    Result result;
    bool haveFirstSample = false;
    uint16_t currentValue = 0;

    // 0 until the first wrap reveals it
    uint16_t detectedSequenceLength = 0;
};

#endif // TESTDATAANALYSER_H
