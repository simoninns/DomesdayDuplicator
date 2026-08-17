/************************************************************************

    player_text.h

    What the interface says about the player
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <cstdint>
#include <optional>

#include "auto_capture_plan.h"
#include "auto_capture_sequence.h"
#include "disc_examiner.h"
#include "disc_profile.h"
#include "player_command.h"
#include "player_connection.h"
#include "player_request.h"
#include "player_state.h"
#include "player_status.h"

namespace ddd::gui {

// Every user-visible string about the player, as pure functions of a value.
//
// Separated from the panel that shows them for the same reason firmware_text.h
// is: most of what these have to say is about things that are absent — no
// player, the wrong player, a player that will not say what it is — and a
// wording that can only be seen by arranging the hardware to misbehave is a
// wording nobody checks.
//
// It also keeps the panel, the status bar and the log saying the same thing,
// which they did not in the old application.

// Which platform's advice a message should carry.
//
// Passed in rather than read from the build inside the wording, so that all
// three sentences are testable on one machine. A permission message is exactly
// the kind of text that is written once, never seen by its author again, and
// wrong on the two platforms they do not use.
enum class HostPlatform : uint8_t { kLinux, kMacOs, kWindows };

// The platform this build is for.
HostPlatform ThisPlatform();

// What to do about a serial port this user is not allowed to open.
//
// One sentence per platform, naming the actual remedy rather than "check your
// permissions": on Linux a group, on macOS a driver that was never installed or
// was blocked, on Windows a port another program is holding. It is the most
// likely thing to go wrong on a machine that has never run this application,
// and the support question this documentation and this wording exist to answer
// once rather than repeatedly.
QString SerialPermissionAdvice(HostPlatform platform);

// The headline: one short line naming the state.
QString PlayerConnectionSummary(const PlayerConnection& connection);

// The sentence under it. Empty when there is nothing worth adding — a working
// connection to the expected model does not need explaining.
QString PlayerConnectionDetail(const PlayerConnection& connection);

// How the player was reached: port, rate, model and firmware. Empty when there
// is no connection.
QString PlayerConnectionSource(const PlayerConnection& connection);

// The player's own name for what it is doing.
QString PlayerStateName(player::PlayerState state);

QString TrayStateName(player::TrayState tray);

QString DiscTypeName(player::DiscType type);

// A time code as a clock: 1234500 is 1:23:45.
//
// The player reports it as seven digits — hours, minutes, seconds, frames —
// and shows it to a user the way the disc sleeve does.
QString FormatTimeCode(int32_t time_code);

// The inverse: a time code a user typed, as the player's seven digits.
//
// Accepts the clock form the sleeve uses — "1:23:45", or "23:45" for a disc
// under an hour — and also the bare seven digits, for somebody working from the
// player's own display. Returns nothing for anything else, so the remote can
// refuse to send rather than seek to a number it invented: "1:99" is not a time
// and guessing what it meant would be worse than saying so.
std::optional<int32_t> ParseTimeCodeEntry(const QString& text);

// A control's name, for a log line and for the sentence explaining why a
// control is unavailable.
QString PlayerCommandName(player::PlayerCommand command);

QString AudioModeName(player::AudioMode mode);

QString PlaybackSpeedName(player::PlaybackSpeed speed);

// Why a control is greyed out, for its tooltip.
//
// Names the models that do offer it, where any registered model does. That is a
// sweep of the registry rather than a table here, so a player family added
// later is named by this without the wording being touched — and where nothing
// in the build offers it, it says that instead of leaving the user hunting for
// a model that does not exist.
QString UnsupportedControlNote(const PlayerConnection& connection,
                               player::PlayerCommand command);

// One line describing an exchange: what was sent and what came back.
//
// The same wording in the log and in the remote's manual command field, which
// is the point of it being here. The reply is shown verbatim — a manual command
// exists precisely to find out what a player really answers, and a wording that
// tidied that up would defeat it.
QString PlayerReplyText(const PlayerReply& reply);

// Bytes as a hex dump with an ASCII gutter, sixteen to a line.
//
// Offsets are decimal rather than the conventional hex, because what is being
// counted here is position within a fixed-length record — the Pioneer user
// code's Key Data starts at character 120, and that is the question somebody
// reading this actually has. `first_offset` is what the first byte is numbered
// as, so a region dumped on its own still carries its place in the whole.
QString FormatByteDump(const QByteArray& bytes, qsizetype first_offset = 0);

// Everything worth saying about a reply: the exchange on one line, then — where
// the reply is data rather than a word — how long it is, how much of it the
// player could not read, and the dump.
//
// One rule for the user-code box and the manual command field alike, because
// both exist for the same reason: finding out what the player really said. A
// short and wholly printable reply like "P04" is already legible and gets no
// dump; anything longer than a dump line, or carrying a byte that is not
// printable, gets one.
//
// A reply to the Pioneer user-code query gets the regional treatment below
// instead, since for that one the structure is known.
QString PlayerReplyReport(const PlayerReply& reply);

// A Pioneer user code, split into the three regions the format defines.
//
// The difference this makes is not cosmetic. Dumped as 200 undifferentiated
// characters, the Casper disc on the project's bench reads as "sixty of these
// failed" and invites a guess about which sixty; split at the documented
// boundaries it reads as "the Disc Control Data is intact and the whole of the
// Key Data — the customer's own disc-identifying information — could not be
// read", which is a fact about that disc worth recording.
QString PioneerUserCodeReport(const PlayerReply& reply);

// Where the player is, in whichever way this disc is addressed. "Lead-in" and
// "Lead-out" are positions in their own right and are said rather than shown as
// a number, because the number means nothing there.
QString PlayerAddressText(const player::PlayerStatus& status);

// The optical assembly's position, on the one model that reports it. Empty
// everywhere else, so a panel can simply hide the row.
QString PhysicalPositionText(const player::PlayerStatus& status);

// The single line for the status bar, which cannot be hidden and so has to
// carry the state whatever else is on screen.
QString PlayerStatusBarText(const PlayerConnection& connection,
                            const player::PlayerStatus& status);

// The note shown for a model whose definition has never met the hardware it
// describes. Empty for a definition that has. See players/README.md — this is
// the interface's half of that promise.
QString PlayerVerificationNote(const PlayerConnection& connection);

// --- Examining the disc ----------------------------------------------------

// What the examination is doing now, in plain language.
//
// A sentence rather than a command name, because the thing it explains is a
// player that has gone quiet and started seeking: "finding the end of the side"
// is why the disc is spinning backwards, and "?F" is not.
QString ExamineStageName(player::ExamineStage stage);

// How the examination ended, as the clause that follows "Examination ".
QString ExamineOutcomeText(player::ExamineOutcome outcome);

// One line of the serial trace, naming the step as well as the bytes. The
// stage is what makes a trace of two identical address queries readable.
QString ExamineStepText(player::ExamineStage stage, const QString& sent,
                        const QString& reply);

// How a fact was arrived at, for the report's second column.
//
// Shown for every field and not just the surprising ones. A report where only
// the guesses are labelled is a report where an unlabelled line means "trust
// me", and the whole reason for carrying provenance is that some of these
// really are measurements and some really are not.
QString ProvenanceNote(player::Provenance provenance);

QString VideoStandardName(player::VideoStandard standard);

// A disc diameter as a disc is sold: 12-inch and 8-inch, not 30 cm and 20 cm.
QString DiscSizeName(player::DiscSize size);

// An address in whichever way this disc is addressed: a frame number as a
// number, a time code as a clock.
QString FormatDiscAddress(int32_t address, player::AddressMode mode);

// The headline: one line saying whether there is a disc and what it is.
QString ExamineSummary(const player::DiscProfile& disc,
                       player::ExamineOutcome outcome);

// The whole report, as text.
//
// Text rather than a model behind a table, and deliberately: this is what a
// user pastes into an issue when a disc behaves strangely, so the thing on
// screen and the thing in the clipboard have to be the same thing. It is also
// why the raw disc-status reply is in it — undecoded, labelled as undecoded,
// and therefore accumulating in exactly the reports a decode would have to be
// written from.
//
// `bytes_per_second` is the current capture settings' estimate, so the size a
// capture of this disc would take is figured at the settings the user is
// actually going to capture with. Nothing is estimated where the length is not
// known, and nothing is estimated for a CAV disc whose video standard nobody
// has declared — see ProgrammeDuration.
QString DiscProfileReport(const player::DiscProfile& disc,
                          player::ExamineOutcome outcome,
                          double bytes_per_second);

// --- Setting up and running an automatic capture ---------------------------

// What a capture of this shape covers, as the label on its option.
QString CaptureShapeName(player::CaptureShape shape);

// Why a plan cannot be run, as the sentence shown beside a start button that is
// not available.
//
// A different sentence per problem, which is the whole reason PlanProblem is an
// enumeration. The old application had one message for all of them — "The disc
// in the player does not match the selected capture option" — and it arrived
// several seconds after the disc had started spinning.
QString PlanProblemText(player::PlanProblem problem);

// What the automatic capture is doing now, in plain language.
QString AutoCaptureStageName(player::AutoCaptureStage stage);

// How the run ended, as the clause following "Automatic capture ".
QString AutoCaptureOutcomeText(player::AutoCaptureOutcome outcome);

// The sentence a finished run leaves on screen: what happened, and what it
// means for the file.
QString AutoCaptureSummary(player::AutoCaptureOutcome outcome);

// What a capture of this plan is expected to cost, in time and in bytes.
//
// Empty where the length cannot be worked out — a CAV plan on a disc whose
// video standard nobody established, which is the one case a frame count is not
// a duration. `bytes_per_second` is the current capture settings' estimate, so
// the figure is for the settings the capture will actually run with.
QString AutoCaptureEstimate(const player::AutoCapturePlan& plan,
                            const player::DiscProfile& disc,
                            double bytes_per_second);

// How much longer the run has to go, from where the player is now.
//
// **The disc plays in real time, so the programme left to play *is* the time
// left to wait.** There is nothing to measure and no rate to estimate, which is
// why this is a pure function of two addresses rather than something that
// watches a clock — and why it is right from the first reading rather than
// settling down over the first minute the way an observed-rate estimate would.
//
// Empty where it cannot be worked out: a CAV disc whose video standard nobody
// established, a player that has not reached the programme yet, or an address
// already at the end. It is deliberately a little short on the one shape that
// spins the disc down inside the capture, for the same reason the setup's own
// estimate is — see PlannedDuration.
QString AutoCaptureRemainingText(const player::AutoCapturePlan& plan,
                                 const player::DiscProfile& disc,
                                 int32_t address);

// A capture name built from what is known about the disc, or empty where
// nothing is.
//
// Deliberately not a scheme: it is a prefill somebody can type over, and full
// advanced naming is a separate piece of work. What it does do is put the side
// number in the name, because the two files a user makes in a row are the two
// sides of one disc and telling them apart afterwards is the whole problem.
QString SuggestedCaptureName(const player::DiscProfile& disc);

}  // namespace ddd::gui
