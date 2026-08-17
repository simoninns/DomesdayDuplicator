/************************************************************************

    test_player_hardware.cpp

    T5 tests against an attached LaserDisc player
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QString>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "disc_examiner.h"
#include "disc_profile.h"
#include "player_controls.h"
#include "player_session.h"
#include "player_text.h"
#include "qt_serial_port.h"
#include "serial_port_scanner.h"

namespace ddd::gui {
namespace {

// The `hil-player` tier: these need a **player**, which is not the same
// hardware as `hil`'s Domesday Duplicator and is not on the same bench in every
// case. A machine with one and not the other must be able to run the tier it
// can, which is the whole reason this label exists separately:
//
//   ctest --test-dir build -L hil-player
//
// Like `hil`, nothing here skips. A player test that passed because no player
// answered would be worse than no test at all, so each of these fails instead
// and says what was missing.
//
// **These move the disc.** The examination spins it up, seeks past both ends of
// the side and settles it — a minute or so of mechanism, exactly as the
// application's own Examine does. Nothing here writes to anything on the player
// that survives a power cycle; there is nothing on a player that could be.

// Where to look, if the bench is not to be scanned for.
//
// A scan opens every serial port on the machine and writes a few bytes to it,
// which is the thing the risks section of the plan is most careful about. It is
// what the application does when it has not been told, so it is what this
// exercises by default — but a bench with other equipment on it should say.
const char* EnvOrNull(const char* name) {
  const char* value = std::getenv(name);
  return (value != nullptr && value[0] != '\0') ? value : nullptr;
}

// A number from the environment, or nothing — including for a value that is not
// a number at all, which is appended to `problem` rather than passed over.
//
// The distinction is worth the few lines: a mistyped DDD_PLAYER_BAUD read as
// zero would silently become "search every rate", and the operator would be
// told nothing about the setting they thought they had made.
//
// Reported through a string rather than through ADD_FAILURE because one of the
// callers runs in SetUpTestSuite, where a failure does not attach to a test —
// gtest marks the suite bad and *skips* the tests, which is precisely the
// "passed because there was no hardware" outcome this tier must never produce.
std::optional<int64_t> EnvNumber(const char* name, std::string& problem) {
  const char* value = EnvOrNull(name);
  if (value == nullptr) {
    return std::nullopt;
  }

  char* end = nullptr;
  const int64_t number = std::strtoll(value, &end, 10);
  if (end == value || *end != '\0') {
    problem += std::string(name) + " is not a number: \"" + value + "\". ";
    return std::nullopt;
  }
  return number;
}

// The disc the operator says is in the tray, if they said.
//
// "A real examination against a known disc" is only a test if the answer is
// known independently — otherwise it is a measurement with nothing to compare
// against. Without these the examination still has to complete and still has to
// establish the type and the length; with them it also has to get them right.
//
//   DDD_PLAYER_DISC_TYPE   cav | clv
//   DDD_PLAYER_LAST_FRAME  the last address of the side, as the player's own
//                          display shows it (54321, or 1234500 for a time code)
std::optional<player::DiscType> ExpectedDiscType() {
  const char* value = EnvOrNull("DDD_PLAYER_DISC_TYPE");
  if (value == nullptr) {
    return std::nullopt;
  }

  const std::string text(value);
  if (text == "cav" || text == "CAV") {
    return player::DiscType::kCav;
  }
  if (text == "clv" || text == "CLV") {
    return player::DiscType::kClv;
  }
  return std::nullopt;
}

std::optional<int32_t> ExpectedLastAddress() {
  std::string problem;
  const std::optional<int64_t> value =
      EnvNumber("DDD_PLAYER_LAST_FRAME", problem);
  if (!problem.empty()) {
    ADD_FAILURE() << problem;
  }
  if (!value.has_value()) {
    return std::nullopt;
  }
  return static_cast<int32_t>(*value);
}

// A tolerance on the measured end of the side, in addresses.
//
// The measurement seeks past the end and reads where the player stopped, and
// where a player stops is a mechanical answer rather than an arithmetic one: it
// is the last address it could read, which on a scuffed run-out is not
// necessarily the last address encoded. Ten frames is a third of a second and
// is well inside "the operator read the same number off the front panel".
constexpr int32_t kAddressTolerance = 10;

void Say(const std::string& line) {
  std::cout << "[          ] " << line << "\n";
}

class PlayerHardwareTest : public ::testing::Test {
 protected:
  // Connected once for the whole binary rather than per test. A scan is four
  // baud rates on every port on the machine, and paying for it three times
  // would be minutes of writing to other people's equipment for no gain.
  // Why there is no player, or empty. Filled in below and asserted on by every
  // test, rather than failed here: a failure raised in SetUpTestSuite does not
  // belong to a test, and gtest answers one by skipping the suite — which is a
  // run that reports nothing failed on a bench with no player attached.
  static std::string problem_;

  static void SetUpTestSuite() {
    port_ = std::make_unique<QtSerialPort>();
    session_ = std::make_unique<player::PlayerSession>(port_.get());

    std::vector<std::string> paths;
    if (const char* fixed = EnvOrNull("DDD_PLAYER_PORT"); fixed != nullptr) {
      paths.emplace_back(fixed);
    } else {
      for (const SerialPortCandidate& candidate :
           RankSerialPorts(EnumerateSerialPorts(), QStringList{})) {
        paths.push_back(candidate.path.toStdString());
      }
    }

    std::optional<uint32_t> rate;
    if (const std::optional<int64_t> fixed =
            EnvNumber("DDD_PLAYER_BAUD", problem_)) {
      rate = static_cast<uint32_t>(*fixed);
    }

    for (const std::string& path : paths) {
      Say("probing " + path);
      const player::ProbeResult result = session_->Probe(path, rate);

      if (result.connected()) {
        Say("player on " + path + " at " + std::to_string(result.baud_rate) +
            " baud: " + std::string(result.identity.definition->name) + " (" +
            result.identity.model_code + ")");
        return;
      }

      // Said out loud rather than swallowed, because on a bench that finds no
      // player these lines are the whole diagnosis — a refused port and a
      // silent one send you to quite different places.
      if (result.status == player::ProbeResult::Status::kPortUnavailable &&
          result.open_error == player::PortOpenError::kNotPermitted) {
        Say("  " + path +
            ": not permitted — see the serial permissions note "
            "in TESTING.md §7");
      }
    }

    Say("no player answered on any port");
    problem_ +=
        "no player is attached — this tier needs one. Set DDD_PLAYER_PORT to "
        "name the port, and DDD_PLAYER_BAUD if the player's rate is not to be "
        "searched for.";
  }

  static void TearDownTestSuite() {
    if (session_) {
      session_->Disconnect();
    }
    session_.reset();
    port_.reset();
  }

  void SetUp() override {
    ASSERT_TRUE(problem_.empty()) << problem_;
    ASSERT_TRUE(session_ != nullptr && session_->connected());
  }

  static std::unique_ptr<QtSerialPort> port_;
  static std::unique_ptr<player::PlayerSession> session_;
};

std::string PlayerHardwareTest::problem_;
std::unique_ptr<QtSerialPort> PlayerHardwareTest::port_;
std::unique_ptr<player::PlayerSession> PlayerHardwareTest::session_;

// The first half of the tier: a real player answers the model request, and what
// it answers is what a definition claims.
TEST_F(PlayerHardwareTest, APlayerIsFoundAndSaysWhatItIs) {
  const player::PlayerIdentity& identity = session_->identity();

  ASSERT_TRUE(identity.valid());
  EXPECT_FALSE(identity.model_code.empty());
  EXPECT_FALSE(identity.id_code.empty());

  Say("model code:  " + identity.model_code);
  Say("model:       " + std::string(identity.definition->name));
  Say("firmware:    " + identity.firmware_version);
  Say("bench:       " + std::string(identity.definition->bench_verified
                                        ? "verified"
                                        : "NOT verified — see players/README"));

  // An unrecognised player is a finding rather than a failure: it connects on
  // the generic command set and everything below still runs. It does mean this
  // bench session should end with a definition being written, so it is said
  // rather than passed over.
  if (!identity.recognised) {
    Say("this model has no definition — it is being driven generically, and "
        "the model code above is what a new one should claim");
  }

  // The identity the operator expected, where they said. A player that
  // identifies as something other than the model on its own front panel is the
  // one fault this tier exists to catch, and it is silent otherwise.
  if (const char* expected = EnvOrNull("DDD_PLAYER_MODEL_ID");
      expected != nullptr) {
    EXPECT_EQ(identity.id_code, std::string(expected));
  }
}

// The read-only queries, sent to real hardware. Each one moves nothing, so this
// runs before the examination and leaves the player exactly as it found it.
TEST_F(PlayerHardwareTest, TheCheapQueriesAreAnsweredByTheRealPlayer) {
  const player::PlayerControls controls = player::ControlsFor(
      *session_->identity().definition, session_->identity().firmware_version);

  // The active mode is the one query every player must answer: the examination
  // asks it first and gives up on the disc if it does not.
  const player::Reply mode =
      session_->Execute(player::PlayerCommand::kQueryActiveMode, std::nullopt);
  EXPECT_TRUE(mode.ok()) << "?P was not answered — sent \"" << mode.sent
                         << "\", got \"" << mode.text << "\"";
  Say("active mode: " + mode.text);

  // The other two are per-model, and a model that has them and will not answer
  // them is a definition claiming a capability its hardware has not got.
  if (controls.Has(player::PlayerCommand::kQueryDiscStatus)) {
    const player::Reply status = session_->Execute(
        player::PlayerCommand::kQueryDiscStatus, std::nullopt);
    EXPECT_TRUE(status.ok()) << "?D was not answered";
    Say("disc status: " + status.text);
  }

  if (controls.Has(player::PlayerCommand::kQueryTvSystem)) {
    const player::Reply tv =
        session_->Execute(player::PlayerCommand::kQueryTvSystem, std::nullopt);
    EXPECT_TRUE(tv.ok()) << "?S was not answered";
    Say("tv system:   " + tv.text);
  }
}

// The other half of the tier, and the one that takes a minute: the whole
// examine sequence driven against a real disc.
//
// Everything about the sequence's logic is already tested at T1 with no player
// attached — every refusal, every open tray, every link that dies halfway
// through. What is left, and what only a disc can answer, is whether the bytes
// this project believes it is sending mean to a real player what its manual
// says they do.
TEST_F(PlayerHardwareTest, AnExaminationOfARealDiscCompletes) {
  player::DiscExaminer examiner(*session_->identity().definition,
                                session_->identity().firmware_version);

  while (const std::optional<player::ExamineStep> step = examiner.Next()) {
    const player::Reply reply =
        session_->Execute(step->command, step->argument);
    Say(ExamineStageName(step->stage).toStdString() + ": sent \"" + reply.sent +
        "\", got \"" + reply.text + "\"");
    examiner.Apply(reply);
  }

  const player::DiscProfile& disc = examiner.profile();

  // Printed whatever the outcome, because a failed examination's report is the
  // thing worth reading. It is the same text the application shows and the
  // same text a user would paste into an issue.
  std::cout << DiscProfileReport(disc, examiner.outcome(), 0.0).toStdString()
            << "\n";

  ASSERT_EQ(examiner.outcome(), player::ExamineOutcome::kCompleted)
      << "the examination did not finish — with the tray shut on a disc that "
         "plays, it should";

  // The two facts every later step depends on. A profile without them is one
  // the automatic capture cannot build a plan from, so an examination that
  // "completed" without them has not done its job.
  ASSERT_TRUE(disc.disc_type.known());
  ASSERT_TRUE(disc.programme_end.known());
  EXPECT_EQ(disc.programme_end.provenance, player::Provenance::kMeasured);

  // Measured by seeking past the end and reading back where that landed, so it
  // must be past the start and must be a real address.
  EXPECT_GT(disc.programme_end.value, 0);
  if (disc.programme_start.known()) {
    EXPECT_LT(disc.programme_start.value, disc.programme_end.value);
  }

  if (const std::optional<player::DiscType> expected = ExpectedDiscType()) {
    EXPECT_EQ(disc.disc_type.value, *expected)
        << "the disc in the tray is not the type DDD_PLAYER_DISC_TYPE says";
  }

  if (const std::optional<int32_t> expected = ExpectedLastAddress()) {
    EXPECT_NEAR(static_cast<double>(disc.programme_end.value),
                static_cast<double>(*expected), kAddressTolerance)
        << "the measured end of the side is not the one on the player's own "
           "display";
  }
}

}  // namespace
}  // namespace ddd::gui
