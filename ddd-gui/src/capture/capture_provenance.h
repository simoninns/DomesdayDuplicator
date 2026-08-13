/************************************************************************

    capture_provenance.h

    What a capture file says about where it came from
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <ctime>
#include <string>
#include <vector>

#include "flac_writer.h"

namespace ddd::capture {

// Vorbis comment names. Constants rather than literals at the call site because
// a reader elsewhere in the toolchain matches on them exactly, and a typo in
// one of these produces a file that looks right and carries nothing.
inline constexpr const char* kTagTitle = "TITLE";
inline constexpr const char* kTagEncoder = "ENCODER";
inline constexpr const char* kTagDate = "DATE";
inline constexpr const char* kTagVersion = "DDD_VERSION";
inline constexpr const char* kTagSampleRate = "DDD_SAMPLE_RATE_HZ";
inline constexpr const char* kTagTestMode = "DDD_TEST_MODE";
inline constexpr const char* kTagFrontEndGain = "DDD_FRONT_END_GAIN";

// The facts a capture carries about itself.
struct CaptureProvenance {
  // The name the capture was given, without the suffix
  std::string title;

  // This build's version stamp
  std::string application_version;

  bool test_mode = false;

  // The front-end gain the user declared, as a sentence — or empty for a gain
  // that was never declared.
  //
  // Empty is the important case and the reason this is a string rather than a
  // number. The tag is written only when a declaration was actually made: a
  // capture that carried a default gain figure nobody had checked would look
  // like calibration data and would be wrong, which is worse than a capture
  // that says nothing and forces the question to be asked.
  std::string front_end_gain;

  // When the capture started
  std::time_t started = 0;
};

// The tags a capture file is stamped with.
//
// This is what makes a capture readable years later without its sidecar: the
// sample rate the FLAC header cannot express, the build that produced it, and
// whether it is signal or a test ramp. DDD_TEST_MODE in particular is not a
// nicety — a test capture and a real one are indistinguishable by inspection
// until somebody decodes one.
std::vector<FlacWriter::Tag> BuildProvenanceTags(
    const CaptureProvenance& provenance);

// An ISO 8601 date, which is what DATE is defined to hold.
std::string FormatProvenanceDate(std::time_t when);

}  // namespace ddd::capture
