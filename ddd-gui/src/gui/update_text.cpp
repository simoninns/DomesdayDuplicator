/************************************************************************

    update_text.cpp

    What the update page says, in one place
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_text.h"

#include <QCoreApplication>
#include <optional>
#include <string>

#include "firmware_version.h"

namespace ddd::gui {
namespace {

QString Translate(const char* text) {
  return QCoreApplication::translate("UpdateText", text);
}

QString FromStdString(const std::string& text) {
  return QString::fromStdString(text);
}

QString NotReported() { return Translate("Not reported"); }

// Two commits that name the same build read as the same version, even when
// one of them is seven characters and the other eight.
bool SameBuild(const std::string& left, const std::string& right) {
  return capture::CommitsMatch(left, right);
}

}  // namespace

std::vector<UpdateVersionRow> UpdateVersionRows(
    const QString& application_version, const capture::DeviceIdentity& device,
    bool device_attached, const capture::UpdateManifest* bundle,
    capture::DevicePersonality personality) {
  std::vector<UpdateVersionRow> rows;

  // A device that is not running the Duplicator's firmware reports no
  // versions, because there is nothing on it to report them. Saying "not
  // reported" three times would look like three faults; saying what the
  // device actually is, once per row, says the one true thing.
  const bool reports_versions =
      personality == capture::DevicePersonality::kApplication;

  // The application, first, and in the list even though this dialog cannot
  // update it: leaving it out would answer two thirds of "am I up to date".
  UpdateVersionRow application;
  application.name = Translate("Application");
  application.installed =
      application_version.isEmpty() ? NotReported() : application_version;
  if (bundle != nullptr && !bundle->version.empty()) {
    application.available = FromStdString(bundle->version);

    // Compared as dotted versions, because that is the one comparison a
    // release version supports and a commit does not.
    const std::optional<int> ordering = capture::CompareDottedVersions(
        application_version.toStdString(), bundle->version);
    application.changes = ordering.has_value() && *ordering < 0;
  }
  rows.push_back(application);

  UpdateVersionRow firmware;
  firmware.name = Translate("Firmware");
  if (!device_attached) {
    firmware.installed = Translate("No device attached");
  } else if (!reports_versions) {
    firmware.installed = Translate("None installed");
  } else {
    const std::optional<std::string> commit =
        capture::ParseFirmwareCommit(device.product_string);
    firmware.installed =
        commit.has_value() ? FromStdString(*commit) : NotReported();
  }
  if (bundle != nullptr && bundle->firmware.has_value()) {
    firmware.available = FromStdString(bundle->firmware->identity);

    const std::optional<std::string> commit =
        capture::ParseFirmwareCommit(device.product_string);
    firmware.changes =
        device_attached && (!reports_versions ||
                            !(commit.has_value() &&
                              SameBuild(*commit, bundle->firmware->identity)));
  }
  rows.push_back(firmware);

  UpdateVersionRow gateware;
  gateware.name = Translate("Gateware");
  if (!device_attached) {
    gateware.installed = Translate("No device attached");
  } else if (!reports_versions) {
    // Not "none": the FPGA is a separate part with its own memory, and
    // whatever is in it is untouched by the FX3 having no firmware. What is
    // true is that nothing can ask it.
    gateware.installed = Translate("Cannot be read");
  } else if (!device.gateware_present) {
    gateware.installed = NotReported();
  } else if (device.GatewareIsRecovery()) {
    // Not the commit, which would be the resident image's and would read as
    // a perfectly ordinary gateware version. What is installed, as far as
    // anything that could capture is concerned, is nothing.
    gateware.installed = Translate("Recovery gateware");
  } else if (device.gateware_commit.empty()) {
    gateware.installed = Translate("Unknown");
  } else {
    gateware.installed = FromStdString(device.gateware_commit);
  }
  if (bundle != nullptr && bundle->gateware.has_value()) {
    gateware.available = FromStdString(bundle->gateware->identity);

    // A unit in recovery is always changed by a gateware update, whatever
    // the factory image's commit happens to be — it is not the commit that
    // is being replaced, it is the absence of a working one.
    gateware.changes =
        device_attached && reports_versions &&
        (device.GatewareIsRecovery() ||
         !SameBuild(device.gateware_commit, bundle->gateware->identity));
  }
  rows.push_back(gateware);

  return rows;
}

QString UpdateVersionTable(const std::vector<UpdateVersionRow>& rows) {
  QString html = QStringLiteral(
                     "<table cellspacing='0' cellpadding='4'><tr>"
                     "<th align='left'>%1</th><th align='left'>%2</th>"
                     "<th align='left'>%3</th></tr>")
                     .arg(Translate("Part").toHtmlEscaped(),
                          Translate("Installed").toHtmlEscaped(),
                          Translate("In this update").toHtmlEscaped());

  for (const UpdateVersionRow& row : rows) {
    const QString available =
        row.available.isEmpty() ? Translate("—") : row.available;

    // Only the rows the update would change are emphasised, so what the
    // update is *for* is visible without reading three lines of hex.
    const QString value =
        row.changes ? QStringLiteral("<b>%1</b>").arg(available.toHtmlEscaped())
                    : available.toHtmlEscaped();

    html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                .arg(row.name.toHtmlEscaped(), row.installed.toHtmlEscaped(),
                     value);
  }

  html += QStringLiteral("</table>");
  return html;
}

QString UpdateStageTitle(capture::UpdateStage stage) {
  switch (stage) {
    case capture::UpdateStage::kChecking:
      return Translate("Checking the update");
    case capture::UpdateStage::kPreparing:
      return Translate("Starting the device up");
    case capture::UpdateStage::kTransferring:
      return Translate("Sending the update to the device");
    case capture::UpdateStage::kWriting:
      return Translate("The device is writing the update");
    case capture::UpdateStage::kVerifying:
      return Translate("The device is checking what it wrote");
    case capture::UpdateStage::kRestarting:
      return Translate("Restarting the device");
    case capture::UpdateStage::kConfirming:
      return Translate("Confirming the new version");
    case capture::UpdateStage::kComplete:
      return Translate("Update complete");
    case capture::UpdateStage::kFailed:
      return Translate("The update did not finish");
  }
  return Translate("Working");
}

QString UpdateStageTitle(capture::UpdateStage stage,
                         capture::UpdateTarget target) {
  const bool gateware = target == capture::UpdateTarget::kGateware;

  switch (stage) {
    case capture::UpdateStage::kTransferring:
      return gateware ? Translate("Sending the gateware to the device")
                      : Translate("Sending the firmware to the device");
    case capture::UpdateStage::kWriting:
      return gateware ? Translate("The device is writing the gateware")
                      : Translate("The device is writing the firmware");
    case capture::UpdateStage::kVerifying:
      return gateware
                 ? Translate("The device is checking the gateware it wrote")
                 : Translate("The device is checking the firmware it wrote");

    default:
      // Every other stage happens once for the whole update rather than once
      // per component, so naming one would be naming the wrong thing.
      return UpdateStageTitle(stage);
  }
}

QString FormatUpdateEstimate(int seconds) {
  if (seconds <= 0) {
    return Translate("a few seconds");
  }

  if (seconds < 90) {
    // Rounded to the half minute below a couple of minutes, because a figure
    // to the second would be precise about something that is a guess.
    const int rounded = ((seconds + 14) / 15) * 15;
    return Translate("about %1 seconds").arg(rounded);
  }

  const int minutes = (seconds + 59) / 60;
  if (minutes == 1) {
    return Translate("about a minute");
  }
  return Translate("about %1 minutes").arg(minutes);
}

QString UpdateHoldStillInstruction() {
  return Translate("Leave the device plugged in and powered.");
}

QString UpdateBundleSummary(const capture::UpdateManifest& manifest) {
  QString summary = Translate("Update %1")
                        .arg(FromStdString(manifest.version).toHtmlEscaped());

  if (!manifest.commit.empty()) {
    summary += QStringLiteral(" (%1)").arg(
        FromStdString(manifest.commit).toHtmlEscaped());
  }

  QStringList parts;
  if (manifest.firmware.has_value()) {
    parts << Translate("firmware");
  }
  if (manifest.gateware.has_value()) {
    parts << Translate("gateware");
  }
  if (!parts.isEmpty()) {
    summary += QStringLiteral("<br>") +
               Translate("Contains: %1").arg(parts.join(QStringLiteral(", ")));
  }

  if (!manifest.release_notes.empty()) {
    summary += QStringLiteral("<br>") +
               FromStdString(manifest.release_notes).toHtmlEscaped();
  }

  return summary;
}

QString DevelopmentBundleBanner() {
  return Translate(
      "This is a development update. Its signature proves the file is well "
      "formed and proves nothing about where it came from, because its "
      "signing key is public. Only install it if you made it or you know who "
      "did.");
}

QString UpdateGateText(const capture::UpdateGateResult& gate) {
  if (gate.reasons.empty()) {
    return {};
  }

  QStringList lines;
  for (const std::string& reason : gate.reasons) {
    lines << FromStdString(reason).toHtmlEscaped();
  }
  return lines.join(QStringLiteral("<br>"));
}

QString UpdateCompleteText(const capture::DeviceIdentity& identity) {
  const std::optional<std::string> firmware =
      capture::ParseFirmwareCommit(identity.product_string);

  QString text =
      Translate("Your device now reports firmware %1")
          .arg(firmware.has_value() ? FromStdString(*firmware).toHtmlEscaped()
                                    : NotReported().toHtmlEscaped());

  if (identity.gateware_present && !identity.gateware_commit.empty()) {
    text += Translate(" and gateware %1")
                .arg(FromStdString(identity.gateware_commit).toHtmlEscaped());
  }

  text += Translate(" — update complete.");
  return text;
}

QString UpdateFailureText(const QString& problem) {
  // Every failure names what happened and the next step, and the engine's
  // messages already do both. What is added here is the reassurance, which is
  // true of every failure this mechanism can produce and is the question a
  // user is actually asking: an interrupted update leaves the device either
  // as it was or in a recovery state this application can repair, and never
  // half-working.
  return problem.toHtmlEscaped() + QStringLiteral("<br><br>") +
         Translate(
             "Your device is not damaged. An update that stops part way "
             "leaves it either as it was or in recovery mode, and this "
             "window can repair it either way.");
}

QString DevicePersonalityText(capture::DevicePersonality personality) {
  switch (personality) {
    case capture::DevicePersonality::kApplication:
      return {};

    case capture::DevicePersonality::kRecovery:
      return Translate(
          "<b>This device is in recovery mode.</b> Its firmware is missing, "
          "which means either that it has never been programmed or that an "
          "update did not finish. Either way it is not damaged, and choosing "
          "an update file below will program it.");

    case capture::DevicePersonality::kFlashProgrammer:
      return Translate(
          "<b>This device is running a programming tool.</b> Something left "
          "it there part way through writing its firmware. Unplug it, plug it "
          "back in, and it will return to recovery mode, where this window "
          "can program it.");

    case capture::DevicePersonality::kLegacy:
      // Named as old rather than as broken, because it is neither damaged nor
      // misconfigured: it is a working device from before this mechanism
      // existed, and the reason this window cannot help is that there is
      // nothing on it to help with.
      return Translate(
          "<b>This device is running the original Duplicator firmware.</b> It "
          "works, but it predates the update mechanism, so it has no way to "
          "receive an update and this window cannot program it. Its firmware "
          "and gateware have to be programmed directly before this "
          "application can use it.");
  }
  return {};
}

QString GatewareRecoveryText(const capture::DeviceIdentity& device) {
  if (!device.GatewareIsRecovery()) {
    return {};
  }

  // The same words the *If an update fails* documentation page uses. A user
  // reading "recovery gateware" here and finding a page that calls it
  // something else has been given two problems.
  return Translate(
      "<b>This device is running its recovery gateware.</b> Its FPGA holds a "
      "small resident image that cannot capture, which is what it falls back "
      "to when a gateware update does not finish. The device is not damaged, "
      "and choosing an update file below will reinstall the gateware.");
}

QString InstallActionLabel(capture::DevicePersonality personality,
                           bool gateware_recovery) {
  // The one device this button can never act on, and the only label here
  // that is not an offer. A disabled button reading "Program this device"
  // beside a paragraph saying this window cannot program it reads as a fault
  // in the application rather than as a fact about the device.
  if (personality == capture::DevicePersonality::kLegacy) {
    return Translate("Cannot be programmed from here");
  }

  // "Program", not "repair", and the difference matters: somebody holding a
  // board they have just built has not broken anything, and a device that has
  // never been programmed is indistinguishable on the wire from one whose
  // update was interrupted. One word that is true of both is better than a
  // guess between them.
  if (personality != capture::DevicePersonality::kApplication) {
    return Translate("Program this device");
  }

  // A unit in gateware recovery is a different case and can be named
  // exactly, because the FPGA says which image it is running: the firmware
  // is fine, the gateware is the resident one, and what is wanted is the
  // half that is missing.
  return gateware_recovery ? Translate("Reinstall gateware")
                           : Translate("Update");
}

QString DeviceListPersonalitySuffix(capture::DevicePersonality personality) {
  switch (personality) {
    case capture::DevicePersonality::kApplication:
      return {};
    case capture::DevicePersonality::kRecovery:
      return Translate(" — recovery mode, no firmware installed");
    case capture::DevicePersonality::kFlashProgrammer:
      return Translate(" — running a programming tool; unplug and reconnect");
    case capture::DevicePersonality::kLegacy:
      return Translate(" — original firmware, too old for this application");
  }
  return {};
}

}  // namespace ddd::gui
