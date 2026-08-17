/************************************************************************

    bringup_text.cpp

    What the bring-up wizard says, in one place
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "bringup_text.h"

#include <QCoreApplication>
#include <QStringList>
#include <string>

#include "firmware_version.h"
#include "update_gate.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

QString Translate(const char* text) {
  return QCoreApplication::translate("BringUpText", text);
}

// The one sentence that appears on every page asking for a power cycle, and
// the reason it is a function rather than three copies of a string: if it is
// ever reworded it has to be reworded everywhere, because a user who reads
// "unplug both cables" once and "unplug the cable" the next time has been
// given permission to do the thing that silently does nothing.
QString BothCables() {
  return Translate(
      "Unplug <b>both</b> USB cables — the kit's USB 3.0 cable and the "
      "DE0-Nano's mini-USB. Either one on its own keeps the unit powered, so "
      "pulling only one changes nothing while the board stays lit and looks "
      "perfectly normal.");
}

}  // namespace

QString BringUpPageTitle(BringUpPage page) {
  switch (page) {
    case BringUpPage::kOverview:
      return Translate("1 of 9");
    case BringUpPage::kConnect:
      return Translate("2 of 9");
    case BringUpPage::kImage:
      return Translate("3 of 9");
    case BringUpPage::kJumper:
      return Translate("4 of 9");
    case BringUpPage::kFirmware:
      return Translate("5 of 9");
    case BringUpPage::kRemoveJumper:
      return Translate("6 of 9");
    case BringUpPage::kGateware:
      return Translate("7 of 9");
    case BringUpPage::kPowerCycle:
      return Translate("8 of 9");
    case BringUpPage::kVerify:
      return Translate("9 of 9");
  }
  return QString();
}

QString BringUpPageHeading(BringUpPage page) {
  switch (page) {
    case BringUpPage::kOverview:
      return Translate("What this does, and what it will ask of you");
    case BringUpPage::kConnect:
      return Translate("Both boards, connected");
    case BringUpPage::kImage:
      return Translate("The provisioning set");
    case BringUpPage::kJumper:
      return Translate("Fit jumper J4");
    case BringUpPage::kFirmware:
      return Translate("Program the FX3");
    case BringUpPage::kRemoveJumper:
      return Translate("Remove jumper J4");
    case BringUpPage::kGateware:
      return Translate("Program the FPGA");
    case BringUpPage::kPowerCycle:
      return Translate("Power cycle");
    case BringUpPage::kVerify:
      return Translate("What the device is running now");
  }
  return QString();
}

QString BringUpOverviewText() {
  return Translate(
      "<p>This programs both halves of a Domesday Duplicator from nothing: "
      "the FX3's firmware and the FPGA's configuration flash. It is what a "
      "newly built board needs, and what a board running the original "
      "Duplicator firmware needs before it can be updated from this "
      "application at all.</p>"

      "<p><b>Everything physical, before anything starts.</b> Doing the whole "
      "list now means doing it once:</p>"

      "<ul>"
      "<li><b>Take the unit out of its enclosure.</b> The DE0-Nano's mini-USB "
      "connector cannot be reached with the case on, and the FPGA half of this "
      "cannot be done without it.</li>"
      "<li><b>Connect both cables</b> — the kit's USB 3.0 cable and the "
      "DE0-Nano's mini-USB — and leave both connected throughout.</li>"
      "<li>Expect to <b>fit and then remove a jumper</b>, and to <b>unplug "
      "both cables twice</b> along the way. Once, for a board that is already "
      "waiting in its boot ROM.</li>"
      "</ul>"

      "<p>Both halves are always done. A board whose firmware is out of date "
      "almost always has gateware of the same age — they are built and "
      "released together — so this does not offer to skip the FPGA on the "
      "strength of a guess and then send you back for a screwdriver.</p>"

      "<p><b>Where this leaves you.</b> Your Duplicator will be running "
      "current firmware with the recovery gateware: working, enumerating, and "
      "not yet able to capture. One ordinary firmware update finishes the job, "
      "and it needs no cables moved and no case opened.</p>");
}

QString BringUpDurationText() {
  return Translate(
      "About fifteen minutes, most of it spent watching the FPGA's flash being "
      "written.");
}

// --- the connectivity page ------------------------------------------------

QString BringUpMark(BringUpRowState state) {
  switch (state) {
    case BringUpRowState::kReady:
      return QStringLiteral("✓");
    case BringUpRowState::kWaiting:
      return QStringLiteral("•");
    case BringUpRowState::kProblem:
      return QStringLiteral("✕");
  }
  return QStringLiteral("✕");
}

QString BringUpMarkColour(BringUpRowState state) {
  switch (state) {
    case BringUpRowState::kReady:
      return QStringLiteral("#27ae60");
    case BringUpRowState::kWaiting:
      return QStringLiteral("#d68910");
    case BringUpRowState::kProblem:
      return QStringLiteral("#c0392b");
  }
  return QStringLiteral("#c0392b");
}

QString BringUpConnectLegend() {
  // The amber case is the one worth the words. A coloured mark that is not
  // green reads as a fault, and here it usually is not: it means this board is
  // in a state the wizard already expects and will deal with a few pages
  // further on.
  const QString mark = [](BringUpRowState state) {
    return QStringLiteral("<span style=\"color:%1\"><b>%2</b></span>")
        .arg(BringUpMarkColour(state), BringUpMark(state));
  }(BringUpRowState::kReady);

  return Translate(
             "<p>%1 means that board is ready as it is. %2 means the wizard "
             "will ask you to do something to it further on — fit a jumper, or "
             "pull both cables — and <b>not</b> that anything is wrong with "
             "it. "
             "%3 is something to put right before going on.</p>")
      .arg(mark,
           QStringLiteral("<span style=\"color:%1\"><b>%2</b></span>")
               .arg(BringUpMarkColour(BringUpRowState::kWaiting),
                    BringUpMark(BringUpRowState::kWaiting)),
           QStringLiteral("<span style=\"color:%1\"><b>%2</b></span>")
               .arg(BringUpMarkColour(BringUpRowState::kProblem),
                    BringUpMark(BringUpRowState::kProblem)));
}

BringUpStatusRow BringUpFx3Row(const std::optional<capture::DeviceInfo>& fx3,
                               capture::UsbPresence debug_bridge) {
  BringUpStatusRow row;
  row.title = Translate("FX3 board (SuperSpeed Explorer Kit)");

  if (fx3.has_value()) {
    // The commit the running firmware reports, when it reports one. Naming it
    // is what separates "this is the build you just installed" from "this is
    // some firmware or other", which is the whole of what somebody looking at
    // this row wants to know.
    const std::optional<std::string> commit =
        capture::ParseFirmwareCommit(fx3->product_string);
    const QString named =
        commit.has_value()
            ? Translate(" (%1)").arg(QString::fromStdString(*commit))
            : QString();

    switch (fx3->personality) {
      case capture::DevicePersonality::kRecovery:
        row.state = BringUpRowState::kReady;
        row.detail = Translate(
            "Waiting in its boot ROM, which is where this needs it. Nothing "
            "has to be done to the jumper.");
        return row;

      case capture::DevicePersonality::kApplication:
        row.state = BringUpRowState::kWaiting;
        if (capture::ProtocolVersionIsSupported(fx3->protocol_version)) {
          // A board that already works. Somebody who only wants to update it
          // is in the wrong window, and the row says so rather than leading
          // them through nine pages of jumpers to find out.
          row.detail =
              Translate(
                  "Running this application's own firmware%1, so this board "
                  "does not need bringing up. If you only want to update it, "
                  "close this and use <b>Tools ▸ Firmware ▸ Update "
                  "firmware…</b> instead. Carrying on here is safe and will "
                  "reinstall both halves — it will be asked to reach its boot "
                  "ROM, which means fitting a jumper.")
                  .arg(named);
        } else {
          row.detail =
              Translate(
                  "Running Duplicator firmware%1 that predates this "
                  "application's update agent: it enumerates under the current "
                  "identifiers but has no way to update itself, which is one "
                  "of the two boards this wizard exists for. It will be asked "
                  "to reach its boot ROM, which means fitting a jumper.")
                  .arg(named);
        }
        return row;

      case capture::DevicePersonality::kLegacy:
        row.state = BringUpRowState::kWaiting;
        row.detail = Translate(
            "Running the <b>original</b> Duplicator firmware — the one from "
            "before this application existed, enumerating as 1d50:603b. This "
            "is exactly what this wizard is for: it will be asked to reach its "
            "boot ROM, which means fitting a jumper.");
        return row;

      case capture::DevicePersonality::kFlashProgrammer:
        row.state = BringUpRowState::kProblem;
        row.detail = Translate(
            "Running the Cypress secondary loader, left in memory by a "
            "programming session that did not finish. Unplug both USB cables, "
            "wait a moment and reconnect them.");
        return row;
    }
  }

  if (debug_bridge == capture::UsbPresence::kPresent) {
    row.state = BringUpRowState::kProblem;
    row.detail = Translate(
        "The kit's debug serial port is answering, so the board has power — "
        "but nothing is answering on the USB 3.0 link. Check that cable, and "
        "that it is in a USB 3.0 socket.");
    return row;
  }

  row.state = BringUpRowState::kProblem;
  row.detail = Translate(
      "Nothing found. Connect the kit's USB 3.0 cable, and on Linux check that "
      "the device rules are installed "
      "(fx3/programmer/configs/70-domesday-duplicator.rules).");
  return row;
}

BringUpStatusRow BringUpFpgaRow(bool opened, capture::UsbPresence presence,
                                const QString& problem) {
  BringUpStatusRow row;
  row.title = Translate("FPGA board (DE0-Nano, on-board USB-Blaster)");

  if (opened) {
    row.state = BringUpRowState::kReady;
    row.detail = Translate("Found and opened.");
    return row;
  }

  row.state = BringUpRowState::kProblem;

  // Attached and unopenable is a different problem from absent, and it is the
  // one with a remedy the user can act on. The cable driver's own sentence
  // says which — Quartus's jtagd holding it, the udev rules on Linux, the
  // driver binding on Windows.
  if (presence == capture::UsbPresence::kPresent) {
    row.detail =
        problem.isEmpty()
            ? Translate(
                  "A USB-Blaster is attached but could not be opened. On Linux "
                  "this is the udev rules "
                  "(fpga/configs/70-altera-usb-blaster.rules); on Windows it "
                  "is the driver binding. Quartus's own jtagd holds the cable "
                  "open whenever it is running.")
            : problem;
    return row;
  }

  // The charge-only cable first, and deliberately: it is the documented
  // failure of this exact step, and in an assembled unit it is especially
  // misleading, because the board is lit from the USB 3.0 side no matter what
  // the mini-USB is doing.
  row.detail = Translate(
      "Nothing found on the DE0-Nano's mini-USB connector. The commonest cause "
      "is a charge-only cable, which carries power and no data — and the board "
      "will be lit either way, because the USB 3.0 cable is powering it. Try a "
      "cable you know carries data, then check the connector itself.");
  if (!problem.isEmpty()) {
    row.detail += QStringLiteral("<br><br>") + problem;
  }
  return row;
}

// --- the physical pages ---------------------------------------------------

QString BringUpFitJumperText() {
  return Translate(
             "<p>The FX3 has to be running its boot ROM before its firmware "
             "can be "
             "written, and the jumper is what puts it there.</p>"

             "<p><b>Fit jumper J4</b> on the FX3 board — the two-pin "
             "<tt>PMODE</tt> "
             "header — as shown below. Then:</p>") +
         QStringLiteral("<p>") + BothCables() + QStringLiteral("</p>") +
         Translate(
             "<p>Reconnect them both, and this page will notice the board "
             "come back. The jumper only takes effect on a boot, and the unit "
             "does not boot while either cable is still feeding it.</p>");
}

QString BringUpRemoveJumperText() {
  return Translate(
      "<p>The FX3's firmware is written. <b>Remove jumper J4</b>, so that the "
      "board boots from its own EEPROM from now on rather than waiting for a "
      "host.</p>"

      "<p>Same board, same view as the last photograph — the header is bare "
      "this time.</p>"

      "<p>Do not unplug anything yet. There is one power cycle at the end and "
      "it serves both halves of this.</p>");
}

QString BringUpPowerCycleText() {
  return Translate("<p>One power cycle, and it discharges two obligations: ") +
         Translate(
             "the FX3 has to re-read where it boots from, and the FPGA has to "
             "reload from the flash that has just been written — it is "
             "currently running Altera's serial flash loader, which is what "
             "put the image there.</p>") +
         QStringLiteral("<p>") + BothCables() + QStringLiteral("</p>") +
         Translate(
             "<p>Wait a couple of seconds, then reconnect them both. This page "
             "will notice the Duplicator come back.</p>");
}

QString BringUpPowerCycleTimeoutText() {
  return Translate(
      "<p><b>Did both cables come out?</b> That is nearly always what this "
      "is. Either cable on its own keeps the unit powered, so if one stayed "
      "in, nothing rebooted — and the board will have looked exactly as it "
      "should throughout.</p>"

      "<p>Unplug both, count to three, and reconnect them.</p>"

      "<p>If it still does not come back: the jumper may still be fitted, "
      "which would leave the FX3 waiting in its boot ROM. Nothing here is "
      "damaged, and this wizard can simply be run again.</p>");
}

QString BringUpPhotographPath(BringUpPage page) {
  switch (page) {
    case BringUpPage::kOverview:
      return QStringLiteral(":/photographs/fpga-usb-port.jpg");
    case BringUpPage::kJumper:
      return QStringLiteral(":/photographs/fx3-j4-fitted.jpg");
    case BringUpPage::kRemoveJumper:
      return QStringLiteral(":/photographs/fx3-j4-removed.jpg");
    default:
      return QString();
  }
}

QString BringUpPhotographCaption(BringUpPage page) {
  switch (page) {
    case BringUpPage::kOverview:
      return Translate(
          "The DE0-Nano's mini-USB connector, underneath the FX3 kit. This is "
          "the one that has to be connected as well.");
    case BringUpPage::kJumper:
      return Translate("Jumper J4 fitted — the USB-boot position.");
    case BringUpPage::kRemoveJumper:
      return Translate("Jumper J4 removed — the EEPROM-boot position.");
    default:
      return QString();
  }
}

// --- the working pages ----------------------------------------------------

QString BringUpFirmwareText() {
  return Translate(
      "<p>The FX3's boot ROM is handed the firmware, runs it from memory, and "
      "that firmware then writes and checks its own EEPROM — the same "
      "mechanism, the same digests and the same readback an ordinary update "
      "uses.</p>"

      "<p>The device is <b>not</b> restarted at the end of this step, because "
      "the jumper is still fitted and a restart would land it back in its boot "
      "ROM. The restart comes later, once the jumper is off.</p>"

      "<p><b>Leave both cables connected.</b> A minute or so.</p>");
}

QString BringUpGatewareText(int seconds) {
  const int minutes = (seconds + 59) / 60;
  return Translate(
             "<p>The FPGA's configuration flash is written through the "
             "DE0-Nano's own USB-Blaster, which is the only route to a board "
             "with no working gateware on it.</p>"

             "<p>This writes the <b>factory image</b>: the one the board falls "
             "back to, and the one that makes ordinary gateware updates "
             "possible from then on.</p>") +
         Translate("<p><b>Expect this to take about %1 minutes.</b> ")
             .arg(minutes) +
         Translate(
             "It pauses for long stretches while blocks of flash are erased, "
             "which is the flash doing its job rather than anything being "
             "stuck. Leave both cables connected throughout.</p>");
}

QString BringUpBundledSetText() {
  return Translate(
      "<p><b>This application carries a provisioning set</b>, published with "
      "the firmware release it was built beside, and it has been chosen for "
      "you.</p>"
      "<p>It is checked here exactly as a downloaded one would be — the "
      "signature first, then every payload's digest — because arriving with "
      "the application is not a reason to trust a file. Choose a different one "
      "below if you have a newer set.</p>");
}

QString BringUpChosenSetText() {
  return Translate(
      "<p>Using the file you chose. The set that came with this application is "
      "still there — <b>Use the bundled set</b> goes back to it.</p>");
}

QString BringUpBundledSetUnusableText() {
  return Translate(
      "<p><b>The provisioning set that came with this application could not be "
      "used</b>, and what is wrong with it is below.</p>"
      "<p>Nothing here can repair it — a set is either intact and signed or it "
      "is not. Download "
      "<code>domesday-duplicator-provisioning-&lt;version&gt;.dddfw</code> "
      "from the firmware release page and choose it below.</p>");
}

QString BringUpNoBundledSetText() {
  return Translate(
      "<p><b>This build carries no provisioning set</b>, so there is one thing "
      "to fetch before starting: download "
      "<code>domesday-duplicator-provisioning-&lt;version&gt;.dddfw</code> "
      "from the firmware release page and choose it below.</p>"
      "<p>Do that on a machine with a network if this one has none — the file "
      "is all that is needed, and nothing else in this procedure goes near "
      "the internet.</p>");
}

QString BringUpImageSummary(const capture::UpdateManifest& manifest) {
  QString text = Translate("Version %1 (%2)")
                     .arg(QString::fromStdString(manifest.version),
                          QString::fromStdString(manifest.commit));

  QStringList carries;
  if (manifest.firmware.has_value()) {
    carries << Translate("FX3 firmware");
  }
  if (manifest.provisioning.has_value()) {
    carries << Translate("FPGA provisioning gateware");
  }
  if (!carries.isEmpty()) {
    text += QStringLiteral("<br>") +
            Translate("Carries: %1").arg(carries.join(Translate(", ")));
  }

  if (!manifest.release_notes.empty()) {
    text += QStringLiteral("<br>") +
            QString::fromStdString(manifest.release_notes).toHtmlEscaped();
  }
  return text;
}

QString BringUpImageProblem(const capture::UpdateManifest& manifest) {
  const bool has_firmware = manifest.firmware.has_value();
  const bool has_vectors = manifest.provisioning.has_value();

  if (has_firmware && has_vectors) {
    return QString();
  }

  if (!has_firmware && !has_vectors) {
    return Translate(
        "This is an ordinary update file. Bringing a board up needs a "
        "provisioning set, which carries the FPGA's gateware as JTAG vectors "
        "as well as the firmware.");
  }
  if (!has_vectors) {
    return Translate(
        "This file carries firmware but no provisioning gateware, so it cannot "
        "bring up the FPGA. Choose a provisioning set.");
  }
  return Translate(
      "This file carries provisioning gateware but no firmware. Bring-up "
      "programs the FX3 first — always, because the original firmware must "
      "never be running underneath the new gateware — so a set without "
      "firmware cannot be used.");
}

// --- the end --------------------------------------------------------------

std::vector<BringUpCheck> BringUpVerification(
    bool attached, capture::DevicePersonality personality,
    const capture::DeviceIdentity& identity, const QString& expected_commit) {
  std::vector<BringUpCheck> checks;

  BringUpCheck enumerated;
  enumerated.description =
      Translate("The Duplicator's own firmware is running");
  enumerated.passed =
      attached && personality == capture::DevicePersonality::kApplication;
  checks.push_back(enumerated);

  if (!enumerated.passed) {
    // Nothing below can be read off a device that is not answering, and a
    // list of failures caused by one failure describes one problem four
    // times.
    return checks;
  }

  BringUpCheck protocol;
  protocol.description = Translate("It speaks a protocol this build knows");
  protocol.passed =
      capture::ProtocolVersionIsSupported(identity.protocol_version);
  checks.push_back(protocol);

  if (!expected_commit.isEmpty()) {
    BringUpCheck commit;
    commit.description =
        Translate("Its firmware is the build this set carries (%1)")
            .arg(expected_commit);
    commit.passed = capture::CommitsMatch(
        capture::ParseFirmwareCommit(identity.product_string)
            .value_or(std::string()),
        expected_commit.toStdString());
    checks.push_back(commit);
  }

  BringUpCheck gateware;
  gateware.description =
      Translate("Its FPGA is answering, on the recovery gateware");
  gateware.passed = identity.gateware_present &&
                    identity.register_map_version >=
                        capture::kRegisterMapVersionWithImageRole &&
                    identity.image_role == capture::kImageRoleFactory;
  checks.push_back(gateware);

  return checks;
}

QString BringUpCompleteText() {
  return Translate(
      "<p><b>Bring-up complete.</b> The device is running current firmware and "
      "its recovery gateware — working and enumerating, and not yet able to "
      "capture.</p>"

      "<p>One ordinary update finishes it: <b>Tools ▸ Firmware ▸ Update "
      "firmware…</b>, with the current release bundle. Nothing after this "
      "point is physical, so <b>the case can go back on first</b>.</p>");
}

QString BringUpIncompleteText() {
  return Translate(
      "<p>The programming steps finished, but not everything could be "
      "confirmed by reading the device back. Nothing is damaged, and nothing "
      "is half-written: both halves were written and checked before this "
      "page.</p>"

      "<p>Unplug both USB cables, reconnect them, and run this wizard again — "
      "it is safe to run as many times as you like.</p>");
}

QString BringUpFailureText(const QString& problem) {
  return QStringLiteral("<p><b>") + problem.toHtmlEscaped() +
         QStringLiteral("</b></p>") +
         Translate(
             "<p><b>Nothing here can be broken by stopping part way.</b> The "
             "FX3 can always be reached again by fitting jumper J4, and the "
             "FPGA's flash can always be reached again through the "
             "USB-Blaster. Every step of this can simply be run again from the "
             "beginning.</p>");
}

QString BringUpStoppedText() {
  return Translate(
      "<p>Stopped. Nothing is damaged — a partly written flash is not a "
      "broken one, and the board reads from it only after a power cycle.</p>"

      "<p>Run this wizard again from the beginning when you are ready.</p>");
}

}  // namespace ddd::gui
