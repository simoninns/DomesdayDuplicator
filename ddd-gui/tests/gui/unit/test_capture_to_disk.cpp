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
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "capture_controller.h"
#include "capture_format.h"
#include "capture_metadata.h"
#include "capture_provenance.h"
#include "capture_reader.h"
#include "fake_usb_device.h"
#include "firmware_version.h"
#include "front_end_gain.h"
#include "logger.h"
#include "sample_format.h"
#include "synthetic_source.h"
#include "test_data_analysis.h"

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

    // Given a logger rather than nullptr, so that every test here runs the
    // controller's own logging as well as its behaviour — a line that only
    // exists on the path nobody tests is a line that can crash a release and
    // pass CI.
    controller_ = std::make_unique<CaptureController>(device_.get(), &logger_);

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

  // Records arrive on engine threads as well as this one, so the collection has
  // a lock of its own. The logger serialises its callbacks; what it cannot do
  // is stop a test reading the vector while a capture thread appends to it.
  bool LogContains(const std::string& fragment) const {
    const std::lock_guard<std::mutex> guard(log_mutex_);
    for (const std::string& message : log_) {
      if (message.find(fragment) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  // A product string in the shape the firmware reports, so the device the
  // controller sees names a build. Any commit will do: nothing compares it
  // against the application's, which is released separately.
  static std::string MatchingProductString() {
    return "Domesday Duplicator (a1b2c3d4)";
  }

  // Every capture file left in the test's own directory, sorted, so a test can
  // say what was written without knowing the timestamp it was named after.
  //
  // The metadata sidecars are filtered out, because a capture and the text file
  // beside it are not two captures — every assertion here about "how many files
  // were written" means recordings. MetadataFiles() below is the other half.
  std::vector<std::filesystem::path> WrittenFiles() const {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
      if (!capture::MatchedCaptureFileSuffix(entry.path().string()).empty()) {
        files.push_back(entry.path());
      }
    }
    std::sort(files.begin(), files.end());
    return files;
  }

  std::vector<std::filesystem::path> MetadataFiles() const {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
      if (entry.path().extension() == ".yaml") {
        files.push_back(entry.path());
      }
    }
    std::sort(files.begin(), files.end());
    return files;
  }

  static std::string ReadWholeFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
  }

  void Settings(void (*change)(CaptureSettings&)) {
    CaptureSettings settings = controller_->settings();
    change(settings);
    controller_->SetSettings(settings);
  }

  std::filesystem::path directory_;
  std::unique_ptr<capture::FakeUsbDevice> device_;

  // Declared before the controller so that it outlives it: the controller holds
  // the pointer for as long as it exists, and the engine threads it starts log
  // through it right up to the moment they are joined in its destructor.
  mutable std::mutex log_mutex_;
  std::vector<std::string> log_;
  capture::CallbackLogger logger_{
      [this](capture::LogLevel /*level*/, const std::string& message) {
        const std::lock_guard<std::mutex> guard(log_mutex_);
        log_.push_back(message);
      },
      capture::LogLevel::kDebug};

  std::unique_ptr<CaptureController> controller_;
};

// --- What the log says about a capture ------------------------------------

