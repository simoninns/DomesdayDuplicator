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
#include "capture_format.h"
#include "fake_usb_device.h"
#include "firmware_version.h"
#include "synthetic_source.h"
#include "version.h"
#include "wire_protocol.h"

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
                             ProductStringNamingACommit());

    controller_ = std::make_unique<CaptureController>(device_.get(), nullptr);
  }

  void TearDown() override {
    controller_.reset();
    device_.reset();
    QSettings().clear();
  }

  // A product string naming a commit, so there is nothing to warn about unless
  // a test asks for it.
  //
  // Any commit will do. This used to have to be built from the application's
  // own stamp, because the two were compared and a difference raised a
  // warning; they come from separate release streams and are not compared any
  // more, which is what makes a literal correct here.
  static std::string ProductStringNamingACommit() {
    return "Domesday Duplicator (a1b2c3d4)";
  }

  // And one that names none, which is the only state left that warns: a device
  // that could not be opened to be asked, or firmware older than the stamp.
  static std::string ProductStringNamingNoCommit() {
    return "Domesday Duplicator";
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

TEST_F(CaptureControllerTest, AFirmwareThatNamesItsBuildSaysNothing) {
  QSignalSpy warnings(controller_.get(), &CaptureController::FirmwareWarning);
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();

  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 1; }));
  EXPECT_EQ(warnings.count(), 0);
}

// A device from another release is not warned about at all any more: the
// application and the firmware come from separate streams, so a different
// commit is the ordinary state of an up-to-date Duplicator.
TEST_F(CaptureControllerTest, FirmwareFromAnotherReleaseIsNotWarnedAbout) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (deadbe01)");

  QSignalSpy warnings(controller_.get(), &CaptureController::FirmwareWarning);
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();

  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 1; }));
  PumpUntil([] { return false; }, 200ms);
  EXPECT_EQ(warnings.count(), 0);
}

TEST_F(CaptureControllerTest, ASilentFirmwareWarnsExactlyOncePerConnection) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           ProductStringNamingNoCommit());

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

// Re-plugging is what a user does after installing udev rules, so it has to
// warn again rather than staying quiet because it warned about this path
// before.
TEST_F(CaptureControllerTest, ReplacingTheDeviceWarnsAgain) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           ProductStringNamingNoCommit());

  QSignalSpy warnings(controller_.get(), &CaptureController::FirmwareWarning);
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return warnings.count() >= 1; }));

  device_->SetDevices({});
  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 2; }));

  device_->SetSingleDevice("bus-2", capture::DeviceSpeed::kSuper,
                           ProductStringNamingNoCommit());
  ASSERT_TRUE(PumpUntil([&] { return warnings.count() >= 2; }));
}

// The gateware version is read when a device appears, not when somebody opens
// the Firmware dialog, so that showing it costs nothing and cannot block the
// window on a control transfer.
TEST_F(CaptureControllerTest, TheGatewareVersionIsReadWhenADeviceAppears) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (deadbe01)");
  device_->SetGatewareCommit("0123abcd");

  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 1; }));

  EXPECT_TRUE(controller_->fpga_version().present);
  EXPECT_EQ(controller_->fpga_version().commit, "0123abcd");

  // Read once per device, not once per poll. The read opens the device and
  // does a control transfer, which is not something to do five times a second.
  const uint64_t reads = device_->register_read_count();
  PumpUntil([] { return false; }, 200ms);
  EXPECT_EQ(device_->register_read_count(), reads);
}

// A device whose FPGA never answered is an ordinary state, not a failure: the
// application has to monitor and capture exactly as it otherwise would.
TEST_F(CaptureControllerTest, ASilentGatewareLeavesTheVersionUnknown) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (deadbe01)");
  device_->SetGatewareUnavailable();

  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return devices.count() >= 1; }));

  EXPECT_FALSE(controller_->fpga_version().present);
  EXPECT_TRUE(controller_->fpga_version().commit.empty());
}

// Unplugging must clear it, or the dialog would report the gateware of a device
// that is no longer there.
TEST_F(CaptureControllerTest, RemovingTheDeviceForgetsItsGatewareVersion) {
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (deadbe01)");
  device_->SetGatewareCommit("0123abcd");

  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  controller_->Start();
  ASSERT_TRUE(PumpUntil([&] { return controller_->fpga_version().present; }));

  device_->SetDevices({});
  ASSERT_TRUE(PumpUntil([&] { return !controller_->fpga_version().present; }));
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

// The device has to be told a capture is running, and told again when it
// stops. Not a formality: the firmware holds the USB link out of U1/U2 for
// exactly as long as this says a capture is running, and a link that drops
// into U2 mid-capture loses samples inside a transfer the host still sees as
// complete. The gateware ignores the matching GPIO, so nothing about the
// stream itself fails without this - it just quietly stops being bit-perfect
// on any host whose USB 3 stack enables link power management.
TEST_F(CaptureControllerTest, MonitoringTellsTheDeviceACaptureIsRunning) {
  UseSmallQueue();

  QSignalSpy stats(controller_.get(), &CaptureController::StatsUpdated);
  controller_->StartMonitoring();

  ASSERT_TRUE(controller_->monitoring());
  EXPECT_TRUE(device_->collecting())
      << "the device should be told before the stream is opened";
  EXPECT_EQ(device_->collection_change_count(), 1U);

  ASSERT_TRUE(PumpUntil([&] {
    return stats.count() > 0 &&
           qvariant_cast<ddd::capture::CaptureStats>(stats.back().at(0))
                   .buffers_processed > 0;
  }));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  EXPECT_FALSE(device_->collecting())
      << "a run that has ended has to hand U1/U2 back to the driver";
  EXPECT_EQ(device_->collection_change_count(), 2U);
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

  EXPECT_TRUE(device_->configured_test_mode());
  EXPECT_EQ(device_->configured_path(), "bus-1");

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// The sample rate goes the same way and at the same moment, because it is the
// same kind of thing: the gateware applies it immediately and without
// acknowledgement, so it has to be settled before any data is flowing.
//
// Decimating is the device's job. Halving the rate means low-passing the signal
// at 10 MHz first, or everything above that folds down on top of it — and that
// filter is sixteen multipliers in the FPGA rather than several cores here. All
// this application does is ask.
TEST_F(CaptureControllerTest, TheSampleRateIsSentToTheDeviceBeforeItIsOpened) {
  UseSmallQueue();

  CaptureSettings settings = controller_->settings();
  settings.decimation_factor = capture::kTapeDecimationFactor;
  controller_->SetSettings(settings);

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  EXPECT_EQ(device_->written_to(capture::kRegisterDecimation),
            std::optional<uint8_t>(capture::kDecimationHalfRate));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// And an undecimated capture says so rather than saying nothing. The register
// is left at whatever the last run set it to otherwise, so a full-rate capture
// taken after a decimated one would come back at half rate.
TEST_F(CaptureControllerTest, TheFullRateIsAskedForExplicitly) {
  UseSmallQueue();

  ASSERT_EQ(controller_->settings().decimation_factor,
            capture::kUndecimatedFactor);

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  EXPECT_EQ(device_->written_to(capture::kRegisterDecimation),
            std::optional<uint8_t>(capture::kDecimationEverySample));

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
