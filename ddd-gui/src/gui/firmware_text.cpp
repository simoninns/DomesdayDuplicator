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

// The commit each half of the *device* reports, or nothing where none was
// reported.
//
// The application is not here, and that is the change rather than an omission.
// It releases under its own tag, from its own commit, so it has no business in
// a comparison about whether the device's two halves match — see
// firmware_version.h.
struct Commits {
  std::optional<std::string> firmware;
  std::optional<std::string> gateware;
};

Commits CollectCommits(const FirmwareVersions& versions) {
  Commits commits;

  if (!versions.device_attached ||
      versions.personality != capture::DevicePersonality::kApplication) {
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

  // Nothing on the device can reach the FPGA when the FX3 has no firmware, so
  // none of the explanations below applies: the gateware was not asked.
  if (versions.personality != capture::DevicePersonality::kApplication) {
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

  // The recovery gateware answers every question in this dialog and cannot
  // capture. Saying so here matters more than any other note on this page:
  // everything else the user can see looks entirely normal.
  if (versions.gateware.IsRecoveryGateware()) {
    return Translate(
        "Recovery gateware is running. The device cannot capture until its "
        "gateware is reinstalled.");
  }

  return {};
}

QString FirmwareValue(const Commits& commits) {
  if (!commits.firmware.has_value()) {
    return Translate("Not reported");
  }
  return FromStdString(*commits.firmware);
}

QString FirmwareNote(const FirmwareVersions& versions, const Commits& commits) {
  if (!versions.device_attached || commits.firmware.has_value()) {
    return {};
  }

  // "Firmware older than the version stamp" is the wrong explanation for a
  // device that has no firmware at all, and it is the one a user would act on.
  if (versions.personality != capture::DevicePersonality::kApplication) {
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

  // Said before anything is compared, because there is nothing to compare and
  // because this is the one state in this dialog with something to do about
  // it. The words are the ones the Update tab and the documentation use.
  if (versions.personality == capture::DevicePersonality::kRecovery) {
    return Translate(
        "This device is in recovery mode: its firmware is missing, so it "
        "reports no versions and cannot capture. It is not damaged — either "
        "it has never been programmed, or an update did not finish. The "
        "Update tab will program it.");
  }
  // Working hardware, and said so: what is missing is not firmware but the
  // mechanism this dialog is built on, so there is nothing here to repair and
  // nothing to compare.
  if (versions.personality == capture::DevicePersonality::kLegacy) {
    return Translate(
        "This device is running the original Duplicator firmware, from before "
        "the version stamp and the update mechanism. It reports no versions, "
        "it cannot capture with this application, and it cannot update "
        "itself: its firmware and gateware have to be programmed directly.");
  }
  if (versions.personality == capture::DevicePersonality::kFlashProgrammer) {
    return Translate(
        "This device is running a programming tool left behind part way "
        "through writing its firmware. Unplug it and plug it back in; it will "
        "return to recovery mode, and the Update tab can program it from "
        "there.");
  }

  const bool firmware_known = commits.firmware.has_value();
  const bool gateware_known = commits.gateware.has_value();

  if (!firmware_known || !gateware_known) {
    return Translate(
        "Not every part of the device reported which build it is running, so "
        "the two cannot be compared. Capture works normally either way; "
        "updating the device when convenient is what makes this dialog able to "
        "answer the question.");
  }

  // The one comparison worth making here, and the only one these three
  // versions support. The firmware and the gateware are installed together,
  // from one signed bundle, built from one commit — so a difference between
  // them is a device that was half updated, which is a real thing to say.
  //
  // The application is deliberately not in it. It releases separately, under
  // its own tag, so it shares a commit with the device only by coincidence.
  // Comparing it here is what used to make this dialog report a correctly
  // updated Duplicator as a mismatched set.
  if (capture::CommitsMatch(*commits.firmware, *commits.gateware)) {
    return Translate(
        "The firmware and the gateware came from the same build, which is "
        "what a device update installs. The application is released "
        "separately and is not expected to match them.");
  }

  return Translate(
      "The firmware and the gateware came from different builds. They are "
      "installed together from one update, so this usually means an update "
      "that did not finish, or one half programmed by hand. Capture will work "
      "normally; installing the latest update is what puts them back in step.");
}

}  // namespace

QString FirmwareText(const FirmwareVersions& versions) {
  const Commits commits = CollectCommits(versions);

  const QString unattached = Translate("No device attached");

  const bool reports_versions =
      versions.device_attached &&
      versions.personality == capture::DevicePersonality::kApplication;

  QString table = QStringLiteral("<table cellpadding=\"3\">");
  table += Row(Translate("Application"), versions.application, {});
  table += Row(Translate("FX3 firmware"),
               !versions.device_attached ? unattached
               : reports_versions        ? FirmwareValue(commits)
                                         : Translate("None installed"),
               FirmwareNote(versions, commits));
  table += Row(Translate("FPGA gateware"),
               !versions.device_attached ? unattached
               : reports_versions        ? GatewareValue(versions)
                                         : Translate("Cannot be read"),
               GatewareNote(versions));
  table += QStringLiteral("</table>");

  return QStringLiteral("<h3>%1</h3><p>%2</p>%3<p>%4</p>")
      .arg(Translate("Firmware versions"),
           Translate("All three are the commit each was built from. The FX3 "
                     "firmware and the FPGA gateware are installed together "
                     "and come from one commit, so they match on a consistent "
                     "device. The application is released separately and is "
                     "not expected to match them."),
           table, Verdict(versions, commits));
}

}  // namespace ddd::gui
