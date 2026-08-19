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

// One mark, in its colour. The rows, the legend, the verification list and
// both status lines all draw their marks through here, so a tick can never
// mean one thing in one place and another somewhere else.
QString Marked(BringUpRowState state) {
  return QStringLiteral("<span style=\"color:%1\"><b>%2</b></span>")
      .arg(BringUpMarkColour(state), BringUpMark(state));
}

// How long a step takes, said the way somebody deciding whether to wait would
// say it. The estimates arrive as a number of seconds, and under a couple of
// minutes that is exactly what a user wants — "about 40 seconds" is a wait to
// sit through. Past that it stops being readable: "about 236 seconds" asks
// the reader to do the division themselves before they know whether they have
// time to make a cup of tea, so anything longer is rounded to the nearest
// minute and said in minutes.
//
// The rounding is deliberately coarse. These are estimates from a bytes-per-
// second rate, not measurements, and a figure quoted to the second invites a
// user to treat a bar that runs ten seconds over as something having gone
// wrong.
QString DurationPhrase(int seconds) {
  if (seconds < 120) {
    return Translate("%1 seconds").arg(seconds);
  }
  return Translate("%1 minutes").arg((seconds + 30) / 60);
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
    case BringUpPage::kConfigure:
      return Translate("5 of 9");
    case BringUpPage::kProgram:
      return Translate("6 of 9");
    case BringUpPage::kRemoveJumper:
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
      return Translate("The update file");
    case BringUpPage::kJumper:
      return Translate("Fit jumper J4");
    case BringUpPage::kConfigure:
      return Translate("Load the gateware into the FPGA");
    case BringUpPage::kProgram:
      return Translate("Program the board");
    case BringUpPage::kRemoveJumper:
      return Translate("Remove jumper J4");
    case BringUpPage::kPowerCycle:
      return Translate("Power cycle");
    case BringUpPage::kVerify:
      return Translate("What the device is running now");
  }
  return QString();
}

QString BringUpOverviewText() {
  return Translate(
      "<p>This programs a Domesday Duplicator completely — the FX3's firmware, "
      "and both images in the FPGA's configuration flash. At the end the board "
      "is ready to capture, and there is nothing to do afterwards.</p>"

      "<p><b>Have these ready before you start:</b></p>"

      "<ul>"
      "<li>the unit <b>out of its enclosure</b> — the DE0-Nano's mini-USB "
      "connector cannot be reached with the case on;</li>"
      "<li><b>both cables connected</b>, the kit's USB 3.0 cable and the "
      "DE0-Nano's mini-USB, and left connected throughout;</li>"
      "<li>a <b>jumper</b> for the FX3's two-pin PMODE header — needed "
      "whatever the board is running, including one that has never been "
      "programmed;</li>"
      "<li>an <b>update file</b> — this application normally carries one, and "
      "step 3 says so if it does not.</li>"
      "</ul>"

      "<p><b>You will be asked to:</b> fit a jumper, unplug both cables and "
      "reconnect them, press two buttons, remove the jumper, unplug both "
      "cables and reconnect them once more, and take the mini-USB off at the "
      "end. Nothing physical is asked for twice.</p>"

      "<p><b>It does not matter what the board is running now</b>, and running "
      "this twice is harmless.</p>");
}

QString BringUpDurationText() {
  return Translate(
      "About five minutes, most of it spent writing and checking the three "
      "images.");
}

QString BringUpWaitingText(const QString& what) {
  return QStringLiteral("<p>%1 <b>%2</b> %3</p>")
      .arg(Marked(BringUpRowState::kWaiting), Translate("Waiting for"), what);
}

