/************************************************************************

    capture_format.h

    What the capture application writes, and what it can read back
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace ddd::capture {

// This application writes native FLAC and nothing else.
//
// The historical .ldf is FLAC inside an Ogg container, and the encapsulation
// was a workaround for FLAC limitations that were fixed years ago. Dropping it
// costs nothing and gains a format two toolchains accept rather than one:
// ld-decode routes .flac and .ldf through the same libavcodec loader
// (lddecode/utils.py), while general audio editors — Tenacity, Audacity —
// import native FLAC and cannot open Ogg FLAC at all.
//
// Reading is where the two lists differ. The reader also accepts uncompressed
// signed 16-bit, because it is four lines of code and it gives the test-pattern
// analyser a format in common with the old application, so the two can be
// checked against each other on the same file. Nothing writes it. The packed
// 10-bit .lds and the Ogg .ldf are neither written nor read here: gui/ remains
// the tool for those, and carrying a second decoder into a new application to
// re-read files an existing application already reads would be duplication for
// its own sake.

// The rate stamped into a FLAC header.
//
// Not a measurement. FLAC's sample-rate field tops out at 655,350 Hz and the
// device runs at 40,000,000, so the file carries a label instead and everything
// downstream treats it as one. The value matches lddecode/compress.py's
// SAMPLE_RATE — a different number here would produce a file ld-decode reads at
// the wrong speed.
inline constexpr uint32_t kFlacSampleRateLabel = 40'000;

// Channels and bit depth in the written file. Mono is definitional: a stereo
// file is not a Domesday Duplicator capture, and the reader says so rather than
// silently reading one channel.
inline constexpr uint32_t kFlacChannels = 1;
inline constexpr uint32_t kFlacBitsPerSample = 16;

// The suffix a capture is given by default.
//
// Two extensions rather than one, and deliberately: ".flac" is what makes the
// file open in ld-decode and in an audio editor, and ".ddd" in front of it says
// where the samples came from. A user who finds one of these a year later can
// tell it apart from an ordinary audio file without opening it.
inline constexpr const char* kCaptureFileSuffix = ".ddd.flac";

// Extensions the reader recognises, without the leading dot
inline constexpr const char* kFlacExtension = "flac";
inline constexpr const char* kSigned16BitExtension = "raw";

// Append the capture suffix to a name, unless it is already there.
//
// Idempotent because the name reaches this from a text field a user can type
// into, and "disc1.ddd.flac.ddd.flac" is the obvious way to get that wrong.
std::filesystem::path AddCaptureFileSuffix(const std::filesystem::path& stem);

// The lower-cased extension of a path, without the dot. Separated out because
// both the reader's format detection and the GUI's file dialogs need it, and
// the two must agree.
std::string LowerCaseExtension(const std::filesystem::path& file_path);

}  // namespace ddd::capture
