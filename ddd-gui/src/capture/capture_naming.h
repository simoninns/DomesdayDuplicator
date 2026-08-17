/************************************************************************

    capture_naming.h

    What a capture file is called, and where it is put
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

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