QString BringUpStepDoneText(const QString& what) {
  // **All done** first, and the button to press last. Everything a user
  // wants off a finished step is at the two ends of one short line: whether it
  // worked, and what to do about it. What actually happened goes in the
  // middle, where it can be read by somebody who wants it and skipped by
  // somebody who does not.
  QString text =
      QStringLiteral("<p>%1 <b>%2</b>")
          .arg(Marked(BringUpRowState::kReady), Translate("All done."));
  if (!what.isEmpty()) {
    text += QStringLiteral(" ") + what;
  }
  return text + QStringLiteral(" <b>") +
         Translate("Click “Next ›” to continue.") + QStringLiteral("</b></p>");
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
  return Translate(
             "<p>%1 means that board is ready as it is. %2 means the wizard "
             "will ask you to do something to it further on — fit a jumper, or "
             "pull both cables — and <b>not</b> that anything is wrong with "
             "it. "
             "%3 is something to put right before going on.</p>")
      .arg(Marked(BringUpRowState::kReady), Marked(BringUpRowState::kWaiting),
           Marked(BringUpRowState::kProblem));
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
        // Amber rather than green, and the sentence says why. A board sitting
        // in its boot ROM looks like a board with nothing left to arrange, and
        // it is the one board where that reading does most harm: an FX3 whose
        // EEPROM has never been written comes up here jumper or no jumper, and
        // leaves again at the first restart if there is no jumper holding it.
        row.state = BringUpRowState::kWaiting;
        row.detail = Translate(
            "Waiting in its boot ROM, which is where this needs it — most "
            "likely a newly built kit, whose EEPROM is empty. The jumper is "
            "still asked for further on: an empty board comes up here whether "
            "or not one is fitted, and without it the board would leave the "
            "boot ROM part way through the programming.");
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
                  "firmware…</b> instead, which needs no cables moved and no "
                  "case opened. Carrying on here is safe and reprograms "
                  "everything — it will be asked to reach its boot ROM, which "
                  "means fitting a jumper.")
                  .arg(named);
        } else {
          // Out of range in *either* direction, and the reachable direction is
          // newer rather than older: an application can easily be older than
          // the firmware in front of it. Firmware older than the update agent
          // but under these identifiers was never released — it exists only as
          // an intermediate build inside the repository — so this says what is
          // actually known rather than naming a generation of firmware nobody
          // has.
          row.detail =
              Translate(
                  "Running Duplicator firmware%1 whose protocol this build of "
                  "the application does not know, most likely newer than it. "
                  "Bring-up replaces it with the firmware from the file you "
                  "choose, so this is safe to carry on with.")
                  .arg(named);
        }
        return row;

      case capture::DevicePersonality::kLegacy:
        row.state = BringUpRowState::kWaiting;
        row.detail = Translate(
            "Running the <b>original</b> Duplicator firmware — the one from "
            "before this application existed, enumerating as 1d50:603b. This "
            "is exactly what this wizard is for.");
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
                  "(fx3/programmer/configs/70-domesday-duplicator.rules); on "
                  "Windows it is the driver binding. Quartus's own jtagd holds "
                  "the cable open whenever it is running.")
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
             "<p><b>1. Fit jumper J4</b> on the FX3 board — the two-pin "
             "<tt>PMODE</tt> header shown below. <b>Fit it even if the "
             "FX3 has already been reported as waiting in its boot "
             "ROM.</b> A board whose EEPROM has never been written "
             "comes up there with or without the jumper — and one "
             "that got there without it leaves again at the first "
             "restart, part way through the writing, which is where "
             "the bring-up fails. The jumper is what makes the boot "
             "ROM the place the board comes back to every time.</p>"

             "<p><b>2. </b>") +
         BothCables() + QStringLiteral("</p>") +
         Translate(
             "<p><b>3. Plug both back in.</b></p>"

             "<p>The jumper only takes effect when the board boots, and the "
             "unit does not boot while either cable is still feeding it — "
             "which is why both have to come out.</p>");
}

QString BringUpRemoveJumperText() {
  return Translate(
      "<p><b>Remove jumper J4</b> from the FX3 board — the same header as "
      "before, bare this time, as shown below.</p>"

      "<p><b>Do not unplug anything yet.</b> There is one power cycle on the "
      "next page and it does everything.</p>");
}

