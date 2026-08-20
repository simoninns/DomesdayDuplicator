/************************************************************************

    log_format.h

    Numbers as a log line says them
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <string>

namespace ddd::capture {

// A log line a developer reads after a capture went wrong is only as useful as
// its figures are legible, and 268435456 is not legible. These put the engine's
// own numbers into the forms a person reads at a glance, in one place so that
// every line spells them the same way.
//
// The Qt layer has its own formatting for the same quantities, and that is not
// duplication to be removed: these run in a library that links no Qt and are
// read in a log file, where a locale-dependent separator would make two runs of
// the same build disagree about what a number even looks like.

// A fixed-point number, always with '.' as the separator whatever the machine's
// locale asks for, and always finite — a value that is not a number is written
// as zero rather than as `nan`, because a log is read long after the run and
// arithmetic nobody checked should not look like a measurement.
std::string FormatDecimal(double value, int decimals);

// A size in the largest binary unit that leaves a figure of at least one:
// "512 B", "2.0 MiB", "1.4 GiB". Binary because every size in this engine is a
// buffer or a multiple of one.
std::string FormatBytes(uint64_t bytes);

// A duration, in whichever of four forms carries the most meaning at that
// length: "412 ms", "3.24 s", "4 m 07 s", "1 h 12 m 04 s". Milliseconds below a
// second because that is the scale an encoder flush is measured on; hours and
// minutes above one because that is the scale a disc side is.
std::string FormatDuration(double seconds);

// A count of samples as the length of stream it stands for, at the rate the
// device was delivering. Zero rate gives "0 s", which is what a caller that
// does not know the rate should say rather than guessing.
std::string FormatSampleDuration(uint64_t samples, uint32_t sample_rate_hz);

}  // namespace ddd::capture
