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
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "fake_device_programmer.h"
#include "fake_device_updater.h"
#include "firmware_dialog.h"
#include "logger.h"
#include "update_bundle.h"
#include "update_fixtures.h"
#include "update_key.h"
#include "update_page.h"
#include "update_step_list.h"

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

  // Records arrive from the worker thread as well as this one, so the
  // collection has a lock of its own: the logger serialises its callbacks, and
  // what it cannot do is stop a test reading the vector while the update
  // thread appends to it.
  mutable std::mutex log_mutex;
  std::vector<std::string> log;
  capture::CallbackLogger logger{
      [this](capture::LogLevel /*level*/, const std::string& message) {
        const std::lock_guard<std::mutex> guard(log_mutex);
        log.push_back(message);
      },
      capture::LogLevel::kDebug};

  bool LogContains(const std::string& fragment) const {
    const std::lock_guard<std::mutex> guard(log_mutex);
    for (const std::string& line : log) {
      if (line.find(fragment) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  // Declared after the logger so that it is destroyed before it: the page
  // holds the pointer for as long as it exists.
  std::unique_ptr<UpdatePage> page;

  explicit PageUnderTest(
      const QString& application_commit = QStringLiteral("0123abcd"),
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

    // Given a logger, so that every test here also runs the page's mirroring
    // of its own rolling log into the application's. A line that only exists
    // on the path nobody tests is a line that can crash a release.
    seam.logger = &logger;

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

    page = std::make_unique<UpdatePage>(application_commit, std::move(seam));

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
  UpdateStepList* Steps() const {
    return page->findChild<UpdateStepList*>(
        QLatin1String(UpdatePage::kStepListName));
  }
  QPlainTextEdit* Log() const {
    return page->findChild<QPlainTextEdit*>(
        QLatin1String(UpdatePage::kLogViewName));
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

  // And the other build that legitimately accepts it: a debug build implies
  // the opt-in, so that a developer does not type --dev-update-key on every
  // run. That is DefaultUpdateKeyPolicy's own decision and this test is about
  // what a shipped build does, so it is asked of shipped builds only — which
  // is what "default" means here.
  if (capture::DefaultUpdateKeyPolicy().accept_development_key) {
    GTEST_SKIP() << "this is a debug build, whose default policy accepts the "
                    "development key deliberately";
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

// The compatibility gate seen from the interface: a verdict the gate refuses
// leaves the button disabled and puts the gate's own reason on screen, rather
// than letting a user press install and find out.
//
// Driven here through a legacy device, which the gate refuses because nothing
// this application sends it would be received. This used to be driven through
// the manifest's minimum application version — the page passed its own dotted
// release version to the gate, and a lower one was refused. That comparison is
// gone: the application stamps a commit, and what stops a user driving a device
// past what this build understands is the protocol and register map range,
// checked in test_update_gate.cpp. The manifest here is signed, so its
// interface versions cannot be varied to reach that path from a widget test.
TEST(UpdatePageTest, ADeviceTheGateRefusesDisablesTheButtonAndSaysWhy) {
  const BundleFile bundle;

  PageUnderTest test(QStringLiteral("0123abcd"),
                     capture::DevicePersonality::kLegacy);
  test.page->LoadBundle(bundle.path());

  EXPECT_FALSE(test.Button(UpdatePage::kInstallButtonName)->isEnabled());
  EXPECT_TRUE(test.Label(UpdatePage::kBundleLabelName)
                  ->text()
                  .contains(QStringLiteral("original Duplicator firmware")))
      << test.Label(UpdatePage::kBundleLabelName)->text().toStdString();
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

// --- The steps, the one bar, and the details window ------------------------
//
// The shape of the flow, as a user meets it: the whole procedure listed and
// greyed before it starts, one bar that fills once across all of it, the step
// in hand picked out, and every line the engine produced kept behind a button
// for whoever wants it.

// The list is on screen before the button is pressed, because what a user
// needs in order to decide whether to start an update is what starting it
// commits them to.
TEST(UpdatePageStepsTest, TheStepsAreListedAndGreyedBeforeTheUpdateStarts) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());

  UpdateStepList* const steps = test.Steps();
  ASSERT_NE(steps, nullptr);

  // The fixture bundle carries both halves: check, firmware, gateware,
  // restart, confirm.
  ASSERT_EQ(steps->count(), 5);
  EXPECT_TRUE(steps->TitleAt(1).contains(QStringLiteral("firmware")));
  EXPECT_TRUE(steps->TitleAt(2).contains(QStringLiteral("gateware")));

  for (int step = 0; step < steps->count(); ++step) {
    EXPECT_EQ(steps->StateAt(step), UpdateStepList::State::kPending)
        << "step " << step << " is not greyed before anything has happened";
  }
}

TEST(UpdatePageStepsTest, ThereIsNothingToListUntilAFileIsChosen) {
  const PageUnderTest test;

  ASSERT_NE(test.Steps(), nullptr);
  EXPECT_EQ(test.Steps()->count(), 0);
}

// A file that turns out to be unreadable must not leave the previous file's
// plan on the screen, promising steps that will never run.
TEST(UpdatePageStepsTest, AFileThatCannotBeUsedClearsThePlan) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());
  ASSERT_GT(test.Steps()->count(), 0);

  test.page->LoadBundle(QStringLiteral("/nonexistent/update.dddfw"));
  EXPECT_EQ(test.Steps()->count(), 0);
}

// Pressing the button has a visible effect on the frame after it is pressed,
// rather than whenever a worker thread gets round to its first report.
TEST(UpdatePageStepsTest, TheFirstStepIsLitAsSoonAsTheButtonIsPressed) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());
  test.Button(UpdatePage::kInstallButtonName)->click();

  // Read before the event loop is pumped, so nothing the worker has sent can
  // have arrived: this is what the button itself did.
  EXPECT_EQ(test.Steps()->StateAt(0), UpdateStepList::State::kActive);
  EXPECT_EQ(test.Steps()->StateAt(1), UpdateStepList::State::kPending);

  for (int attempt = 0; attempt < 2000 && test.page->busy(); ++attempt) {
    QTest::qWait(5);
  }
}

TEST(UpdatePageStepsTest, EveryStepIsTickedWhenTheUpdateFinishes) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  for (int step = 0; step < test.Steps()->count(); ++step) {
    EXPECT_EQ(test.Steps()->StateAt(step), UpdateStepList::State::kDone)
        << "step " << step << " is not ticked after a successful update";
  }

  EXPECT_EQ(test.Progress()->value(), 100);
}

