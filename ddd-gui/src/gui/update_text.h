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
// The application is in the list even though this dialog cannot update it,
// because leaving it out would answer two thirds of "am I up to date". Where
// it is out of date the update page says so and routes to the platform's own
// channel rather than pretending it can self-update.
std::vector<UpdateVersionRow> UpdateVersionRows(
    const QString& application_version, const capture::DeviceIdentity& device,
    bool device_attached, const capture::UpdateManifest* bundle);

// The rows as rich text, laid out as a table.
QString UpdateVersionTable(const std::vector<UpdateVersionRow>& rows);

// A stage's title, in the plain language the documentation uses.
QString UpdateStageTitle(capture::UpdateStage stage);

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

}  // namespace ddd::gui
