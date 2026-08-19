/************************************************************************

    update_text.h

    What the update page says, in one place
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <vector>

#include "device_updater.h"
#include "update_gate.h"
#include "update_manifest.h"
#include "update_orchestrator.h"
#include "usb_device_info.h"

namespace ddd::gui {

// The update flow is written for the archivist, not the developer: at every
// moment the screen has to answer *what is happening, how long it will take,
// and what I should (not) do*. That is a requirement on this phase, not a
// polish item for later, and the wording is separated from the widgets here
// for the same reason firmware_text.h is — so that what it says can be
// checked without a window, a device, or an update in progress.
//
// The words here and the words on the "Updating your Domesday Duplicator"
// documentation page are meant to be the same words. A user reading
// "recovery mode" in a dialog and then finding a page that calls it
// something else has been given two problems.

// One row of the installed-versus-available comparison.
struct UpdateVersionRow {
  QString name;
  QString installed;
  QString available;

  // Whether this row is a difference the update would change. Drives the
  // emphasis, so a user can see at a glance what the update is *for*.
  bool changes = false;
};

// The comparison table's rows: the application, the firmware, and the
// gateware.
//
// All three are the commit each was built from. The application is in the list
// even though this dialog cannot update it, because leaving it out would answer
// two thirds of "am I up to date" — but it is there to be read rather than
// compared: it is released separately from the device's two halves and nothing
// in a firmware bundle orders it.
std::vector<UpdateVersionRow> UpdateVersionRows(
    const QString& application_commit, const capture::DeviceIdentity& device,
    bool device_attached, const capture::UpdateManifest* bundle,
    capture::DevicePersonality personality =
        capture::DevicePersonality::kApplication);

// The rows as rich text, laid out as a table.
QString UpdateVersionTable(const std::vector<UpdateVersionRow>& rows);

// A stage's title, in the plain language the documentation uses.
QString UpdateStageTitle(capture::UpdateStage stage);

// The same, naming which half of the device the stage is about.
//
// Three of the stages carry a component and the rest do not, and a bundle
// with both in it visits those three twice. Without the name the screen
// would show the same three titles twice over with the bar restarting in
// the middle, which reads as an update that went wrong rather than one that
// is half done.
QString UpdateStageTitle(capture::UpdateStage stage,
                         capture::UpdateTarget target);

// "about 4 minutes", "about 30 seconds". Deliberately coarse: a figure to the
// second would be precise about something that is a guess, and the number a
// user needs is the one that decides whether to go and make tea.
QString FormatUpdateEstimate(int seconds);

// The one instruction that matters, stated before the first byte moves and
// shown throughout.
QString UpdateHoldStillInstruction();

// What the chosen bundle is: version, commit, and its release-notes line.
QString UpdateBundleSummary(const capture::UpdateManifest& manifest);

// The banner a development-signed bundle carries, every time.
//
// A development signature proves the file is well formed and proves nothing
// whatever about where it came from, because the key's secret half is
// committed to the repository. That has to be said in the interface and not
// only in the documentation.
QString DevelopmentBundleBanner();

// Why the install button is disabled, when it is. Empty when the gate allows
// the install and had nothing to remark on.
QString UpdateGateText(const capture::UpdateGateResult& gate);

// The confirmation, after the device has come back and been read.
QString UpdateCompleteText(const capture::DeviceIdentity& identity);

// A failure, in the calm terms the *If an update fails* documentation page
// uses: what happened, whether the device is safe, and the one next step.
QString UpdateFailureText(const QString& problem);

// What a device that is not running the Duplicator's firmware is, and what to
// do about it. Empty for a device that is running it.
//
// The two cases it covers read very differently to the person in front of
// them and are identical on the wire, so this says both rather than guessing
// at one: a device in the boot ROM has either never been programmed or had an
// update interrupted, and the same one action fixes it either way. Somebody
// who has just soldered a kit is not told they have broken it, and somebody
// whose update was interrupted is not told their device is new.
//
// The words here are the words the *If an update fails* documentation page
// uses. A user reading "recovery mode" in this dialog and finding a page that
// calls it something else has been given two problems.
QString DevicePersonalityText(capture::DevicePersonality personality);

// What a device running its factory gateware is, and what to do about it.
// Empty for every other device, including one whose gateware predates the
// two-image model and therefore has no recovery state to be in.
//
// This is the FPGA's counterpart to DevicePersonalityText: the unit is
// working, enumerated and answering, and it cannot capture. Said in the
// interface because nothing else about the device would tell a user —
// the version rows would show a gateware commit, and it would be the wrong
// one to be reassured by.
QString GatewareRecoveryText(const capture::DeviceIdentity& device);

// The label on the button that starts the install: "Update" for an ordinary
// device, "Program this device" for one with no firmware, and "Reinstall
// gateware" for one running its factory image.
//
// Three words for three states, and the third is the one worth being
// careful about: a unit in gateware recovery is not broken and is not new.
// It is a unit whose last gateware update did not finish, and the honest
// verb for finishing it is "reinstall".
QString InstallActionLabel(capture::DevicePersonality personality,
                           bool gateware_recovery = false);

// How the device list names a device that is not running the Duplicator's
// firmware, appended to its path. Empty for one that is.
QString DeviceListPersonalitySuffix(capture::DevicePersonality personality);

}  // namespace ddd::gui