// The distinction the list exists to make on a failure: "the firmware went in
// and the gateware did not" is a different situation from "nothing happened",
// and the ticks are where a user can see which of them they are in.
TEST(UpdatePageStepsTest,
     TheStepItStoppedOnIsMarkedAndTheEarlierOnesKeepTicks) {
  const BundleFile bundle;
  PageUnderTest test;

  // Refused at the first chunk of the firmware, which is step 2 of the five.
  test.device->SetFault(FakeDeviceUpdater::Fault::kRefuseChunk);
  test.device->SetFailAtChunk(0);
  test.device->SetFailureError(capture::DeviceUpdateError::kStreamDigest);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  EXPECT_EQ(test.Steps()->StateAt(0), UpdateStepList::State::kDone)
      << "the check that did pass lost its tick";
  EXPECT_EQ(test.Steps()->StateAt(1), UpdateStepList::State::kFailed);
  EXPECT_EQ(test.Steps()->StateAt(2), UpdateStepList::State::kPending)
      << "a step that never ran is shown as having been tried";
}

// One bar over the whole update, and it only ever fills. The engine's byte
// counts restart from zero when it moves on to the second component, and a
// bar that slipped back there would be read as an update going wrong.
TEST(UpdatePageStepsTest, TheOneBarOnlyEverMovesForwards) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());

  std::vector<int> values;
  QObject::connect(test.Progress(), &QProgressBar::valueChanged,
                   test.Progress(),
                   [&values](int value) { values.push_back(value); });

  ASSERT_TRUE(test.RunToCompletion());

  ASSERT_FALSE(values.empty()) << "the bar never moved at all";
  for (size_t index = 1; index < values.size(); ++index) {
    EXPECT_GE(values[index], values[index - 1])
        << "the bar went backwards at report " << index;
  }

  // And it is a real proportion throughout rather than a busy indicator with
  // no numbers on it.
  EXPECT_EQ(test.Progress()->minimum(), 0);
  EXPECT_EQ(test.Progress()->maximum(), 100);
}

// The details window is for the user who wants it and out of the way of the
// user who does not.
TEST(UpdatePageStepsTest,
     TheDetailsWindowIsClosedAndUnofferedUntilSomethingRuns) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());

  QPushButton* const details = test.Button(UpdatePage::kDetailsButtonName);
  ASSERT_NE(details, nullptr);
  EXPECT_FALSE(details->isVisibleTo(test.page.get()))
      << "a details button is offered when there is nothing to detail";
  EXPECT_FALSE(test.Log()->isVisibleTo(test.page.get()));
}

