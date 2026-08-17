/************************************************************************

    test_capture_to_disk.cpp

    T1 tests for attaching a writer to a running monitor
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include "capture_controller.h"
#include "capture_format.h"
#include "capture_provenance.h"
#include "capture_reader.h"
#include "fake_usb_device.h"
#include "firmware_version.h"
#include "front_end_gain.h"
#include "sample_format.h"
#include "synthetic_source.h"
#include "test_data_analysis.h"
#include "version.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

// The slot has to be at least one sequence-counter period (65,536 samples) or
// the validator can never lock on, so 256 KiB is the floor rather than a
// preference.
constexpr size_t kTestSlotBytes = size_t{256} << 10;
constexpr size_t kTestSlotCount = 6;

capture::SyntheticSource::Options TestSourceOptions() {
  capture::SyntheticSource::Options options;
  options.slot_size_bytes = kTestSlotBytes;
  options.slot_count = kTestSlotCount;
  return options;
}

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

class CaptureToDiskTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-capture-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-capture-test-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);

    device_ = std::make_unique<capture::FakeUsbDevice>();
    device_->SetSourceOptions(TestSourceOptions());
    device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                             MatchingProductString());

    controller_ = std::make_unique<CaptureController>(device_.get(), nullptr);

    CaptureSettings settings = controller_->settings();
    settings.queue_size_bytes = capture::DiskBufferRing::kMinimumQueueSizeBytes;
    settings.preferred_device_path = QStringLiteral("bus-1");
    settings.capture_directory = QString::fromStdString(directory_.string());

    // Level 0. The tests here are about the pipeline's handling of a writer,
    // not about how well it compresses, and a higher level only makes them
    // slower.
    settings.compression_level = 0;
    controller_->SetSettings(settings);
  }

  void TearDown() override {
    controller_.reset();
    device_.reset();
    std::filesystem::remove_all(directory_);
    QSettings().clear();
  }

  static std::string MatchingProductString() {
    const std::optional<std::string> commit =
        capture::NormaliseCommit(capture::Version());
    return "Domesday Duplicator (" + commit.value_or("a1b2c3d4") + ")";
  }

  // Every capture file left in the test's own directory, sorted, so a test can
  // say what was written without knowing the timestamp it was named after.
  std::vector<std::filesystem::path> WrittenFiles() const {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
      files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
  }

  void Settings(void (*change)(CaptureSettings&)) {
    CaptureSettings settings = controller_->settings();
    change(settings);
    controller_->SetSettings(settings);
  }

  std::filesystem::path directory_;
  std::unique_ptr<capture::FakeUsbDevice> device_;
  std::unique_ptr<CaptureController> controller_;
};

// --- Starting and stopping ------------------------------------------------

// The plan's acceptance criterion, first half: start from idle opens the
// device, monitors and records, all from one action.
TEST_F(CaptureToDiskTest, StartingFromIdleOpensMonitorsAndRecords) {
  QSignalSpy capturing(controller_.get(), &CaptureController::CapturingChanged);

  controller_->StartCapture();

  EXPECT_TRUE(controller_->monitoring());
  EXPECT_TRUE(controller_->capturing());
  EXPECT_EQ(device_->open_count(), 1U);
  ASSERT_EQ(capturing.count(), 1);
  EXPECT_TRUE(capturing.front().at(0).toBool());
  EXPECT_FALSE(capturing.front().at(1).toString().isEmpty());

  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 0;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// And the second half: starting from an existing monitor session attaches a
// sink to the stream that is already running, without reopening the device.
TEST_F(CaptureToDiskTest, StartingFromMonitorAttachesWithoutReopening) {
  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());
  ASSERT_EQ(device_->open_count(), 1U);

  controller_->StartCapture();
  EXPECT_TRUE(controller_->capturing());

  // The device is opened once for the whole session. A capture that reopened it
  // would interrupt the stream, which is the thing monitor mode exists to
  // avoid.
  EXPECT_EQ(device_->open_count(), 1U);

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// "Stop returns to monitor, not idle" — what makes taking both sides of a disc
// possible without reopening the device between them.
TEST_F(CaptureToDiskTest, StoppingACaptureLeavesTheStreamRunning) {
  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  QSignalSpy monitoring(controller_.get(),
                        &CaptureController::MonitoringChanged);

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));

  EXPECT_TRUE(controller_->monitoring());
  EXPECT_EQ(monitoring.count(), 0)
      << "stopping a capture must not stop the stream";

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureToDiskTest, TwoCapturesInOneSessionGiveTwoFiles) {
  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());
  const QString first = controller_->capture_path();

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());
  const QString second = controller_->capture_path();

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));

  // Two files, not one written twice. Both captures started in the same second
  // in a test this fast, so this is also the check that the uniquifier is doing
  // its job — without it the second would silently overwrite the first.
  EXPECT_NE(first, second);
  EXPECT_EQ(WrittenFiles().size(), 2U);

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureToDiskTest, TheFinishedFileIsReported) {
  QSignalSpy finished(controller_.get(), &CaptureController::CaptureFinished);

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  ASSERT_TRUE(PumpUntil([&] {
    return controller_->settings().capture_directory.isEmpty() ||
           (!WrittenFiles().empty() &&
            std::filesystem::file_size(WrittenFiles().front()) > 1024);
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return finished.count() >= 1; }));

  EXPECT_EQ(finished.front().at(0).toString(), controller_->capture_path());
  EXPECT_GT(finished.front().at(1).toULongLong(), 0U);

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureToDiskTest, MonitoringAloneWritesNothingToTheFolder) {
  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  PumpUntil([] { return false; }, 200ms);
  EXPECT_TRUE(WrittenFiles().empty());

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureToDiskTest, StoppingWhenNotCapturingIsHarmless) {
  controller_->StopCapture();
  EXPECT_FALSE(controller_->capturing());
  EXPECT_TRUE(WrittenFiles().empty());
}

TEST_F(CaptureToDiskTest, StartingTwiceDoesNotOpenASecondFile) {
  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  controller_->StartCapture();
  EXPECT_EQ(WrittenFiles().size(), 1U);

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// --- Naming ---------------------------------------------------------------

TEST_F(CaptureToDiskTest, TheDefaultNameIsTheTimeItWasTaken) {
  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  const QString path = controller_->capture_path();
  EXPECT_TRUE(path.contains(QStringLiteral("RF-Sample_")))
      << path.toStdString();
  EXPECT_TRUE(path.endsWith(QLatin1String(capture::kCaptureFileSuffix)))
      << path.toStdString();

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureToDiskTest, ATypedNameIsUsed) {
  Settings([](CaptureSettings& settings) {
    settings.capture_name = QStringLiteral("Blade Runner side 1");
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  EXPECT_TRUE(controller_->capture_path().endsWith(
      QStringLiteral("Blade Runner side 1.ddd.flac")))
      << controller_->capture_path().toStdString();

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// Task 5.3's forced naming, checked where it matters — on the file that is
// actually created, not only in the naming function. A test capture is a ramp
// with no signal in it, and a file called "Blade Runner side 1" full of ramps
// is a trap.
TEST_F(CaptureToDiskTest, ATestCaptureIsNamedTestDataWhateverWasTyped) {
  Settings([](CaptureSettings& settings) {
    settings.capture_name = QStringLiteral("Blade Runner side 1");
    settings.test_mode = true;
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  const QString path = controller_->capture_path();
  EXPECT_TRUE(path.contains(QStringLiteral("TestData_"))) << path.toStdString();
  EXPECT_FALSE(path.contains(QStringLiteral("Blade Runner")))
      << path.toStdString();

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// --- The file that comes out ----------------------------------------------

TEST_F(CaptureToDiskTest, TheCaptureIsAReadableFlacFileWithItsProvenance) {
  Settings([](CaptureSettings& settings) {
    // A declared gain, so the tag that is only written when a declaration was
    // made is exercised on a real file.
    settings.front_end_gain_switches = 0x8;
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 4096;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  ASSERT_EQ(WrittenFiles().size(), 1U);
  const std::filesystem::path path = WrittenFiles().front();

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(reader.Open(path, capture::CaptureReader::Format::kFlac, error))
      << error;

  std::vector<uint16_t> samples;
  bool end_of_file = false;
  ASSERT_TRUE(reader.Read(samples, 4096, end_of_file));
  EXPECT_FALSE(samples.empty());

  const auto tag = [&reader](const std::string& name) -> std::string {
    for (const auto& [key, value] : reader.Tags()) {
      if (key == name) {
        return value;
      }
    }
    return {};
  };

  EXPECT_EQ(tag(capture::kTagSampleRate), "40000000");
  EXPECT_EQ(tag(capture::kTagTestMode), "false");
  EXPECT_FALSE(tag(capture::kTagVersion).empty());
  EXPECT_FALSE(tag(capture::kTagFrontEndGain).empty());
}

// The gain tag is a declaration, never a value this application inferred. With
// nothing declared the tag is absent rather than carrying a default that would
// read as calibration data.
TEST_F(CaptureToDiskTest, AnUndeclaredGainLeavesNoGainTagOnTheFile) {
  ASSERT_FALSE(controller_->settings().DeclaredGain().declared());

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());
  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 4096;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(reader.Open(WrittenFiles().front(),
                          capture::CaptureReader::Format::kFlac, error))
      << error;

  for (const auto& [key, value] : reader.Tags()) {
    EXPECT_NE(key, capture::kTagFrontEndGain);
  }
}

// A test-mode capture, read back and checked for ramp breaks — the offline half
// of the capture-integrity procedure, run against a file this application wrote
// rather than against one a test constructed.
TEST_F(CaptureToDiskTest, ATestModeCaptureAnalysesClean) {
  Settings([](CaptureSettings& settings) { settings.test_mode = true; });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 65536;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  const capture::TestDataAnalysis analysis =
      capture::AnalyseTestData(WrittenFiles().front());

  EXPECT_EQ(analysis.outcome, capture::TestDataAnalysis::Outcome::kPassed)
      << analysis.message;
  EXPECT_EQ(analysis.ExitCode(), 0) << analysis.message;
}

// --- Format and sample rate -----------------------------------------------

// The uncompressed format end to end: the suffix it is named with, and a file
// the same reader takes back.
TEST_F(CaptureToDiskTest, AnUncompressedCaptureIsWrittenAndReadsBack) {
  Settings([](CaptureSettings& settings) {
    settings.output_format = capture::CaptureOutputFormat::kSigned16Bit;
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  EXPECT_TRUE(controller_->capture_path().endsWith(
      QLatin1String(capture::kSigned16BitCaptureFileSuffix)))
      << controller_->capture_path().toStdString();

  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 4096;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  ASSERT_EQ(WrittenFiles().size(), 1U);
  const std::filesystem::path path = WrittenFiles().front();

  // A headerless file is exactly two bytes per sample, which is what makes the
  // reader's total-sample figure come from the size.
  EXPECT_EQ(std::filesystem::file_size(path) % capture::kBytesPerSample, 0U);

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(
      reader.Open(path, capture::CaptureReader::Format::kSigned16Bit, error))
      << error;

  std::vector<uint16_t> samples;
  bool end_of_file = false;
  ASSERT_TRUE(reader.Read(samples, 4096, end_of_file));
  EXPECT_FALSE(samples.empty());
}

// A 2:1 capture holds half as many samples as the stream carried, and the file
// says which rate it was written at — the only evidence that survives being
// copied off the machine that made it.
TEST_F(CaptureToDiskTest, ADecimatedCaptureHalvesTheFileAndSaysSo) {
  Settings([](CaptureSettings& settings) {
    settings.decimation_factor = capture::kTapeDecimationFactor;
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 4096;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  ASSERT_EQ(WrittenFiles().size(), 1U);

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(reader.Open(WrittenFiles().front(),
                          capture::CaptureReader::Format::kFlac, error))
      << error;

  const auto tag = [&reader](const std::string& name) -> std::string {
    for (const auto& [key, value] : reader.Tags()) {
      if (key == name) {
        return value;
      }
    }
    return {};
  };

  EXPECT_EQ(tag(capture::kTagDecimation), "2");
  EXPECT_EQ(tag(capture::kTagSampleRate),
            std::to_string(capture::kSampleRateHz / 2));

  // Half a buffer's worth of samples per buffer, so the count is a multiple of
  // that rather than of the whole buffer.
  const std::optional<uint64_t> total = reader.TotalSamples();
  ASSERT_TRUE(total.has_value());
  const uint64_t samples_per_buffer = kTestSlotBytes / capture::kBytesPerSample;
  EXPECT_EQ(total.value_or(0) % (samples_per_buffer / 2), 0U)
      << "a decimated capture did not land on a buffer boundary";
}

// Test mode captures every sample whatever the setting says: the pattern is a
// ramp checked sample by sample, and a decimated one would read as a break on
// the first buffer. Asserted where it matters — on the analysis of a real file,
// which is the integrity oracle the whole project rests on.
TEST_F(CaptureToDiskTest, ADecimationSettingCannotBreakATestModeCapture) {
  Settings([](CaptureSettings& settings) {
    settings.test_mode = true;
    settings.decimation_factor = capture::kTapeDecimationFactor;
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 65536;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  const capture::TestDataAnalysis analysis =
      capture::AnalyseTestData(WrittenFiles().front());

  EXPECT_EQ(analysis.outcome, capture::TestDataAnalysis::Outcome::kPassed)
      << analysis.message;
}

// The same analysis over an uncompressed test capture, which is the other half
// of the integrity gate now being reachable in both formats.
TEST_F(CaptureToDiskTest, AnUncompressedTestCaptureAnalysesClean) {
  Settings([](CaptureSettings& settings) {
    settings.test_mode = true;
    settings.output_format = capture::CaptureOutputFormat::kSigned16Bit;
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 65536;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  const capture::TestDataAnalysis analysis =
      capture::AnalyseTestData(WrittenFiles().front());

  EXPECT_EQ(analysis.outcome, capture::TestDataAnalysis::Outcome::kPassed)
      << analysis.message;
  EXPECT_EQ(analysis.ExitCode(), 0) << analysis.message;
}

// --- The duration limit ---------------------------------------------------

// The limit is held in seconds although the panel offers minutes, and this is
// what that buys: a real end-to-end test. One second of capture is 40 million
// samples, which the unpaced synthetic source produces in well under a second
// of wall-clock time; one minute would be 2.4 billion and no test would run it.
TEST_F(CaptureToDiskTest, ADurationLimitStopsTheCaptureButNotTheStream) {
  // Paced at the device's real rate rather than run flat out, because the
  // overshoot is what is being measured and it is a length of *time*: the limit
  // is checked on the statistics tick, so a source producing data as fast as
  // the machine allows would overshoot by however much it managed in one tick,
  // which says nothing about the behaviour on real hardware.
  capture::SyntheticSource::Options options = TestSourceOptions();
  options.rate_bytes_per_second = capture::kWireBytesPerSecond;
  device_->SetSourceOptions(options);

  Settings(
      [](CaptureSettings& settings) { settings.duration_limit_seconds = 1; });

  QSignalSpy finished(controller_.get(), &CaptureController::CaptureFinished);

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  // Created after the start, so it records only what happens from here — the
  // start itself emits MonitoringChanged(true) and would otherwise be counted.
  QSignalSpy monitoring(controller_.get(),
                        &CaptureController::MonitoringChanged);

  // Nobody pressed stop. The limit did it.
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }, 30000ms));

  // The stream is still running, which is the half that is easy to get wrong: a
  // limit that stopped the whole run would make an unattended capture end the
  // session rather than end the file.
  EXPECT_TRUE(controller_->monitoring());
  EXPECT_EQ(monitoring.count(), 0);

  ASSERT_TRUE(PumpUntil([&] { return finished.count() >= 1; }));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  ASSERT_EQ(WrittenFiles().size(), 1U);

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(reader.Open(WrittenFiles().front(),
                          capture::CaptureReader::Format::kFlac, error))
      << error;

  const std::optional<uint64_t> total = reader.TotalSamples();
  ASSERT_TRUE(total.has_value());

  constexpr uint64_t kOneSecondOfSamples = capture::kSampleRateHz;
  const uint64_t samples_per_buffer = kTestSlotBytes / capture::kBytesPerSample;

  // Never short. A limit that stopped early would silently truncate a capture
  // somebody asked to run for a stated length.
  EXPECT_GE(total.value_or(0), kOneSecondOfSamples);

  // And never far over. The check runs on the statistics tick, so the overshoot
  // is one tick plus the buffer being written when it fires; the bound here is
  // ten ticks' worth, which is loose enough to survive a loaded build machine
  // and tight enough that a limit which never fired at all would fail.
  const uint64_t tick_samples =
      (static_cast<uint64_t>(CaptureController::kStatsIntervalMilliseconds) *
       capture::kSampleRateHz) /
      1000;
  EXPECT_LT(total.value_or(0),
            kOneSecondOfSamples + (10 * tick_samples) + samples_per_buffer);

  // The boundary alignment the plan asks for, stated as a number rather than as
  // an intention: a sink only ever receives whole buffers, so a file whose
  // length is not a multiple of one was cut somewhere it should not have been.
  EXPECT_EQ(total.value_or(0) % samples_per_buffer, 0U)
      << "the stop did not land on a buffer boundary";
}

// The limit is a length of time, and the counter it is checked against is
// samples that reached the file — so a decimated capture puts half as many in
// per second. A limit that ignored that would run for twice as long as it was
// asked to, which on an unattended capture is the difference between one side
// and two.
TEST_F(CaptureToDiskTest, ADurationLimitIsATimeEvenWhenDecimating) {
  capture::SyntheticSource::Options options = TestSourceOptions();
  options.rate_bytes_per_second = capture::kWireBytesPerSecond;
  device_->SetSourceOptions(options);

  Settings([](CaptureSettings& settings) {
    settings.duration_limit_seconds = 1;
    settings.decimation_factor = capture::kTapeDecimationFactor;
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }, 30000ms));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  ASSERT_EQ(WrittenFiles().size(), 1U);

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(reader.Open(WrittenFiles().front(),
                          capture::CaptureReader::Format::kFlac, error))
      << error;

  const std::optional<uint64_t> total = reader.TotalSamples();
  ASSERT_TRUE(total.has_value());

  // One second of signal at half the rate: twenty million samples in the file,
  // not forty.
  constexpr uint64_t kOneSecondDecimated = capture::kSampleRateHz / 2;
  const uint64_t samples_per_buffer =
      kTestSlotBytes / capture::kBytesPerSample / 2;

  EXPECT_GE(total.value_or(0), kOneSecondDecimated);

  const uint64_t tick_samples =
      (static_cast<uint64_t>(CaptureController::kStatsIntervalMilliseconds) *
       kOneSecondDecimated) /
      1000;
  EXPECT_LT(total.value_or(0),
            kOneSecondDecimated + (10 * tick_samples) + samples_per_buffer);
}

TEST_F(CaptureToDiskTest, NoDurationLimitMeansTheCaptureRunsUntilStopped) {
  ASSERT_EQ(controller_->settings().duration_limit_seconds, 0);

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  // Far longer than the one-second capture above took to reach its limit, so a
  // limit firing when none was set would be caught here.
  PumpUntil([] { return false; }, 500ms);
  EXPECT_TRUE(controller_->capturing());

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// --- Failures -------------------------------------------------------------

// A destination that cannot be written to is reported before anything is
// attached, and the stream is left alone: a capture that could not start is not
// a reason to stop monitoring.
TEST_F(CaptureToDiskTest, AFolderThatCannotBeWrittenToIsReported) {
  // A path under a regular file, which cannot be turned into a directory on any
  // platform this runs on. The file has to be one this test made: the previous
  // /dev/null/not-a-directory is unusable only on Unix, and on Windows those
  // are ordinary names that get created on demand.
  const std::filesystem::path blocker = directory_ / "a-regular-file";
  {
    std::ofstream file(blocker);
    ASSERT_TRUE(file.good());
  }

  // Set directly rather than through Settings(), which takes a plain function
  // pointer and so cannot carry the path this test just made.
  CaptureSettings settings = controller_->settings();
  settings.capture_directory =
      QString::fromStdString((blocker / "not-a-directory").string());
  controller_->SetSettings(settings);

  QSignalSpy failures(controller_.get(), &CaptureController::Failed);

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  controller_->StartCapture();

  EXPECT_FALSE(controller_->capturing());
  ASSERT_EQ(failures.count(), 1);
  EXPECT_TRUE(failures.front().at(0).toString().contains(
      QStringLiteral("file-creation-error")))
      << failures.front().at(0).toString().toStdString();

  // Still monitoring. The stream did not stop because a file could not be made.
  EXPECT_TRUE(controller_->monitoring());

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// --- Never overwriting, and never quietly renaming --------------------------

// The engine has never overwritten a capture. What this pins is the other half:
// that the rename it does instead is reported, because a typed name carries no
// timestamp and so is taken every time after the first.
TEST_F(CaptureToDiskTest, ASecondCaptureOfTheSameNameIsRenamedAndSaidSo) {
  Settings([](CaptureSettings& settings) {
    settings.capture_name = QStringLiteral("Casper side 1");
  });

  QSignalSpy renamed(controller_.get(), &CaptureController::CaptureRenamed);

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());
  ASSERT_TRUE(PumpUntil([&] { return !WrittenFiles().empty(); }));

  // The first one is the name that was asked for, and nothing is said.
  EXPECT_EQ(renamed.count(), 0);
  const QString first = controller_->capture_path();
  EXPECT_TRUE(first.contains(QStringLiteral("Casper side 1.ddd.flac")));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  // The second is written beside it rather than over it, and the rename is
  // reported rather than left for somebody to notice in a directory listing.
  ASSERT_EQ(renamed.count(), 1);
  EXPECT_EQ(renamed.front().at(0).toString(), QStringLiteral("Casper side 1"));
  EXPECT_EQ(renamed.front().at(1).toString(),
            QStringLiteral("Casper side 1_2"));

  EXPECT_TRUE(controller_->capture_path().contains(
      QStringLiteral("Casper side 1_2.ddd.flac")));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();

  // Both files are there, and the first still has its own contents.
  ASSERT_EQ(WrittenFiles().size(), 2U);
  EXPECT_GT(std::filesystem::file_size(WrittenFiles().front()), 0U);
}

TEST_F(CaptureToDiskTest, TheGeneratedNameIsNeverReportedAsRenamed) {
  ASSERT_TRUE(controller_->settings().capture_name.isEmpty());

  QSignalSpy renamed(controller_.get(), &CaptureController::CaptureRenamed);

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());
  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();

  // It carries a timestamp, so it is free by construction — and a warning that
  // fired on every capture would be one nobody read.
  EXPECT_EQ(renamed.count(), 0);
}

}  // namespace
}  // namespace ddd::gui
