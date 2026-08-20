/************************************************************************

    test_headless_capture_runner.cpp

    T1 tests for a capture with no window around it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture_cli.h"
#include "capture_control_server.h"
#include "capture_controller.h"
#include "capture_format.h"
#include "capture_metadata.h"
#include "capture_stop_client.h"
#include "disk_buffer_ring.h"
#include "fake_usb_device.h"
#include "headless_capture_runner.h"
#include "logger.h"
#include "sample_format.h"
#include "synthetic_source.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

constexpr size_t kTestSlotBytes = size_t{256} << 10;
constexpr size_t kTestSlotCount = 6;

capture::SyntheticSource::Options TestSourceOptions() {
  capture::SyntheticSource::Options options;
  options.slot_size_bytes = kTestSlotBytes;
  options.slot_count = kTestSlotCount;
  return options;
}

template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 15000ms) {
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

class HeadlessCaptureRunnerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const QString test_name = QLatin1String(info->name());

    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-headless-%1").arg(test_name));
    QSettings().clear();

    socket_name_ = QStringLiteral("ddd-gui-headless-test-%1").arg(test_name);

    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-headless-test-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);

    device_ = std::make_unique<capture::FakeUsbDevice>();
    device_->SetSourceOptions(TestSourceOptions());
    device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                             "Domesday Duplicator (a1b2c3d4)");

    controller_ = std::make_unique<CaptureController>(device_.get(), &logger_);

    CaptureSettings settings = controller_->settings();
    settings.queue_size_bytes = capture::DiskBufferRing::kMinimumQueueSizeBytes;
    settings.preferred_device_path = QStringLiteral("bus-1");
    settings.capture_directory = QString::fromStdString(directory_.string());
    settings.compression_level = 0;

    // Applied rather than set, so that nothing here writes into the settings
    // file — which is what a headless run does with what a command line gave
    // it, and so is also how this test should arrange one.
    controller_->ApplySessionSettings(settings);
  }

  void TearDown() override {
    finished_.reset();
    runner_.reset();
    server_.reset();
    controller_.reset();
    device_.reset();
    std::filesystem::remove_all(directory_);
    QSettings().clear();
  }

  static HeadlessCaptureOptions DefaultOptions() {
    HeadlessCaptureOptions options;
    options.device_wait_milliseconds = 5000;
    options.finish_wait_milliseconds = 20000;
    return options;
  }

  // Build the runner and start it, in the order a headless run does it: the
  // runner is connected before the controller starts looking, because the first
  // device report is the one that starts the capture.
  HeadlessCaptureRunner& Begin(
      HeadlessCaptureOptions options = DefaultOptions()) {
    runner_ = std::make_unique<HeadlessCaptureRunner>(
        controller_.get(), out_stream_, error_stream_, options);
    finished_ = std::make_unique<QSignalSpy>(runner_.get(),
                                             &HeadlessCaptureRunner::Finished);
    runner_->Begin();
    controller_->Start();
    return *runner_;
  }

  bool WaitForExit() {
    return PumpUntil([this] { return finished_->count() >= 1; });
  }

  int ExitCode() const {
    return finished_->count() >= 1 ? finished_->front().at(0).toInt() : -1;
  }

  QString Out() {
    out_stream_.flush();
    return out_text_;
  }

  QString Said() {
    error_stream_.flush();
    return error_text_;
  }

  void Change(void (*change)(CaptureSettings&)) {
    CaptureSettings settings = controller_->settings();
    change(settings);
    controller_->ApplySessionSettings(settings);
  }

  std::vector<std::filesystem::path> WrittenFiles() const {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
      if (!capture::MatchedCaptureFileSuffix(entry.path().string()).empty()) {
        files.push_back(entry.path());
      }
    }
    return files;
  }

  // Wait until a capture is being written and has something in it, so that a
  // test which stops one is stopping a capture rather than racing its start.
  bool CapturingForReal() {
    return PumpUntil([this] {
      return controller_->capturing() && !WrittenFiles().empty() &&
             std::filesystem::file_size(WrittenFiles().front()) > 0;
    });
  }

  QString socket_name_;
  std::filesystem::path directory_;
  std::unique_ptr<capture::FakeUsbDevice> device_;

  // A logger that keeps nothing, and given rather than left null so that the
  // controller's own logging runs on every path here. A line that only exists
  // where nothing exercises it is a line that can crash a release and pass CI.
  capture::CallbackLogger logger_{
      [](capture::LogLevel /*level*/, const std::string& /*message*/) {},
      capture::LogLevel::kDebug};

  std::unique_ptr<CaptureController> controller_;
  std::unique_ptr<CaptureControlServer> server_;

  QString out_text_;
  QString error_text_;
  QTextStream out_stream_{&out_text_};
  QTextStream error_stream_{&error_text_};

  std::unique_ptr<HeadlessCaptureRunner> runner_;
  std::unique_ptr<QSignalSpy> finished_;
};

