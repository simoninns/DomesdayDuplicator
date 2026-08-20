/************************************************************************

    test_capture_control_server.cpp

    T1 tests for the socket a script stops a capture through
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSettings>
#include <QString>
#include <QTextStream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture_cli.h"
#include "capture_control_server.h"
#include "capture_controller.h"
#include "capture_format.h"
#include "capture_stop_client.h"
#include "disk_buffer_ring.h"
#include "fake_usb_device.h"
#include "logger.h"
#include "synthetic_source.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

// --- The protocol ---------------------------------------------------------
//
// No socket, no server and no event loop: the wording of a request and the
// reading of a reply are arithmetic, and everything below tests them as such.

TEST(CaptureControlProtocolTest, ARequestSaysWhatItWants) {
  const QString line = FormatControlRequest(QLatin1String(kStopVerb));

  EXPECT_TRUE(line.endsWith(QLatin1Char('\n')));
  EXPECT_EQ(ParseControlVerb(line.toUtf8()).value_or(QString()),
            QLatin1String(kStopVerb));
}

// The reason this is JSON rather than a word and a path with a space between
// them. A capture is named after a disc, and discs have commas, spaces and
// accents in their titles.
TEST(CaptureControlProtocolTest, ANameWithPunctuationInItSurvives) {
  const QString path =
      QStringLiteral("/captures/Bram Stoker's Dracula, Side 2 (café).ddd.flac");

  const ControlReply reply =
      ParseControlReply(FormatControlReplyStopped(path, 4096).toUtf8())
          .value_or(ControlReply{});

  EXPECT_TRUE(reply.ok);
  EXPECT_EQ(reply.file_path, path);
  EXPECT_EQ(reply.bytes, 4096U);
}

// A capture is tens of gigabytes, which is past what a 32-bit count holds.
TEST(CaptureControlProtocolTest, ALargeCaptureIsCountedProperly) {
  constexpr quint64 kBytes = quint64{40} << 30;

  const ControlReply reply =
      ParseControlReply(FormatControlReplyStopped(
                            QStringLiteral("/captures/a.ddd.flac"), kBytes)
                            .toUtf8())
          .value_or(ControlReply{});

  EXPECT_EQ(reply.bytes, kBytes);
}

TEST(CaptureControlProtocolTest, ARefusalCarriesItsReason) {
  const ControlReply reply =
      ParseControlReply(
          FormatControlReplyError(QStringLiteral("No capture is running."))
              .toUtf8())
          .value_or(ControlReply{});

  EXPECT_FALSE(reply.ok);
  EXPECT_EQ(reply.error, QStringLiteral("No capture is running."));
}

// A line that cannot be read at all is a different answer from one that says
// no, and the two must not be confused: the first means the connection is not
// talking this protocol, the second is this protocol working.
TEST(CaptureControlProtocolTest, SomethingThatIsNotAMessageIsNotOne) {
  EXPECT_FALSE(ParseControlVerb(QByteArrayLiteral("hello")).has_value());
  EXPECT_FALSE(ParseControlVerb(QByteArrayLiteral("[1,2,3]")).has_value());
  EXPECT_FALSE(
      ParseControlVerb(QByteArrayLiteral("{\"verb\":\"\"}")).has_value());
  EXPECT_FALSE(ParseControlVerb(QByteArrayLiteral("{\"verb\":7}")).has_value());
  EXPECT_FALSE(ParseControlReply(QByteArrayLiteral("{}")).has_value());
  EXPECT_FALSE(ParseControlReply(QByteArrayLiteral("nonsense")).has_value());
}

// --- The socket -----------------------------------------------------------

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

// Let the event loop run for a moment, for the cases where what is being
// checked is that nothing happened yet.
void PumpFor(std::chrono::milliseconds duration) {
  PumpUntil([] { return false; }, duration);
}

class CaptureControlServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    const QString test_name = QLatin1String(info->name());

    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-control-%1").arg(test_name));
    QSettings().clear();

    // A socket of this test's own. CTest runs discovered tests as separate
    // processes and may run several at once, so a shared name would have one
    // test connecting to another's server — and none of them may go anywhere
    // near the socket a real application on this machine is listening on.
    name_ = QStringLiteral("ddd-gui-control-test-%1").arg(test_name);

    directory_ = std::filesystem::temp_directory_path() /
                 (std::string("ddd-control-test-") + info->name());
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
    controller_->ApplySessionSettings(settings);

    server_ =
        std::make_unique<CaptureControlServer>(controller_.get(), &logger_);
  }

  void TearDown() override {
    // The server borrows the controller, so it goes first.
    server_.reset();
    controller_.reset();
    device_.reset();
    std::filesystem::remove_all(directory_);
    QSettings().clear();
  }

  void Listen() {
    QString error;
    ASSERT_TRUE(server_->Listen(name_, &error)) << error.toStdString();
  }

  struct Run {
    int code = -1;
    QString out;
    QString error;
  };

  Run Stop(int reply_timeout_milliseconds = 10000) {
    Run run;
    QTextStream out(&run.out);
    QTextStream error(&run.error);

    StopCaptureOptions options;
    options.server_name = name_;
    options.connect_timeout_milliseconds = 2000;
    options.reply_timeout_milliseconds = reply_timeout_milliseconds;

    run.code = RunStopCapture(out, error, options);
    out.flush();
    error.flush();
    return run;
  }

  // Send a raw line and read whatever comes back, for the cases a well-behaved
  // client would never produce.
  QByteArray Exchange(const QByteArray& request) {
    QLocalSocket socket;
    socket.connectToServer(name_);
    if (!PumpUntil([&socket] {
          return socket.state() == QLocalSocket::ConnectedState;
        })) {
      return {};
    }

    socket.write(request);
    socket.flush();

    QByteArray buffer;
    PumpUntil([&socket, &buffer] {
      buffer.append(socket.readAll());
      return buffer.contains('\n');
    });
    return buffer;
  }

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

  bool ASidecarWasWritten() const {
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
      if (entry.path().extension() == ".yaml") {
        return true;
      }
    }
    return false;
  }

  QString name_;
  std::filesystem::path directory_;
  std::unique_ptr<capture::FakeUsbDevice> device_;

  capture::CallbackLogger logger_{
      [](capture::LogLevel /*level*/, const std::string& /*message*/) {},
      capture::LogLevel::kDebug};

  std::unique_ptr<CaptureController> controller_;
  std::unique_ptr<CaptureControlServer> server_;
};

