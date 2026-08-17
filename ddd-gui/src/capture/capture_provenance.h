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
inline constexpr const char* kTagDecimation = "DDD_DECIMATION";
inline constexpr const char* kTagTestMode = "DDD_TEST_MODE";
inline constexpr const char* kTagFrontEndGain = "DDD_FRONT_END_GAIN";

// What the disc was, where a player was there to be asked. Every one of these
// is written only when it is known — a file that carried "side 1" because
// nothing said otherwise would be a file asserting something nobody
// established.
inline constexpr const char* kTagPlayer = "DDD_PLAYER";
inline constexpr const char* kTagDiscType = "DDD_DISC_TYPE";
inline constexpr const char* kTagDiscSize = "DDD_DISC_SIZE";
inline constexpr const char* kTagDiscSide = "DDD_DISC_SIDE";
inline constexpr const char* kTagVideoStandard = "DDD_VIDEO_STANDARD";
inline constexpr const char* kTagProgrammeStart = "DDD_PROGRAMME_START";
inline constexpr const char* kTagProgrammeEnd = "DDD_PROGRAMME_END";

// What an examination found out about the disc, ready to be written into a
// file.
//
// Strings rather than the player library's typed values, and deliberately: this
// header belongs to the capture engine, which links no player code and should
// not start to. The player side formats these once — "CAV", "PAL", "2" — and
// the engine writes whatever it is given, so a field added to the profile
// later needs a line here rather than a dependency.
//
// Every field is empty until something establishes it, and an empty field is
// simply not written. That is the whole rule: a capture file says what is known
// about the disc it came from and nothing else.
struct DiscProvenance {
  // The model that was driving the disc, as its name and firmware.
  std::string player;

  std::string disc_type;
  std::string disc_size;
  std::string disc_side;
  std::string video_standard;

  // The measured ends of the programme, in whichever way the disc is addressed
  // — a frame number on a CAV disc, a time code on a CLV one. Written as they
  // are shown, so a reader does not have to know which.
  std::string programme_start;
  std::string programme_end;

  bool empty() const {
    return player.empty() && disc_type.empty() && disc_size.empty() &&
           disc_side.empty() && video_standard.empty() &&
           programme_start.empty() && programme_end.empty();
  }
};

// The facts a capture carries about itself.
struct CaptureProvenance {
  // The name the capture was given, without the suffix
  std::string title;

  // This build's version stamp
  std::string application_version;

  bool test_mode = false;

  // How many device samples each sample in the file stands for: 1 for a capture
  // written at the device's own rate, 2 for a 2:1 decimated one.
  //
  // This is the fact that makes a decimated capture usable years later. The
  // FLAC header's rate field carries a label rather than a rate, so without
  // this the only evidence a file is half-rate is that it decodes to a signal
  // an octave out — which is exactly the sort of thing that gets blamed on the
  // player.
  int decimation_factor = 1;

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

  // What was in the player, where there was one. Default-constructed for every
  // capture taken without player control, which is every capture the
  // application took before this existed.
  DiscProvenance disc;
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
