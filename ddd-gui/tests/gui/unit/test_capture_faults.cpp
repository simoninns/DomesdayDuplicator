/************************************************************************

    test_capture_faults.cpp

    T1 fault-injection tests: every failure reaches the user as its own message
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
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include "capture_controller.h"
#include "capture_failure_presenter.h"
#include "capture_reader.h"
#include "fake_usb_device.h"
#include "firmware_version.h"
#include "synthetic_source.h"
#include "version.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

// Task 5.2's acceptance criterion: each error path triggered by fault
// injection, and each showing its specific message rather than a generic one.
//
// Driven through the controller rather than through the pipeline, because the
// message is what is being tested and the controller is where a result code
// becomes one. The taxonomy itself is covered in test_capture_pipeline.cpp; the
// wording, in test_capture_failure_presenter.cpp. This is the join between
// them, and it is the join that a user actually experiences.
//
// Two codes cannot be reached this way and are not pretended to be:
// kUsbMemoryLimit is raised by the Linux libusb backend when the kernel refuses
// the submission, and kHostUnderflow by the Windows backend's own probe.
// Neither has a synthetic equivalent, because neither is a property of the
// stream — both are properties of an operating system's USB stack. Their
// messages are covered by the presenter test, and their production by the
// hardware tier.

constexpr size_t kTestSlotBytes = size_t{256} << 10;
constexpr size_t kTestSlotCount = 6;

// What the user was actually shown when a capture failed.
struct Shown {
  QString title;
  QString message;
};

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

class CaptureFaultTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-fault-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-fault-test-") + info->name());
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

  void InjectFault(capture::SyntheticSource::Fault fault,
                   uint64_t at_slot = 3) {
    capture::SyntheticSource::Options options = TestSourceOptions();
    options.fault = fault;
    options.fault_at_slot = at_slot;
    device_->SetSourceOptions(options);
  }

  void UseTestMode() {
    CaptureSettings settings = controller_->settings();
    settings.test_mode = true;
    controller_->SetSettings(settings);
  }

  // Start a capture, wait for it to fail, and hand back what the user was
  // shown.
  Shown CaptureUntilItFails() {
    QSignalSpy failures(controller_.get(), &CaptureController::Failed);

    controller_->StartCapture();
    if (!controller_->capturing()) {
      return {
          failures.count() > 0 ? failures.front().at(0).toString() : QString(),
          failures.count() > 0 ? failures.front().at(1).toString() : QString()};
    }

    EXPECT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
    EXPECT_TRUE(PumpUntil([&] { return failures.count() >= 1; }));

    if (failures.count() == 0) {
      return {};
    }
    return {failures.front().at(0).toString(),
            failures.front().at(1).toString()};
  }

  std::vector<std::filesystem::path> WrittenFiles() const {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
      files.push_back(entry.path());
    }
    return files;
  }

  std::filesystem::path directory_;
  std::unique_ptr<capture::FakeUsbDevice> device_;
  std::unique_ptr<CaptureController> controller_;
};

// Every check below asserts three things about the message: that it names the
// failure code, that it carries that failure's own remedy, and that the remedy
// is not one of the other failures' remedies. The third is what makes this more
// than a spelling test — without it a presenter that returned the same sentence
// for everything would pass.

void ExpectSpecific(const Shown& shown, capture::TransferResult expected) {
  EXPECT_TRUE(
      shown.title.contains(QString::fromUtf8(TransferResultName(expected))))
      << shown.title.toStdString();

  const CaptureFailureView view =
      PresentCaptureFailure(expected, QString(), QString());
  EXPECT_TRUE(shown.message.contains(view.remedy))
      << shown.message.toStdString();

  // And not somebody else's remedy. A generic message would satisfy everything
  // above and fail here.
  const capture::TransferResult others[] = {
      capture::TransferResult::kSequenceMismatch,
      capture::TransferResult::kUsbTransferFailure,
      capture::TransferResult::kSourceStalled,
      capture::TransferResult::kVerificationError,
      capture::TransferResult::kFileWriteError,
      capture::TransferResult::kFileCreationError,
  };
  for (const capture::TransferResult other : others) {
    if (other == expected) {
      continue;
    }
    const CaptureFailureView wrong =
        PresentCaptureFailure(other, QString(), QString());
    EXPECT_FALSE(shown.message.contains(wrong.remedy))
        << "the message for " << TransferResultName(expected)
        << " also carries the remedy for " << TransferResultName(other);
  }
}

TEST_F(CaptureFaultTest, ASequenceBreakIsReportedAsASequenceBreak) {
  InjectFault(capture::SyntheticSource::Fault::kSequenceBreak);
  ExpectSpecific(CaptureUntilItFails(),
                 capture::TransferResult::kSequenceMismatch);
}

TEST_F(CaptureFaultTest, ALostDeviceIsReportedAsATransferFailure) {
  InjectFault(capture::SyntheticSource::Fault::kTransferFailure);
  ExpectSpecific(CaptureUntilItFails(),
                 capture::TransferResult::kUsbTransferFailure);
}

TEST_F(CaptureFaultTest, AShortPacketIsReportedAsATransferFailure) {
  InjectFault(capture::SyntheticSource::Fault::kShortDelivery);
  ExpectSpecific(CaptureUntilItFails(),
                 capture::TransferResult::kUsbTransferFailure);
}

TEST_F(CaptureFaultTest, ASilentDeviceIsReportedAsAStall) {
  InjectFault(capture::SyntheticSource::Fault::kStall);
  ExpectSpecific(CaptureUntilItFails(),
                 capture::TransferResult::kSourceStalled);
}

// The corruption the sequence markers cannot see. The counters carry on in
// perfect order and only the ramp check finds it, so a pipeline that reported
// this as a sequence mismatch would be sending somebody to look in the wrong
// place.
TEST_F(CaptureFaultTest, ABrokenRampIsReportedAsAVerificationFailure) {
  UseTestMode();
  InjectFault(capture::SyntheticSource::Fault::kRampBreak);
  ExpectSpecific(CaptureUntilItFails(),
                 capture::TransferResult::kVerificationError);
}

TEST_F(CaptureFaultTest, AFileThatCannotBeCreatedIsReportedAsSuch) {
  // A path below a regular file, which no platform will turn into a directory.
  // This used to be /dev/null/not-a-directory, which is only unusable on Unix:
  // on Windows those are ordinary names, the directory was created, and the
  // capture started perfectly happily.
  const std::filesystem::path blocker = directory_ / "a-regular-file";
  {
    std::ofstream file(blocker);
    ASSERT_TRUE(file.good());
  }

  CaptureSettings settings = controller_->settings();
  settings.capture_directory =
      QString::fromStdString((blocker / "not-a-directory").string());
  controller_->SetSettings(settings);

  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  QSignalSpy failures(controller_.get(), &CaptureController::Failed);
  controller_->StartCapture();
  ASSERT_FALSE(controller_->capturing());
  ASSERT_EQ(failures.count(), 1);

  ExpectSpecific(
      {failures.front().at(0).toString(), failures.front().at(1).toString()},
      capture::TransferResult::kFileCreationError);

  controller_->StopMonitoring();
  ASSERT_TRUE(PumpUntil([&] { return !controller_->monitoring(); }));
}

// --- What is left on the disk --------------------------------------------

// The plan's other acceptance criterion for Task 5.2: a capture that fails
// mid-write keeps what was written and finalises the container, so the partial
// file is readable rather than a stream with no end.
TEST_F(CaptureFaultTest, AFailureMidCaptureLeavesAReadableFile) {
  // Late enough that several buffers have been written before it goes.
  InjectFault(capture::SyntheticSource::Fault::kTransferFailure, 12);

  const Shown shown = CaptureUntilItFails();
  ASSERT_FALSE(shown.title.isEmpty());

  ASSERT_EQ(WrittenFiles().size(), 1U);
  const std::filesystem::path path = WrittenFiles().front();
  EXPECT_GT(std::filesystem::file_size(path), 0U);

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(reader.Open(path, capture::CaptureReader::Format::kFlac, error))
      << error;

  std::vector<uint16_t> samples;
  bool end_of_file = false;
  ASSERT_TRUE(reader.Read(samples, 4096, end_of_file));
  EXPECT_FALSE(samples.empty());

  // Finalised, not merely flushed. A stream whose header was never patched
  // reports no length at all, and this is the difference between a partial
  // capture somebody can still use and one every tool treats as damaged.
  EXPECT_TRUE(reader.TotalSamples().has_value())
      << "the FLAC header was not patched, so the file does not know its own "
         "length";
}

// And the user is told where it is, which is the first thing anybody wants to
// know after losing a capture.
TEST_F(CaptureFaultTest, TheMessageSaysWhereThePartialCaptureIs) {
  InjectFault(capture::SyntheticSource::Fault::kTransferFailure, 12);

  const Shown shown = CaptureUntilItFails();

  ASSERT_EQ(WrittenFiles().size(), 1U);
  EXPECT_TRUE(shown.message.contains(
      QString::fromStdString(WrittenFiles().front().string())))
      << shown.message.toStdString();
}

// A failure while merely monitoring names no file, because there is none. A
// message that mentioned one would send somebody looking for a capture that was
// never started.
TEST_F(CaptureFaultTest, AMonitorFailureMentionsNoFile) {
  InjectFault(capture::SyntheticSource::Fault::kTransferFailure);

  QSignalSpy failures(controller_.get(), &CaptureController::Failed);
  controller_->StartMonitoring();
  ASSERT_TRUE(controller_->monitoring());

  ASSERT_TRUE(PumpUntil([&] { return failures.count() >= 1; }));
  EXPECT_FALSE(
      failures.front().at(1).toString().contains(QStringLiteral("readable")));
  EXPECT_TRUE(WrittenFiles().empty());
}

}  // namespace
}  // namespace ddd::gui
