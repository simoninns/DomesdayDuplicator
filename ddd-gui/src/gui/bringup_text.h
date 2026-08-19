/************************************************************************

    bringup_text.h

    What the bring-up wizard says, in one place
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <optional>
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
// **Instructions first, explanation second, and only as much explanation as
// changes what somebody does.** A page of this wizard is read by somebody with
// a board in one hand, and what they need out of it is the action: fit this,
// unplug both, press that. So each page opens with what to do, in the
// imperative, and keeps the reasoning to the sentences that stop a plausible
// mistake — that either cable alone keeps the unit powered, that the configure
// step writes nothing. Everything else about how this works belongs in the
// documentation, where somebody can read it without a screwdriver in the way.
//
// Three things this file says over and over, and all three are deliberate:
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
//   - **it does not matter what the board is running now.** The jumper reaches
//     the FX3's boot ROM from any state whatever, so this flow never diagnoses
//     a board and never branches on what it finds. Somebody arriving with a
//     unit they know nothing about should be told that in the first paragraph,
//     because it is the fact that lets them start. It cuts the other way too:
//     a board *already* reporting its boot ROM is still asked for the jumper,
//     because an empty EEPROM puts it there just as readily and the wizard
//     cannot tell the two apart.

// The nine pages, in the order they are worked through. Here rather than
// inside the wizard so that the wording functions can be asked about a page
// without a window existing.
//
// **The order carries a hardware-safety property**, and the enumeration is
// where it is written down. The FX3 reaches its boot ROM before the FPGA is
// touched, and only ever runs the new firmware afterwards — so the gateware
// can change underneath an FX3 whose pins are all undriven, and the pairing
// where the original firmware drives CTL_07 into gateware driving the same net
// is unreachable rather than merely avoided. See bringup_orchestrator.h, which
// refuses the writes until the configure has happened.
enum class BringUpPage {
  // What this does, and everything physical it will ask for — listed before
  // anything starts, because this is the page that decides whether the job is
  // done once or three times.
  kOverview,

  // Live status for both boards, each opened rather than merely enumerated.
  kConnect,

  // The update file: signature, digests, channel, and whether it carries
  // everything a bring-up needs.
  kImage,

  // Fit the jumper and power-cycle, to reach the FX3's boot ROM. Asked for on
  // every run, including one whose FX3 is reporting the boot ROM already: a
  // kit with an empty EEPROM comes up there with or without the jumper, and
  // one that got there without it leaves again at the first restart.
  kJumper,

  // Play the vectors into the FPGA over the USB-Blaster. Volatile: it writes
  // nothing, and it is what gives the firmware a flash bridge to write
  // through.
  kConfigure,

  // Everything permanent: the EEPROM, the factory image and the application
  // image, in that order.
  kProgram,

  // Take the jumper off again.
  kRemoveJumper,

  // The one power cycle, which makes every image written above the running
  // one.
  kPowerCycle,

  // What the wizard can prove, and the fact that there is nothing left to do.
  kVerify,
};

// "3 of 9", and the page's own title. Every run visits all nine, so the
// numbering is the same in every telling of the procedure — which is what lets
// two runs of it be talked about in the same words.
QString BringUpPageTitle(BringUpPage page);
QString BringUpPageHeading(BringUpPage page);

// --- what every page says about its own state ------------------------------
//
// Every page that has anything to wait for ends in one of these two lines, and
// that they are the same two lines everywhere is the point of them being here.
//
// A user who has just fitted a jumper, or pressed a button and watched a bar,
// needs one question answered: has this step finished or not? So a finished
// step says **All done** first, in green, in its own line under the control
// that did the work — not as a clause somewhere inside the paragraph that
// explains the step, which is the one part of the page nobody re-reads.
//
// And a page that is not finished says what it is waiting for, in those words.
// "Waiting for you to press Load the gateware" and "waiting for the board to
// come back in its boot ROM" are different situations that look identical from
// the outside: in both, nothing is happening.
QString BringUpWaitingText(const QString& what);
QString BringUpStepDoneText(const QString& what);

// The overview, as three lists rather than three paragraphs: what this does,
// what to have ready, and what it will ask for. The point of the page is that
// somebody reads it once and then does the physical job once — so it is a
// checklist, and the only prose on it is the sentence that says the state the
// board is in now does not matter, which is what lets somebody with an unknown
// board start at all.
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

// The mark a row state is shown with, and the colour it is shown in. Here
// rather than in the widget so that the rows and the legend that explains them
// cannot disagree about what amber means — and as QString, because these are
// multi-byte characters and a byte-at-a-time reading of one is three
// characters of mojibake.
QString BringUpMark(BringUpRowState state);
QString BringUpMarkColour(BringUpRowState state);

// What the three marks mean, shown above the rows.
//
// Worth a line of its own because the amber one is the state somebody is most
// likely to misread: it says the wizard will ask something of that board later
// on, not that anything is wrong with it.
QString BringUpConnectLegend();

// The FX3 row.
//
// Takes the device itself rather than its personality alone, because the
// personality does not say enough: a board enumerating under the current
// identifiers may be speaking a protocol this build knows or one it does not,
// and those two want different sentences. The commit the device reports is
// named when it reports one, so that "running the Duplicator's firmware"
// cannot be read as "running some Duplicator firmware or other".
//
// **This row informs and decides nothing.** Every personality below is a
// personality the wizard can bring up, because the jumper reaches the boot ROM
// from all of them; what the row is for is telling somebody what they have in
// front of them, and — for a board that already works — that they may be in
// the wrong window.
//
// `debug_bridge` is whether the kit's on-board USB-UART answered, which is the
// one thing that distinguishes an unpowered kit from a powered one whose USB
// 3.0 link is not working — a distinction worth a great deal, because the two
// send somebody to different ends of the bench.
BringUpStatusRow BringUpFx3Row(const std::optional<capture::DeviceInfo>& fx3,
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

// Fit the jumper, then pull both cables, then put them back — numbered, in
// that order, because the first does nothing until the other two happen and a
// jumper fitted without a reboot is the commonest way to be stuck on this
// page while everything looks correct.
QString BringUpFitJumperText();

// Take it off again, and touch nothing else. No power cycle here: there is one
// on the next page and it serves everything, and somebody who pulls the cables
// twice has done the physical work twice.
QString BringUpRemoveJumperText();

// Unplug both cables, wait, plug them back in. Numbered like the jumper page,
// and for the same reason.
QString BringUpPowerCycleText();

// Shown when nothing has changed on the bus at all. Leads with the partial
// power cycle, because it is by far the likeliest cause and the only one that
// leaves everything looking correct.
QString BringUpPowerCycleTimeoutText();

// The device went away and has not come back. A different situation from the
// one above and it gets different advice: the cables are known to have come
// out, so what is left is one of them not being back in.
QString BringUpDeviceNotBackText();

// It came back in its boot ROM, which says exactly one thing: jumper J4 is
// still fitted. Worth a sentence of its own rather than a timeout, because it
// is a diagnosis rather than a guess — nothing else puts a board there.
QString BringUpStillInBootRomText();

// It came back running its firmware, and the FPGA is still holding the image
// JTAG put into it — so the board never lost power.
//
// **This is the partial power cycle, caught rather than guessed at.** Pulling
// the USB 3.0 cable alone makes the device vanish from the bus and return,
// while the mini-USB keeps the board alive throughout: the firmware in RAM
// survives, the gateware in the FPGA survives, and every outward sign is of a
// power cycle that worked. The image role is what gives it away.
QString BringUpNotReloadedText();

// The photographs, by resource path, and the words that go under them. Alt
// text and caption say fitted and removed, like everything else.
QString BringUpPhotographPath(BringUpPage page);
QString BringUpPhotographCaption(BringUpPage page);

// --- the working pages ----------------------------------------------------

// The JTAG step. Opens by naming the button to press and how long it takes,
// because that is the whole of what this page asks for.
//
// That it writes nothing is the one piece of explanation kept. It is the only
// step in this application that talks to a board through a second cable, and
// somebody watching a progress bar labelled with an FPGA has every reason to
// assume it is being programmed — so the page says, in as many words, that
// nothing is being written and that the board is unchanged if this is stopped.
QString BringUpConfigureText(int seconds);

// The step that writes. Names the button, then the three images in the order
// they go in, then how long it takes and what not to touch while it does.
//
// `windows_binding` adds the one warning this page cannot leave to the failure
// that would otherwise deliver it. Part way through this step the board
// restarts and comes back under an identifier it has never presented before —
// 1209:2347, the Duplicator it has just become — and Windows binds drivers per
// identifier, so the bindings made for the FX3 and the USB-Blaster before the
// wizard started do not cover it. On a machine that has never had a working
// Duplicator plugged into it there is nothing that can be done about that in
// advance: nothing presents 1209:2347 until this step has run, and Zadig can
// only bind a device it can see. So the page says what is about to happen
// instead, which turns thirty seconds of apparently silent failure into a step
// somebody is waiting for with Zadig already open.
//
// A parameter rather than a compiled-in platform test, so that both halves of
// the sentence are readable on any machine and the page stays pure text.
QString BringUpProgramText(int seconds, bool windows_binding);

// Where the file on the image page came from, which is three different
// sentences and never a silent difference.
//
// A packaged build carries an update bundle, so that a board can be brought up
// on a machine with no network — which is the ordinary case, since a board
// being brought up is by definition one that cannot be updated over USB. What
// the page must not do is let "it came with the application" read as "so it
// was not checked": it is verified exactly as a downloaded one is, signature
// first and then every digest, and the wording says so.
QString BringUpBundledFileText();

// The same page when a file has been chosen over a bundled one, saying that
// the bundled one is still there.
QString BringUpChosenFileText();

// And when the bundled one did not verify. A rare state and a real one — a
// truncated install, a file somebody replaced — and the remedy is not one the
// user can apply to the bundled copy, so it says to download a file instead of
// suggesting anything can be repaired here.
QString BringUpBundledFileUnusableText();

// And when this build carries none at all, which is what a build from source
// looks like unless it was told otherwise. Names the file to download.
QString BringUpNoBundledFileText();

// The chosen file: version, commit, and what it carries. Empty when nothing
// has been chosen.
QString BringUpImageSummary(const capture::UpdateManifest& manifest);

// Why the chosen file cannot be used, or empty when it can.
//
// Bring-up needs **all four** payloads, and that is stricter than the update
// window is on purpose: an update installs what it can and leaves the rest, and
// a bring-up that installed what it could would leave a board half programmed
// in a state nobody planned for. So the completeness is checked here, on the
// page where a different file can still be chosen, rather than discovered
// three writes in.
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
// report. An empty one is not a failed check — it is a file that did not say,
// and a check that cannot be made is not reported as one that passed.
std::vector<BringUpCheck> BringUpVerification(
    bool attached, capture::DevicePersonality personality,
    const capture::DeviceIdentity& identity, const QString& expected_commit);

// The end: what has been done, and that there is nothing left to do. Says that
// the case can go back on, because nothing after this point is physical.
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
