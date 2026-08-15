/************************************************************************

    test_update_page.cpp

    The update flow as a widget, driven end to end with no device
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <vector>

#include "fake_device_programmer.h"
#include "fake_device_updater.h"
#include "firmware_dialog.h"
#include "update_bundle.h"
#include "update_fixtures.h"
#include "update_key.h"
#include "update_page.h"

namespace ddd::gui {
namespace {

using capture::FakeDeviceUpdater;

// The whole point of this file: every state of the update flow, including
// every error and every rescue branch, is drivable here against a fake, on a
// machine with nothing plugged in. Half of these branches cannot be arranged
// on real hardware at all — "the device's flash readback did not match" is
// not something a bench can be asked for.

// A real, signed bundle on disk, assembled from the same fixtures the engine
// tests use. Written to a file rather than handed over as bytes, because the
// page's job includes reading one.
class BundleFile {
 public:
  BundleFile() {
    capture::UstarWriter writer;
    writer.AddFile(capture::kManifestEntryName,
                   capture::test::Bytes(capture::test::kManifestJson));
    writer.AddFile(capture::kSignatureEntryName,
                   capture::test::Bytes(capture::test::kManifestSignature));
    writer.AddFile("firmware.img",
                   capture::test::Bytes(capture::test::kFirmwarePayload));
    writer.AddFile("gateware-app.rpd",
                   capture::test::Bytes(capture::test::kGatewarePayload));

    const std::vector<uint8_t> archive = writer.Finish();

    path_ = directory_.filePath(QStringLiteral("update.dddfw"));
    QFile file(path_);
    if (file.open(QIODevice::WriteOnly)) {
      file.write(reinterpret_cast<const char*>(archive.data()),
                 static_cast<qint64>(archive.size()));
      file.close();
    }
  }

  const QString& path() const { return path_; }

 private:
  QTemporaryDir directory_;
  QString path_;
};

// The device the fixture bundle is for: firmware 0123abcd, gateware 0123abcd,
// register map 2 (the manifest's compatibility floor).
capture::DeviceIdentity FixtureDevice() {
  capture::DeviceIdentity identity;
  identity.product_string = "Domesday Duplicator (0123abcd)";
  identity.protocol_version = 1;
  identity.gateware_present = true;
  identity.register_map_version = 2;
  identity.image_role = capture::kImageRoleApplication;
  identity.gateware_commit = "0123abcd";
  return identity;
}

// The same device with its FPGA running the resident factory image: the
// firmware is fine, the register bank answers, and there is no capture path
// at all until the gateware is reinstalled.
capture::DeviceIdentity GatewareRecoveryDevice() {
  capture::DeviceIdentity identity = FixtureDevice();
  identity.image_role = capture::kImageRoleFactory;
  return identity;
}

// A non-owning IDeviceUpdater that forwards to a fake the test keeps.
//
// The page hands its worker a unique_ptr and the worker destroys it, so the
// fake itself cannot be what is handed over: a test that wanted to read what
// happened afterwards would be reading freed memory. This is the one line of
// indirection that makes "what did the device receive" answerable after the
// update has finished.
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

// A non-owning IDeviceProgrammer, for the same reason BorrowedUpdater is.
class BorrowedProgrammer : public capture::IDeviceProgrammer {
 public:
  explicit BorrowedProgrammer(capture::FakeDeviceProgrammer* fake)
      : fake_(fake) {}

  bool WriteRam(uint32_t address, std::span<const uint8_t> data) override {
    return fake_->WriteRam(address, data);
  }
  bool Start(uint32_t entry_address) override {
    return fake_->Start(entry_address);
  }
  std::optional<std::string> WaitForApplication(
      std::chrono::milliseconds timeout) override {
    return fake_->WaitForApplication(timeout);
  }

 private:
  capture::FakeDeviceProgrammer* fake_ = nullptr;
};

// A page, and the fakes it will hand to its worker. They outlive the page, so
// a test can read what happened after the update has finished.
struct PageUnderTest {
  std::unique_ptr<FakeDeviceUpdater> device =
      std::make_unique<FakeDeviceUpdater>();
  std::unique_ptr<capture::FakeDeviceProgrammer> programmer =
      std::make_unique<capture::FakeDeviceProgrammer>();
  std::unique_ptr<UpdatePage> page;

  explicit PageUnderTest(
      const QString& application_version = QStringLiteral("1.4.0"),
      capture::DevicePersonality personality =
          capture::DevicePersonality::kApplication,
      bool gateware_recovery = false) {
    // What the device reports once the update has finished with it, which is
    // an ordinary working unit in every case — including the one that
    // started in gateware recovery, because that is what repairing it means.
    device->SetIdentity(FixtureDevice());

    UpdatePage::Device seam;
    seam.attached = true;
    seam.personality = personality;

    // A device in recovery mode reports no identity, because there is nothing
    // running on it to report one.
    if (personality == capture::DevicePersonality::kApplication) {
      seam.identity =
          gateware_recovery ? GatewareRecoveryDevice() : FixtureDevice();
    }

    FakeDeviceUpdater* const raw = device.get();
    seam.open =
        [raw](const std::string&) -> std::unique_ptr<capture::IDeviceUpdater> {
      return std::make_unique<BorrowedUpdater>(raw);
    };

    capture::FakeDeviceProgrammer* const raw_programmer = programmer.get();
    seam.open_programmer =
        [raw_programmer]() -> std::unique_ptr<capture::IDeviceProgrammer> {
      return std::make_unique<BorrowedProgrammer>(raw_programmer);
    };

    page = std::make_unique<UpdatePage>(application_version, std::move(seam));

    // The fixture bundle is signed with the development key, and whether the
    // default policy accepts one depends on whether the build pinned a release
    // key — which is a property of how this was configured, not of the flow
    // under test. Set explicitly so these tests measure the same thing in
    // every build; AReleaseBuildRefusesADevelopmentBundle below is what covers
    // the default.
    capture::UpdateKeyPolicy policy;
    policy.accept_development_key = true;
    page->SetKeyPolicy(std::move(policy));
  }

  QLabel* Label(const char* name) const {
    return page->findChild<QLabel*>(QLatin1String(name));
  }
  QPushButton* Button(const char* name) const {
    return page->findChild<QPushButton*>(QLatin1String(name));
  }
  QProgressBar* Progress() const {
    return page->findChild<QProgressBar*>(
        QLatin1String(UpdatePage::kProgressBarName));
  }

  // Run the update and wait for it to finish. Returns false if it never did.
  bool RunToCompletion() {
    Button(UpdatePage::kInstallButtonName)->click();

    // The update runs on a worker thread and reports back through queued
    // signals, so the event loop has to be pumped for any of it to arrive.
    for (int attempt = 0; attempt < 2000 && page->busy(); ++attempt) {
      QTest::qWait(5);
    }
    return !page->busy();
  }
};

TEST(UpdatePageTest, ItOpensWithNothingChosenAndNothingToInstall) {
  const PageUnderTest test;

  ASSERT_NE(test.Label(UpdatePage::kVersionsLabelName), nullptr);
  ASSERT_NE(test.Button(UpdatePage::kInstallButtonName), nullptr);

  EXPECT_FALSE(test.Button(UpdatePage::kInstallButtonName)->isEnabled())
      << "the update button is offered before a file has been chosen";
  EXPECT_TRUE(test.Label(UpdatePage::kBundleLabelName)
                  ->text()
                  .contains(QStringLiteral("No update file")));
}

TEST(UpdatePageTest, TheVersionComparisonIsShownBeforeAnyFileIsChosen) {
  const PageUnderTest test;

  const QString versions = test.Label(UpdatePage::kVersionsLabelName)->text();
  EXPECT_TRUE(versions.contains(QStringLiteral("0123abcd")))
      << "the attached device's versions are not shown";
}

TEST(UpdatePageTest, AVerifiedBundleEnablesTheUpdateAndSaysSo) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());

  const QString summary = test.Label(UpdatePage::kBundleLabelName)->text();
  EXPECT_TRUE(summary.contains(QStringLiteral("1.4.0")));
  EXPECT_TRUE(summary.contains(QStringLiteral("verified")))
      << "nothing tells the user the file checked out";
  EXPECT_TRUE(summary.contains(QStringLiteral("plugged in")))
      << "the one instruction that matters is not stated before the start";

  EXPECT_TRUE(test.Button(UpdatePage::kInstallButtonName)->isEnabled());
}

// A development signature proves the file is well formed and proves nothing
// about where it came from. That is bannered every time, not just noted in
// the documentation.
TEST(UpdatePageTest, ADevelopmentBundleIsBanneredAsSuch) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());

  QLabel* const banner = test.Label(UpdatePage::kBannerLabelName);
  ASSERT_NE(banner, nullptr);
  EXPECT_TRUE(banner->isVisibleTo(test.page.get()));
  EXPECT_TRUE(banner->text().contains(QStringLiteral("development")));
}

// The other half of the same policy, and the half that is easy to lose: with a
// release key pinned, the default is the release key alone, and a bundle signed
// with the development key is refused rather than quietly installed. This build
// only pins a key when tools/keys/release.pub is present, so the assertion is
// conditional on that — the alternative would be a test that fails on every
// developer's machine or passes vacuously on CI, and neither says anything.
TEST(UpdatePageTest,
     TheDefaultPolicyRefusesADevelopmentBundleWhenAKeyIsPinned) {
  if (!capture::HasReleaseUpdateKey()) {
    GTEST_SKIP() << "this build pins no release key, so the development key is "
                    "the only one it could verify anything against";
  }

  const BundleFile bundle;
  PageUnderTest test;

  // Undo the fixture's opt-in: what is under test here is what a shipped
  // build does with a file it has no reason to trust.
  test.page->SetKeyPolicy(capture::DefaultUpdateKeyPolicy());
  test.page->LoadBundle(bundle.path());

  EXPECT_FALSE(test.Button(UpdatePage::kInstallButtonName)->isEnabled())
      << "a release build offered to install a development-signed bundle";
  EXPECT_FALSE(test.Label(UpdatePage::kBundleLabelName)->text().isEmpty())
      << "the refusal was silent";
}

TEST(UpdatePageTest, AFileThatIsNotABundleIsRefusedWithAReason) {
  QTemporaryDir directory;
  const QString path = directory.filePath(QStringLiteral("rubbish.dddfw"));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write("this is not an archive");
  file.close();

  PageUnderTest test;
  test.page->LoadBundle(path);

  EXPECT_FALSE(test.Button(UpdatePage::kInstallButtonName)->isEnabled());
  EXPECT_FALSE(test.Label(UpdatePage::kBundleLabelName)->text().isEmpty());
}

TEST(UpdatePageTest, AFileThatIsNotThereIsRefusedWithAReason) {
  PageUnderTest test;
  test.page->LoadBundle(QStringLiteral("/nonexistent/update.dddfw"));

  EXPECT_FALSE(test.Button(UpdatePage::kInstallButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(UpdatePage::kBundleLabelName)
                  ->text()
                  .contains(QStringLiteral("could not be read")));
}

// The rule the compatibility gate exists for, seen from the interface: a
// user cannot drive the device past what the application driving it
// understands, and the button is what enforces it.
TEST(UpdatePageTest, ABundleNeedingANewerApplicationDisablesTheButton) {
  const BundleFile bundle;

  // The fixture manifest requires application 1.4.0.
  PageUnderTest test(QStringLiteral("1.0.0"));
  test.page->LoadBundle(bundle.path());

  EXPECT_FALSE(test.Button(UpdatePage::kInstallButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(UpdatePage::kBundleLabelName)
                  ->text()
                  .contains(QStringLiteral("Update the application first")));
}

TEST(UpdatePageTest, ASuccessfulUpdateReportsWhatTheDeviceNowRuns) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  const QString status = test.Label(UpdatePage::kStatusLabelName)->text();
  EXPECT_TRUE(status.contains(QStringLiteral("complete")))
      << "status was: " << status.toStdString();
  EXPECT_TRUE(status.contains(QStringLiteral("0123abcd")))
      << "the confirmation does not quote what the device reports";

  EXPECT_EQ(test.Progress()->value(), 100);
  EXPECT_FALSE(test.page->busy());
}

TEST(UpdatePageTest, ASuccessfulUpdateSendsTheBundleAndRestartsTheDevice) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  EXPECT_EQ(test.device->begin_count(), 2u)
      << "both components should have been installed";
  EXPECT_EQ(test.device->reset_count(), 1u);
  EXPECT_EQ(test.device->reconfigure_count(), 1u);
}

// --- The failure branches --------------------------------------------------

TEST(UpdatePageTest, ADeviceThatIsCapturingIsReportedCalmly) {
  const BundleFile bundle;
  PageUnderTest test;

  test.device->SetFault(FakeDeviceUpdater::Fault::kRefuseBegin);
  test.device->SetFailureError(capture::DeviceUpdateError::kBusy);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  const QString status = test.Label(UpdatePage::kStatusLabelName)->text();
  EXPECT_TRUE(status.contains(QStringLiteral("capturing")));

  // Every failure answers "is my device broken", because that is the question
  // the user is actually asking.
  EXPECT_TRUE(status.contains(QStringLiteral("not damaged")));
}

TEST(UpdatePageTest, AFailedUpdateOffersToTryAgainWithoutChoosingTheFileAgain) {
  const BundleFile bundle;
  PageUnderTest test;

  test.device->SetFault(FakeDeviceUpdater::Fault::kFailDuringWrite);
  test.device->SetFailureError(capture::DeviceUpdateError::kWrite);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  QPushButton* const install = test.Button(UpdatePage::kInstallButtonName);
  EXPECT_EQ(install->text(), QStringLiteral("Try again"));
  EXPECT_TRUE(install->isEnabled());
}

TEST(UpdatePageTest, ACorruptedTransferIsCaughtBeforeAnythingIsCommitted) {
  const BundleFile bundle;
  PageUnderTest test;

  test.device->SetFault(FakeDeviceUpdater::Fault::kRefuseChunk);
  test.device->SetFailAtChunk(0);
  test.device->SetFailureError(capture::DeviceUpdateError::kStreamDigest);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  EXPECT_EQ(test.device->reset_count(), 0u)
      << "the device was restarted after a failed transfer";
  EXPECT_TRUE(test.Label(UpdatePage::kStatusLabelName)
                  ->text()
                  .contains(QStringLiteral("did not arrive")));
}

TEST(UpdatePageTest, ADeviceThatNeverComesBackIsReportedWithANextStep) {
  const BundleFile bundle;
  PageUnderTest test;

  test.device->SetFault(FakeDeviceUpdater::Fault::kNeverReturns);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  const QString status = test.Label(UpdatePage::kStatusLabelName)->text();
  EXPECT_TRUE(status.contains(QStringLiteral("Unplug it")));
}

TEST(UpdatePageTest, TheWrongBuildComingBackIsNotCalledASuccess) {
  const BundleFile bundle;
  PageUnderTest test;

  test.device->SetFault(FakeDeviceUpdater::Fault::kWrongIdentityAfterUpdate);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  const QString status = test.Label(UpdatePage::kStatusLabelName)->text();
  EXPECT_FALSE(status.contains(QStringLiteral("Update complete")));
  EXPECT_TRUE(status.contains(QStringLiteral("deadbeef")));
}

// --- A device with no firmware ---------------------------------------------
//
// The state every kit arrives in, and the state an interrupted update leaves
// behind. They are indistinguishable on the wire, so the page says what is
// true of both and offers the one action that fixes either.

TEST(UpdatePageRecoveryTest, ADeviceWithNoFirmwareIsNamedAndExplained) {
  const PageUnderTest test(QStringLiteral("1.4.0"),
                           capture::DevicePersonality::kRecovery);

  const QString versions = test.Label(UpdatePage::kVersionsLabelName)->text();

  EXPECT_TRUE(versions.contains(QStringLiteral("recovery mode")))
      << "the device's state is not named: " << versions.toStdString();
  EXPECT_TRUE(versions.contains(QStringLiteral("never been programmed")))
      << "a user with a newly built board is not told this is normal";
  EXPECT_TRUE(versions.contains(QStringLiteral("not damaged")))
      << "the question the user is actually asking goes unanswered";
  EXPECT_TRUE(versions.contains(QStringLiteral("None installed")))
      << "the firmware row does not say there is no firmware";
}

// "Program", not "repair": somebody holding a board they have just built has
// not broken anything, and the application cannot tell the two cases apart.
TEST(UpdatePageRecoveryTest, TheButtonOffersToProgramRatherThanToRepair) {
  const PageUnderTest test(QStringLiteral("1.4.0"),
                           capture::DevicePersonality::kRecovery);

  EXPECT_EQ(test.Button(UpdatePage::kInstallButtonName)->text(),
            QStringLiteral("Program this device"));
}

TEST(UpdatePageRecoveryTest, AVerifiedBundleEnablesProgrammingARecoveryDevice) {
  const BundleFile bundle;
  PageUnderTest test(QStringLiteral("1.4.0"),
                     capture::DevicePersonality::kRecovery);

  test.page->LoadBundle(bundle.path());

  EXPECT_TRUE(test.Button(UpdatePage::kInstallButtonName)->isEnabled())
      << "a verified bundle cannot be installed on a device that needs it";
}

// The fixture bundle's firmware payload is text rather than an FX3 image, so
// this drives the whole recovery flow as far as the parser and stops there —
// which is the check that a bundle carrying something that is not firmware
// never reaches a device's memory.
TEST(UpdatePageRecoveryTest, APayloadThatIsNotFirmwareNeverReachesTheDevice) {
  const BundleFile bundle;
  PageUnderTest test(QStringLiteral("1.4.0"),
                     capture::DevicePersonality::kRecovery);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  EXPECT_EQ(test.programmer->sections_written(), 0u);
  EXPECT_FALSE(test.programmer->started());

  const QString status = test.Label(UpdatePage::kStatusLabelName)->text();
  EXPECT_TRUE(status.contains(QStringLiteral("not damaged")))
      << "status was: " << status.toStdString();
}

TEST(UpdatePageRecoveryTest, ADeviceRunningAProgrammingToolSaysHowToClearIt) {
  const PageUnderTest test(QStringLiteral("1.4.0"),
                           capture::DevicePersonality::kFlashProgrammer);

  const QString versions = test.Label(UpdatePage::kVersionsLabelName)->text();
  EXPECT_TRUE(versions.contains(QStringLiteral("Unplug it")))
      << "no way out is offered: " << versions.toStdString();
}

// --- The dialog around it --------------------------------------------------

// --- A device whose FPGA is running its recovery gateware -------------------
//
// A different state from a device with no firmware, and one the application
// can name exactly rather than guess at: the FPGA says which of its two
// images answered. The unit works, enumerates and cannot capture.

TEST(UpdatePageGatewareRecoveryTest, TheStateIsNamedAndExplained) {
  const PageUnderTest test(QStringLiteral("1.4.0"),
                           capture::DevicePersonality::kApplication, true);

  const QString versions = test.Label(UpdatePage::kVersionsLabelName)->text();

  EXPECT_TRUE(versions.contains(QStringLiteral("recovery gateware"),
                                Qt::CaseInsensitive))
      << "the device's state is not named: " << versions.toStdString();
  EXPECT_TRUE(versions.contains(QStringLiteral("not damaged")))
      << "the question the user is actually asking goes unanswered";
  EXPECT_TRUE(versions.contains(QStringLiteral("Recovery gateware")))
      << "the gateware row does not say what is installed";
}

// One calm button, and the verb is the true one: the firmware is fine and
// what is wanted is the half whose last update did not finish.
TEST(UpdatePageGatewareRecoveryTest, TheButtonOffersToReinstallTheGateware) {
  const PageUnderTest test(QStringLiteral("1.4.0"),
                           capture::DevicePersonality::kApplication, true);

  EXPECT_EQ(test.Button(UpdatePage::kInstallButtonName)->text(),
            QStringLiteral("Reinstall gateware"));
}

// The repair is an ordinary update: the same bundle, the same protocol, the
// same commit ordering. Nothing about this path is special-cased, which is
// what makes it the path that has been exercised every time.
TEST(UpdatePageGatewareRecoveryTest, TheRepairIsAnOrdinaryUpdate) {
  const BundleFile bundle;
  PageUnderTest test(QStringLiteral("1.4.0"),
                     capture::DevicePersonality::kApplication, true);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.Button(UpdatePage::kInstallButtonName)->isEnabled())
      << "a unit in gateware recovery is not offered the update that repairs "
         "it";

  ASSERT_TRUE(test.RunToCompletion());

  const std::span<const uint8_t> expected =
      capture::test::Bytes(capture::test::kGatewarePayload);
  EXPECT_EQ(test.device->received(capture::UpdateTarget::kGateware),
            std::vector<uint8_t>(expected.begin(), expected.end()));
  EXPECT_EQ(test.device->reconfigure_count(), 1u);

  // And the device that came back is described by what it now reports, so
  // the state it was in is gone from the screen as well as from the flash.
  const QString versions = test.Label(UpdatePage::kVersionsLabelName)->text();
  EXPECT_FALSE(versions.contains(QStringLiteral("recovery gateware"),
                                 Qt::CaseInsensitive))
      << "the repaired device is still described as being in recovery";
  EXPECT_EQ(test.Button(UpdatePage::kInstallButtonName)->text(),
            QStringLiteral("Update"));
}

TEST(FirmwareDialogUpdateTest, TheDialogGainsAnUpdatePageWhenGivenADevice) {
  UpdatePage::Device seam;
  seam.attached = false;

  FirmwareVersions versions;
  versions.application = QStringLiteral("1.4.0");

  FirmwareDialog dialog(versions, std::move(seam));

  EXPECT_NE(dialog.update_page(), nullptr);

  // And still shows the versions it always did.
  auto* const text =
      dialog.findChild<QLabel*>(QLatin1String(FirmwareDialog::kTextLabelName));
  ASSERT_NE(text, nullptr);
  EXPECT_TRUE(text->text().contains(QStringLiteral("1.4.0")));
}

TEST(FirmwareDialogUpdateTest, WithoutADeviceThereIsNoUpdatePage) {
  FirmwareVersions versions;
  versions.application = QStringLiteral("1.4.0");

  FirmwareDialog dialog(versions);

  EXPECT_EQ(dialog.update_page(), nullptr);
}

}  // namespace
}  // namespace ddd::gui