// The developer's account of a recording: what reached the file, what the
// signal in it looked like, and what the device lost while it was open. Pinned
// by fragment rather than by whole line — the wording is meant to be edited,
// and what must not change is that each figure is reported at all.
TEST_F(CaptureToDiskTest, TheFilesOwnAccountIsLoggedWhenItFinishes) {
  QSignalSpy finished(controller_.get(), &CaptureController::CaptureFinished);

  controller_->StartCapture();
  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 0;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return finished.count() == 1; }));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));

  // The setup the file was recorded under, said at the moment it was opened so
  // that a log can be read without the settings file that has since changed.
  EXPECT_TRUE(LogContains("Capture settings: "));
  EXPECT_TRUE(LogContains("Msps, ring "));
  EXPECT_TRUE(LogContains("Destination has "));

  // And what it came out as.
  EXPECT_TRUE(LogContains("Capture file: "));
  EXPECT_TRUE(LogContains("that arrived"));
  EXPECT_TRUE(LogContains("Signal in the file: "));
  EXPECT_TRUE(LogContains("While this file was open: device lost "));
  EXPECT_TRUE(LogContains("session peak ring depth "));
}

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

  // Waited on for contents rather than for the file merely to appear: what the
  // second capture must not write over has to be something before the second
  // one starts, or the assertion at the end of this test proves nothing.
  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 0;
  }));

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
            QStringLiteral("Casper side 1 (1)"));

  EXPECT_TRUE(controller_->capture_path().contains(
      QStringLiteral("Casper side 1 (1).ddd.flac")));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  controller_->StopMonitoring();

  // Both files are there, and the first still has its own contents. Named
  // rather than taken from the front of the listing: " (1)" sorts ahead of
  // ".ddd", so the front of it is the second capture, whose length is only
  // however much happened to arrive before it was stopped.
  ASSERT_EQ(WrittenFiles().size(), 2U);
  EXPECT_GT(
      std::filesystem::file_size(std::filesystem::path(first.toStdString())),
      0U);
}

// The same guarantee at the other end of the capture, which is where it used to
// stop holding.
//
// A name is resolved before the file is opened, but that is not the name the
// file ends up with when the naming appends the capture's length: the finished
// file is renamed, and std::filesystem::rename replaces whatever is at the
// destination without a word. Two captures of one name that ran for the same
// length both wanted "<name>_00H00M00S", so the second quietly destroyed the
// first — after the first had been reported as finished, and with its sidecar
// going the same way.
//
// It is not an unlikely shape. It is what a script produces on every run: one
// --capture-name, one --duration-limit, the same answer every time.
TEST_F(CaptureToDiskTest, ADurationRenameNeverLandsOnAnEarlierCapture) {
  Settings([](CaptureSettings& settings) {
    settings.capture_name = QStringLiteral("Casper side 1");
    settings.naming.append_duration = true;
  });

  QSignalSpy renamed(controller_.get(), &CaptureController::CaptureRenamed);

  // The samples the run has recorded so far, read from the statistics rather
  // than from the size of the open file on disk.
  //
  // This wait is what decides the name, so it has to be short: the duration is
  // samples over the sample rate, and both captures have to come in under a
  // second for them to collide at all. The statistics are a counter in memory
  // published every 50 ms, whereas a live file's size is only as fresh as the
  // platform makes it — on Windows the directory entry is not updated while
  // the handle is open, so waiting for it to grow ran for most of a second and
  // named the capture 00H00M01S, with the second one landing somewhere else.
  //
  // Connected through a receiver of its own, declared after the counter and so
  // destroyed before it: the lambda writes to a local, and an assertion that
  // ends this test early must not leave it connected to a controller that goes
  // on publishing into a variable that is no longer there.
  uint64_t samples = 0;
  QObject tap;
  QObject::connect(controller_.get(), &CaptureController::StatsUpdated, &tap,
                   [&samples](const capture::CaptureStats& stats) {
                     samples = stats.samples_written;
                   });

  const auto capture_once = [this, &samples] {
    samples = 0;
    controller_->StartCapture();
    ASSERT_TRUE(controller_->capturing());

    // Something has to have reached the file before it is stopped, or the
    // assertion at the end of this test proves nothing — and a capture of no
    // samples has no duration to append and would never be renamed at all.
    ASSERT_TRUE(PumpUntil([&samples] { return samples > 0; }));

    controller_->StopCapture();
    ASSERT_TRUE(PumpUntil([this] { return !controller_->capturing(); }));
  };

  ASSERT_NO_FATAL_FAILURE(capture_once());
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 1U; }));
  ASSERT_EQ(WrittenFiles().size(), 1U);

  const std::filesystem::path first = WrittenFiles().front();
  const auto first_size = std::filesystem::file_size(first);
  ASSERT_GT(first_size, 0U);

  // Both captures are stopped on the first statistics tick that shows a sample,
  // which is fifty milliseconds of recording and change. The duration in the
  // name is samples over the sample rate, so that is a name of 00H00M00S with
  // an order of magnitude to spare even where the synthetic source outruns the
  // clock — and it is the same name both times, which is what makes them
  // collide.
  ASSERT_NE(first.filename().string().find("_00H00M00S"), std::string::npos)
      << first.filename().string();

  ASSERT_NO_FATAL_FAILURE(capture_once());
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 2U; }));
  controller_->StopMonitoring();

  // Two recordings and two sidecars. This was one of each.
  ASSERT_EQ(WrittenFiles().size(), 2U);
  EXPECT_TRUE(std::filesystem::exists(first));
  EXPECT_EQ(std::filesystem::file_size(first), first_size);

  // And it was said out loud, exactly as a collision found before the file is
  // opened is said out loud.
  ASSERT_EQ(renamed.count(), 1);
  EXPECT_EQ(renamed.front().at(0).toString(),
            QStringLiteral("Casper side 1_00H00M00S"));
  EXPECT_EQ(renamed.front().at(1).toString(),
            QStringLiteral("Casper side 1_00H00M00S (1)"));
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

