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

// This application writes native FLAC, and uncompressed signed 16-bit for
// anyone who would rather spend disk than CPU.
//
// The historical .ldf is FLAC inside an Ogg container, and the encapsulation
// was a workaround for FLAC limitations that were fixed years ago. Dropping it
// costs nothing and gains a format two toolchains accept rather than one:
// ld-decode routes .flac and .ldf through the same libavcodec loader
// (lddecode/utils.py), while general audio editors — Tenacity, Audacity —
// import native FLAC and cannot open Ogg FLAC at all.
//
// The packed 10-bit .lds and the Ogg .ldf are neither written nor read here:
// gui/ remains the tool for those, and carrying a second decoder into a new
// application to re-read files an existing application already reads would be
// duplication for its own sake.

// What a capture is written as.
enum class CaptureOutputFormat {
  // Mono 16-bit native FLAC. The default, and what an archival capture wants:
  // the encoding cost is paid once and the storage cost is paid for years.
  kFlac,

  // The same samples with nothing wrapped round them — signed 16-bit
  // little-endian, no header, no metadata.
  //
  // Twice the file for none of the encoder, which is the trade worth having on
  // a machine that cannot sustain the encode, or on a run whose output is going
  // straight into another tool. Nothing about the file says what it is or where
  // it came from, so the provenance a FLAC capture carries in its tags is
  // simply lost — which is why this is an option and not the default.
  kSigned16Bit,
};

// The rate stamped into a FLAC header.
//
// Not a measurement. FLAC's sample-rate field tops out at 655,350 Hz and the
// device runs at 40,000,000, so the file carries a label instead and everything
// downstream treats it as one. The value matches lddecode/compress.py's
// SAMPLE_RATE — a different number here would produce a file ld-decode reads at
// the wrong speed.
inline constexpr uint32_t kFlacSampleRateLabel = 40'000;

// The decimation factors a capture may be written at.
//
// The device always samples at 40 Msps; decimation happens on this side of the
// USB link, on the way into the file. One is every sample, which is what a
// LaserDisc capture needs. Two keeps every second sample and halves both the
// rate and the file — enough for tape RF, whose bandwidth is a fraction of a
// LaserDisc's, and the reason this exists at all.
//
// Plain selection, with no filter in front of it. That is what gui/ does for
// its 4:1 CD decimation, and it keeps the arithmetic off a real-time path; a
// signal with energy above half the new rate will alias, which is a decision
// about the front end rather than about this code.
inline constexpr int kUndecimatedFactor = 1;
inline constexpr int kTapeDecimationFactor = 2;

// Whether a factor is one this application will write.
bool IsSupportedDecimationFactor(int factor);

// The rate label for a file written at a given decimation. A 2:1 capture is a
// real 20 Msps stream and says so, on the same convention as the undecimated
// label above.
uint32_t FlacSampleRateLabelFor(int decimation_factor);

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

// And the uncompressed one, on the same pattern. ".s16" rather than ".raw"
// because it names the layout: signed 16-bit, and the only thing that can be
// read out of a headerless file is what its name says is in it.
inline constexpr const char* kSigned16BitCaptureFileSuffix = ".ddd.s16";

// Extensions the reader recognises, without the leading dot.
//
// Two for the uncompressed format. ".s16" is what this application writes;
// ".raw" is accepted as well because it is what the old application's
// uncompressed captures are called, and the test-pattern analyser is more
// useful for being able to read both.
inline constexpr const char* kFlacExtension = "flac";
inline constexpr const char* kSigned16BitExtension = "s16";
inline constexpr const char* kLegacySigned16BitExtension = "raw";

// The suffix a capture in this format is written with.
const char* CaptureFileSuffix(CaptureOutputFormat format);

// Append the capture suffix to a name, unless it is already there.
//
// Idempotent because the name reaches this from a text field a user can type
// into, and "disc1.ddd.flac.ddd.flac" is the obvious way to get that wrong.
std::filesystem::path AddCaptureFileSuffix(
    const std::filesystem::path& stem,
    CaptureOutputFormat format = CaptureOutputFormat::kFlac);

// The lower-cased extension of a path, without the dot. Separated out because
// both the reader's format detection and the GUI's file dialogs need it, and
// the two must agree.
std::string LowerCaseExtension(const std::filesystem::path& file_path);

}  // namespace ddd::capture
