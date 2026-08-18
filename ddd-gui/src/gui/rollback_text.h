/************************************************************************

    rollback_text.h

    What the rollback wizard says, in one place
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <optional>
#include <vector>

#include "bringup_text.h"
#include "device_updater.h"
#include "update_manifest.h"
#include "usb_device_info.h"

namespace ddd::gui {

// Rolling a unit back to the software it shipped with: the original firmware
// and the original gateware, both written by the unit itself over one cable.
//
// Separated from the widgets for the same reason bringup_text.h is — so what
// it says can be checked without a window or a device — and it is the file in
// this application whose wording matters most, because this is the one flow
// that deliberately takes capabilities away. Two things it must never do:
// soften what is lost, or dramatise it. A user choosing this has a reason, and
// the sentence they deserve is the true one.
//
// What it says that the bring-up wizard does not have to:
//
//   - **one cable**, not two. A rollback target is running this application's
//     own firmware over gateware with a flash bridge, so both images go over
//     the USB 3.0 link and nothing here needs the DE0-Nano's mini-USB. The
//     case stays on. Bringing the unit back afterwards does not — and that
//     asymmetry is said on the first page, because it is what somebody would
//     want to know before agreeing rather than after.
//   - **what stops working**, named one at a time rather than summarised.

// The seven pages, in the order they are worked through.
enum class RollbackPage {
  // What this does, what stops working, and the typed confirmation. The one
  // deliberately frightening page in the application.
  kOverview,

  // The device, opened rather than merely enumerated.
  kConnect,

  // The rollback file: signature, digests, channel, and that it is a rollback
  // file at all.
  kImage,

  // The FPGA first, while the current firmware is still running and doing the
  // writing.
  kGateware,

  // Then the FX3, by an ordinary update transfer.
  kFirmware,

  // The power cycle on which both halves become the running ones.
  kPowerCycle,

  // The one thing a legacy device can be asked, and what to do from here.
  kVerify,
};

QString RollbackPageTitle(RollbackPage page);
QString RollbackPageHeading(RollbackPage page);

// The overview: what is lost, what is not, and what brings it back.
QString RollbackOverviewText();

// What has to be typed to go on, and the sentence asking for it. A typed word
// rather than a checkbox because this is the one operation in the application
// that cannot be undone from the application.
QString RollbackConfirmWord();
QString RollbackConfirmPrompt();

// The device row on the connectivity page. One row, not two: this flow never
// opens a USB-Blaster, so a missing one is not a fault to report.
BringUpStatusRow RollbackDeviceRow(
    const std::optional<capture::DeviceInfo>& device,
    const std::optional<capture::DeviceIdentity>& identity);

// Whether that row is one the wizard can go on from.
bool RollbackDeviceIsReady(
    const std::optional<capture::DeviceInfo>& device,
    const std::optional<capture::DeviceIdentity>& identity);

// The image page: what to choose, and what a chosen file turns out to be.
QString RollbackImageText();
QString RollbackImageSummary(const capture::UpdateManifest& manifest);

// Why a chosen file cannot be used, or empty when it can. Separate from the
// compatibility gate, which needs a device: this is what can be said about the
// file alone.
QString RollbackImageProblem(const capture::UpdateManifest& manifest);

// The two programming pages, and the power cycle between them and the end.
QString RollbackGatewareText(int seconds);
QString RollbackFirmwareText(int seconds);
QString RollbackPowerCycleText();
QString RollbackPowerCycleTimeoutText();

// The last page. `legacy` is whether the device came back as the original
// firmware, which is the one thing such a device can be asked.
QString RollbackVerifyText(bool legacy);
QString RollbackVerifySummary(bool legacy);

}  // namespace ddd::gui