// --- A run that goes the way it is meant to -------------------------------

// The unattended capture: nobody presses anything, the limit ends it, and the
// process exits with a file on disk and its path on standard output.
TEST_F(HeadlessCaptureRunnerTest, ADurationLimitEndsTheRunByItself) {
  // Paced at the device's real rate, so that one second of capture is one
  // second rather than however much an unpaced source managed before the limit
  // was noticed.
  capture::SyntheticSource::Options options = TestSourceOptions();
  options.rate_bytes_per_second = capture::kWireBytesPerSecond;
  device_->SetSourceOptions(options);

  Change(
      [](CaptureSettings& settings) { settings.duration_limit_seconds = 1; });

  Begin();
  ASSERT_TRUE(WaitForExit());
  EXPECT_EQ(ExitCode(), kExitSuccess);

  ASSERT_EQ(WrittenFiles().size(), 1U);
  const std::filesystem::path written = WrittenFiles().front();
  EXPECT_GT(std::filesystem::file_size(written), 0U);

  // The path on stdout is the path of the file that is there, and it is the
  // only thing on stdout. A script reads that line and opens what it names.
  EXPECT_EQ(Out(),
            QString::fromStdString(written.string()) + QLatin1Char('\n'));

  // And the file is finished rather than merely closed: the sidecar is written
  // after the capture stops, so its being there is the proof that the run
  // waited for the end rather than exiting when the writer detached.
  EXPECT_TRUE(std::filesystem::exists(capture::CaptureMetadataPath(written)));
}

TEST_F(HeadlessCaptureRunnerTest, WhatAPersonReadsGoesToTheOtherStream) {
  Begin();
  ASSERT_TRUE(CapturingForReal());

  runner_->RequestStop();
  ASSERT_TRUE(WaitForExit());
  ASSERT_EQ(ExitCode(), kExitSuccess);

  const QString said = Said();
  EXPECT_TRUE(said.contains(QStringLiteral("Capturing to ")))
      << said.toStdString();
  EXPECT_TRUE(said.contains(QStringLiteral("Finished."))) << said.toStdString();

  // One line, and it is a path. Everything else went to the stream a script
  // does not read.
  EXPECT_EQ(Out().trimmed().split(QLatin1Char('\n')).size(), 1);
}

// --- Stopping -------------------------------------------------------------

// Ctrl+C, and the whole reason SignalWatcher exists: the capture is stopped
// politely and the file is finished before the process is allowed to end.
TEST_F(HeadlessCaptureRunnerTest,
       AnInterruptFinishesTheFileRatherThanCuttingIt) {
  Begin();
  ASSERT_TRUE(CapturingForReal());

  runner_->RequestStop();

  ASSERT_TRUE(WaitForExit());
  EXPECT_EQ(ExitCode(), kExitSuccess);

  ASSERT_EQ(WrittenFiles().size(), 1U);
  const std::filesystem::path written = WrittenFiles().front();
  EXPECT_TRUE(std::filesystem::exists(capture::CaptureMetadataPath(written)));
  EXPECT_EQ(Out(),
            QString::fromStdString(written.string()) + QLatin1Char('\n'));
}

