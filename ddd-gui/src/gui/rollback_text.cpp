/************************************************************************

    rollback_text.cpp

    What the rollback wizard says, in one place
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "rollback_text.h"

#include <QCoreApplication>

#include "firmware_version.h"
#include "update_text.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

QString Translate(const char* text) {
  return QCoreApplication::translate("RollbackText", text);
}

}  // namespace

QString RollbackPageTitle(RollbackPage page) {
  switch (page) {
    case RollbackPage::kOverview:
      return Translate("1 of 7");
    case RollbackPage::kConnect:
      return Translate("2 of 7");
    case RollbackPage::kImage:
      return Translate("3 of 7");
    case RollbackPage::kGateware:
      return Translate("4 of 7");
    case RollbackPage::kFirmware:
      return Translate("5 of 7");
    case RollbackPage::kPowerCycle:
      return Translate("6 of 7");
    case RollbackPage::kVerify:
      return Translate("7 of 7");
  }
  return QString();
}

QString RollbackPageHeading(RollbackPage page) {
  switch (page) {
    case RollbackPage::kOverview:
      return Translate("What you are about to give up");
    case RollbackPage::kConnect:
      return Translate("The Duplicator, connected");
    case RollbackPage::kImage:
      return Translate("The rollback file");
    case RollbackPage::kGateware:
      return Translate("Write the original gateware");
    case RollbackPage::kFirmware:
      return Translate("Write the original firmware");
    case RollbackPage::kPowerCycle:
      return Translate("Power cycle");
    case RollbackPage::kVerify:
      return Translate("What the device is running now");
  }
  return QString();
}

QString RollbackOverviewText() {
  return Translate(
      "<p>This puts the <b>original Domesday Duplicator firmware and "
      "gateware</b> back on this unit — the software it shipped with, from "
      "before this application existed. It is a deliberate act and it is "
      "reversible, but not from here.</p>"

      "<p><b>What stops working.</b></p>"
      "<ul>"
      "<li>This application. A unit on the original firmware enumerates as a "
      "different device and cannot capture, examine or be driven from "
      "here.</li>"
      "<li>Updating. That firmware predates the update mechanism entirely, so "
      "there is nothing on the device to receive one. This is the whole "
      "reason coming back is harder than going.</li>"
      "<li>Everything built on the register interface: test-data mode, the "
      "gateware's identity, the flash bridge.</li>"
      "</ul>"

      "<p><b>What does not change.</b> Nothing physical, and nothing on your "
      "discs or your captures. The board is not modified and cannot be "
      "damaged by this.</p>"

      "<p><b>How you come back.</b> <i>Tools ▸ Firmware ▸ Legacy ▸ Bring up a "
      "new or legacy board…</i> puts this unit back exactly as it is now. It "
      "is worth reading before you agree, because it is not as easy as this "
      "is: it needs the case off, the DE0-Nano's mini-USB cable, and a "
      "jumper moved twice. This page is the easy direction.</p>"

      "<p>Everything here happens over the USB 3.0 cable you already have "
      "connected. About two minutes.</p>");
}

QString RollbackConfirmWord() {
  // Two words rather than one, and English rather than a tick: what is being
  // asked for is a moment's deliberate typing, and "yes" is a thing people
  // type without reading.
  return Translate("ROLL BACK");
}

QString RollbackConfirmPrompt() {
  return Translate(
             "Type <b>%1</b> to confirm that you want the original firmware "
             "back.")
      .arg(RollbackConfirmWord());
}

BringUpStatusRow RollbackDeviceRow(
    const std::optional<capture::DeviceInfo>& device,
    const std::optional<capture::DeviceIdentity>& identity) {
  BringUpStatusRow row;
  row.title = Translate("Domesday Duplicator");

  if (!device.has_value()) {
    row.state = BringUpRowState::kProblem;
    row.detail = Translate(
        "<b>Nothing found.</b> Check the USB 3.0 cable, and that this machine "
        "has permission to open the device.");
    return row;
  }

  if (device->personality == capture::DevicePersonality::kLegacy) {
    row.state = BringUpRowState::kProblem;
    row.detail = Translate(
        "<b>Already running the original firmware.</b> There is nothing to "
        "roll back. <i>Bring up a new or legacy board…</i> is the flow that "
        "brings it forward again.");
    return row;
  }

  if (device->personality != capture::DevicePersonality::kApplication) {
    row.state = BringUpRowState::kProblem;
    row.detail = Translate(
        "<b>Not running its own firmware.</b> A rollback is written by the "
        "device itself, so the device has to be working first. Finish "
        "bringing it up, then come back here.");
    return row;
  }

  if (!identity.has_value()) {
    row.state = BringUpRowState::kProblem;
    row.detail = Translate(
        "<b>Found, but it could not be opened.</b> Something else may have "
        "it, or this machine may not have permission.");
    return row;
  }

  if (!identity->gateware_present) {
    row.state = BringUpRowState::kProblem;
    row.detail = Translate(
        "<b>The FPGA is not answering.</b> The original gateware is written "
        "through the current gateware's flash bridge, so there is no route to "
        "the flash while the FPGA is silent. Reconnect the unit and try "
        "again.");
    return row;
  }

  if (!identity->GatewareCanBeUpdated()) {
    row.state = BringUpRowState::kProblem;
    row.detail = Translate(
        "<b>This gateware predates the flash bridge</b>, so it cannot replace "
        "itself. There is nothing here to roll back from.");
    return row;
  }

  row.state = BringUpRowState::kReady;
  row.detail =
      Translate(
          "<b>Running this application's own firmware</b> (%1), with a "
          "gateware that can rewrite its own flash. This is the unit "
          "this page is for.")
          .arg(QString::fromStdString(
                   capture::ParseFirmwareCommit(identity->product_string)
                       .value_or("commit unknown"))
                   .toHtmlEscaped());
  return row;
}

bool RollbackDeviceIsReady(
    const std::optional<capture::DeviceInfo>& device,
    const std::optional<capture::DeviceIdentity>& identity) {
  return RollbackDeviceRow(device, identity).state == BringUpRowState::kReady;
}

QString RollbackImageText() {
  return Translate(
      "<p>A <b>legacy rollback file</b>, published with the firmware releases "
      "and carrying the original firmware and gateware. It is an ordinary "
      "signed update file with a different payload and a different label, and "
      "it is checked here exactly as any other is — the signature first, then "
      "every payload's digest.</p>"

      "<p>An ordinary update file chosen here is refused with a sentence "
      "saying so, and the reverse is true too: this file cannot be installed "
      "from <i>Update firmware…</i>, because what it does is not an "
      "update.</p>");
}

QString RollbackImageSummary(const capture::UpdateManifest& manifest) {
  QString summary =
      Translate("<p><b>Rollback set %1</b>, built from %2.</p>")
          .arg(QString::fromStdString(manifest.version).toHtmlEscaped(),
               QString::fromStdString(manifest.commit).toHtmlEscaped());

  QStringList carried;
  if (manifest.firmware.has_value()) {
    carried << Translate("the original firmware");
  }
  if (manifest.factory_gateware.has_value()) {
    carried << Translate("the original gateware");
  }
  if (!carried.isEmpty()) {
    summary +=
        QStringLiteral("<p>") +
        Translate("It carries %1.").arg(carried.join(Translate(" and "))) +
        QStringLiteral("</p>");
  }

  if (!manifest.release_notes.empty()) {
    summary += QStringLiteral("<p><i>") +
               QString::fromStdString(manifest.release_notes).toHtmlEscaped() +
               QStringLiteral("</i></p>");
  }

  return summary;
}

QString RollbackImageProblem(const capture::UpdateManifest& manifest) {
  if (manifest.purpose != capture::UpdatePurpose::kRollback) {
    return Translate(
        "This is an ordinary update file rather than a legacy rollback. Use "
        "Tools ▸ Firmware ▸ Update firmware… to install it.");
  }

  // Both halves, or neither. A rollback that installed only the firmware
  // would leave the original firmware driving the current gateware, which is
  // the one pairing every ordering in this application exists to avoid.
  if (!manifest.firmware.has_value() ||
      !manifest.factory_gateware.has_value()) {
    return Translate(
        "A rollback file has to carry both the original firmware and the "
        "original gateware, and this one does not. Rolling back one half "
        "alone would leave the two ends of the board disagreeing about a "
        "wire they share.");
  }

  return QString();
}

QString RollbackGatewareText(int seconds) {
  return Translate(
             "<p>The FPGA goes first, and the order matters: the original "
             "firmware and the current gateware both drive one wire between "
             "the two boards, so the firmware is the last thing to change. "
             "While this runs, the unit is still running the firmware that is "
             "doing the writing.</p>"

             "<p>The original gateware is written to the start of the "
             "configuration flash, over the factory image — through the "
             "current gateware's own flash bridge, exactly as an ordinary "
             "gateware update is written.</p>") +
         Translate("<p><b>Expect about %1 seconds.</b> ").arg(seconds) +
         Translate(
             "It pauses every few seconds while a block is erased. Nothing "
             "changes about what the unit is running until the power "
             "cycle.</p>");
}

QString RollbackFirmwareText(int seconds) {
  return Translate(
             "<p>And now the firmware, by an ordinary update transfer — no "
             "jumper, no boot ROM, nothing to move. The firmware running now "
             "is its own flasher, and this is the last thing it does.</p>"

             "<p>The device is <b>not</b> restarted at the end of this step. "
             "Both halves become the running ones at the power cycle on the "
             "next page, or neither does.</p>") +
         Translate("<p><b>Expect about %1 seconds.</b></p>").arg(seconds);
}

QString RollbackPowerCycleText() {
  return Translate(
      "<p>Unplug the USB 3.0 cable, wait a couple of seconds, and plug it "
      "back in.</p>"

      "<p>This is the moment the unit becomes a legacy Duplicator: the FX3 "
      "re-reads where it boots from and the FPGA loads the gateware that has "
      "just been written. Until now it has been running the current software "
      "with the original one sitting in its flash.</p>"

      "<p>If the DE0-Nano's mini-USB cable happens to be connected as well, "
      "that one has to come out too — either cable alone keeps the unit "
      "powered, and a unit that never lost power never reboots.</p>");
}

QString RollbackPowerCycleTimeoutText() {
  return Translate(
      "<p>Nothing has appeared yet. The likeliest reason is that the unit "
      "never actually lost power: if the DE0-Nano's mini-USB cable is "
      "connected, it keeps the whole assembly alive on its own, so pulling "
      "only the USB 3.0 cable changes nothing while the board stays lit.</p>"

      "<p>A rolled-back unit also appears as a <i>different</i> device, so a "
      "machine that asks about new hardware may be waiting for an answer.</p>");
}

QString RollbackVerifyText(bool legacy) {
  if (!legacy) {
    return Translate(
        "<p>The unit has not come back as a legacy Duplicator. Both images "
        "were written and checked, so the likeliest explanation is that it "
        "has not been power-cycled yet — or that only one of two cables came "
        "out.</p>"

        "<p>Nothing is broken by stopping here: the images are on the device "
        "and the next power cycle will start them.</p>");
  }

  return Translate(
      "<p>The unit is running the original firmware and the original "
      "gateware. This application can see it and can say what it is, and that "
      "is all it can do with it — the legacy software is what drives it from "
      "here.</p>"

      "<p><b>To come back</b>, use <i>Tools ▸ Firmware ▸ Legacy ▸ Bring up a "
      "new or legacy board…</i>. That flow needs the case off and the "
      "DE0-Nano's mini-USB cable, and it puts this unit back exactly as it "
      "was.</p>");
}

QString RollbackVerifySummary(bool legacy) {
  return legacy ? Translate("Rollback complete.")
                : Translate("The device has not come back yet.");
}

}  // namespace ddd::gui
