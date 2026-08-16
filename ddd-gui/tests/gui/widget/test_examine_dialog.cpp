/************************************************************************

    test_examine_dialog.cpp

    T1 tests for the examine window
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "examine_dialog.h"
#include "fake_serial_port.h"
#include "player_controller.h"
#include "player_text.h"
#include "serial_port_scanner.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

constexpr const char* kPortPath = "/dev/ttyFAKE0";
constexpr const char* kLdV4300DReply = "P1515A1";

template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 5000ms) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    QApplication::processEvents();
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

PlayerConnection LiveConnection() {
  PlayerConnection connection;
  connection.state = PlayerConnectionState::kConnected;
  connection.port_path = QLatin1String(kPortPath);
  connection.baud_rate = 9600;
  connection.model_name = QStringLiteral("Pioneer LD-V4300D");
  connection.recognised_model = true;
  return connection;
}

// A CAV disc of 54,000 frames, answered end to end.
void ScriptCavDisc(player::FakeSerialPort& port) {
  port.AddResponse(9600, "?P\r", "P01\r");
  port.AddResponse(9600, "?D\r", "10001\r");
  port.AddResponse(9600, "PL\r", "R\r");
  port.AddResponse(9600, "$Y\r", "Y1000\r");
  port.AddResponse(9600, "?U\r", "#59-014  *MCA / CASPER\r");
  port.AddResponse(9600, "?S\r", "220\r");
  port.AddResponse(9600, "FR60000SE\r", "E04\r");
  port.AddResponse(9600, "FR1SE\r", "R\r");
  port.AddResponse(9600, "PA\r", "R\r");
  port.AddResponseSequence(9600, "?F\r", {"054000\r", "<00001\r"});
}

player::DiscProfile ExaminedCavDisc() {
  player::DiscProfile disc;
  disc.disc_present.Record(true, player::Provenance::kMeasured);
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  disc.addressing.Record(player::AddressMode::kFrame,
                         player::Provenance::kInferred);
  disc.programme_start.Record(1, player::Provenance::kMeasured);
  disc.programme_end.Record(54000, player::Provenance::kMeasured);
  disc.disc_size.Record(player::DiscSize::k30cm, player::Provenance::kReported);
  disc.disc_side.Record(2, player::Provenance::kReported);
  return disc;
}

class ExamineDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-examine-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    SerialPortCandidate candidate;
    candidate.path = QLatin1String(kPortPath);
    candidate.usb_adapter = true;
    ports_.push_back(candidate);
    port_.set_only_path(kPortPath);
  }

  void TearDown() override {
    dialog_.reset();
    controller_.reset();
    QSettings().clear();
  }

  // No controller at all: the dialog builds, lays out and drives nothing, and
  // its state can still be set through the slots. This is what lets the report
  // and the gating be tested against discs nobody has on a bench.
  void BuildAlone() { dialog_ = std::make_unique<ExamineDialog>(nullptr); }

  void BuildWithController() {
    PlayerBackend backend;
    backend.make_port = [this] {
      return std::make_unique<player::BorrowedSerialPort>(&port_);
    };
    backend.list_ports = [this] { return ports_; };
    backend.clock = port_.clock();

    controller_ = std::make_unique<PlayerController>(std::move(backend));
    dialog_ = std::make_unique<ExamineDialog>(controller_.get());
  }

  template <typename T>
  T* Find(const char* name) const {
    return dialog_->findChild<T*>(QLatin1String(name));
  }

  QString ReportText() const {
    return Find<QPlainTextEdit>(ExamineDialog::kReportViewName)->toPlainText();
  }

  player::FakeSerialPort port_;
  std::vector<SerialPortCandidate> ports_;
  std::unique_ptr<PlayerController> controller_;
  std::unique_ptr<ExamineDialog> dialog_;
};

TEST_F(ExamineDialogTest, ItBuildsAndDrivesNothingWithNoControllerAtAll) {
  BuildAlone();

  EXPECT_NE(Find<QLabel>(ExamineDialog::kHeadlineLabelName), nullptr);
  EXPECT_NE(Find<QProgressBar>(ExamineDialog::kProgressBarName), nullptr);
  EXPECT_NE(Find<QPlainTextEdit>(ExamineDialog::kReportViewName), nullptr);

  // Present but inert, so the window has the same shape in every build of it.
  EXPECT_FALSE(Find<QPushButton>(ExamineDialog::kStartButtonName)->isEnabled());
  EXPECT_FALSE(
      Find<QPushButton>(ExamineDialog::kCancelButtonName)->isEnabled());

  // And pressing it does nothing rather than crashing.
  Find<QPushButton>(ExamineDialog::kStartButtonName)->click();
  EXPECT_FALSE(dialog_->running());
}

TEST_F(ExamineDialogTest, ExamineIsOfferedOnlyWhenThereIsAPlayer) {
  BuildAlone();

  auto* const start = Find<QPushButton>(ExamineDialog::kStartButtonName);
  EXPECT_FALSE(start->isEnabled());

  dialog_->SetConnection(LiveConnection());
  EXPECT_TRUE(start->isEnabled());

  // A cable pulled out mid-session leaves the window open and greyed rather
  // than making it vanish.
  dialog_->SetConnection(PlayerConnection{});
  EXPECT_FALSE(start->isEnabled());
}

TEST_F(ExamineDialogTest, TheWindowSaysWhatExaminingWillDoBeforeItIsAskedFor) {
  // The disc spins up and seeks twice, which is a thing worth knowing in
  // advance rather than discovering.
  BuildAlone();
  dialog_->SetConnection(LiveConnection());

  const QString note =
      Find<QLabel>(ExamineDialog::kStageLabelName)->text().toLower();
  EXPECT_TRUE(note.contains(QStringLiteral("spins the disc up")));
  EXPECT_TRUE(note.contains(QStringLiteral("seeks to the end")));
  EXPECT_TRUE(note.contains(QStringLiteral("identifying codes")));
}

TEST_F(ExamineDialogTest, AProgressReportWithNothingRunningIsIgnored) {
  BuildAlone();
  dialog_->SetConnection(LiveConnection());

  // Driven directly, which is what the slots are for: no controller, no
  // player, and every step of the sequence still reaches the window.
  dialog_->SetProgress(player::ExamineStage::kFindingEnd, 5, 10);

  // Nothing is running, so a stray progress report is ignored rather than
  // making the window claim to be busy.
  EXPECT_FALSE(dialog_->running());
  EXPECT_NE(Find<QLabel>(ExamineDialog::kStageLabelName)->text(),
            ExamineStageName(player::ExamineStage::kFindingEnd));
}

TEST_F(ExamineDialogTest, TheReportIsWhatTheProfileSaysAndIsCopyable) {
  BuildAlone();
  dialog_->SetConnection(LiveConnection());
  dialog_->SetResult(ExaminedCavDisc(), player::ExamineOutcome::kCompleted);

  EXPECT_FALSE(dialog_->running());
  EXPECT_TRUE(ReportText().contains(QStringLiteral("Frame 54000")));
  EXPECT_TRUE(ReportText().contains(QStringLiteral("measured")));

  // Copy is offered once there is something to copy, and not before.
  EXPECT_TRUE(Find<QPushButton>(ExamineDialog::kCopyButtonName)->isEnabled());

  Find<QPushButton>(ExamineDialog::kCopyButtonName)->click();
  EXPECT_EQ(QApplication::clipboard()->text(), ReportText());
}

TEST_F(ExamineDialogTest, NothingToCopyMeansNothingToCopyWith) {
  BuildAlone();
  dialog_->SetConnection(LiveConnection());

  EXPECT_FALSE(Find<QPushButton>(ExamineDialog::kCopyButtonName)->isEnabled());
}

TEST_F(ExamineDialogTest, AProfileWithHolesInItRendersThemAsUnknown) {
  // Not as blanks and not as zeroes. Zero is a real frame number and a blank
  // in a report somebody is about to paste into an issue reads as a bug in the
  // report.
  BuildAlone();
  dialog_->SetConnection(LiveConnection());

  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  dialog_->SetResult(disc, player::ExamineOutcome::kCompleted);

  EXPECT_TRUE(ReportText().contains(QStringLiteral("not known")));
  EXPECT_FALSE(ReportText().contains(QStringLiteral("Frame 0")));
}

TEST_F(ExamineDialogTest, AnOpenTrayIsExplainedRatherThanShownAsAnEmptyDisc) {
  BuildAlone();
  dialog_->SetConnection(LiveConnection());
  dialog_->SetResult(player::DiscProfile{}, player::ExamineOutcome::kTrayOpen);

  EXPECT_TRUE(Find<QLabel>(ExamineDialog::kHeadlineLabelName)
                  ->text()
                  .contains(QStringLiteral("tray is open")));
}

TEST_F(ExamineDialogTest, AWholeExaminationRunsAgainstAScriptedPlayer) {
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  ScriptCavDisc(port_);
  BuildWithController();

  controller_->Start();
  controller_->SetEnabled(true);
  ASSERT_TRUE(PumpUntil([this] { return controller_->connected(); }));

  auto* const start = Find<QPushButton>(ExamineDialog::kStartButtonName);
  ASSERT_TRUE(PumpUntil([start] { return start->isEnabled(); }));

  start->click();
  EXPECT_TRUE(dialog_->running());

  // While it runs, the window offers stopping and not starting again.
  EXPECT_FALSE(start->isEnabled());
  EXPECT_TRUE(Find<QPushButton>(ExamineDialog::kCancelButtonName)->isEnabled());

  ASSERT_TRUE(PumpUntil([this] { return !dialog_->running(); }));

  EXPECT_EQ(dialog_->profile().disc_type.value, player::DiscType::kCav);
  EXPECT_EQ(dialog_->profile().programme_end.value, 54000);
  EXPECT_TRUE(ReportText().contains(QStringLiteral("Frame 54000")));

  // The disc's own programme status came back with it, and the user code was
  // read without anybody having to ask for it.
  EXPECT_EQ(dialog_->profile().disc_side.value, 1);
  EXPECT_TRUE(dialog_->profile().pioneer_user_code.read());
  EXPECT_TRUE(ReportText().contains(QStringLiteral("Side: side 1")));

  // And the standard, which the window used to have no way of establishing at
  // all — so a CAV disc now has a playing time and a size estimate too.
  EXPECT_EQ(dialog_->profile().video_standard.value,
            player::VideoStandard::kPal);
  EXPECT_TRUE(ReportText().contains(QStringLiteral("Video standard: PAL")));
  EXPECT_TRUE(ReportText().contains(QStringLiteral("Playing time:")));

  // The progress bar ends full rather than part way, whatever the plan turned
  // out to contain.
  auto* const progress = Find<QProgressBar>(ExamineDialog::kProgressBarName);
  EXPECT_EQ(progress->value(), progress->maximum());

  // And it can be run again, which is what a user does with the second side.
  EXPECT_TRUE(start->isEnabled());
}

TEST_F(ExamineDialogTest, TheLastDiscsReportIsClearedBeforeTheNextOne) {
  // The two discs somebody examines in a row are the two sides of the same
  // one, so a report left on screen would be read as the new side's.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  ScriptCavDisc(port_);
  BuildWithController();

  controller_->Start();
  controller_->SetEnabled(true);
  ASSERT_TRUE(PumpUntil([this] { return controller_->connected(); }));

  dialog_->SetResult(ExaminedCavDisc(), player::ExamineOutcome::kCompleted);
  ASSERT_FALSE(ReportText().isEmpty());

  dialog_->Start();
  EXPECT_TRUE(ReportText().isEmpty());
  EXPECT_FALSE(Find<QPushButton>(ExamineDialog::kCopyButtonName)->isEnabled());
}

TEST_F(ExamineDialogTest, StoppingSaysSoBeforeItHasTakenEffect) {
  BuildWithController();
  dialog_->SetConnection(LiveConnection());

  // Forced into the running state without a player, which is exactly the state
  // the cancel wording is about: the request is made and not yet granted.
  dialog_->Start();
  ASSERT_TRUE(dialog_->running());

  dialog_->Cancel();

  EXPECT_TRUE(Find<QLabel>(ExamineDialog::kStageLabelName)
                  ->text()
                  .contains(QStringLiteral("Stopping")));
  EXPECT_FALSE(
      Find<QPushButton>(ExamineDialog::kCancelButtonName)->isEnabled());
}

}  // namespace
}  // namespace ddd::gui