// A second interrupt while the encoder is closing the file. It is the moment a
// user is most likely to press Ctrl+C again, and the one where doing what they
// asked would cost them the capture.
TEST_F(HeadlessCaptureRunnerTest, InterruptingAgainDoesNotAbandonTheFile) {
  Begin();
  ASSERT_TRUE(CapturingForReal());

  runner_->RequestStop();
  runner_->RequestStop();

  ASSERT_TRUE(WaitForExit());
  EXPECT_EQ(ExitCode(), kExitSuccess);

  ASSERT_EQ(WrittenFiles().size(), 1U);
  EXPECT_TRUE(std::filesystem::exists(
      capture::CaptureMetadataPath(WrittenFiles().front())));
  EXPECT_TRUE(Said().contains(QStringLiteral("Still finishing")))
      << Said().toStdString();
}

// The Windows stop, and the one a script uses everywhere: another process asks
// over the control socket. The runner is not told about it and does not need to
// be — it sees the capture stop and waits for the file, exactly as it does for
// an interrupt.
TEST_F(HeadlessCaptureRunnerTest, AStopFromASecondProcessEndsTheRun) {
  Begin();
  ASSERT_TRUE(CapturingForReal());

  server_ = std::make_unique<CaptureControlServer>(controller_.get(), &logger_);
  QString listen_error;
  ASSERT_TRUE(server_->Listen(socket_name_, &listen_error))
      << listen_error.toStdString();

  QString client_out_text;
  QString client_error_text;
  QTextStream client_out(&client_out_text);
  QTextStream client_error(&client_error_text);

  StopCaptureOptions options;
  options.server_name = socket_name_;
  options.connect_timeout_milliseconds = 2000;
  options.reply_timeout_milliseconds = 20000;

  EXPECT_EQ(RunStopCapture(client_out, client_error, options), kExitSuccess);
  client_out.flush();

  ASSERT_TRUE(WaitForExit());
  EXPECT_EQ(ExitCode(), kExitSuccess);

  // Both halves name the same file: the one the client was told about and the
  // one the run itself printed. A script may read either.
  ASSERT_EQ(WrittenFiles().size(), 1U);
  EXPECT_EQ(client_out_text.trimmed(),
            QString::fromStdString(WrittenFiles().front().string()));
  EXPECT_EQ(Out().trimmed(), client_out_text.trimmed());
}

// --- Nothing to capture from ----------------------------------------------

TEST_F(HeadlessCaptureRunnerTest, NothingPluggedInIsItsOwnExitCode) {
  device_->SetDevices({});

  HeadlessCaptureOptions options = DefaultOptions();
  options.device_wait_milliseconds = 500;

  Begin(options);
  ASSERT_TRUE(WaitForExit());

  EXPECT_EQ(ExitCode(), kExitNoDevice);
  EXPECT_TRUE(WrittenFiles().empty());
  EXPECT_TRUE(Out().isEmpty()) << Out().toStdString();
  EXPECT_TRUE(
      Said().contains(QStringLiteral("No Domesday Duplicator was found")))
      << Said().toStdString();
}

// Interrupted while still waiting. Nothing was captured and there is no file,
// which is the same thing the exit code above says — so it says the same thing.
TEST_F(HeadlessCaptureRunnerTest, AnInterruptBeforeADeviceCapturesNothing) {
  device_->SetDevices({});

  Begin();

  // After a device report has been and gone, so this is an interrupt during the
  // wait rather than one that raced the runner's own start.
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  ASSERT_TRUE(PumpUntil([&devices] { return devices.count() >= 1; }));

  runner_->RequestStop();
  ASSERT_TRUE(WaitForExit());

  EXPECT_EQ(ExitCode(), kExitNoDevice);
  EXPECT_TRUE(WrittenFiles().empty());
  EXPECT_TRUE(Out().isEmpty()) << Out().toStdString();
}

// --- Failures -------------------------------------------------------------

// The ordering that makes an exit code worth reading. When a run fails
// mid-capture the controller reports the finished file first and the failure
// second, in the same call — so a runner that exited on the first of them would
// tell a script that a broken capture went perfectly well.
TEST_F(HeadlessCaptureRunnerTest, AFailedRunSaysSoEvenThoughAFileWasWritten) {
  capture::SyntheticSource::Options options = TestSourceOptions();
  options.fault = capture::SyntheticSource::Fault::kTransferFailure;
  options.fault_at_slot = 12;
  device_->SetSourceOptions(options);

  Begin();
  ASSERT_TRUE(WaitForExit());

  EXPECT_EQ(ExitCode(), kExitCaptureFailed);
  EXPECT_TRUE(Said().contains(QStringLiteral("Error:")))
      << Said().toStdString();

  // And the file it did write is still named, because it is still there and
  // still readable. A failure is a reason to go and look at a capture, not a
  // reason to be told nothing about it.
  ASSERT_EQ(WrittenFiles().size(), 1U);
  EXPECT_EQ(Out().trimmed(),
            QString::fromStdString(WrittenFiles().front().string()));
}

