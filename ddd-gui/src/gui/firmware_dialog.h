/************************************************************************

    firmware_dialog.h

    The Firmware box: which build each half of the device is running
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>

#include "firmware_text.h"

class QLabel;

namespace ddd::gui {

// Shows the application's build alongside the FX3 firmware's and the FPGA
// gateware's, and says whether they agree.
//
// Reachable from the Help menu rather than raised on its own, because a version
// difference is a note and not a fault — the application monitors and captures
// normally whatever this says. What it exists for is the moment when something
// is behaving oddly and the first question is whether the three halves of the
// device match.
//
// Everything is passed in at construction. The dialog reads no device and does
// no work, so opening it can never block and never changes what the device is
// doing.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class FirmwareDialog : public QDialog {
  Q_OBJECT

 public:
  explicit FirmwareDialog(const FirmwareVersions& versions,
                          QWidget* parent = nullptr);

  // The width the text is laid out at, matching the About dialog's.
  static constexpr int kTextWidthPixels = 520;

  // Object names, so a widget test can reach the text without depending on the
  // order things were added to the layout.
  static constexpr const char* kTextLabelName = "firmware_text";
  static constexpr const char* kScrollAreaName = "firmware_scroll";

 private:
  QLabel* text_ = nullptr;
};

}  // namespace ddd::gui