QString BringUpPowerCycleText() {
  return Translate("<p><b>1. </b>") + BothCables() + QStringLiteral("</p>") +
         Translate(
             "<p><b>2. Wait a couple of seconds.</b></p>"

             "<p><b>3. Plug both back in.</b></p>"

             "<p>This is what makes the three images you have just written the "
             "running ones: the FX3 re-reads where it boots from, and the FPGA "
             "loads the factory image, which hands over to the application "
             "image beside it.</p>");
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

QString BringUpDeviceNotBackText() {
  return Translate(
      "<p><b>The board went away and has not come back.</b> Both cables came "
      "out — that much was seen — so what is left is one of them not being "
      "back in.</p>"

      "<p><b>Plug both cables in again</b>, and check the USB 3.0 one is "
      "properly seated and in a USB 3.0 socket. Nothing here is damaged.</p>");
}

QString BringUpStillInBootRomText() {
  return Translate(
      "<p><b>The board has come back in its boot ROM</b> rather than running "
      "its own firmware, and there is only one thing that does that: "
      "<b>jumper J4 is still fitted</b>.</p>"

      "<p>Go back a page, take it off, then unplug both cables and reconnect "
      "them. Nothing is wrong with what has been written.</p>");
}

QString BringUpNotReloadedText() {
  return Translate(
      "<p><b>The board did not lose power.</b> It is running its new firmware, "
      "but its FPGA is still holding the gateware that was loaded over the "
      "mini-USB cable earlier — which only happens if the board stayed alive "
      "throughout.</p>"

      "<p><b>Almost certainly only one cable came out.</b> Either one on its "
      "own keeps the unit powered, so pulling just the USB 3.0 cable makes the "
      "board disappear and come back while never restarting.</p>"

      "<p>Unplug <b>both</b> — the kit's USB 3.0 cable and the DE0-Nano's "
      "mini-USB — count to three, and reconnect them.</p>");
}

QString BringUpPhotographPath(BringUpPage page) {
  switch (page) {
    case BringUpPage::kOverview:
      return QStringLiteral(":/photographs/fpga-usb-port.png");
    case BringUpPage::kJumper:
      return QStringLiteral(":/photographs/fx3-j4-fitted.png");
    case BringUpPage::kRemoveJumper:
      return QStringLiteral(":/photographs/fx3-j4-removed.png");
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

QString BringUpConfigureText(int seconds) {
  return Translate(
             "<p><b>Press “Load the gateware” below.</b> It plays the gateware "
             "into the FPGA through the DE0-Nano's own USB-Blaster. Expect it "
             "to take about %1.</p>"

             "<p><b>Nothing is written to the board by this step.</b> The FPGA "
             "holds the gateware in memory only. It is what gives the next "
             "step a way to reach the flash.</p>"

             "<p>Leave both cables connected, and leave the jumper alone. The "
             "FX3 stays in its boot ROM throughout, which is why this comes "
             "before the firmware rather than after it.</p>")
      .arg(DurationPhrase(seconds));
}

QString BringUpProgramText(int seconds) {
  return Translate(
             "<p><b>Press “Program the board” below.</b> It writes three "
             "things, in this order:</p>"

             "<ul>"
             "<li>the FX3's own <b>EEPROM</b>;</li>"
             "<li>the FPGA's <b>factory image</b>, which is what the board "
             "falls back to;</li>"
             "<li>the FPGA's <b>application image</b>, which is what it "
             "captures with.</li>"
             "</ul>"

             "<p><b>Expect the programming to take about %1.</b> Leave both "
             "cables connected and leave the jumper alone. The writing pauses "
             "every few seconds while a block of flash is erased, which is "
             "normal.</p>"

             "<p>Nothing restarts at the end — the power cycle two pages from "
             "now is what starts all three images at once.</p>")
      .arg(DurationPhrase(seconds));
}

QString BringUpBundledFileText() {
  return Translate(
      "<p><b>This application carries an update file</b>, published with the "
      "firmware release it was built beside, and it has been chosen for "
      "you.</p>"
      "<p>Its signature and every payload's digest have been checked here, "
      "exactly as a downloaded one would be — arriving with the application is "
      "not a reason to trust a file. Choose a different one below only if you "
      "have a newer release.</p>");
}

QString BringUpChosenFileText() {
  return Translate(
      "<p>Using the file you chose. The one that came with this application is "
      "still there — <b>Use the bundled file</b> goes back to it.</p>");
}

QString BringUpBundledFileUnusableText() {
  return Translate(
      "<p><b>The update file that came with this application could not be "
      "used</b>, and what is wrong with it is below.</p>"
      "<p>Nothing here can repair it — a file is either intact and signed or "
      "it is not. Download "
      "<code>domesday-duplicator-update-&lt;version&gt;.dddfw</code> from the "
      "firmware release page and choose it below.</p>");
}

QString BringUpNoBundledFileText() {
  return Translate(
      "<p><b>This build carries no update file</b>, so there is one thing to "
      "fetch before starting: download "
      "<code>domesday-duplicator-update-&lt;version&gt;.dddfw</code> from the "
      "firmware release page and choose it below.</p>"
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
    carries << Translate("FPGA gateware as JTAG vectors");
  }
  if (manifest.factory_gateware.has_value()) {
    carries << Translate("factory image");
  }
  if (manifest.gateware.has_value()) {
    carries << Translate("application gateware");
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
  QStringList missing;
  if (!manifest.firmware.has_value()) {
    missing << Translate("the FX3 firmware");
  }
  if (!manifest.provisioning.has_value()) {
    missing << Translate("the gateware as JTAG vectors");
  }
  if (!manifest.factory_gateware.has_value()) {
    missing << Translate("the factory image");
  }
  if (!manifest.gateware.has_value()) {
    missing << Translate("the application gateware");
  }

  if (missing.isEmpty()) {
    return QString();
  }

  // Named one by one rather than reported as "incomplete". The likeliest file
  // to be chosen here by mistake is a real release bundle built before the
  // bring-up payloads existed, and "this file has no JTAG vectors" is a
  // sentence somebody can act on where "this file is unsuitable" is not.
  return Translate(
             "This file cannot bring a board up: it is missing %1. An update "
             "file from the firmware release page carries all four.")
      .arg(missing.join(Translate(", ")));
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
        Translate("Its firmware is the build this file carries (%1)")
            .arg(expected_commit);
    commit.passed = capture::CommitsMatch(
        capture::ParseFirmwareCommit(identity.product_string)
            .value_or(std::string()),
        expected_commit.toStdString());
    checks.push_back(commit);
  }

  // The application image, not the factory one. A board that comes back on its
  // factory image is a board whose application image did not take — everything
  // still works and nothing is damaged, but it cannot capture, so this is the
  // check that separates "brought up" from "brought most of the way up".
  BringUpCheck gateware;
  gateware.description =
      Translate("Its FPGA is answering, on the application gateware");
  gateware.passed = identity.gateware_present &&
                    identity.image_role == capture::kImageRoleApplication;
  checks.push_back(gateware);

  return checks;
}

QString BringUpCompleteText() {
  return Translate(
      "<p><b>All done. Bring-up complete.</b> The device is running the "
      "firmware and gateware from the file you chose, and it is ready to "
      "capture. There is nothing else to do.</p>"

      "<p><b>Unplug the DE0-Nano's mini-USB cable, put the case back on and "
      "click Close.</b> The mini-USB was only ever the way in to a board that "
      "could not yet be reached over USB 3.0, and nothing from here on uses "
      "it. Leave the kit's USB 3.0 cable where it is — that is the one you "
      "capture through.</p>"

      "<p>From now on this board updates itself: <b>Tools ▸ Firmware ▸ "
      "Update firmware…</b>, with no cables moved and no case opened.</p>");
}

QString BringUpIncompleteText() {
  return Translate(
      "<p>The programming steps finished, but not everything could be "
      "confirmed by reading the device back. Nothing is damaged, and nothing "
      "is half-written: every image was written and checked before this "
      "page.</p>"

      "<p><b>Unplug both USB cables, reconnect them, and run this wizard "
      "again.</b> It is safe to run as many times as you like.</p>");
}

QString BringUpFailureText(const QString& problem) {
  return QStringLiteral("<p><b>") + problem.toHtmlEscaped() +
         QStringLiteral("</b></p>") +
         Translate(
             "<p><b>Nothing here can be broken by stopping part way.</b> The "
             "FX3 can always be reached again by fitting jumper J4, and the "
             "FPGA can always be reached again through the USB-Blaster. Every "
             "step of this can simply be run again from the beginning.</p>");
}

QString BringUpStoppedText() {
  return Translate(
      "<p>Stopped. Nothing is damaged — a partly written flash is not a "
      "broken one, and the board reads from it only after a power cycle.</p>"

      "<p>Run this wizard again from the beginning when you are ready.</p>");
}

}  // namespace ddd::gui