// A capture that never opened a file at all. There is nothing to wait for, so
// the run ends at once rather than sitting out the finish timeout.
TEST_F(HeadlessCaptureRunnerTest, ACaptureThatCannotBeOpenedEndsAtOnce) {
  const std::filesystem::path blocker = directory_ / "a-regular-file";
  {
    const std::ofstream file(blocker);
    ASSERT_TRUE(file.good());
  }

  CaptureSettings settings = controller_->settings();
  settings.capture_directory =
      QString::fromStdString((blocker / "not-a-directory").string());
  controller_->ApplySessionSettings(settings);

  Begin();
  ASSERT_TRUE(WaitForExit());

  EXPECT_EQ(ExitCode(), kExitCaptureFailed);
  EXPECT_TRUE(Out().isEmpty()) << Out().toStdString();
  EXPECT_TRUE(Said().contains(QStringLiteral("Error:")))
      << Said().toStdString();
}

// --- The windowed half of --start-capture ---------------------------------

TEST_F(HeadlessCaptureRunnerTest, TheWindowedStartWaitsForTheDeviceToArrive) {
  device_->SetDevices({});

  StartCaptureWhenDeviceAppears(controller_.get());
  controller_->Start();

  // An empty report first, and it must not spend the watch. It is what the
  // monitor produces on its first poll with nothing attached, so a one-shot
  // connection would be used up on it and the capture would never start.
  QSignalSpy devices(controller_.get(), &CaptureController::DevicesChanged);
  ASSERT_TRUE(PumpUntil([&devices] { return devices.count() >= 1; }));
  EXPECT_FALSE(controller_->capturing());

  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (a1b2c3d4)");

  ASSERT_TRUE(PumpUntil([this] { return controller_->capturing(); }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([this] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([this] { return !controller_->monitoring(); }));
}

TEST_F(HeadlessCaptureRunnerTest, AWindowedStartWithADeviceAlreadyThereIsNow) {
  controller_->Start();
  ASSERT_TRUE(PumpUntil([this] { return !controller_->devices().empty(); }));

  StartCaptureWhenDeviceAppears(controller_.get());
  EXPECT_TRUE(controller_->capturing());

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([this] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([this] { return !controller_->monitoring(); }));
}

// It starts one capture and then lets go. A watch that stayed connected would
// take the next device report as another instruction to capture, and a user who
// unplugged the Duplicator to move it would come back to a second recording
// nobody asked for.
//
// Checked after the session has ended rather than during it: the controller
// suspends the device monitor for as long as the stream is open — opening a
// device to enumerate it underneath a running transfer is exactly what that
// suspension exists to prevent — so a mid-capture report is not a thing that
// happens. The reports resume when monitoring stops, which is where the watch
// would show itself if it were still there.
TEST_F(HeadlessCaptureRunnerTest, TheWindowedStartOnlyEverStartsOneCapture) {
  device_->SetDevices({});

  StartCaptureWhenDeviceAppears(controller_.get());
  controller_->Start();

  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (a1b2c3d4)");
  ASSERT_TRUE(PumpUntil([this] { return controller_->capturing(); }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([this] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([this] { return !controller_->monitoring(); }));

  // The device goes and comes back, which is two more reports — and the second
  // of them looks exactly like the one that started the capture above.
  device_->SetDevices({});
  ASSERT_TRUE(PumpUntil([this] { return controller_->devices().empty(); }));
  device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                           "Domesday Duplicator (a1b2c3d4)");
  ASSERT_TRUE(PumpUntil([this] { return !controller_->devices().empty(); }));

  EXPECT_FALSE(controller_->capturing());
  EXPECT_EQ(WrittenFiles().size(), 1U);
}

}  // namespace
}  // namespace ddd::gui
