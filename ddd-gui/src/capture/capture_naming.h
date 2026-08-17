/************************************************************************

    capture_naming.h

    What a capture file is called, and where it is put
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>

#include "capture_format.h"

namespace ddd::capture {

// Naming is here, in the engine, rather than in the panel that shows it. It has
// to be: a future command-line capture tool names its files by the same rules
// and cannot reach a Qt widget, and the rule that a test capture is called
// something recognisable is a property of the capture rather than of the
// interface used to start it.

// What an ordinary capture is called before the timestamp
inline constexpr const char* kCaptureNamePrefix = "RF-Sample_";

// And what a test-mode capture is called. Not a preference: see
// DefaultCaptureStem.
inline constexpr const char* kTestCaptureNamePrefix = "TestData_";

// A timestamp as a name carries it: 2026-08-13_12-00-00, in local time.
//
// Local rather than UTC because the person who took the capture is the person
// who will look for it, and they remember what time it was where they were
// standing. Dashes rather than colons because a colon is not a legal filename
// character on Windows and is a path separator on classic macOS-era tooling.
std::string FormatCaptureTimestamp(std::time_t when);

// The name a capture gets when the user has not typed one.
//
// In test mode the name is forced rather than defaulted. A test capture is a
// ramp from the gateware's pattern generator and contains no signal at all, so
// a file called "disc1 side 1" full of ramps is a trap that costs somebody an
// afternoon. Test captures are called TestData_, always, and the interface does
// not offer to call them anything else.
std::string DefaultCaptureStem(bool test_mode, std::time_t when);

// Reduce a typed name to something that can be a filename everywhere this
// application runs.
//
// Windows is the constraint: it rejects <>:"/\|?* and the control characters,
// treats a trailing dot or space as invisible, and reserves a list of device
// names that cannot be used even with an extension. A name that works on the
// machine it was typed on and fails on a colleague's is worth preventing here,
// where the rule can be stated once, rather than at three call sites.
//
// Returns an empty string if nothing usable is left, which callers treat as
// "no name given" rather than as an error.
std::string SanitiseCaptureStem(const std::string& text);

// The path a capture is written to: the directory, the name (sanitised, or the
// default when nothing usable was given) and the suffix its format is written
// with.
std::filesystem::path BuildCapturePath(
    const std::filesystem::path& directory, const std::string& stem,
    bool test_mode, std::time_t when,
    CaptureOutputFormat format = CaptureOutputFormat::kFlac);

// The same path, with a number appended if something is already there.
//
// A capture that silently overwrote an earlier one would destroy an archival
// recording to save a dialog, which is not a trade this application makes.
// Gives up after kMaximumNameAttempts and returns the last path it tried, at
// which point creating the file fails and the user is told why — better than
// looping forever against a directory that is doing something unexpected.
//
// The format is read off the path rather than passed in, so that this stays
// usable on any capture path a caller has in hand.
std::filesystem::path MakeUniqueCapturePath(
    const std::filesystem::path& preferred);

inline constexpr int kMaximumNameAttempts = 1000;

// --- What the user says the disc is ---------------------------------------

// The three fields whose value is one of a small set rather than free text.
//
// Each carries an "unset" case that is distinct from every answer, because
// these are recorded in a file that outlives the session: a capture that said
// "CAV" because that was the first radio button is a capture asserting
// something nobody established, which is worse than one that says nothing.
enum class DiscTypeChoice : uint8_t { kUnset, kCav, kClv };
enum class VideoStandardChoice : uint8_t { kUnset, kNtsc, kPal };

// kDefault is a real answer and not a synonym for unset: the old application
// offered it as one of four audio options, meaning "whatever the disc's own
// default is" rather than "not stated".
enum class AudioTypeChoice : uint8_t {
  kUnset,
  kDefault,
  kAnalogue,
  kAc3,
  kDts
};

// What the user typed about the disc they are capturing.
//
// The same set of fields the old application collected in its Advanced Naming
// dialog, kept because they are what an archivist actually writes down and
// because files already exist that were named by those rules. They serve two
// purposes at once, which is the thing to understand about this struct: they
// are recorded in the sidecar beside the capture, and — some of them, when
// asked — they are folded into the capture's file name.
//
// A field is used only when its `..._used` flag is set. That mirrors the old
// dialog's per-field check boxes, and it is a genuine third state rather than a
// convenience: an empty title and a title nobody was asked for are different
// facts, and a spin box showing "side 1" cannot say which of the two it is.
//
// The old dialog had three filename modes — automatic, automatic with
// metadata, and manual. There are two things here instead: a typed name, which
// wins outright when there is one, and `metadata_in_name`, which says whether
// the disc details join the automatic name. That is the same three
// combinations with one fewer control, and it leaves the Capture panel's Name
// field meaning exactly what it meant before this existed.
//
// Qt-free like the rest of the engine, so the naming rules can be exercised
// without a dialog — which matters because most of what is interesting about
// them is what happens when fields are left out.
struct CaptureNamingFields {
  bool title_used = false;
  std::string title;

  bool disc_type_used = false;
  DiscTypeChoice disc_type = DiscTypeChoice::kUnset;

  bool video_standard_used = false;
  VideoStandardChoice video_standard = VideoStandardChoice::kUnset;

  bool audio_used = false;
  AudioTypeChoice audio = AudioTypeChoice::kUnset;

  bool side_used = false;
  int side = 1;

  bool notes_used = false;
  std::string notes;

  // "Mint marks": the condition notes an archivist records for a disc, in the
  // old application's own vocabulary. Kept under that name because that is what
  // the existing files call the field.
  bool mint_marks_used = false;
  std::string mint_marks;

  // Free text for the sidecar, and only for the sidecar. It never reaches a
  // file name — a paragraph is not a file name — which is why it has no
  // `_used` flag: empty means there is none.
  std::string metadata_notes;

  // Fold the disc details into the automatic file name as well as recording
  // them. Off by default: the details are worth having in the sidecar for every
  // capture, and worth having in the file name only for somebody whose
  // filing system is built on them.
  bool metadata_in_name = false;

  // Append the capture's length to the file name once it is known.
  //
  // Done by renaming the finished file, because the length is not a fact until
  // the capture has stopped. The old application did the same, and files named
  // that way exist.
  bool append_duration = false;

  // Keep notes and mint marks separately for each side.
  //
  // Session-scoped rather than persisted, and it is the dialog that holds the
  // per-side text; what is here is only whether it should. Somebody capturing
  // both sides of a disc in one sitting has different notes for each, and
  // typing over them twice is how the second side ends up carrying the first
  // one's.
  bool per_side_notes = false;
  bool per_side_mint_marks = false;

  bool operator==(const CaptureNamingFields&) const = default;
};

// The word each choice is recorded under in the sidecar.
//
// Spelled out rather than abbreviated, and separate from the file-name token
// below: a file name is short because it is a file name, and a metadata field
// is read by somebody who was not there.
const char* DiscTypeChoiceName(DiscTypeChoice choice);
const char* VideoStandardChoiceName(VideoStandardChoice choice);
const char* AudioTypeChoiceName(AudioTypeChoice choice);

// The token each choice contributes to a file name, or an empty string for a
// choice that contributes nothing. kDefault audio is the one that contributes
// nothing while still being a real answer — the old application's rule, kept.
const char* DiscTypeChoiceToken(DiscTypeChoice choice);
const char* VideoStandardChoiceToken(VideoStandardChoice choice);
const char* AudioTypeChoiceToken(AudioTypeChoice choice);

// The name a capture gets, given everything that has been said about it.
//
// The order of precedence, which is the whole of the rule:
//
//   1. Test mode. The name is forced to TestData_<timestamp> and nothing else
//      is consulted — see DefaultCaptureStem for why that is not negotiable.
//   2. A typed name. Used verbatim, with no timestamp, because somebody who
//      typed "Casper side 1" meant that and not "Casper side 1_2026-08-17…".
//   3. The fields, followed by a timestamp: the title where there is one and
//      RF-Sample where there is not, then the disc details when they were asked
//      for, then the side, the notes and the mint marks.
//
// Never returns an empty string.
std::string BuildCaptureStem(const CaptureNamingFields& fields,
                             const std::string& typed_name, bool test_mode,
                             std::time_t when);

// The same name with a capture's length on the end: "…_01H23M45S".
//
// Hours, minutes and seconds with letters between them rather than colons,
// because a colon is not a legal filename character on Windows — the same
// constraint that shapes the timestamp. Whole seconds: a capture is minutes
// long and a fractional second in a file name is noise.
std::string AppendDurationToStem(const std::string& stem, double seconds);

// Where a capture with this name will actually be written, and whether that is
// the name that was asked for.
//
// The two questions are answered together because they have one answer, and
// answering them separately is how an interface comes to show a name that is
// not the name on disk. A capture has never overwritten another — the path is
// made unique before the file is opened — but until this existed the renaming
// was silent, so somebody who captured "Casper side 1" twice got
// "Casper side 1_2.ddd.flac" with nothing on screen having said so.
//
// A typed name is used verbatim, with no timestamp, which is what makes this
// worth surfacing: it is the *ordinary* case for a name to be taken the second
// time somebody uses it, not an edge case. The generated name carries a
// timestamp and so is free by construction.
struct CaptureDestination {
  std::filesystem::path path;

  // The name that path carries, without the directory or the suffix — what a
  // name field should show, so that what is on screen is what is written.
  std::string stem;

  // False when something was already at the requested name and a number had to
  // be appended.
  bool as_requested = true;
};

// Resolve a name to the file that will really be created.
//
// Touches the filesystem only to ask what is there, so it is safe to call from
// an interface on every keystroke.
CaptureDestination ResolveCaptureDestination(
    const std::filesystem::path& directory, const std::string& stem,
    bool test_mode, std::time_t when,
    CaptureOutputFormat format = CaptureOutputFormat::kFlac);

}  // namespace ddd::capture
