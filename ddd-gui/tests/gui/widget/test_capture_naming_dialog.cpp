/************************************************************************

    test_capture_naming_dialog.cpp

    T1 tests for the dialog that says what the disc is
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture_controller.h"
#include "capture_naming.h"
#include "capture_naming_dialog.h"
#include "disc_profile.h"
#include "fake_serial_port.h"
#include "fake_usb_device.h"
#include "player_controller.h"
#include "serial_port_scanner.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

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

class CaptureNamingDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-naming-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    device_ = std::make_unique<capture::FakeUsbDevice>();
    controller_ = std::make_unique<CaptureController>(device_.get(), nullptr);
    dialog_ = std::make_unique<CaptureNamingDialog>(controller_.get());
  }

  void TearDown() override {
    dialog_.reset();
    controller_.reset();
    device_.reset();
    QSettings().clear();
  }

  // Rebuild the dialog from whatever the settings now say, which is what
  // happens when the button is pressed a second time.
  void Reopen() {
    dialog_ = std::make_unique<CaptureNamingDialog>(controller_.get());
  }

  QCheckBox* Check(const char* name) const {
    return dialog_->findChild<QCheckBox*>(QLatin1String(name));
  }
  QLineEdit* Edit(const char* name) const {
    return dialog_->findChild<QLineEdit*>(QLatin1String(name));
  }
  QComboBox* Combo(const char* name) const {
    return dialog_->findChild<QComboBox*>(QLatin1String(name));
  }
  QSpinBox* Spin(const char* name) const {
    return dialog_->findChild<QSpinBox*>(QLatin1String(name));
  }
  QPushButton* Button(const char* name) const {
    return dialog_->findChild<QPushButton*>(QLatin1String(name));
  }
  QLabel* Label(const char* name) const {
    return dialog_->findChild<QLabel*>(QLatin1String(name));
  }
  QPlainTextEdit* MetadataNotes() const {
    return dialog_->findChild<QPlainTextEdit*>(
        QLatin1String(CaptureNamingDialog::kMetadataNotesEditName));
  }

  const capture::CaptureNamingFields& Fields() const {
    return controller_->settings().naming;
  }

  std::unique_ptr<capture::FakeUsbDevice> device_;
  std::unique_ptr<CaptureController> controller_;
  std::unique_ptr<CaptureNamingDialog> dialog_;
};

TEST_F(CaptureNamingDialogTest, EveryFieldStartsUnaskedFor) {
  // The state that matters most, because it is the one every capture is taken
  // in: nothing has been said about the disc, so nothing is claimed about it.
  EXPECT_FALSE(Check(CaptureNamingDialog::kTitleCheckName)->isChecked());
  EXPECT_FALSE(Check(CaptureNamingDialog::kSideCheckName)->isChecked());
  EXPECT_FALSE(Fields().title_used);
  EXPECT_FALSE(Fields().side_used);
}

TEST_F(CaptureNamingDialogTest, AValueIsGreyedOutUntilItsBoxIsTicked) {
  // An unticked field has to read as absent rather than as empty, or somebody
  // types into it and wonders why nothing happens.
  EXPECT_FALSE(Edit(CaptureNamingDialog::kTitleEditName)->isEnabled());

  Check(CaptureNamingDialog::kTitleCheckName)->setChecked(true);
  EXPECT_TRUE(Edit(CaptureNamingDialog::kTitleEditName)->isEnabled());
}

TEST_F(CaptureNamingDialogTest, TypingAppliesStraightAwayAndSurvivesReopening) {
  // There is no OK button, so this is the whole of the apply. It also has to
  // reach the settings file: somebody capturing both sides of a disc types the
  // title once.
  Check(CaptureNamingDialog::kTitleCheckName)->setChecked(true);
  Edit(CaptureNamingDialog::kTitleEditName)->setText(QStringLiteral("Casper"));

  EXPECT_TRUE(Fields().title_used);
  EXPECT_EQ(Fields().title, "Casper");

  Reopen();
  EXPECT_TRUE(Check(CaptureNamingDialog::kTitleCheckName)->isChecked());
  EXPECT_EQ(Edit(CaptureNamingDialog::kTitleEditName)->text(),
            QStringLiteral("Casper"));
}

TEST_F(CaptureNamingDialogTest, TheChoicesReachTheSettingsAsChoices) {
  Check(CaptureNamingDialog::kDiscTypeCheckName)->setChecked(true);
  Combo(CaptureNamingDialog::kDiscTypeComboName)
      ->setCurrentIndex(
          Combo(CaptureNamingDialog::kDiscTypeComboName)
              ->findData(static_cast<int>(capture::DiscTypeChoice::kClv)));

  Check(CaptureNamingDialog::kAudioCheckName)->setChecked(true);
  Combo(CaptureNamingDialog::kAudioComboName)
      ->setCurrentIndex(
          Combo(CaptureNamingDialog::kAudioComboName)
              ->findData(static_cast<int>(capture::AudioTypeChoice::kDts)));

  EXPECT_EQ(Fields().disc_type, capture::DiscTypeChoice::kClv);
  EXPECT_EQ(Fields().audio, capture::AudioTypeChoice::kDts);
}

TEST_F(CaptureNamingDialogTest, TheDetailsOnlyReachTheNameWhenAskedFor) {
  Check(CaptureNamingDialog::kTitleCheckName)->setChecked(true);
  Edit(CaptureNamingDialog::kTitleEditName)->setText(QStringLiteral("Casper"));
  Check(CaptureNamingDialog::kDiscTypeCheckName)->setChecked(true);

  QLabel* const preview = Label(CaptureNamingDialog::kPreviewLabelName);
  ASSERT_NE(preview, nullptr);
  EXPECT_TRUE(preview->text().contains(QStringLiteral("Casper")));
  EXPECT_FALSE(preview->text().contains(QStringLiteral("_CAV")));

  Check(CaptureNamingDialog::kMetadataInNameCheckName)->setChecked(true);
  EXPECT_TRUE(preview->text().contains(QStringLiteral("Casper_CAV")));
}

TEST_F(CaptureNamingDialogTest, ThePreviewSaysWhenATypedNameIsWinning) {
  // Otherwise somebody ticks five boxes, sees none of them in the name, and
  // reasonably concludes the boxes do not work.
  CaptureSettings settings = controller_->settings();
  settings.capture_name = QStringLiteral("my capture");
  controller_->SetSettings(settings);

  Reopen();
  Check(CaptureNamingDialog::kTitleCheckName)->setChecked(true);
  Edit(CaptureNamingDialog::kTitleEditName)->setText(QStringLiteral("Casper"));

  const QString text = Label(CaptureNamingDialog::kPreviewLabelName)->text();
  EXPECT_TRUE(text.contains(QStringLiteral("my capture.ddd.flac")))
      << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("Name field")))
      << text.toStdString();
}

TEST_F(CaptureNamingDialogTest, ThePreviewNamesTheMetadataFileBesideIt) {
  const QString text = Label(CaptureNamingDialog::kPreviewLabelName)->text();
  EXPECT_TRUE(text.contains(QStringLiteral(".ddd.yaml"))) << text.toStdString();
}

TEST_F(CaptureNamingDialogTest, TestModeSaysItWillOverrideAllOfThis) {
  CaptureSettings settings = controller_->settings();
  settings.test_mode = true;
  controller_->SetSettings(settings);
  Reopen();

  const QString text = Label(CaptureNamingDialog::kPreviewLabelName)->text();
  EXPECT_TRUE(text.contains(QStringLiteral("TestData_"))) << text.toStdString();
}

TEST_F(CaptureNamingDialogTest, ClearingEmptiesEveryFieldForTheNextDisc) {
  // The button that makes persisting these fields safe: without it the second
  // disc of a session inherits the first one's title.
  Check(CaptureNamingDialog::kTitleCheckName)->setChecked(true);
  Edit(CaptureNamingDialog::kTitleEditName)->setText(QStringLiteral("Casper"));
  Check(CaptureNamingDialog::kSideCheckName)->setChecked(true);
  Spin(CaptureNamingDialog::kSideSpinName)->setValue(2);
  Check(CaptureNamingDialog::kNotesCheckName)->setChecked(true);
  Edit(CaptureNamingDialog::kNotesEditName)->setText(QStringLiteral("rot"));
  MetadataNotes()->setPlainText(QStringLiteral("a paragraph"));
  Check(CaptureNamingDialog::kMetadataInNameCheckName)->setChecked(true);

  Button(CaptureNamingDialog::kClearButtonName)->click();

  EXPECT_EQ(Fields(), capture::CaptureNamingFields{});
  EXPECT_TRUE(Edit(CaptureNamingDialog::kTitleEditName)->text().isEmpty());
  EXPECT_TRUE(MetadataNotes()->toPlainText().isEmpty());
  EXPECT_EQ(Spin(CaptureNamingDialog::kSideSpinName)->value(), 1);
}

TEST_F(CaptureNamingDialogTest, ClearingLeavesTheWayOfWorkingAlone) {
  // The two per-side options are a way of working rather than a fact about a
  // disc, so somebody who turned them on means them for the next disc too.
  Check(CaptureNamingDialog::kNotesCheckName)->setChecked(true);
  Check(CaptureNamingDialog::kPerSideNotesCheckName)->setChecked(true);
  ASSERT_TRUE(Fields().per_side_notes);

  Button(CaptureNamingDialog::kClearButtonName)->click();
  EXPECT_TRUE(Fields().per_side_notes);
}

TEST_F(CaptureNamingDialogTest, PerSideNotesComeBackWhenTheSideDoes) {
  Check(CaptureNamingDialog::kNotesCheckName)->setChecked(true);
  Check(CaptureNamingDialog::kSideCheckName)->setChecked(true);
  Check(CaptureNamingDialog::kPerSideNotesCheckName)->setChecked(true);

  Edit(CaptureNamingDialog::kNotesEditName)
      ->setText(QStringLiteral("side one notes"));
  Spin(CaptureNamingDialog::kSideSpinName)->setValue(2);

  // Side 2 starts blank rather than inheriting side 1's notes, which is the
  // whole point: without this the second side silently carries the first's.
  EXPECT_TRUE(Edit(CaptureNamingDialog::kNotesEditName)->text().isEmpty());

  Edit(CaptureNamingDialog::kNotesEditName)
      ->setText(QStringLiteral("side two notes"));
  Spin(CaptureNamingDialog::kSideSpinName)->setValue(1);

  EXPECT_EQ(Edit(CaptureNamingDialog::kNotesEditName)->text(),
            QStringLiteral("side one notes"));
}

TEST_F(CaptureNamingDialogTest, WithoutTheOptionTheNotesFollowTheSide) {
  // The default. Somebody who has not asked for per-side notes has one set of
  // notes for the disc, and changing the side must not blank them.
  Check(CaptureNamingDialog::kNotesCheckName)->setChecked(true);
  Check(CaptureNamingDialog::kSideCheckName)->setChecked(true);
  Edit(CaptureNamingDialog::kNotesEditName)->setText(QStringLiteral("rot"));

  Spin(CaptureNamingDialog::kSideSpinName)->setValue(2);
  EXPECT_EQ(Edit(CaptureNamingDialog::kNotesEditName)->text(),
            QStringLiteral("rot"));
}

TEST_F(CaptureNamingDialogTest, ADialogWithNoControllerIsStillUsable) {
  // Built the same way in every test of the window that has no capture engine,
  // and it must not be the thing that crashes.
  CaptureNamingDialog orphan(nullptr);
  EXPECT_NE(orphan.findChild<QPushButton*>(
                QLatin1String(CaptureNamingDialog::kClearButtonName)),
            nullptr);
}

// --- Asking the player ------------------------------------------------------
//
// Three of the fields above are things the disc itself can be asked, and asking
// takes seconds where typing takes a minute and can be got wrong. What matters
// about the button is not that it fills fields in but *which* fields it leaves
// alone: it reaches into a form somebody has been typing in.

TEST_F(CaptureNamingDialogTest, WithNoPlayerLayerThereIsNothingToAsk) {
  // Absent rather than disabled. A build with no player support has nothing to
  // ask, and a greyed-out button would invite somebody to look for the setting
  // that turns it on.
  EXPECT_EQ(Button(CaptureNamingDialog::kAskButtonName), nullptr);
}

TEST_F(CaptureNamingDialogTest, WhatThePlayerReportedIsFilledInAndTicked) {
  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kClv, player::Provenance::kReported);
  disc.video_standard.Record(player::VideoStandard::kPal,
                             player::Provenance::kReported);
  disc.disc_side.Record(2, player::Provenance::kReported);

  dialog_->form()->FillFromProfile(disc);

  // Ticked as well as set: an unticked field is one nobody established, and the
  // player has just established these.
  EXPECT_TRUE(Check(CaptureNamingDialog::kDiscTypeCheckName)->isChecked());
  EXPECT_TRUE(Check(CaptureNamingDialog::kStandardCheckName)->isChecked());
  EXPECT_TRUE(Check(CaptureNamingDialog::kSideCheckName)->isChecked());

  // And it reached the settings, which is where the capture reads them from.
  EXPECT_EQ(Fields().disc_type, capture::DiscTypeChoice::kClv);
  EXPECT_EQ(Fields().video_standard, capture::VideoStandardChoice::kPal);
  EXPECT_EQ(Fields().side, 2);
  EXPECT_TRUE(Fields().side_used);
}

TEST_F(CaptureNamingDialogTest, AFactThePlayerCouldNotReportIsLeftAlone) {
  // The whole design of DiscProfile: a refused query leaves its field unknown
  // rather than guessed, and this form has to carry that through rather than
  // ticking a box for something nobody established.
  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  // The standard and the side are left unknown, as a model that cannot be asked
  // for them leaves them.

  dialog_->form()->FillFromProfile(disc);

  EXPECT_TRUE(Check(CaptureNamingDialog::kDiscTypeCheckName)->isChecked());
  EXPECT_FALSE(Check(CaptureNamingDialog::kStandardCheckName)->isChecked());
  EXPECT_FALSE(Check(CaptureNamingDialog::kSideCheckName)->isChecked());
  EXPECT_FALSE(Fields().video_standard_used);
  EXPECT_FALSE(Fields().side_used);
}

TEST_F(CaptureNamingDialogTest, NothingTypedIsEverOverwrittenByAsking) {
  // The one thing this button must never do. A title, notes and mint marks are
  // things only a person knows; a disc being spun up says nothing about them.
  Check(CaptureNamingDialog::kTitleCheckName)->setChecked(true);
  Edit(CaptureNamingDialog::kTitleEditName)->setText(QStringLiteral("Casper"));
  Check(CaptureNamingDialog::kNotesCheckName)->setChecked(true);
  Edit(CaptureNamingDialog::kNotesEditName)->setText(QStringLiteral("rot"));
  Check(CaptureNamingDialog::kMintCheckName)->setChecked(true);
  Edit(CaptureNamingDialog::kMintEditName)->setText(QStringLiteral("NM"));
  MetadataNotes()->setPlainText(QStringLiteral("a paragraph"));

  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  disc.disc_side.Record(1, player::Provenance::kReported);
  dialog_->form()->FillFromProfile(disc);

  EXPECT_EQ(Edit(CaptureNamingDialog::kTitleEditName)->text(),
            QStringLiteral("Casper"));
  EXPECT_EQ(Edit(CaptureNamingDialog::kNotesEditName)->text(),
            QStringLiteral("rot"));
  EXPECT_EQ(Edit(CaptureNamingDialog::kMintEditName)->text(),
            QStringLiteral("NM"));
  EXPECT_EQ(MetadataNotes()->toPlainText(), QStringLiteral("a paragraph"));
}

TEST_F(CaptureNamingDialogTest, AnAnswerThatContradictsWhatWasTypedWins) {
  // The other half of the rule, and it is not the same as the one above. These
  // three fields *are* the player's to answer: somebody who ticked CAV by hand
  // and then asked the disc, which said CLV, asked because they wanted the
  // disc's answer.
  Check(CaptureNamingDialog::kDiscTypeCheckName)->setChecked(true);
  Combo(CaptureNamingDialog::kDiscTypeComboName)
      ->setCurrentIndex(
          Combo(CaptureNamingDialog::kDiscTypeComboName)
              ->findData(static_cast<int>(capture::DiscTypeChoice::kCav)));
  ASSERT_EQ(Fields().disc_type, capture::DiscTypeChoice::kCav);

  player::DiscProfile disc;
  disc.disc_type.Record(player::DiscType::kClv, player::Provenance::kReported);
  dialog_->form()->FillFromProfile(disc);

  EXPECT_EQ(Fields().disc_type, capture::DiscTypeChoice::kClv);
}

// The whole path, end to end, against a scripted player: pressing the button
// runs an identifying examination and the answers land in the fields.
//
// The direct FillFromProfile tests above say what the fill rules are; this one
// says the button is actually wired to them — that the identify scope reaches
// the worker, that the examine signals come back, and that the form recognises
// them as answers to its own question.
class CaptureNamingAskTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(QStringLiteral("ddd-gui-naming-ask-%1")
                                             .arg(QLatin1String(info->name())));
    QSettings().clear();

    SerialPortCandidate candidate;
    candidate.path = QStringLiteral("/dev/ttyFAKE0");
    candidate.usb_adapter = true;
    ports_.push_back(candidate);
    port_.set_only_path("/dev/ttyFAKE0");

    // A PAL CLV disc on side 2, in a parked player.
    port_.AddPioneerPlayer(9600, "P1515A1");
    port_.AddStatusResponses(9600, "P01", "11010", "0012345");
    port_.AddResponse(9600, "PL\r", "R\r");
    port_.AddResponse(9600, "PA\r", "R\r");
    port_.AddResponse(9600, "?S\r", "220\r");

    PlayerBackend backend;
    backend.make_port = [this] {
      return std::make_unique<player::BorrowedSerialPort>(&port_);
    };
    backend.list_ports = [this] { return ports_; };
    backend.clock = port_.clock();

    player_ = std::make_unique<PlayerController>(std::move(backend));
    device_ = std::make_unique<capture::FakeUsbDevice>();
    controller_ = std::make_unique<CaptureController>(device_.get(), nullptr);
    dialog_ =
        std::make_unique<CaptureNamingDialog>(controller_.get(), player_.get());
  }

  void TearDown() override {
    dialog_.reset();
    player_.reset();
    controller_.reset();
    device_.reset();
    QSettings().clear();
  }

  bool Connect() {
    player_->Start();
    player_->SetEnabled(true);
    return PumpUntil([this] { return player_->connected(); });
  }

  template <typename T>
  T* Find(const char* name) const {
    return dialog_->findChild<T*>(QLatin1String(name));
  }

  const capture::CaptureNamingFields& Fields() const {
    return controller_->settings().naming;
  }

  player::FakeSerialPort port_;
  std::vector<SerialPortCandidate> ports_;
  std::unique_ptr<PlayerController> player_;
  std::unique_ptr<capture::FakeUsbDevice> device_;
  std::unique_ptr<CaptureController> controller_;
  std::unique_ptr<CaptureNamingDialog> dialog_;
};

TEST_F(CaptureNamingAskTest, WithNoPlayerConnectedThereIsNothingToAsk) {
  auto* const ask = Find<QPushButton>(CaptureNamingDialog::kAskButtonName);
  ASSERT_NE(ask, nullptr)
      << "the button is missing with a player layer present";
  EXPECT_FALSE(ask->isEnabled());

  // Said rather than left to a greyed-out button to imply: "no player is
  // connected" and "this build cannot ask" are different problems.
  EXPECT_FALSE(
      Find<QLabel>(CaptureNamingDialog::kAskStatusLabelName)->text().isEmpty());
}

TEST_F(CaptureNamingAskTest, PressingItFillsInWhatTheDiscSays) {
  ASSERT_TRUE(Connect());

  auto* const ask = Find<QPushButton>(CaptureNamingDialog::kAskButtonName);
  ASSERT_NE(ask, nullptr);
  ASSERT_TRUE(ask->isEnabled());

  ask->click();

  ASSERT_TRUE(PumpUntil([this] { return Fields().disc_type_used; }))
      << "the examination never came back";

  EXPECT_EQ(Fields().disc_type, capture::DiscTypeChoice::kClv);
  EXPECT_EQ(Fields().video_standard, capture::VideoStandardChoice::kPal);
  EXPECT_TRUE(Fields().video_standard_used);
  EXPECT_EQ(Fields().side, 2);
  EXPECT_TRUE(Fields().side_used);

  // And it says what it touched, which is what makes a button that reaches into
  // a form safe to press.
  const QString note =
      Find<QLabel>(CaptureNamingDialog::kAskStatusLabelName)->text();
  EXPECT_TRUE(note.contains(QStringLiteral("disc type"))) << note.toStdString();
  EXPECT_TRUE(note.contains(QStringLiteral("untouched"))) << note.toStdString();

  // The button comes back rather than staying dead: the next disc is the other
  // side of this one.
  EXPECT_TRUE(ask->isEnabled());
}

TEST_F(CaptureNamingAskTest, TheDiscIsNeverSeekedForANameAlone) {
  ASSERT_TRUE(Connect());

  Find<QPushButton>(CaptureNamingDialog::kAskButtonName)->click();
  ASSERT_TRUE(PumpUntil([this] { return Fields().disc_type_used; }));

  // The point of the identifying scope, asserted on the wire. A seek would take
  // a minute and leave the disc somewhere else, and nothing on this form is
  // worth that.
  for (const std::string& command : port_.writes()) {
    EXPECT_EQ(command.rfind("FR", 0), std::string::npos)
        << "a seek went out for a naming field: " << command;
    EXPECT_NE(command, "?U\r")
        << "the Pioneer user code search went out for a naming field";
  }

  // What it did send is the cheap end: spin up, read, settle.
  const std::vector<std::string> writes = port_.writes();
  const auto sent = [&writes](const std::string& bytes) {
    return std::find(writes.begin(), writes.end(), bytes) != writes.end();
  };
  EXPECT_TRUE(sent("?D\r"));
  EXPECT_TRUE(sent("?S\r"));
  EXPECT_TRUE(sent("PA\r")) << "the disc was left spinning";
}

TEST_F(CaptureNamingDialogTest, ASideNumberTheFormCannotHoldIsNotFollowed) {
  // A player reporting a side outside what the spin box offers is one this
  // application should not follow into a field that cannot represent it — the
  // box would silently clamp, and a clamped side is a wrong side recorded as a
  // fact.
  player::DiscProfile disc;
  disc.disc_side.Record(CaptureNamingDialog::kMaximumDiscSide + 1,
                        player::Provenance::kReported);

  dialog_->form()->FillFromProfile(disc);

  EXPECT_FALSE(Check(CaptureNamingDialog::kSideCheckName)->isChecked());
  EXPECT_FALSE(Fields().side_used);
}

}  // namespace
}  // namespace ddd::gui
