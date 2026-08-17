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
#include <memory>

#include "capture_controller.h"
#include "capture_naming.h"
#include "capture_naming_dialog.h"
#include "fake_usb_device.h"

namespace ddd::gui {
namespace {

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

}  // namespace
}  // namespace ddd::gui
