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
    bool device_attached, const capture::UpdateManifest* bundle) {
  std::vector<UpdateVersionRow> rows;

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
  if (device_attached) {
    const std::optional<std::string> commit =
        capture::ParseFirmwareCommit(device.product_string);
    firmware.installed =
        commit.has_value() ? FromStdString(*commit) : NotReported();
  } else {
    firmware.installed = Translate("No device attached");
  }
  if (bundle != nullptr && bundle->firmware.has_value()) {
    firmware.available = FromStdString(bundle->firmware->identity);

    const std::optional<std::string> commit =
        capture::ParseFirmwareCommit(device.product_string);
    firmware.changes =
        device_attached &&
        !(commit.has_value() && SameBuild(*commit, bundle->firmware->identity));
  }
  rows.push_back(firmware);

  UpdateVersionRow gateware;
  gateware.name = Translate("Gateware");
  if (!device_attached) {
    gateware.installed = Translate("No device attached");
  } else if (!device.gateware_present) {
    gateware.installed = NotReported();
  } else if (device.gateware_commit.empty()) {
    gateware.installed = Translate("Unknown");
  } else {
    gateware.installed = FromStdString(device.gateware_commit);
  }
  if (bundle != nullptr && bundle->gateware.has_value()) {
    gateware.available = FromStdString(bundle->gateware->identity);
    gateware.changes =
        device_attached &&
        !SameBuild(device.gateware_commit, bundle->gateware->identity);
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

}  // namespace ddd::gui
