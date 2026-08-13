/************************************************************************

    testdataanalyser.cpp

    Domesday Duplicator - test-pattern integrity check (P7-19)

    This file is part of the Domesday Duplicator.
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "testdataanalyser.h"

namespace
{
// The two ramp lengths the gateware has used. 1024 is the original 10-bit counter; 1021 is
// what the current test-pattern generator produces.
constexpr uint16_t knownSequenceLengthOld = 1024;
constexpr uint16_t knownSequenceLengthNew = 1021;
} // namespace

//----------------------------------------------------------------------------------------------------------------------
bool TestDataAnalyser::Feed(const uint16_t *samples, size_t count)
{
    if (!result.passed)
    {
        return false;
    }

    size_t index = 0;

    // The first sample of the capture is wherever in the ramp the device happened to be, so
    // it seeds the expectation rather than being checked against one.
    if (!haveFirstSample && count > 0)
    {
        currentValue = samples[0];
        haveFirstSample = true;
        index = 1;
        ++result.samplesChecked;
    }

    for (; index < count; ++index)
    {
        ++currentValue;
        if (detectedSequenceLength != 0 && currentValue == detectedSequenceLength)
        {
            currentValue = 0;
        }

        const uint16_t actual = samples[index];
        if (actual != currentValue)
        {
            // The first disagreement may be the sequence wrapping rather than a fault: if
            // the stream restarts at 0 exactly where one of the known ramp lengths would
            // end, that is the length being revealed, not a lost sample.
            if (detectedSequenceLength == 0 && actual == 0 &&
                (currentValue == knownSequenceLengthNew || currentValue == knownSequenceLengthOld))
            {
                detectedSequenceLength = currentValue;
                result.sequenceLength = currentValue;
                currentValue = 0;
                ++result.samplesChecked;
                continue;
            }

            result.passed = false;
            result.expectedValue = currentValue;
            result.actualValue = actual;
            return false;
        }

        ++result.samplesChecked;
    }

    return true;
}
