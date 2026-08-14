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

#include "fake_device_updater.h"
#include "firmware_dialog.h"
#include "update_bundle.h"
#include "update_fixtures.h"
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
  identity.gateware_commit = "0123abcd";
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

// A page, and the fake it will hand to its worker. The fake outlives the
// page, so a test can read what happened after the update has finished.
struct PageUnderTest {
  std::unique_ptr<FakeDeviceUpdater> device =
      std::make_unique<FakeDeviceUpdater>();
  std::unique_ptr<UpdatePage> page;

  explicit PageUnderTest(
      const QString& application_version = QStringLiteral("1.4.0")) {
    device->SetIdentity(FixtureDevice());

    UpdatePage::Device seam;
    seam.attached = true;
    seam.identity = FixtureDevice();

    FakeDeviceUpdater* const raw = device.get();
    seam.open = [raw]() -> std::unique_ptr<capture::IDeviceUpdater> {
      return std::make_unique<BorrowedUpdater>(raw);
    };

    page = std::make_unique<UpdatePage>(application_version, std::move(seam));
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

// --- The dialog around it --------------------------------------------------

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
