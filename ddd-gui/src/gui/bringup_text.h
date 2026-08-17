/************************************************************************

    bringup_text.h

    What the bring-up wizard says, in one place
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <vector>

#include "device_updater.h"
#include "update_manifest.h"
#include "usb_device_info.h"
#include "usb_presence.h"

namespace ddd::gui {

// Bringing a board up is the one flow in this application that asks somebody
// to put their hands inside their equipment: take the unit out of its case,
// move a jumper, unplug two cables twice. Every one of those is an instruction
// that is either followed exactly or produces a state nobody planned for, so
// the wording is separated from the widgets here for the same reason
// update_text.h is — so that what it says can be checked without a window, a
// device or a screwdriver.
//
// Two things this file says over and over, and both are deliberate:
//
//   - **both cables**, in those words, every time a power cycle is asked for.
//     The assembled unit is powered through either one alone, so pulling the
//     USB 3.0 cable and leaving the mini-USB in place changes nothing while
//     the board stays lit and looks fine. It is the likeliest way for somebody
//     to get stuck, and it is invisible.
//   - **fitted** and **removed** for the jumper, never open and closed. The
//     documentation, the firmware README and this wizard all use one pair of
//     words, so nobody has to translate between two vocabularies for one
//     jumper.

// The nine pages, in the order they are worked through. Here rather than
// inside the wizard so that the wording functions can be asked about a page
// without a window existing.
enum class BringUpPage {
  // What this does, and everything physical it will ask for — listed before
  // anything starts, because this is the page that decides whether the job is
  // done once or three times.
  kOverview,

  // Live status for both boards, each opened rather than merely enumerated.
  kConnect,

  // The provisioning set: signature, digests, channel.
  kImage,

  // Fit the jumper and power-cycle, to reach the FX3's boot ROM. Skipped when
  // the FX3 is already there.
  kJumper,

  // Write the FX3's EEPROM.
  kFirmware,

  // Take the jumper off again. Skipped when kJumper was.
  kRemoveJumper,

  // Play the provisioning vectors into the FPGA's configuration flash.
  kGateware,

  // The one power cycle that discharges both obligations.
  kPowerCycle,

  // What the wizard can prove, and what to do next.
  kVerify,
};

// "3 of 9", and the page's own title. Numbered including the pages a
// particular run skips: a wizard whose step numbers changed depending on what
// was plugged in would make two runs of the same procedure impossible to talk
// about.
QString BringUpPageTitle(BringUpPage page);
QString BringUpPageHeading(BringUpPage page);

// The overview: what will happen, what it will ask for, and — stated plainly,
// because it is the thing that surprises people — what state the unit is in at
// the end.
QString BringUpOverviewText();

// How long the whole thing takes, honestly and coarsely.
QString BringUpDurationText();

// --- the connectivity page ------------------------------------------------

// What one status row is saying.
enum class BringUpRowState {
  // Found, opened, and ready to be worked on.
  kReady,

  // Found, and something has to happen to it before the wizard can proceed —
  // a jumper, or a power cycle. Not a fault.
  kWaiting,

  // Not found, or found and not usable.
  kProblem,
};

struct BringUpStatusRow {
  QString title;
  QString detail;
  BringUpRowState state = BringUpRowState::kProblem;

  bool usable() const { return state != BringUpRowState::kProblem; }
};

// The FX3 row.
//
// `personality` is what the application enumerated, and `attached` says
// whether it enumerated anything at all. `debug_bridge` is whether the kit's
// on-board USB-UART answered, which is the one thing that distinguishes an
// unpowered kit from a powered one whose USB 3.0 link is not working — a
// distinction worth a great deal, because the two send somebody to different
// ends of the bench.
BringUpStatusRow BringUpFx3Row(bool attached,
                               capture::DevicePersonality personality,
                               capture::UsbPresence debug_bridge);

// The FPGA row.
//
// `problem` is the cable driver's own sentence when opening failed, which is
// the one that says whether nothing is attached, something is attached that
// this code does not drive, or the cable is there and will not open. That last
// case is nearly always the udev rules on Linux and the driver binding on
// Windows, and it is named as such rather than reported as absence.
//
// A **charge-only cable** is named first among the not-found causes, and for a
// reason particular to this hardware: the assembled unit is lit from the USB
// 3.0 side whatever the mini-USB is doing, so "the lights are on" says nothing
// at all about whether the Blaster is connected.
BringUpStatusRow BringUpFpgaRow(bool opened, capture::UsbPresence presence,
                                const QString& problem);

// --- the physical pages ---------------------------------------------------

// Fit the jumper, then pull both cables. Two instructions on one page because
// the first does nothing until the second happens.
QString BringUpFitJumperText();

// Take it off again. No power cycle here: there is one at the end and it
// serves both halves.
QString BringUpRemoveJumperText();

// Unplug both cables, wait, plug them back in. Says why, not just what.
QString BringUpPowerCycleText();

// Shown when the device has not come back. Leads with the partial power cycle,
// because it is by far the likeliest cause and the only one that leaves
// everything looking correct.
QString BringUpPowerCycleTimeoutText();

// The photographs, by resource path, and the words that go under them. Alt
// text and caption say fitted and removed, like everything else.
QString BringUpPhotographPath(BringUpPage page);
QString BringUpPhotographCaption(BringUpPage page);

// --- the working pages ----------------------------------------------------

// What the FX3 step is about to do, and the one instruction that matters.
QString BringUpFirmwareText();

// The same for the FPGA step, including how long it is expected to take.
QString BringUpGatewareText(int seconds);

// The chosen provisioning set: version, commit, and what it carries. Empty
// when nothing has been chosen.
QString BringUpImageSummary(const capture::UpdateManifest& manifest);

// Why the chosen file cannot be used, or empty when it can. A provisioning set
// needs both halves — the firmware and the vectors — because a bring-up does
// both and the order it does them in is what keeps the hardware safe.
QString BringUpImageProblem(const capture::UpdateManifest& manifest);

// --- the end --------------------------------------------------------------

// One check the wizard performed, and whether it passed.
struct BringUpCheck {
  QString description;
  bool passed = false;
};

// What the wizard can actually prove about the device in front of it, read off
// the device rather than assumed from what was written.
//
// `expected_commit` is the firmware commit the manifest said the device would
// report. An empty one is not a failed check — it is a set that did not say,
// and a check that cannot be made is not reported as one that passed.
std::vector<BringUpCheck> BringUpVerification(
    bool attached, capture::DevicePersonality personality,
    const capture::DeviceIdentity& identity, const QString& expected_commit);

// The hand-over: what has been done, what state the unit is in, and the one
// remaining step. Says that the case can go back on, because nothing after
// this point is physical.
QString BringUpCompleteText();

// The same when one of the checks did not pass. Calm, like the update page's:
// what is known, and that nothing is broken.
QString BringUpIncompleteText();

// A failure, at any point. Every step of this flow is safe to fail at and safe
// to run again, and that is said rather than implied — the FX3 cannot be
// bricked because the jumper always reaches its boot ROM, and the DE0-Nano
// cannot be bricked because JTAG always reaches its flash.
QString BringUpFailureText(const QString& problem);

// The same for a run somebody stopped, which is not a failure at all.
QString BringUpStoppedText();

}  // namespace ddd::gui