// --- The metadata file beside it ------------------------------------------

TEST_F(CaptureToDiskTest, EveryCaptureGetsAMetadataFileBesideIt) {
  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());
  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));

  // The sidecar is written when the file is closed, which happens on the
  // processing thread and so lands a tick or two after the stop was asked for.
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 1U; }));

  ASSERT_EQ(WrittenFiles().size(), 1U);
  const std::filesystem::path capture_file = WrittenFiles().front();
  EXPECT_EQ(MetadataFiles().front(),
            capture::CaptureMetadataPath(capture_file));

  const std::string document = ReadWholeFile(MetadataFiles().front());
  EXPECT_NE(document.find("schema_version"), std::string::npos);
  EXPECT_NE(document.find(capture_file.filename().string()), std::string::npos);
  EXPECT_NE(document.find("\"completed\": true"), std::string::npos)
      << document;

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// Metadata is data about the data. A capture taken after a stretch of
// monitoring must describe the samples in the file, not everything that went
// past while somebody was setting up — so the count the document reports is the
// count the file actually holds.
//
// The sharper form of this — that a loud buffer seen while monitoring does not
// raise the file's maximum — is in test_sample_metrics.cpp, where the two spans
// can be fed different signals. A synthetic source produces the same signal
// before and after the writer is attached, so end to end there is nothing to
// tell the two apart by except the count.
TEST_F(CaptureToDiskTest, TheFiguresCountOnlyWhatReachedTheFile) {
  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  // A stretch of the session with nothing being written, so that a figure
  // covering the session would be visibly larger than one covering the file.
  // There is nothing to wait *for* here — the point is that samples go past —
  // so the pump runs to its deadline on a predicate that is never true.
  PumpUntil([] { return false; }, 100ms);

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 4096;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 1U; }));

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(reader.Open(WrittenFiles().front(),
                          capture::CaptureReader::Format::kFlac, error))
      << error;

  const std::optional<uint64_t> in_the_file = reader.TotalSamples();
  ASSERT_TRUE(in_the_file.has_value());

  const std::string document = ReadWholeFile(MetadataFiles().front());
  EXPECT_NE(
      document.find("\"samples\": " + std::to_string(in_the_file.value_or(0))),
      std::string::npos)
      << document;

  // And the signal section is there, measured over that same span.
  EXPECT_NE(document.find("\"signal\":"), std::string::npos) << document;

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureToDiskTest, WhatTheUserSaidTheDiscWasReachesBothTheNameAndFile) {
  Settings([](CaptureSettings& settings) {
    settings.naming.title_used = true;
    settings.naming.title = "Casper";
    settings.naming.side_used = true;
    settings.naming.side = 2;
    settings.naming.metadata_notes = "Rot on the outer edge.";
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  // The title and the side are in the name; the paragraph of notes is not — a
  // paragraph is not a file name.
  const QString path = controller_->capture_path();
  EXPECT_TRUE(path.contains(QStringLiteral("Casper_side2")))
      << path.toStdString();
  EXPECT_FALSE(path.contains(QStringLiteral("Rot"))) << path.toStdString();

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 1U; }));

  const std::string document = ReadWholeFile(MetadataFiles().front());
  EXPECT_NE(document.find("\"title\": \"Casper\""), std::string::npos)
      << document;
  EXPECT_NE(document.find("\"side\": 2"), std::string::npos) << document;
  EXPECT_NE(document.find("Rot on the outer edge."), std::string::npos)
      << document;

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureToDiskTest, ThePlayerAndTheDiscScanReachTheMetadataFile) {
  // Set the way the automatic-capture coupling sets it, which is the only route
  // either of these takes — and latched when the file is opened, because the
  // coupling clears both the moment its run ends.
  capture::PlayerIdentity player;
  player.model_name = "Pioneer LD-V4300D";
  player.firmware_version = "12";
  player.port = "/dev/ttyUSB0";
  controller_->SetPlayerIdentity(player);

  capture::DiscScan scan;
  scan.examined = true;
  scan.disc_type = capture::ScannedFact{"CLV", "reported"};
  scan.disc_side = capture::ScannedFact{"2", "reported"};
  scan.programme_end = capture::ScannedFact{"1:02:03", "measured"};
  scan.disc_status_reply = "11011";
  controller_->SetDiscScan(scan);

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  // Cleared while the capture is still running, exactly as the coupling clears
  // it when its run finishes. The file must still carry what was true when it
  // was opened.
  controller_->SetDiscScan({});
  controller_->SetPlayerIdentity({});

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 1U; }));

  const std::string document = ReadWholeFile(MetadataFiles().front());
  EXPECT_NE(document.find("Pioneer LD-V4300D"), std::string::npos) << document;
  EXPECT_NE(document.find("\"examined\": true"), std::string::npos) << document;
  EXPECT_NE(document.find("\"source\": \"measured\""), std::string::npos)
      << document;
  EXPECT_NE(document.find("\"disc_status_reply\": \"11011\""),
            std::string::npos)
      << document;

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureToDiskTest, TheDurationJoinsTheNameByRenamingTheFinishedFile) {
  Settings([](CaptureSettings& settings) {
    settings.capture_name = QStringLiteral("Casper");
    settings.naming.append_duration = true;
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());

  const QString opened = controller_->capture_path();
  EXPECT_TRUE(opened.endsWith(QStringLiteral("Casper.ddd.flac")))
      << opened.toStdString();

  ASSERT_TRUE(PumpUntil([&] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 4096;
  }));

  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 1U; }));

  // The length is not a fact until the capture has stopped, so the file is
  // renamed rather than the length having been guessed at the start.
  ASSERT_EQ(WrittenFiles().size(), 1U);
  const std::string written = WrittenFiles().front().filename().string();
  EXPECT_NE(written.find("Casper_00H00M"), std::string::npos) << written;

  // And the metadata file goes with it. A sidecar left under the old name would
  // be orphaned by the rename, which is the whole reason the two happen in one
  // place and in this order.
  EXPECT_EQ(MetadataFiles().front(),
            capture::CaptureMetadataPath(WrittenFiles().front()));
  EXPECT_EQ(controller_->capture_path(),
            QString::fromStdString(WrittenFiles().front().string()));

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

TEST_F(CaptureToDiskTest, AnUncompressedCaptureGetsTheSameMetadataFile) {
  // The format with no tags of its own, which is exactly the case the sidecar
  // matters most for: without it, nothing anywhere says what the file is.
  Settings([](CaptureSettings& settings) {
    settings.output_format = capture::CaptureOutputFormat::kSigned16Bit;
  });

  controller_->StartCapture();
  ASSERT_TRUE(controller_->capturing());
  controller_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->capturing(); }));
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 1U; }));

  const std::string document = ReadWholeFile(MetadataFiles().front());
  EXPECT_NE(document.find("\"format\": \"signed 16-bit\""), std::string::npos)
      << document;

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

}  // namespace
}  // namespace ddd::gui
