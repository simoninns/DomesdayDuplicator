/************************************************************************

    firmware_text.h

    What the Firmware dialog says about the three versions
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>

#include "fpga_version.h"

namespace ddd::gui {

// A release builds three things from one commit — this application, the FX3
// firmware and the FPGA gateware — and each carries its own stamp. The
// application knows its own at compile time, the firmware puts its into the USB
// product string, and the gateware answers with its in a register.
//
// Separated from the dialog that shows it so that what it says can be tested
// without a device or a window, which matters because most of what it says is
// about things that are absent.
struct FirmwareVersions {
  // This build's version stamp, as capture::Version() reports it. May be
  // "unknown", or carry a "-dirty" suffix.
  QString application;

  // Whether a device is attached. False makes the other two fields
  // meaningless, and the text says so rather than reporting them as missing.
  bool device_attached = false;

  // The attached device's USB product string, which carries the firmware's
  // commit in brackets.
  QString product_string;

  // What the attached device's gateware reported about itself.
  capture::FpgaVersion gateware;
};

// The dialog's body, as rich text.
//
// Every case where a version is not known is reported as a plain statement and
// never as a fault: old firmware is not broken firmware, and a user in front of
// a working capture is better served by a note than by an alarm.
QString FirmwareText(const FirmwareVersions& versions);

}  // namespace ddd::gui
