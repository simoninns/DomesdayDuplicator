/************************************************************************

    test_auto_capture_controller.cpp

    T1 tests for the coupling, with no player and no Duplicator
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "auto_capture_controller.h"
#include "capture_controller.h"
#include "capture_format.h"
#include "capture_metadata.h"
#include "capture_reader.h"
#include "disk_buffer_ring.h"
#include "fake_serial_port.h"
#include "fake_usb_device.h"
#include "firmware_version.h"
#include "player_controller.h"
#include "serial_port_scanner.h"
#include "synthetic_source.h"
#include "version.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

constexpr const char* kPortPath = "/dev/ttyFAKE0";
constexpr const char* kLdV4300DReply = "P1515A1";

constexpr size_t kTestSlotBytes = size_t{256} << 10;
constexpr size_t kTestSlotCount = 6;

template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 20000ms) {
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

// An NTSC CAV side of 54,000 frames, as an examination would have left it.
player::DiscProfile CavDisc() {
  player::DiscProfile disc;
  disc.disc_present.Record(true, player::Provenance::kReported);
  disc.disc_type.Record(player::DiscType::kCav, player::Provenance::kReported);
  disc.addressing.Record(player::AddressMode::kFrame,
                         player::Provenance::kInferred);
  disc.disc_side.Record(1, player::Provenance::kReported);
  disc.disc_size.Record(player::DiscSize::k30cm, player::Provenance::kReported);
  disc.programme_start.Record(1, player::Provenance::kMeasured);
  disc.programme_end.Record(54000, player::Provenance::kMeasured);
  disc.lead_in_reachable.Record(true, player::Provenance::kMeasured);
  disc.video_standard.Record(player::VideoStandard::kNtsc,
                             player::Provenance::kReported);
  return disc;
}

class AutoCaptureControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-auto-%1").arg(QLatin1String(info->name())));
    QSettings().clear();

    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-auto-test-") + info->name());
    std::filesystem::remove_all(directory_);
    std::filesystem::create_directories(directory_);

    // --- The Duplicator, which is not there ---------------------------------
    capture::SyntheticSource::Options options;
    options.slot_size_bytes = kTestSlotBytes;
    options.slot_count = kTestSlotCount;

    device_ = std::make_unique<capture::FakeUsbDevice>();
    device_->SetSourceOptions(options);
    device_->SetSingleDevice("bus-1", capture::DeviceSpeed::kSuper,
                             MatchingProductString());

    capture_ = std::make_unique<CaptureController>(device_.get(), nullptr);

    CaptureSettings settings = capture_->settings();
    settings.queue_size_bytes = capture::DiskBufferRing::kMinimumQueueSizeBytes;
    settings.preferred_device_path = QStringLiteral("bus-1");
    settings.capture_directory = QString::fromStdString(directory_.string());
    settings.compression_level = 0;
    capture_->SetSettings(settings);

    // --- The player, which is not there either -----------------------------
    SerialPortCandidate candidate;
    candidate.path = QLatin1String(kPortPath);
    candidate.usb_adapter = true;
    ports_.push_back(candidate);
    port_.set_only_path(kPortPath);

    port_.AddPioneerPlayer(9600, kLdV4300DReply);
    port_.AddStatusResponses(9600, "P04", "10001", "0000001");

    // Everything the automatic capture sends, answered as a player would. The
    // address climbs so that the watch sees the disc advancing and then reach
    // the end of the plan.
    port_.AddResponse(9600, "1KL\r", "R\r");
    port_.AddResponse(9600, "0KL\r", "R\r");
    port_.AddResponse(9600, "RJ\r", "R\r");
    port_.AddResponse(9600, "PL64RBMF\r", "R\r");
    port_.AddResponse(9600, "PL\r", "R\r");
  }

  void TearDown() override {
    auto_capture_.reset();
    player_.reset();
    capture_.reset();
    device_.reset();
    std::filesystem::remove_all(directory_);
    QSettings().clear();
  }

  static std::string MatchingProductString() {
    const std::optional<std::string> commit =
        capture::NormaliseCommit(capture::Version());
    return "Domesday Duplicator (" + commit.value_or("a1b2c3d4") + ")";
  }

  // What the player answers the address query with, in order. The last answer
  // repeats, which is how a disc that has reached the end of the plan behaves
  // until something stops it.
  void SetAddresses(std::vector<std::string> replies) {
    for (std::string& reply : replies) {
      reply += "\r";
    }
    port_.AddResponseSequence(9600, "?F\r", std::move(replies));
  }

  void BuildAndConnect() {
    PlayerBackend backend;
    backend.make_port = [this] {
      return std::make_unique<player::BorrowedSerialPort>(&port_);
    };
    backend.list_ports = [this] { return ports_; };

    // The real clock, not the fake's. This test drives the controller through a
    // live event loop with real timers in it — the watch's own half-second
    // pacing among them — and a clock that jumped whenever a read went
    // unanswered would run those timers out of step with it.
    player_ = std::make_unique<PlayerController>(std::move(backend));

    auto_capture_ = std::make_unique<AutoCaptureController>(
        player_.get(), capture_.get(), nullptr);

    player_->Start();
    player_->SetEnabled(true);

    ASSERT_TRUE(PumpUntil([this] { return player_->connected(); }));
  }

  // The recordings only. The metadata sidecar written beside each one is not a
  // second capture, and it lands a statistics tick after the file is closed —
  // so a listing that counted it would be a race as well as a miscount.
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

  std::filesystem::path directory_;
  std::unique_ptr<capture::FakeUsbDevice> device_;
  std::unique_ptr<CaptureController> capture_;

  player::FakeSerialPort port_;
  std::vector<SerialPortCandidate> ports_;
  std::unique_ptr<PlayerController> player_;

  std::unique_ptr<AutoCaptureController> auto_capture_;
};

// --- An automatic capture, end to end --------------------------------------

// The plan's acceptance criterion: a whole automatic capture against a fake
// player and the existing fake USB backend, producing a file with the expected
// provenance — no hardware and no player.
TEST_F(AutoCaptureControllerTest, AWholeSideIsCapturedWithTheDiscFactsInIt) {
  SetAddresses({"0000001", "0001000", "0054000"});
  BuildAndConnect();

  const player::DiscProfile disc = CavDisc();
  player::AutoCapturePlan plan = player::DefaultPlanFor(disc);
  plan.key_lock = true;

  QSignalSpy finished(auto_capture_.get(), &AutoCaptureController::Finished);

  auto_capture_->Start(plan, disc);
  EXPECT_TRUE(auto_capture_->running());

  ASSERT_TRUE(PumpUntil([&] { return finished.count() == 1; }));
  EXPECT_FALSE(auto_capture_->running());

  EXPECT_EQ(finished.front().at(0).value<player::AutoCaptureOutcome>(),
            player::AutoCaptureOutcome::kCompleted);

  // The file is closed and the writer detached.
  ASSERT_TRUE(PumpUntil([&] { return !capture_->capturing(); }));

  const std::vector<std::filesystem::path> files = WrittenFiles();
  ASSERT_EQ(files.size(), 1U);

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(
      reader.Open(files.front(), capture::CaptureReader::Format::kFlac, error))
      << error;

  const auto tag = [&reader](const std::string& name) -> std::string {
    for (const auto& [key, value] : reader.Tags()) {
      if (key == name) {
        return value;
      }
    }
    return {};
  };

  // What the disc was, in the file — which is the whole point of the coupling
  // writing it there rather than into a note somebody keeps separately.
  EXPECT_EQ(tag(capture::kTagDiscType), "CAV");
  EXPECT_EQ(tag(capture::kTagDiscSide), "1");
  EXPECT_EQ(tag(capture::kTagVideoStandard), "NTSC");
  EXPECT_FALSE(tag(capture::kTagPlayer).empty());

  // And the sidecar beside it, which carries what the tags cannot: the whole
  // examination, with how each fact was established. Waited for rather than
  // assumed — it is written when the file is closed, which is a statistics tick
  // after the writer was detached.
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 1U; }));
  EXPECT_EQ(MetadataFiles().front(),
            capture::CaptureMetadataPath(files.front()));

  std::ifstream metadata(MetadataFiles().front(), std::ios::binary);
  const std::string document((std::istreambuf_iterator<char>(metadata)),
                             std::istreambuf_iterator<char>());

  EXPECT_NE(document.find("\"examined\": true"), std::string::npos) << document;
  EXPECT_NE(document.find("\"source\": \"reported\""), std::string::npos)
      << document;
  EXPECT_NE(document.find("\"model_name\""), std::string::npos) << document;

  // And it does not leak into the next capture, which may be of another disc.
  EXPECT_TRUE(capture_->disc_provenance().empty());
}

TEST_F(AutoCaptureControllerTest, TheDiscIsStoppedAndTheFrontPanelReleased) {
  SetAddresses({"0054000"});
  BuildAndConnect();

  const player::DiscProfile disc = CavDisc();
  player::AutoCapturePlan plan = player::DefaultPlanFor(disc);
  plan.key_lock = true;

  QSignalSpy finished(auto_capture_.get(), &AutoCaptureController::Finished);
  auto_capture_->Start(plan, disc);
  ASSERT_TRUE(PumpUntil([&] { return finished.count() == 1; }));

  const std::vector<std::string> written = port_.writes();
  const auto sent = [&written](const std::string& command) {
    return std::find(written.begin(), written.end(), command) != written.end();
  };

  EXPECT_TRUE(sent("1KL\r"));
  EXPECT_TRUE(sent("PL64RBMF\r"));
  EXPECT_TRUE(sent("RJ\r"));
  EXPECT_TRUE(sent("0KL\r"));
}

TEST_F(AutoCaptureControllerTest, CancellingFinishesTheFileProperly) {
  // The address never reaches the end of the plan, so only a cancel ends this.
  SetAddresses({"0000001", "0000002", "0000003", "0000004", "0000005",
                "0000006", "0000007", "0000008"});
  BuildAndConnect();

  const player::DiscProfile disc = CavDisc();
  QSignalSpy finished(auto_capture_.get(), &AutoCaptureController::Finished);

  auto_capture_->Start(player::DefaultPlanFor(disc), disc);

  ASSERT_TRUE(PumpUntil([&] { return capture_->capturing(); }));
  auto_capture_->Cancel();

  ASSERT_TRUE(PumpUntil([&] { return finished.count() == 1; }));
  EXPECT_EQ(finished.front().at(0).value<player::AutoCaptureOutcome>(),
            player::AutoCaptureOutcome::kCancelled);

  // The difference from the old application, which abandoned the run where it
  // stood: the writer is detached and the file finalised.
  EXPECT_FALSE(capture_->capturing());

  // Detached is not finished. Cancelling clears `capturing` the moment the sink
  // is detached, and the encoder is still writing out its last frames and
  // patching the header behind that — which is the whole point of detaching
  // rather than stopping, and is why reading the file on the strength of
  // `capturing` alone was a race that CI lost. The sidecar is the fact to wait
  // for: it is written when the file is closed, a statistics tick later, and
  // after any rename the naming asks for.
  ASSERT_TRUE(PumpUntil([&] { return MetadataFiles().size() == 1U; }));

  const std::vector<std::filesystem::path> files = WrittenFiles();
  ASSERT_EQ(files.size(), 1U);

  capture::CaptureReader reader;
  std::string error;
  ASSERT_TRUE(
      reader.Open(files.front(), capture::CaptureReader::Format::kFlac, error))
      << error;
}

TEST_F(AutoCaptureControllerTest,
       ALinkThatDiesMidCaptureLeavesTheCaptureRunning) {
  SetAddresses({"0000001", "0000002", "0000003", "0000004", "0000005",
                "0000006", "0000007", "0000008"});
  BuildAndConnect();

  const player::DiscProfile disc = CavDisc();
  QSignalSpy finished(auto_capture_.get(), &AutoCaptureController::Finished);

  auto_capture_->Start(player::DefaultPlanFor(disc), disc);
  ASSERT_TRUE(PumpUntil([&] { return capture_->capturing(); }));

  // The cable comes out.
  port_.set_link_broken(true);

  ASSERT_TRUE(PumpUntil([&] { return finished.count() == 1; }));
  EXPECT_EQ(finished.front().at(0).value<player::AutoCaptureOutcome>(),
            player::AutoCaptureOutcome::kLinkFailed);

  // The plan's rule: a player fault never destroys a capture. The automation
  // has ended and the capture is the user's to stop.
  EXPECT_FALSE(auto_capture_->running());
  EXPECT_TRUE(capture_->capturing());

  capture_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !capture_->capturing(); }));
}

TEST_F(AutoCaptureControllerTest, APlanThatCannotBeRunSendsNothing) {
  BuildAndConnect();

  const player::DiscProfile disc = CavDisc();
  player::AutoCapturePlan plan = player::DefaultPlanFor(disc);
  plan.end_address = 99999;  // past the measured end of the side

  QSignalSpy finished(auto_capture_.get(), &AutoCaptureController::Finished);
  auto_capture_->Start(plan, disc);

  ASSERT_EQ(finished.count(), 1);
  EXPECT_EQ(finished.front().at(0).value<player::AutoCaptureOutcome>(),
            player::AutoCaptureOutcome::kInvalidPlan);
  EXPECT_FALSE(auto_capture_->running());
  EXPECT_FALSE(capture_->capturing());
}

// --- The coupling, and the direction it does not run in --------------------

// **A manual capture never touches the player**, and this is the test that says
// so. There is no preference for it: the disc belongs to whoever is operating
// it, and pressing Stop capture is a statement about a file.
//
// It is also the unsafe direction. The stop command is Reject on a Pioneer
// player and a Reject arriving while the disc is already spinning down opens
// the tray — so the old application's preference, which fired on every capture
// stop, could eject somebody's disc with nobody in the room.
TEST_F(AutoCaptureControllerTest, StoppingAManualCaptureLeavesThePlayerAlone) {
  BuildAndConnect();

  capture_->StartCapture();
  ASSERT_TRUE(capture_->capturing());
  capture_->StopCapture();

  // Pumped for long enough that a stop would have been sent had one been going
  // to be. Asserting a negative needs the wait; there is no signal for
  // "nothing happened".
  PumpUntil([] { return false; }, 200ms);

  const std::vector<std::string> written = port_.writes();
  EXPECT_EQ(std::find(written.begin(), written.end(), "RJ\r"), written.end())
      << "stopping a manual capture stopped the player";

  capture_->StopMonitoring();
}

// Nor does starting or stopping monitoring, which is the same rule reached by
// the other route somebody uses the two buttons.
TEST_F(AutoCaptureControllerTest, MonitoringLeavesThePlayerAlone) {
  BuildAndConnect();

  capture_->StartMonitoring();
  ASSERT_TRUE(capture_->monitoring());
  capture_->StopMonitoring();

  PumpUntil([this] { return !capture_->monitoring(); }, 200ms);

  const std::vector<std::string> written = port_.writes();
  EXPECT_EQ(std::find(written.begin(), written.end(), "RJ\r"), written.end());
}

// Nothing opens the tray on its own initiative, ever. "OP" reaches the port
// only from the remote, where a person presses it — a machine ejecting a disc
// unasked is not something this application does.
TEST_F(AutoCaptureControllerTest, NothingEjectsTheDiscByItself) {
  BuildAndConnect();

  capture_->StartCapture();
  ASSERT_TRUE(capture_->capturing());
  capture_->StopCapture();
  PumpUntil([] { return false; }, 200ms);
  capture_->StopMonitoring();

  const std::vector<std::string> written = port_.writes();
  EXPECT_EQ(std::find(written.begin(), written.end(), "OP\r"), written.end())
      << "the disc tray was opened without anybody asking";
}

TEST_F(AutoCaptureControllerTest, OneStoppedReadingDoesNotTruncateACapture) {
  BuildAndConnect();

  PlayerSettings settings = player_->settings();
  settings.stop_capture_with_player = true;
  player_->SetSettings(settings);

  capture_->StartCapture();
  ASSERT_TRUE(capture_->capturing());

  // The single stumble a disc with a defect produces, and then the player is
  // playing again. This is the whole reason the preference is debounced, and it
  // is checked without an event loop because the debounce is a pure count.
  player::PlayerStatus stopped;
  stopped.state = player::PlayerState::kParked;
  player::PlayerStatus playing;
  playing.state = player::PlayerState::kPlaying;

  emit player_->StatusUpdated(stopped);
  emit player_->StatusUpdated(playing);
  emit player_->StatusUpdated(stopped);

  EXPECT_TRUE(capture_->capturing());

  capture_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !capture_->capturing(); }));
  capture_->StopMonitoring();
}

TEST_F(AutoCaptureControllerTest, APlayerThatStaysStoppedStopsTheCapture) {
  BuildAndConnect();

  PlayerSettings settings = player_->settings();
  settings.stop_capture_with_player = true;
  player_->SetSettings(settings);

  capture_->StartCapture();
  ASSERT_TRUE(capture_->capturing());

  player::PlayerStatus playing;
  playing.state = player::PlayerState::kPlaying;
  player::PlayerStatus stopped;
  stopped.state = player::PlayerState::kParked;

  // A disc that was turning, and then was not. The first reading is what makes
  // the ones after it a stop rather than a state.
  emit player_->StatusUpdated(playing);

  for (int reading = 0; reading < AutoCaptureController::kStoppedReadings;
       ++reading) {
    emit player_->StatusUpdated(stopped);
  }

  EXPECT_FALSE(capture_->capturing());
  capture_->StopMonitoring();
}

// The bug this guards against: press Start capture with a player connected and
// parked — which is the ordinary order, the disc being started afterwards — and
// the watch ended the capture within a second of it opening, having read the
// state the capture began in as an event.
TEST_F(AutoCaptureControllerTest, APlayerParkedAllAlongDoesNotStopTheCapture) {
  BuildAndConnect();

  PlayerSettings settings = player_->settings();
  settings.stop_capture_with_player = true;
  player_->SetSettings(settings);

  capture_->StartCapture();
  ASSERT_TRUE(capture_->capturing());

  player::PlayerStatus stopped;
  stopped.state = player::PlayerState::kParked;
  for (int reading = 0; reading < 10; ++reading) {
    emit player_->StatusUpdated(stopped);
  }

  EXPECT_TRUE(capture_->capturing())
      << "a capture was stopped by a player that never started";

  capture_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !capture_->capturing(); }));
  capture_->StopMonitoring();
}

// And the whole of the intended sequence, in the order somebody works in: open
// the file, start the disc, let it run, stop the disc. Only the last of those
// stops the capture.
TEST_F(AutoCaptureControllerTest, TheWatchArmsWhenTheDiscStarts) {
  BuildAndConnect();

  PlayerSettings settings = player_->settings();
  settings.stop_capture_with_player = true;
  player_->SetSettings(settings);

  capture_->StartCapture();
  ASSERT_TRUE(capture_->capturing());

  player::PlayerStatus stopped;
  stopped.state = player::PlayerState::kParked;
  player::PlayerStatus spinning_up;
  spinning_up.state = player::PlayerState::kSettingUp;
  player::PlayerStatus playing;
  playing.state = player::PlayerState::kPlaying;

  // Walking over to the player.
  for (int reading = 0; reading < 10; ++reading) {
    emit player_->StatusUpdated(stopped);
  }
  ASSERT_TRUE(capture_->capturing());

  // Play pressed, and the side captured.
  emit player_->StatusUpdated(spinning_up);
  for (int reading = 0; reading < 10; ++reading) {
    emit player_->StatusUpdated(playing);
  }
  ASSERT_TRUE(capture_->capturing());

  // Stop pressed, at the end of the side.
  for (int reading = 0; reading < AutoCaptureController::kStoppedReadings;
       ++reading) {
    emit player_->StatusUpdated(stopped);
  }

  EXPECT_FALSE(capture_->capturing());
  capture_->StopMonitoring();
}

// A second capture in the same session starts disarmed too. The flag lives with
// the capture, not with the link — otherwise the first side would arm the watch
// and the second capture would be stopped before its disc was started.
TEST_F(AutoCaptureControllerTest, TheWatchDisarmsForEachNewCapture) {
  BuildAndConnect();

  PlayerSettings settings = player_->settings();
  settings.stop_capture_with_player = true;
  player_->SetSettings(settings);

  player::PlayerStatus stopped;
  stopped.state = player::PlayerState::kParked;
  player::PlayerStatus playing;
  playing.state = player::PlayerState::kPlaying;

  capture_->StartCapture();
  ASSERT_TRUE(capture_->capturing());
  emit player_->StatusUpdated(playing);
  for (int reading = 0; reading < AutoCaptureController::kStoppedReadings;
       ++reading) {
    emit player_->StatusUpdated(stopped);
  }
  ASSERT_FALSE(capture_->capturing());

  // The disc turned over, and a second capture opened against a player that is
  // still parked.
  ASSERT_TRUE(PumpUntil([&] { return !capture_->capturing(); }));
  capture_->StartCapture();
  ASSERT_TRUE(capture_->capturing());

  for (int reading = 0; reading < 10; ++reading) {
    emit player_->StatusUpdated(stopped);
  }

  EXPECT_TRUE(capture_->capturing())
      << "the watch stayed armed from the previous capture";

  capture_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !capture_->capturing(); }));
  capture_->StopMonitoring();
}

TEST_F(AutoCaptureControllerTest, ThePreferenceIsOffUntilItIsTurnedOn) {
  BuildAndConnect();

  ASSERT_FALSE(player_->settings().stop_capture_with_player);

  capture_->StartCapture();
  ASSERT_TRUE(capture_->capturing());

  player::PlayerStatus stopped;
  stopped.state = player::PlayerState::kParked;
  for (int reading = 0; reading < 10; ++reading) {
    emit player_->StatusUpdated(stopped);
  }

  EXPECT_TRUE(capture_->capturing());

  capture_->StopCapture();
  ASSERT_TRUE(PumpUntil([&] { return !capture_->capturing(); }));
  capture_->StopMonitoring();
}

}  // namespace
}  // namespace ddd::gui
