/************************************************************************

    test_data_analysis.h

    Checking a finished test-mode capture for breaks
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace ddd::capture {

// Step 4 of the capture-integrity procedure in TESTING.md §5: read a capture
// taken in test mode and check that the gateware's ramp is unbroken from the
// first sample to the last.
//
// The inline check in test_pattern_verifier.h watches the ramp as it arrives
// and is what stops a capture the moment it goes wrong. This is the other half:
// it reads the file back off the disk, so it also covers the encoder, the
// filesystem and the drive — everything between the processing thread and the
// bytes that will still be there tomorrow. A capture can pass the live check
// and fail this one, and that failure is the interesting one.
//
// Qt-free, and with the read loop here rather than in a dialog, because it has
// two callers that must not be allowed to drift apart: the GUI action and the
// --analyse-test-data command line. The old application had the loop written
// out twice and the two could report different things about the same file.

// Samples per read. 4 M is a fraction of a second of capture — enough that the
// read call is not the bottleneck, small enough that a cancel is acted on
// promptly and a progress bar moves.
inline constexpr size_t kAnalysisChunkSamples = size_t{4} << 20;

struct TestDataAnalysis {
  enum class Outcome {
    // The ramp was intact all the way to the end of the file
    kPassed,

    // It broke. Everything from the ADC to the drive is in scope, and the
    // sample offset below is where to start looking.
    kFailed,

    // The file could not be read at all: an extension this application does not
    // handle, a file that is not there, a decoder error partway through. Not a
    // verdict on the capture — a verdict on the attempt.
    kUnreadable,

    // The caller asked to stop before the end. No break had been found, which
    // is not the same as there being none.
    kCancelled,
  };

  Outcome outcome = Outcome::kUnreadable;

  uint64_t samples_checked = 0;
  uint16_t expected_value = 0;
  uint16_t actual_value = 0;

  // The ramp length the file turned out to use, or nothing if it ended before
  // the ramp wrapped. Older gateware ramps 0..1023 and newer 0..1020, so this
  // is discovered rather than assumed — and its absence is worth reporting,
  // because a capture too short to wrap has not exercised what this test is
  // for.
  std::optional<uint16_t> sequence_length;

  // The sentence to show a user or print to stdout. Produced here so the dialog
  // and the command line say the same thing about the same file.
  std::string message;

  // A process exit code: 0 passed, 1 broke, 2 no verdict.
  //
  // Three outcomes rather than a boolean, so a script driving the T5 gate can
  // tell "this capture is bad" from "I could not tell you". A cancelled run
  // shares code 2 with an unreadable one for the same reason: neither is an
  // answer.
  int ExitCode() const;

  bool passed() const { return outcome == Outcome::kPassed; }
};

// Called as the analysis proceeds, with the samples checked so far and the
// file's total where the file knows it. A streamed FLAC whose header was never
// patched — the file a killed capture leaves behind — reports no total, and a
// caller shows a busy indicator rather than inventing a percentage.
using AnalysisProgress = std::function<void(uint64_t samples_checked,
                                            std::optional<uint64_t> total)>;

// Return true to stop early. Consulted once per chunk.
using AnalysisCancelled = std::function<bool()>;

// Read a capture and check its ramp.
//
// Blocking, and slow — minutes for a full disc side — so a GUI caller runs it
// on a thread of its own.
TestDataAnalysis AnalyseTestData(const std::filesystem::path& file_path,
                                 const AnalysisProgress& progress = {},
                                 const AnalysisCancelled& cancelled = {});

}  // namespace ddd::capture