TEST(UpdatePageStepsTest, TheDetailsWindowHoldsWhatTheEngineReported) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  QPushButton* const details = test.Button(UpdatePage::kDetailsButtonName);
  ASSERT_NE(details, nullptr);
  EXPECT_TRUE(details->isVisibleTo(test.page.get()));

  details->click();
  EXPECT_TRUE(test.Log()->isVisibleTo(test.page.get()));
  EXPECT_EQ(details->text(), QStringLiteral("Hide details"));

  const QString log = test.Log()->toPlainText();
  EXPECT_TRUE(log.contains(QStringLiteral("Update started")));
  EXPECT_TRUE(log.contains(QStringLiteral("firmware")))
      << "the log does not name what was installed: " << log.toStdString();
  EXPECT_TRUE(log.contains(QStringLiteral("gateware")));

  // Rolling, not one line per chunk: a transfer reports per chunk and a log
  // that scrolled at that rate would be unreadable, which is the same as not
  // being there.
  EXPECT_LT(log.count(QLatin1Char('\n')), 40)
      << "the log is one line per report rather than one per phase";
}

// The details window closes with the dialog; the application's log does not.
// Everything in one has to be in the other, or a fault report carries the
// version table and nothing about what actually happened.
TEST(UpdatePageStepsTest, EverythingInTheDetailsWindowIsAlsoInTheLog) {
  const BundleFile bundle;
  PageUnderTest test;

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  // The page's own account.
  EXPECT_TRUE(test.LogContains("Update page: Update started"));
  EXPECT_TRUE(test.LogContains("Update file verified: version"));
  EXPECT_TRUE(test.LogContains("Compatibility gate: allowed"));

  // And the engine's, through the same logger — which is the change that
  // makes an update investigable at all: before it, the orchestrator was
  // given no logger and its every word was discarded.
  EXPECT_TRUE(test.LogContains("Update starting: bundle version"));
  EXPECT_TRUE(test.LogContains("Installing firmware"));
  EXPECT_TRUE(test.LogContains("Update finished after"));
}

TEST(UpdatePageStepsTest, AFailedUpdateSaysWhyInTheLogAsWellAsOnThePage) {
  const BundleFile bundle;
  PageUnderTest test;

  test.device->SetFault(FakeDeviceUpdater::Fault::kFailDuringWrite);
  test.device->SetFailureError(capture::DeviceUpdateError::kWrite);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  EXPECT_TRUE(test.LogContains("Failed to install"));
  EXPECT_TRUE(test.LogContains("Device status at the failure"));
  EXPECT_TRUE(test.LogContains("failed at stage"));
}

// A failure is the case the details window exists for, so the reason has to
// be in it and not only in the label above.
TEST(UpdatePageStepsTest, AFailureIsWrittenIntoTheDetailsWindow) {
  const BundleFile bundle;
  PageUnderTest test;

  test.device->SetFault(FakeDeviceUpdater::Fault::kFailDuringWrite);
  test.device->SetFailureError(capture::DeviceUpdateError::kWrite);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());

  EXPECT_FALSE(test.Log()->toPlainText().isEmpty());
  EXPECT_TRUE(
      test.Log()->toPlainText().contains(QStringLiteral("Update started")));
}

// "Try again" runs the same plan from the beginning, so the list has to start
// again with it rather than keeping the marks from the attempt that failed.
TEST(UpdatePageStepsTest, TryingAgainStartsTheListOver) {
  const BundleFile bundle;
  PageUnderTest test;

  test.device->SetFault(FakeDeviceUpdater::Fault::kRefuseChunk);
  test.device->SetFailAtChunk(0);
  test.device->SetFailureError(capture::DeviceUpdateError::kStreamDigest);

  test.page->LoadBundle(bundle.path());
  ASSERT_TRUE(test.RunToCompletion());
  ASSERT_EQ(test.Steps()->StateAt(1), UpdateStepList::State::kFailed);

  test.Button(UpdatePage::kInstallButtonName)->click();

  EXPECT_EQ(test.Steps()->StateAt(0), UpdateStepList::State::kActive);
  EXPECT_EQ(test.Steps()->StateAt(1), UpdateStepList::State::kPending)
      << "the second attempt inherited the first one's cross";
  EXPECT_EQ(test.Progress()->value(), 0);

  for (int attempt = 0; attempt < 2000 && test.page->busy(); ++attempt) {
    QTest::qWait(5);
  }
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