// --- Listening, and what it means -----------------------------------------

TEST_F(CaptureControlServerTest, ListeningIsTheSingleInstanceCheck) {
  ASSERT_NO_FATAL_FAILURE(Listen());
  EXPECT_TRUE(server_->listening());
  EXPECT_EQ(server_->name(), name_);

  // Two processes streaming from one device is not something either of them can
  // do. The second is told so here, before it has opened anything, rather than
  // finding out by racing for the USB claim.
  CaptureControlServer second(controller_.get(), &logger_);
  QString error;
  EXPECT_FALSE(second.Listen(name_, &error));
  EXPECT_TRUE(error.contains(QStringLiteral("already running")))
      << error.toStdString();
  EXPECT_FALSE(second.listening());
}

// The application every user actually runs looks for one name, and it must not
// be one another user's session is already sitting on.
TEST_F(CaptureControlServerTest, TheRealNameIsPerUser) {
  const QString user = qEnvironmentVariable("USER");
  if (user.isEmpty()) {
    GTEST_SKIP() << "No USER in the environment to name a socket after.";
  }

  EXPECT_TRUE(CaptureControlServer::ServerName().contains(user));
}

#ifndef Q_OS_WIN

// An application that was killed leaves its socket behind. Treating that as a
// running instance would mean a machine that could not capture again until
// somebody found and deleted a file they have no reason to know about.
TEST_F(CaptureControlServerTest, ASocketLeftBehindIsRecoveredFrom) {
  QString path;
  {
    QLocalServer finder;
    ASSERT_TRUE(finder.listen(name_));
    path = finder.fullServerName();
    finder.close();
  }
  ASSERT_FALSE(path.isEmpty());

  {
    QFile remains(path);
    ASSERT_TRUE(remains.open(QIODevice::WriteOnly));
    remains.write("what a killed application leaves");
  }
  ASSERT_TRUE(QFile::exists(path));

  QString error;
  EXPECT_TRUE(server_->Listen(name_, &error)) << error.toStdString();
  EXPECT_TRUE(server_->listening());
}

#endif  // Q_OS_WIN

// --- Stopping -------------------------------------------------------------

TEST_F(CaptureControlServerTest, WithNothingRunningThereIsNothingToStop) {
  const Run run = Stop();

  EXPECT_EQ(run.code, kExitNoRunningInstance);
  EXPECT_TRUE(run.out.isEmpty()) << run.out.toStdString();
  EXPECT_TRUE(run.error.contains(QStringLiteral("no Domesday Duplicator")))
      << run.error.toStdString();
}

// Running, but not capturing. A different answer from the one above, and worth
// being a different one: the application is there, and a script that gets this
// has stopped something twice or never started it.
TEST_F(CaptureControlServerTest, AnApplicationWithNoCaptureSaysSo) {
  ASSERT_NO_FATAL_FAILURE(Listen());

  const Run run = Stop();

  EXPECT_EQ(run.code, kExitCaptureFailed);
  EXPECT_TRUE(run.out.isEmpty()) << run.out.toStdString();
  EXPECT_TRUE(run.error.contains(QStringLiteral("No capture is running")))
      << run.error.toStdString();
}

