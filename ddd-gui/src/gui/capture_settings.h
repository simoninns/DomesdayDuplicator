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

#include "disk_buffer_ring.h"
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

  bool operator==(const CaptureSettings& other) const {
    return preferred_device_path == other.preferred_device_path &&
           queue_size_bytes == other.queue_size_bytes &&
           small_transfers == other.small_transfers &&
           transfer_queue_bytes == other.transfer_queue_bytes &&
           front_end_gain_switches == other.front_end_gain_switches &&
           test_mode == other.test_mode;
  }
  bool operator!=(const CaptureSettings& other) const {
    return !(*this == other);
  }

  // The engine-side options these settings imply.
  capture::UsbSourceOptions UsbOptions() const;

  // The declared front-end gain, or the undeclared state. Named DeclaredGain
  // rather than FrontEndGain so the accessor cannot shadow the type it returns.
  analysis::FrontEndGain DeclaredGain() const;
};

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
