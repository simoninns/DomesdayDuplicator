/************************************************************************

    test_capture_controller.cpp

    T1 tests for the bridge between the GUI and the capture engine
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "capture_controller.h"
#include "fake_usb_device.h"
#include "firmware_version.h"
#include "synthetic_source.h"
#include "version.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

// Small enough that a test runs in milliseconds. The slot has to be at least
// one sequence-counter period (65,536 samples) or the validator can never lock
// on, so 256 KiB is the floor rather than a preference.
constexpr size_t kTestSlotBytes = size_t{256} << 10;
constexpr size_t kTestSlotCount = 6;

capture::SyntheticSource::Options TestSourceOptions() {
  capture::SyntheticSource::Options options;
  options.slot_size_bytes = kTestSlotBytes;
  options.slot_count = kTestSlotCount;
  return options;
}

// Pump the event loop until a condition holds, so a test fails with a message
// rather than hanging. Everything here is asynchronous by design — the monitor
// reports from its own thread and the pipeline stops on its own schedule — so
// there is nothing to block on.
template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 5000ms) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents();
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

class CaptureControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(QStringLiteral("ddd-gui-controller-%1")
                                             .arg(QLatin1String(info->name())));
    QSettings().clear();

    device_ = std::make_unique<capture::FakeUsbDevice>();
    device_->SetSourceOptions(TestSourceOptions());
    device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                             MatchingProductString());

    controller_ = std::make_unique<CaptureController>(device_.get(), nullptr);
  }

  void TearDown() override {
    controller_.reset();
    device_.reset();
    QSettings().clear();
  }

  // A product string naming this build's own commit, so the version check has
  // nothing to complain about unless a test asks it to.
  //
  // Built from the normalised commit rather than the raw version stamp: a
  // developer build reports "abcd1234-dirty", and no firmware would ever put
  // that in a descriptor.
  static std::string MatchingProductString() {
    const std::optional<std::string> commit =
        capture::NormaliseCommit(capture::Version());
    return "Domesday Duplicator (" + commit.value_or("a1b2c3d4") + ")";
  }

  // Settings that keep the ring small, so the pipeline starts instantly, and
  // that name a device so the path forwarded to the backend can be checked.
  void UseSmallQueue() {
    CaptureSettings settings = controller_->settings();
    settings.queue_size_bytes = capture::DiskBufferRing::kMinimumQueueSizeBytes;
    settings.preferred_device_path = QStringLiteral("bus-1");
    controller_->SetSettings(settings);
  }

  std::unique_ptr<capture::FakeUsbDevice> device_;
  std::unique_ptr<CaptureController> controller_;
};

TEST_F(CaptureControllerTest, AttachedDevicesReachTheGui) {
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();

  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 1; }));
  EXPECT_EQ(controller_->devices().size(), 1U);
  EXPECT_EQ(controller_->devices().front().path, "bus-1");
}

TEST_F(CaptureControllerTest, ADeviceUnpluggedIsReported) {
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 1; }));

  device_->SetDevices({});
  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 2; }));
  EXPECT_TRUE(controller_->devices().empty());
}

TEST_F(CaptureControllerTest, MatchingFirmwareSaysNothing) {
  QSignalSpy warnings(controller_.get(), &CaptureController::FirmwareWarning);
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();

  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 1; }));
  EXPECT_EQ(warnings.count(), 0);
}

TEST_F(CaptureControllerTest, DifferentFirmwareWarnsExactlyOncePerConnection) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (deadbe01)");

  QSignalSpy warnings(controller_.get(), &CaptureController::FirmwareWarning);
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();

  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 1; }));

  // Many polls go by; the warning must not repeat. A modal that reappeared five
  // times a second would be unusable, and one that reappeared occasionally
  // would be worse.
  PumpUntil([] { return false; }, 200ms);
  EXPECT_EQ(warnings.count(), 1);
}

// Re-plugging is what a user does after updating firmware, so it has to warn
// again rather than staying quiet because it warned about this path before.
TEST_F(CaptureControllerTest, ReplacingTheDeviceWarnsAgain) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (deadbe01)");

  QSignalSpy warnings(controller_.get(), &CaptureController::FirmwareWarning);
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return warnings.count() >= 1; }));

  device_->SetDevices({});
  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 2; }));

  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (deadbe02)");
  ASSERT_TRUE(PumpUntil([&] { return warnings.count() >= 2; }));
}

// --- Monitor mode --------------------------------------------------------

TEST_F(CaptureControllerTest, MonitoringOpensTheDeviceAndPublishesStatistics) {
  UseSmallQueue();

  QSignalSpy monitoring(controller_.get(),
                        &CaptureController::MonitoringChanged);
  QSignalSpy stats(controller_.get(), &CaptureController::StatsUpdated);

  controller_->StartMonitoring();

  ASSERT_TRUE(monitoring.count() >= 1)
      << "monitoring should start synchronously once the device opens";
  EXPECT_TRUE(controller_->monitoring());
  EXPECT_EQ(device_->open_count(), 1U);

  // The preference is forwarded rather than resolved here. Choosing between
  // attached devices belongs to the backend, which is the only thing that knows
  // what is attached; an empty preference means "whichever is there".
  EXPECT_EQ(device_->opened_path(), "bus-1");

  ASSERT_TRUE(PumpUntil([&] {
    return stats.count() > 0 &&
           qvariant_cast<ddd::capture::CaptureStats>(stats.back().at(0))
                   .buffers_processed > 0;
  }));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// Monitor mode writes nothing anywhere. That is the whole distinction between
// it and a capture, and the thing a user relies on when they leave it running
// while they set up a disc.
TEST_F(CaptureControllerTest, MonitoringWritesNothing) {
  UseSmallQueue();

  QSignalSpy stats(controller_.get(), &CaptureController::StatsUpdated);
  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  ASSERT_TRUE(PumpUntil([&] {
    return stats.count() > 0 &&
           qvariant_cast<ddd::capture::CaptureStats>(stats.back().at(0))
                   .buffers_processed > 2;
  }));

  const auto snapshot =
      qvariant_cast<ddd::capture::CaptureStats>(stats.back().at(0));
  EXPECT_EQ(snapshot.bytes_written, 0U);
  EXPECT_GT(snapshot.metrics.sample_count, 0U);

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureControllerTest, TestModeIsSentToTheDeviceBeforeItIsOpened) {
  UseSmallQueue();

  CaptureSettings settings = controller_->settings();
  settings.test_mode = true;
  controller_->SetSettings(settings);

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  EXPECT_EQ(device_->configuration_count(), 1U);
  EXPECT_TRUE(device_->configured_test_mode());
  EXPECT_EQ(device_->configured_path(), "bus-1");

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureControllerTest, TheTransferSettingsReachTheBackend) {
  UseSmallQueue();

  CaptureSettings settings = controller_->settings();
  settings.small_transfers = false;
  settings.transfer_queue_bytes = size_t{4} << 20;
  controller_->SetSettings(settings);

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  EXPECT_FALSE(device_->opened_options().small_transfers);
  EXPECT_EQ(device_->opened_options().transfer_queue_bytes, size_t{4} << 20);

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// Enumeration opens devices and does control transfers on them. While one is
// streaming that is avoidable traffic for an answer that is already obvious.
TEST_F(CaptureControllerTest, EnumerationPausesWhileStreaming) {
  UseSmallQueue();

  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 1; }));

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  PumpUntil([] { return false; }, 100ms);
  const uint64_t enumerations_while_streaming = device_->enumerate_count();
  PumpUntil([] { return false; }, 200ms);
  EXPECT_EQ(device_->enumerate_count(), enumerations_while_streaming);

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  // And it comes back afterwards, or a device pulled during a capture would
  // never be noticed.
  ASSERT_TRUE(PumpUntil([&] {
    return device_->enumerate_count() > enumerations_while_streaming;
  }));
}

// --- Failures ------------------------------------------------------------

TEST_F(CaptureControllerTest, ADeviceThatWillNotOpenIsReportedAndNotMonitored) {
  device_->SetOpenFails(true, capture::TransferResult::kConnectionFailure);

  QSignalSpy failures(controller_.get(), &CaptureController::Failed);
  controller_->StartMonitoring();

  EXPECT_FALSE(controller_->monitoring());
  ASSERT_EQ(failures.count(), 1);
  EXPECT_FALSE(failures.front().at(1).toString().isEmpty());
}

TEST_F(CaptureControllerTest, ADeviceThatRefusesConfigurationIsNotOpened) {
  device_->SetConfigurationFails(true);

  QSignalSpy failures(controller_.get(), &CaptureController::Failed);
  controller_->StartMonitoring();

  EXPECT_FALSE(controller_->monitoring());
  EXPECT_EQ(failures.count(), 1);
  EXPECT_EQ(device_->open_count(), 0U)
      << "a device that would not take its configuration must not be streamed "
         "from, or the mode it is in is unknown";
}

// The acceptance criterion from the plan: pulling the cable mid-monitor
// produces a clean, specific error and a state the application can recover from
// without being restarted.
//
// The cable is pulled by asking the synthetic source for a transfer failure,
// which is what a libusb backend reports when the device goes away.
TEST_F(CaptureControllerTest, ACablePulledMidMonitorIsRecoverable) {
  UseSmallQueue();

  capture::SyntheticSource::Options options = TestSourceOptions();
  options.fault = capture::SyntheticSource::Fault::kTransferFailure;
  options.fault_at_slot = 3;
  device_->SetSourceOptions(options);

  QSignalSpy failures(controller_.get(), &CaptureController::Failed);
  QSignalSpy monitoring(controller_.get(),
                        &CaptureController::MonitoringChanged);

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
  ASSERT_EQ(failures.count(), 1);

  // Specific, not generic: the title carries the result code so a user has
  // something to search for and a maintainer something to act on.
  EXPECT_TRUE(failures.front().at(0).toString().contains(
      QStringLiteral("usb-transfer-failure")))
      << failures.front().at(0).toString().toStdString();

  // And the application is in a state it can be driven from again. This is the
  // half of the criterion that is easy to get wrong: leaving the source open,
  // the monitor suspended or the pipeline half-torn-down would leave the
  // application looking fine and refusing to start.
  capture::SyntheticSource::Options healthy = TestSourceOptions();
  device_->SetSourceOptions(healthy);

  controller_->StartMonitoring();
  EXPECT_TRUE(controller_->monitoring());
  EXPECT_EQ(device_->open_count(), 2U);

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureControllerTest, ASequenceBreakStopsMonitoringAndSaysSo) {
  UseSmallQueue();

  capture::SyntheticSource::Options options = TestSourceOptions();
  options.fault = capture::SyntheticSource::Fault::kSequenceBreak;
  options.fault_at_slot = 3;
  device_->SetSourceOptions(options);

  QSignalSpy failures(controller_.get(), &CaptureController::Failed);
  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
  ASSERT_EQ(failures.count(), 1);
  EXPECT_TRUE(failures.front().at(0).toString().contains(
      QStringLiteral("sequence-mismatch")))
      << failures.front().at(0).toString().toStdString();
}

TEST_F(CaptureControllerTest, AGracefulStopIsNotReportedAsAFailure) {
  UseSmallQueue();

  QSignalSpy failures(controller_.get(), &CaptureController::Failed);
  QSignalSpy stats(controller_.get(), &CaptureController::StatsUpdated);

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  ASSERT_TRUE(PumpUntil([&] {
    return stats.count() > 0 &&
           qvariant_cast<ddd::capture::CaptureStats>(stats.back().at(0))
                   .buffers_processed > 0;
  }));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  EXPECT_EQ(failures.count(), 0);
}

TEST_F(CaptureControllerTest, StartingTwiceDoesNotOpenTheDeviceTwice) {
  UseSmallQueue();

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  controller_->StartMonitoring();
  EXPECT_EQ(device_->open_count(), 1U);

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureControllerTest, StoppingWhenNotMonitoringIsHarmless) {
  controller_->StopMonitoring();
  EXPECT_FALSE(controller_->monitoring());
}

TEST_F(CaptureControllerTest, NoBackendAtAllIsReportedRatherThanCrashing) {
  CaptureController controller(nullptr, nullptr);

  QSignalSpy failures(&controller, &CaptureController::Failed);
  controller.Start();

  EXPECT_EQ(failures.count(), 1);

  controller.StartMonitoring();
  EXPECT_FALSE(controller.monitoring());
}

}  // namespace
}  // namespace ddd::gui