// The whole point of the feature. What comes back is the file as it ended up on
// disk — closed, renamed if the naming asked for a duration, and with its
// sidecar beside it — because a script that stops a capture reads the file
// next.
TEST_F(CaptureControlServerTest, StoppingAnswersWithTheFinishedFile) {
  ASSERT_NO_FATAL_FAILURE(Listen());

  controller_->StartCapture();
  ASSERT_TRUE(PumpUntil([this] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 0;
  }));

  const Run run = Stop();

  EXPECT_EQ(run.code, kExitSuccess) << run.error.toStdString();
  EXPECT_FALSE(controller_->capturing());

  // Bare, on its own line, so that a script can use it as it stands.
  const QString path = run.out.trimmed();
  ASSERT_FALSE(path.isEmpty()) << run.error.toStdString();
  EXPECT_TRUE(std::filesystem::exists(path.toStdString()))
      << path.toStdString();

  // Finished, not merely stopped. The sidecar is written after the file is
  // closed, so its being here is what proves the client waited for the right
  // signal rather than for the writer being detached.
  EXPECT_TRUE(ASidecarWasWritten());

  // The commentary goes to the other stream, where it cannot end up in
  // whatever the script collected.
  EXPECT_TRUE(run.error.contains(QStringLiteral("bytes written")))
      << run.error.toStdString();
}

// Stopping returns to monitoring rather than to idle, which is what makes
// several captures from one setup session possible. Stopping twice is therefore
// an ordinary mistake to make, and it is answered rather than hung on.
TEST_F(CaptureControlServerTest, StoppingTwiceIsRefusedTheSecondTime) {
  ASSERT_NO_FATAL_FAILURE(Listen());

  controller_->StartCapture();
  ASSERT_TRUE(PumpUntil([this] {
    return !WrittenFiles().empty() &&
           std::filesystem::file_size(WrittenFiles().front()) > 0;
  }));

  ASSERT_EQ(Stop().code, kExitSuccess);

  const Run again = Stop();
  EXPECT_EQ(again.code, kExitCaptureFailed);
  EXPECT_TRUE(again.error.contains(QStringLiteral("No capture is running")))
      << again.error.toStdString();
}

// --- Connections that do not behave ---------------------------------------

TEST_F(CaptureControlServerTest, AnUnknownRequestIsAnsweredRatherThanIgnored) {
  ASSERT_NO_FATAL_FAILURE(Listen());

  const ControlReply reply =
      ParseControlReply(Exchange(QByteArrayLiteral("{\"verb\":\"fly\"}\n")))
          .value_or(ControlReply{});

  EXPECT_FALSE(reply.ok);
  EXPECT_TRUE(reply.error.contains(QStringLiteral("fly")))
      << reply.error.toStdString();
}

TEST_F(CaptureControlServerTest, SomethingThatIsNotARequestIsAnsweredToo) {
  ASSERT_NO_FATAL_FAILURE(Listen());

  const ControlReply reply =
      ParseControlReply(Exchange(QByteArrayLiteral("hello?\n")))
          .value_or(ControlReply{});

  EXPECT_FALSE(reply.ok);
  EXPECT_FALSE(reply.error.isEmpty());
}

// A request is one short line and almost always arrives in one piece. Almost
// always is not something a protocol may be written against.
TEST_F(CaptureControlServerTest,
       ARequestSplitAcrossTwoWritesIsStillOneRequest) {
  ASSERT_NO_FATAL_FAILURE(Listen());

  QLocalSocket socket;
  socket.connectToServer(name_);
  ASSERT_TRUE(PumpUntil(
      [&socket] { return socket.state() == QLocalSocket::ConnectedState; }));

  socket.write(QByteArrayLiteral("{\"verb\":\"st"));
  socket.flush();
  PumpFor(20ms);

  socket.write(QByteArrayLiteral("op\"}\n"));
  socket.flush();

  QByteArray buffer;
  ASSERT_TRUE(PumpUntil([&socket, &buffer] {
    buffer.append(socket.readAll());
    return buffer.contains('\n');
  }));

  // Nothing is capturing, so the answer is the refusal — which is the proof
  // that the two halves were read as one request rather than as two bad ones.
  const ControlReply reply = ParseControlReply(buffer).value_or(ControlReply{});
  EXPECT_FALSE(reply.ok);
  EXPECT_TRUE(reply.error.contains(QStringLiteral("No capture is running")))
      << reply.error.toStdString();
}

// A socket that accepts and then says nothing — an application wedged, or one
// that died between accepting and answering. The client has to give up rather
// than hold a script open for ever.
TEST_F(CaptureControlServerTest, AnApplicationThatNeverAnswersIsGivenUpOn) {
  QLocalServer silent;
  ASSERT_TRUE(silent.listen(name_));

  const Run run = Stop(300);

  EXPECT_EQ(run.code, kExitCaptureFailed);
  EXPECT_TRUE(run.out.isEmpty()) << run.out.toStdString();
  EXPECT_FALSE(run.error.isEmpty());
}

}  // namespace
}  // namespace ddd::gui
