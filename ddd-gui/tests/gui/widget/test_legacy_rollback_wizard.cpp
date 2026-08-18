/************************************************************************

    test_legacy_rollback_wizard.cpp

    The rollback flow as a widget, driven end to end with no hardware
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <string>
#include <vector>

#include "boot_image_fixture.h"
#include "digest.h"
#include "fake_device_updater.h"
#include "legacy_rollback_wizard.h"
#include "update_bundle.h"
#include "update_fixtures.h"
#include "update_key.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

using capture::FakeDeviceUpdater;

// A rollback is the one flow that deliberately takes capabilities away, and
// the two things worth proving about it cannot both be arranged on a bench:
// that the FPGA is always written before the FX3, and that a unit which is
// already legacy — or not working at all — is refused rather than half
// programmed.

// A signed rollback file on disk. The manifest is built and signed here rather
// than taken from the shared fixtures, because "purpose: rollback" is the one
// field this whole flow turns on and a file that did not carry it would be
// testing the wrong thing.
class RollbackFile {
 public:
  RollbackFile() {
    const std::vector<uint8_t> firmware = capture::test::MakeBootImage();
    const std::vector<uint8_t> gateware(
        reinterpret_cast<const uint8_t*>(
            capture::test::kFactoryGatewarePayload.data()),
        reinterpret_cast<const uint8_t*>(
            capture::test::kFactoryGatewarePayload.data()) +
            capture::test::kFactoryGatewarePayload.size());

    capture::UstarWriter writer;
    writer.AddFile(capture::kManifestEntryName,
                   capture::test::Bytes(capture::test::kRollbackManifestJson));
    writer.AddFile(
        capture::kSignatureEntryName,
        capture::test::Bytes(capture::test::kRollbackManifestSignature));
    writer.AddFile(capture::kFirmwareEntryName, firmware);
    writer.AddFile(capture::kFactoryGatewareEntryName, gateware);
    Write(writer.Finish(), QStringLiteral("rollback.dddfw"));
  }

  // An ordinary update file, which is the wrong file for this window and the
  // mistake somebody is most likely to make.
  static QString UpdateBundlePath(QTemporaryDir& directory) {
    capture::UstarWriter writer;
    writer.AddFile(capture::kManifestEntryName,
                   capture::test::Bytes(capture::test::kManifestJson));
    writer.AddFile(capture::kSignatureEntryName,
                   capture::test::Bytes(capture::test::kManifestSignature));
    writer.AddFile(capture::kFirmwareEntryName,
                   capture::test::Bytes(capture::test::kFirmwarePayload));
    writer.AddFile(capture::kGatewareEntryName,
                   capture::test::Bytes(capture::test::kGatewarePayload));

    const QString path = directory.filePath(QStringLiteral("update.dddfw"));
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly));
    const std::vector<uint8_t> bytes = writer.Finish();
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<qint64>(bytes.size()));
    file.close();
    return path;
  }

  QString path() const { return path_; }

 private:
  void Write(const std::vector<uint8_t>& bytes, const QString& name) {
    path_ = directory_.filePath(name);
    QFile file(path_);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<qint64>(bytes.size()));
    file.close();
  }

  QTemporaryDir directory_;
  QString path_;
};

// The device the wizard meets, and what it says about itself.
capture::DeviceInfo WorkingDevice() {
  capture::DeviceInfo device;
  device.path = "/fake/duplicator";
  device.personality = capture::DevicePersonality::kApplication;
  return device;
}

capture::DeviceInfo LegacyDevice() {
  capture::DeviceInfo device;
  device.path = "/fake/legacy";
  device.personality = capture::DevicePersonality::kLegacy;
  return device;
}

capture::DeviceIdentity WorkingIdentity() {
  capture::DeviceIdentity identity;
  identity.product_string = "Domesday Duplicator (0123abcd)";
  identity.protocol_version = 1;
  identity.gateware_present = true;
  identity.register_map_version = capture::kRegisterMapVersionMaximum;
  return identity;
}

class BorrowedUpdater : public capture::IDeviceUpdater {
 public:
  explicit BorrowedUpdater(FakeDeviceUpdater* fake) : fake_(fake) {}

  std::optional<capture::DeviceIdentity> ReadIdentity() override {
    return fake_->ReadIdentity();
  }
  std::optional<capture::DeviceUpdateStatus> ReadStatus() override {
    return fake_->ReadStatus();
  }
  bool Begin(capture::UpdateTarget target, uint64_t length,
             const capture::Sha256Digest& digest) override {
    return fake_->Begin(target, length, digest);
  }
  bool SendChunk(capture::UpdateTarget target, uint16_t index,
                 std::span<const uint8_t> data) override {
    return fake_->SendChunk(target, index, data);
  }
  bool Finish(capture::UpdateTarget target) override {
    return fake_->Finish(target);
  }
  bool Reset() override { return fake_->Reset(); }
  bool ReconfigureFpga() override { return fake_->ReconfigureFpga(); }
  std::optional<capture::DeviceIdentity> WaitForReturn(
      std::chrono::milliseconds timeout) override {
    return fake_->WaitForReturn(timeout);
  }

 private:
  FakeDeviceUpdater* fake_ = nullptr;
};

// The window under test, with every factory pointed at a fake.
class WizardUnderTest {
 public:
  WizardUnderTest() {
    updater.SetIdentity(WorkingIdentity());
    devices.push_back(WorkingDevice());

    LegacyRollbackWizard::Access access;
    access.devices = [this] { return devices; };
    access.open_updater =
        [this](const std::string&) -> std::unique_ptr<capture::IDeviceUpdater> {
      return std::make_unique<BorrowedUpdater>(&updater);
    };

    wizard = std::make_unique<LegacyRollbackWizard>(std::move(access));

    // The fixtures are development-signed, which a shipped build refuses. The
    // policy is set the way --dev-update-key sets it, because what is under
    // test here is the flow rather than the key policy.
    capture::UpdateKeyPolicy policy;
    policy.accept_development_key = true;
    wizard->SetKeyPolicy(policy);
  }

  QPushButton* Button(const char* name) const {
    return wizard->findChild<QPushButton*>(QLatin1String(name));
  }
  QLabel* Label(const char* name) const {
    return wizard->findChild<QLabel*>(QLatin1String(name));
  }
  QLineEdit* Field(const char* name) const {
    return wizard->findChild<QLineEdit*>(QLatin1String(name));
  }

  // Type the confirmation and step off the overview page.
  void Confirm() {
    Field(LegacyRollbackWizard::kConfirmFieldName)
        ->setText(RollbackConfirmWord());
    wizard->GoNext();
  }

  // Everything up to the first programming page.
  void ReachGatewarePage(const QString& file) {
    Confirm();
    wizard->Poll();
    wizard->GoNext();
    wizard->LoadRollbackSet(file);
    wizard->GoNext();
  }

  // Let a half run to completion. Both are worker threads, so the test has to
  // spin the event loop rather than assume anything about when they finish.
  void Wait() {
    for (int attempt = 0; attempt < 400 && wizard->busy(); ++attempt) {
      QTest::qWait(10);
    }
  }

  FakeDeviceUpdater updater;
  std::vector<capture::DeviceInfo> devices;
  std::unique_ptr<LegacyRollbackWizard> wizard;
};

// --- the ordering ---------------------------------------------------------
//
// The one property here that protects hardware rather than data. The original
// firmware and the current gateware drive one wire between the two boards, so
// the FPGA is always the first half to become legacy — and a wizard that laid
// its pages out the other way would be a wizard that asked for it.

TEST(LegacyRollbackWizardTest, TheFpgaPageComesBeforeTheFx3Page) {
  WizardUnderTest test;
  const std::vector<RollbackPage> steps = test.wizard->Steps();

  const auto gateware =
      std::find(steps.begin(), steps.end(), RollbackPage::kGateware);
  const auto firmware =
      std::find(steps.begin(), steps.end(), RollbackPage::kFirmware);

  ASSERT_NE(gateware, steps.end());
  ASSERT_NE(firmware, steps.end());
  EXPECT_LT(gateware, firmware)
      << "a rollback that wrote the firmware first would leave the original "
         "firmware driving the current gateware";

  // And the power cycle is after both, so the two halves change identity
  // together or not at all.
  const auto cycle =
      std::find(steps.begin(), steps.end(), RollbackPage::kPowerCycle);
  ASSERT_NE(cycle, steps.end());
  EXPECT_LT(firmware, cycle);
}

// The interface half of the same rule: the FX3 button is refused until the
// FPGA half has been done. The orchestrator refuses too — this is what stops a
// user being offered something that will be refused.
TEST(LegacyRollbackWizardTest, TheFirmwareButtonIsClosedUntilTheFpgaIsDone) {
  const RollbackFile file;
  WizardUnderTest test;

  test.ReachGatewarePage(file.path());
  ASSERT_EQ(test.wizard->page(), RollbackPage::kGateware);

  // Next is refused from here, so the firmware page cannot even be reached.
  test.wizard->GoNext();
  EXPECT_EQ(test.wizard->page(), RollbackPage::kGateware);

  test.wizard->StartGateware();
  test.Wait();
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), RollbackPage::kFirmware);

  EXPECT_TRUE(
      test.Button(LegacyRollbackWizard::kFirmwareStartButtonName)->isEnabled());
}

// --- the confirmation -----------------------------------------------------

TEST(LegacyRollbackWizardTest, TheFirstPageWillNotBePassedWithoutTyping) {
  WizardUnderTest test;

  test.wizard->GoNext();
  EXPECT_EQ(test.wizard->page(), RollbackPage::kOverview)
      << "the wizard walked past its own confirmation";

  test.Field(LegacyRollbackWizard::kConfirmFieldName)
      ->setText(QStringLiteral("yes"));
  test.wizard->GoNext();
  EXPECT_EQ(test.wizard->page(), RollbackPage::kOverview);

  test.Field(LegacyRollbackWizard::kConfirmFieldName)
      ->setText(RollbackConfirmWord());
  test.wizard->GoNext();
  EXPECT_EQ(test.wizard->page(), RollbackPage::kConnect);
}

// --- what it refuses ------------------------------------------------------

TEST(LegacyRollbackWizardTest, AnOrdinaryUpdateFileIsRefusedByName) {
  QTemporaryDir directory;
  const QString update = RollbackFile::UpdateBundlePath(directory);

  WizardUnderTest test;
  test.Confirm();
  test.wizard->Poll();
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), RollbackPage::kImage);

  test.wizard->LoadRollbackSet(update);

  const QString shown =
      test.Label(LegacyRollbackWizard::kImageLabelName)->text();
  EXPECT_TRUE(shown.contains("update file", Qt::CaseInsensitive))
      << shown.toStdString();

  test.wizard->GoNext();
  EXPECT_EQ(test.wizard->page(), RollbackPage::kImage)
      << "an ordinary update file was accepted as a rollback";
}

TEST(LegacyRollbackWizardTest, ADeviceThatIsAlreadyLegacyIsRefused) {
  WizardUnderTest test;
  test.devices = {LegacyDevice()};

  test.Confirm();
  test.wizard->Poll();

  const QString row = test.Label(LegacyRollbackWizard::kDeviceRowName)->text();
  EXPECT_TRUE(row.contains("Already running", Qt::CaseInsensitive))
      << row.toStdString();

  test.wizard->GoNext();
  EXPECT_EQ(test.wizard->page(), RollbackPage::kConnect);
}

TEST(LegacyRollbackWizardTest, ADeviceWhoseFpgaIsSilentIsRefused) {
  WizardUnderTest test;

  capture::DeviceIdentity identity = WorkingIdentity();
  identity.gateware_present = false;
  test.updater.SetIdentity(identity);

  test.Confirm();
  test.wizard->Poll();

  const QString row = test.Label(LegacyRollbackWizard::kDeviceRowName)->text();
  EXPECT_TRUE(row.contains("not answering", Qt::CaseInsensitive))
      << row.toStdString();

  test.wizard->GoNext();
  EXPECT_EQ(test.wizard->page(), RollbackPage::kConnect);
}

// Firmware that predates the factory target refuses it before a byte moves.
// The page has to say what the device said, because "update this device first"
// is the one useful thing to do about it.
TEST(LegacyRollbackWizardTest, FirmwareTooOldToRollBackIsReported) {
  const RollbackFile file;
  WizardUnderTest test;
  test.updater.RefuseTarget(capture::UpdateTarget::kEpcsFactory);

  test.ReachGatewarePage(file.path());
  test.wizard->StartGateware();
  test.Wait();

  const QString status =
      test.Label(LegacyRollbackWizard::kGatewareStatusName)->text();
  EXPECT_TRUE(status.contains("stopped", Qt::CaseInsensitive))
      << status.toStdString();

  test.wizard->GoNext();
  EXPECT_EQ(test.wizard->page(), RollbackPage::kGateware)
      << "the wizard walked on from a write that never happened";
}

// --- the whole flow -------------------------------------------------------

TEST(LegacyRollbackWizardTest, ProgramsBothHalvesAndWaitsForThePowerCycle) {
  const RollbackFile file;
  WizardUnderTest test;

  test.ReachGatewarePage(file.path());
  test.wizard->StartGateware();
  test.Wait();
  test.wizard->GoNext();

  ASSERT_EQ(test.wizard->page(), RollbackPage::kFirmware);
  test.wizard->StartFirmware();
  test.Wait();
  test.wizard->GoNext();

  ASSERT_EQ(test.wizard->page(), RollbackPage::kPowerCycle);

  // Both halves went to the right places, and nothing was restarted: the two
  // images become the running ones together, at the power cycle, or not at
  // all.
  EXPECT_EQ(test.updater.received(capture::UpdateTarget::kEpcsFactory).size(),
            capture::test::kFactoryGatewarePayload.size());
  EXPECT_FALSE(test.updater.received(capture::UpdateTarget::kFirmware).empty());
  EXPECT_EQ(test.updater.reset_count(), 0u);

  // Nothing has come back yet, so Next stays closed.
  test.wizard->Poll();
  test.wizard->GoNext();
  EXPECT_EQ(test.wizard->page(), RollbackPage::kPowerCycle);

  // And now it has, as a different device.
  test.devices = {LegacyDevice()};
  test.wizard->Poll();
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), RollbackPage::kVerify);

  const QString verdict =
      test.Label(LegacyRollbackWizard::kVerifySummaryName)->text();
  EXPECT_TRUE(verdict.contains("complete", Qt::CaseInsensitive))
      << verdict.toStdString();
}

// A power cycle that never happened leads with the reason it usually did not:
// the mini-USB cable keeps the assembly powered on its own.
TEST(LegacyRollbackWizardTest, APowerCycleThatNeverHappenedSaysWhy) {
  const RollbackFile file;
  WizardUnderTest test;

  test.ReachGatewarePage(file.path());
  test.wizard->StartGateware();
  test.Wait();
  test.wizard->GoNext();
  test.wizard->StartFirmware();
  test.Wait();
  test.wizard->GoNext();
  ASSERT_EQ(test.wizard->page(), RollbackPage::kPowerCycle);

  for (int poll = 0; poll < 25; ++poll) {
    test.wizard->Poll();
  }

  const QString status =
      test.Label(LegacyRollbackWizard::kPowerCycleStatusName)->text();
  EXPECT_TRUE(status.contains("mini-USB", Qt::CaseInsensitive))
      << status.toStdString();
}

}  // namespace
}  // namespace ddd::gui
