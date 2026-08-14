/************************************************************************

    firmware_text.cpp

    What the Firmware dialog says about the three versions
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "firmware_text.h"

#include <QCoreApplication>
#include <optional>
#include <string>

#include "firmware_version.h"

namespace ddd::gui {
namespace {

QString Translate(const char* text) {
  return QCoreApplication::translate("FirmwareText", text);
}

QString FromStdString(const std::string& text) {
  return QString::fromStdString(text);
}

// One row of the version table.
QString Row(const QString& name, const QString& value, const QString& note) {
  QString row = QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>")
                    .arg(name.toHtmlEscaped(), value.toHtmlEscaped());
  if (!note.isEmpty()) {
    row += QStringLiteral("<tr><td></td><td><i>%1</i></td></tr>")
               .arg(note.toHtmlEscaped());
  }
  return row;
}

// The commit each of the three reports, or nothing where none was reported.
struct Commits {
  std::optional<std::string> application;
  std::optional<std::string> firmware;
  std::optional<std::string> gateware;
};

Commits CollectCommits(const FirmwareVersions& versions) {
  Commits commits;
  commits.application =
      capture::NormaliseCommit(versions.application.toStdString());

  if (!versions.device_attached) {
    return commits;
  }

  commits.firmware =
      capture::ParseFirmwareCommit(versions.product_string.toStdString());

  if (versions.gateware.present && !versions.gateware.commit.empty()) {
    commits.gateware = capture::NormaliseCommit(versions.gateware.commit);
  }

  return commits;
}

QString GatewareValue(const FirmwareVersions& versions) {
  if (!versions.gateware.present) {
    return Translate("Not reported");
  }
  if (versions.gateware.commit.empty()) {
    return Translate("Unknown");
  }

  QString value = FromStdString(versions.gateware.commit);
  if (versions.gateware.dirty) {
    value += Translate(" (modified)");
  }
  return value;
}

QString GatewareNote(const FirmwareVersions& versions) {
  if (!versions.device_attached) {
    return {};
  }

  // The FPGA answering nothing is the ordinary outcome for gateware older than
  // the register interface, and for a board whose FPGA was never programmed.
  // Neither prevents a capture, and the note says which two things to look at
  // rather than naming a fault.
  if (!versions.gateware.present) {
    return Translate(
        "The FPGA did not answer. Its gateware may predate the version "
        "register, or it may not be programmed.");
  }

  if (versions.gateware.commit.empty()) {
    return Translate("The gateware was built outside a source checkout.");
  }

  // A register map this build does not know is a gateware newer than the
  // application, which is worth saying plainly because everything else in the
  // dialog will still look right.
  if (!versions.gateware.MapVersionIsKnown()) {
    return Translate(
        "The gateware implements a newer register map than this application "
        "was built against.");
  }

  return {};
}

QString FirmwareValue(const Commits& commits) {
  if (commits.firmware.has_value()) {
    return FromStdString(*commits.firmware);
  }
  return Translate("Not reported");
}

QString FirmwareNote(const FirmwareVersions& versions, const Commits& commits) {
  if (!versions.device_attached || commits.firmware.has_value()) {
    return {};
  }
  return Translate(
      "The firmware did not report a build. That usually means firmware older "
      "than the version stamp itself.");
}

// The paragraph under the table.
QString Verdict(const FirmwareVersions& versions, const Commits& commits) {
  if (!versions.device_attached) {
    return Translate(
        "No device is attached, so only this application's own build is "
        "known.");
  }

  // Checked before the device's halves, deliberately, and for the same reason
  // firmware_version.cpp checks it first: a build that cannot name its own
  // commit is in no position to accuse anything else of being out of date.
  if (!commits.application.has_value()) {
    return Translate(
        "This application cannot name the commit it was built from, so there "
        "is nothing to compare the device against.");
  }

  const std::string& application = *commits.application;

  const bool firmware_known = commits.firmware.has_value();
  const bool gateware_known = commits.gateware.has_value();

  const bool firmware_matches =
      firmware_known && capture::CommitsMatch(*commits.firmware, application);
  const bool gateware_matches =
      gateware_known && capture::CommitsMatch(*commits.gateware, application);

  if (firmware_known && gateware_known && firmware_matches &&
      gateware_matches) {
    return Translate(
        "All three were built from the same commit, which is what a release "
        "is.");
  }

  if (!firmware_known || !gateware_known) {
    return Translate(
        "Not every part of the device reported which build it is running, so "
        "they cannot all be compared. Capture works normally either way; "
        "updating the device when convenient is what makes this dialog able to "
        "answer the question.");
  }

  return Translate(
      "These were not all built from the same commit. Every release builds the "
      "application, the firmware and the gateware together, so a difference "
      "means one of them was not updated. Capture will work normally; if "
      "something behaves oddly, matching them up is the first thing to try.");
}

}  // namespace

QString FirmwareText(const FirmwareVersions& versions) {
  const Commits commits = CollectCommits(versions);

  const QString unattached = Translate("No device attached");

  QString table = QStringLiteral("<table cellpadding=\"3\">");
  table += Row(Translate("Application"), versions.application, {});
  table += Row(Translate("FX3 firmware"),
               versions.device_attached ? FirmwareValue(commits) : unattached,
               FirmwareNote(versions, commits));
  table += Row(Translate("FPGA gateware"),
               versions.device_attached ? GatewareValue(versions) : unattached,
               GatewareNote(versions));
  table += QStringLiteral("</table>");

  return QStringLiteral("<h3>%1</h3><p>%2</p>%3<p>%4</p>")
      .arg(Translate("Firmware versions"),
           Translate("Each release builds the application, the FX3 firmware "
                     "and the FPGA gateware from one commit. Each reports the "
                     "commit it came from."),
           table, Verdict(versions, commits));
}

}  // namespace ddd::gui
