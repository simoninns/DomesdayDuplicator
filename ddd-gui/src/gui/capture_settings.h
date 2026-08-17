/************************************************************************

    capture_settings.h

    The settings a capture runs with, and where they are kept
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <cstddef>
#include <cstdint>

#include "capture_format.h"
#include "capture_naming.h"
#include "disk_buffer_ring.h"
#include "flac_writer.h"
#include "free_space.h"
#include "front_end_gain.h"
#include "usb_device.h"

namespace ddd::gui {

// Everything the user can change about how a capture runs.
//
// A plain value rather than a settings object with getters. It is passed whole
// to the controller, compared whole in tests, and saved whole — and keeping it
// a value means there is no way for the dialog and the controller to disagree
// about which half of it has been applied.
struct CaptureSettings {
  // Which device to use when several are attached. Empty means "whichever is
  // first", which is what almost every user has.
  QString preferred_device_path;

  size_t queue_size_bytes = capture::DiskBufferRing::kDefaultQueueSizeBytes;

  // See UsbSourceOptions::small_transfers. Exposed because it is the setting
  // that makes a difference on a machine that is struggling, and because the
  // right answer differs between platforms often enough that it cannot simply
  // be decided here.
  bool small_transfers = true;

  size_t transfer_queue_bytes =
      capture::UsbSourceOptions{}.transfer_queue_bytes;

  // The SW401 switch pattern the user says their board is set to, or zero for
  // "not declared". Persisted, unlike test mode: a gain switch stays where it
  // was put, and asking again every session for something that has not changed
  // is how a setting ends up ignored.
  //
  // Carried in this struct because it is user configuration and this is where
  // user configuration lives — not because the engine sees it. UsbOptions()
  // does not pass it on and nothing below the GUI reads it; it changes what a
  // figure is labelled, never what is captured.
  uint8_t front_end_gain_switches = analysis::kUndeclaredSwitchPattern;

  // Put the gateware into test-pattern mode. Not persisted: it is a diagnostic,
  // and an application that silently started in test mode because of something
  // the user did last week would produce a capture full of ramps.
  bool test_mode = false;

  // --- Where a capture goes ------------------------------------------------

  // The folder captures are written to. Empty means the platform's Movies or
  // Videos folder, resolved at load rather than stored, so a settings file
  // copied between machines still points somewhere that exists.
  QString capture_directory;

  // The name to use, without a suffix. Empty means the name is built from the
  // naming fields below and a timestamp, which for a user who has not opened
  // the naming dialog is the generated RF-Sample_<timestamp> — what almost
  // every capture uses.
  QString capture_name;

  // What the user says the disc is: the title, the type, the standard, the
  // side, the notes. Every capture records these in its sidecar, and some of
  // them reach the file name — see capture_naming.h, where the rules are.
  //
  // Persisted, like the gain declaration and unlike test mode. Somebody
  // capturing both sides of a disc types the title once, and an application
  // that forgot it between the two would be asking for the same answer twice.
  // The naming dialog's "Clear all fields" is what makes that safe: it is
  // one press before the next disc, rather than eight fields to empty by hand.
  capture::CaptureNamingFields naming;

  // What the capture is written as. FLAC by default: it halves the file and
  // carries its own provenance, and the encode is affordable.
  //
  // Persisted, because it is a decision about a workflow rather than about one
  // capture — somebody feeding a tool that wants bare samples wants them every
  // time, and being asked again each session is how a setting ends up ignored.
  capture::CaptureOutputFormat output_format =
      capture::CaptureOutputFormat::kFlac;

  // Keep every nth sample. 1 is the device's own rate and is what a LaserDisc
  // capture uses; 2 halves it to 20 Msps, which is enough for tape RF and half
  // the file.
  //
  // Done by the gateware, not here. Halving the rate means low-passing the
  // signal at 10 MHz first — without that everything above 10 MHz folds down
  // on top of it — and the filter that does it costs 13% of the FPGA's logic
  // rather than a core or two of this machine. All this setting does is
  // reach the device's decimation register.
  //
  // Persisted for the same reason as the format. It applies in test mode too:
  // the gateware generates its test pattern downstream of the decimator, so a
  // decimated test capture is an unbroken ramp at the decimated rate and the
  // integrity check covers that path as well.
  int decimation_factor = capture::kUndecimatedFactor;

  // 0-8, as flac's -0 .. -8. See FlacWriter::Options — the default is 8, which
  // multithreaded libFLAC sustains at the device's full rate.
  //
  // Lowering it is the first remedy for a machine that cannot keep up, which is
  // what the buffer-queue and encoder-backlog figures in the Statistics panel
  // are there to show: a backlog that climbs is the encoder, a queue that
  // climbs with no backlog is the disk. Ignored entirely by the uncompressed
  // format, which has no encoder to ask.
  int compression_level = capture::FlacWriter::Options{}.compression_level;

  // Stop the capture automatically after this long. 0 means run until stopped,
  // which is the default: a limit that fired in the middle of a side would be
  // worse than no limit at all.
  //
  // Held in seconds although the panel offers minutes, because minutes is a
  // presentation choice and seconds is the unit the check is actually made in.
  // A settings file holding a value that is not a whole number of minutes —
  // written by hand, or by a future command-line tool with a finer control — is
  // honoured as written and rounded only for display.
  int duration_limit_seconds = 0;

  // Warn when the destination volume holds less than this many minutes of
  // capture. Warn, not refuse — the estimate is an estimate, and an application
  // that declined to start because it predicted a shortfall would sometimes be
  // wrong in the direction that costs a session.
  int low_space_warning_minutes = kDefaultLowSpaceWarningMinutes;

  static constexpr int kDefaultLowSpaceWarningMinutes = 10;
  static constexpr int kMaximumDurationLimitMinutes = 24 * 60;
  static constexpr int kMaximumDurationLimitSeconds =
      kMaximumDurationLimitMinutes * 60;
  static constexpr int kMaximumLowSpaceWarningMinutes = 24 * 60;

  bool operator==(const CaptureSettings& other) const {
    return preferred_device_path == other.preferred_device_path &&
           queue_size_bytes == other.queue_size_bytes &&
           small_transfers == other.small_transfers &&
           transfer_queue_bytes == other.transfer_queue_bytes &&
           front_end_gain_switches == other.front_end_gain_switches &&
           test_mode == other.test_mode &&
           capture_directory == other.capture_directory &&
           capture_name == other.capture_name && naming == other.naming &&
           output_format == other.output_format &&
           decimation_factor == other.decimation_factor &&
           compression_level == other.compression_level &&
           duration_limit_seconds == other.duration_limit_seconds &&
           low_space_warning_minutes == other.low_space_warning_minutes;
  }
  bool operator!=(const CaptureSettings& other) const {
    return !(*this == other);
  }

  // The engine-side options these settings imply.
  capture::UsbSourceOptions UsbOptions() const;

  // What a capture started now is expected to cost on disk, per second.
  //
  // The free-space readouts are a time rather than a size, so they need this:
  // uncompressed is twice what FLAC costs and decimating halves it, and a
  // figure that assumed a compressed 40 Msps capture would be out by a factor
  // of four at the extremes — in the direction that lets a volume fill up
  // before the warning ever fires.
  double EstimatedBytesPerSecond() const;

  // The rate the samples actually arrive at.
  //
  // What every display that turns samples into time or frequency needs, and the
  // reason it is here rather than being read off the converter's rate: the
  // spectrum's Nyquist, the scope's time axis and the encoder backlog are all
  // properties of the stream, and a decimated stream is half the rate. It is
  // safe to read this while a stream is running because the rate cannot be
  // changed under one — see CapturePanel's sample-rate control, which is locked
  // from the moment monitoring starts.
  uint32_t SampleRateHz() const {
    return capture::SampleRateHzFor(decimation_factor);
  }

  // The declared front-end gain, or the undeclared state. Named DeclaredGain
  // rather than FrontEndGain so the accessor cannot shadow the type it returns.
  analysis::FrontEndGain DeclaredGain() const;

  // The directory a capture will actually be written to: what the user chose,
  // or the platform's default when they have not chosen.
  QString ResolvedCaptureDirectory() const;

  // The name a capture started at `when` would be given.
  //
  // One call rather than three, so the panel's preview, the wizard's suggested
  // name and the file the engine actually opens cannot disagree about what a
  // capture is going to be called. Never empty.
  std::string CaptureStem(std::time_t when) const;
};

// Where captures go when nobody has said otherwise.
//
// The platform's Movies folder rather than Documents or the working directory:
// a capture is tens of gigabytes of media, and this is the location a user's
// backup rules and disk-space expectations are already set up around.
QString DefaultCaptureDirectory();

// Read the saved settings, falling back to the defaults above for anything
// missing or nonsensical.
//
// Clamping rather than rejecting: a settings file edited by hand, or written by
// a version of the application with a different range, should produce a working
// capture rather than a refusal. The one setting that is never read back is
// test mode.
CaptureSettings LoadCaptureSettings();

void SaveCaptureSettings(const CaptureSettings& settings);

}  // namespace ddd::gui
