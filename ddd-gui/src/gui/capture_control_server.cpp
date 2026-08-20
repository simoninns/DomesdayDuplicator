/************************************************************************

    capture_control_server.cpp

    The socket a script stops a running capture through
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_control_server.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLatin1String>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QtGlobal>

#include "capture_controller.h"
#include "logger.h"

namespace ddd::gui {
namespace {

constexpr const char* kVerbKey = "verb";
constexpr const char* kOkKey = "ok";
constexpr const char* kFileKey = "file";
constexpr const char* kBytesKey = "bytes";
constexpr const char* kErrorKey = "error";

// How long to wait for something to answer on a socket that is already there.
// Short, and deliberately: this runs before a scripted capture can start, and
// the case it covers is a socket left behind by an instance that was killed —
// which answers immediately by not answering at all.
constexpr int kProbeTimeoutMilliseconds = 250;

// How long to wait for a reply to reach the client. The reply is one short
// line, so this only matters when the process is about to exit and the bytes
// have nowhere to be pushed from afterwards.
constexpr int kFlushTimeoutMilliseconds = 1000;

// A request is one short line. Anything beyond this is not a request that got
// split up, it is something that is never going to end, and a buffer that grew
// to meet it would be a way to spend this application's memory from outside it.
constexpr int kMaximumRequestBytes = 4096;

QString ToLine(const QJsonObject& object) {
  return QString::fromUtf8(
             QJsonDocument(object).toJson(QJsonDocument::Compact)) +
         QLatin1Char('\n');
}

// Whether something is listening on this name and willing to talk. This is the
// single-instance check, and it is a knock rather than a look at the filesystem
// because the two things that can be there — a running application and the
// socket a killed one left behind — are the same file.
//
// Nothing to connect to fails at once rather than waiting out the timeout: an
// absent socket is ENOENT and an absent named pipe is ERROR_FILE_NOT_FOUND, so
// the ordinary case of starting with nothing running costs nothing.
bool SomethingIsAnswering(const QString& name) {
  QLocalSocket probe;
  probe.connectToServer(name);
  const bool answered = probe.waitForConnected(kProbeTimeoutMilliseconds);
  probe.abort();
  return answered;
}

std::optional<QJsonObject> ToObject(const QByteArray& line) {
  QJsonParseError parse_error;
  const QJsonDocument document = QJsonDocument::fromJson(line, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    return std::nullopt;
  }
  return document.object();
}

}  // namespace

// --- The protocol ---------------------------------------------------------

QString FormatControlRequest(const QString& verb) {
  QJsonObject object;
  object.insert(QLatin1String(kVerbKey), verb);
  return ToLine(object);
}

std::optional<QString> ParseControlVerb(const QByteArray& line) {
  const std::optional<QJsonObject> object = ToObject(line);
  if (!object.has_value()) {
    return std::nullopt;
  }

  const QJsonValue verb = object->value(QLatin1String(kVerbKey));
  if (!verb.isString() || verb.toString().isEmpty()) {
    return std::nullopt;
  }
  return verb.toString();
}

QString FormatControlReplyStopped(const QString& file_path, quint64 bytes) {
  QJsonObject object;
  object.insert(QLatin1String(kOkKey), true);
  object.insert(QLatin1String(kFileKey), file_path);
  object.insert(QLatin1String(kBytesKey), static_cast<qint64>(bytes));
  return ToLine(object);
}

QString FormatControlReplyError(const QString& message) {
  QJsonObject object;
  object.insert(QLatin1String(kOkKey), false);
  object.insert(QLatin1String(kErrorKey), message);
  return ToLine(object);
}

std::optional<ControlReply> ParseControlReply(const QByteArray& line) {
  const std::optional<QJsonObject> object = ToObject(line);
  if (!object.has_value()) {
    return std::nullopt;
  }

  const QJsonValue ok = object->value(QLatin1String(kOkKey));
  if (!ok.isBool()) {
    return std::nullopt;
  }

  ControlReply reply;
  reply.ok = ok.toBool();
  reply.file_path = object->value(QLatin1String(kFileKey)).toString();
  reply.bytes =
      static_cast<quint64>(object->value(QLatin1String(kBytesKey)).toInteger());
  reply.error = object->value(QLatin1String(kErrorKey)).toString();
  return reply;
}

// --- The server -----------------------------------------------------------

CaptureControlServer::CaptureControlServer(CaptureController* controller,
                                           capture::ILogger* logger,
                                           QObject* parent)
    : QObject(parent),
      controller_(controller),
      logger_(logger),
      server_(new QLocalServer(this)) {
  // Deliberately the default socket options, and not UserAccessOption, which
  // is what this would otherwise want: with it set, a second listen() on a name
  // an application is already listening on *succeeds*, replacing the first
  // one's socket rather than failing with AddressInUseError. That would take
  // the single-instance check below apart — two instances would both believe
  // they were the only one, and the first would be left listening on a socket
  // no client could reach. With the default options the operating system's own
  // exclusion applies and a second listen is refused, which is the behaviour
  // being relied on here. RestrictToOwner() puts the access bits back.
  connect(server_, &QLocalServer::newConnection, this,
          &CaptureControlServer::OnNewConnection);
}

CaptureControlServer::~CaptureControlServer() {
  // Explicitly, so the socket is taken out of the filesystem now rather than
  // whenever the object graph unwinds. The next instance to start looks for it.
  server_->close();
}

QString CaptureControlServer::ServerName() {
  // Per user, because the directory a local socket is created in is shared on
  // some platforms and a Windows pipe name is shared on every one of them. Two
  // people logged into one machine have two Duplicator sessions, and the first
  // to start must not be what the second one finds in the way — nor, with the
  // access bits above, something it could use anyway.
  //
  // Each platform's own variable is asked for first. Both are often set at
  // once — a Windows shell that came with a Unix toolchain sets USER as well —
  // and an application started from one kind of shell must land on the same
  // name as a --stop-capture run from the other.
#ifdef Q_OS_WIN
  QString user = qEnvironmentVariable("USERNAME");
  if (user.isEmpty()) {
    user = qEnvironmentVariable("USER");
  }
#else
  QString user = qEnvironmentVariable("USER");
  if (user.isEmpty()) {
    user = qEnvironmentVariable("USERNAME");
  }
#endif

  const QString name = user.isEmpty()
                           ? QStringLiteral("ddd-gui-control")
                           : QStringLiteral("ddd-gui-control-") + user;

#ifdef Q_OS_WIN
  // A named pipe, which is not a path and has nowhere to be put.
  return name;
#else
  // An absolute path, so that the socket does not follow TMPDIR.
  //
  // A bare name is created in QDir::tempPath(), which honours TMPDIR — and a
  // shell that sets one is not unusual: a development shell, a systemd unit
  // with PrivateTmp, a build sandbox. The application and the --stop-capture
  // run that means to stop it would then be looking in two different places,
  // and the script would be told nothing was running while a capture was going
  // on in front of it.
  //
  // The runtime directory is the right place for a socket in any case: it is
  // per user, it is cleaned when the session ends rather than surviving on
  // disk, and inside a Flatpak it is per application and shared between that
  // application's instances, which is exactly the reach this needs.
  const QString runtime =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (runtime.isEmpty()) {
    // macOS has no such directory, and neither has a stripped-down Linux
    // session. The temporary directory is where Qt would have put it anyway.
    return name;
  }
  return runtime + QLatin1Char('/') + name;
#endif
}

bool CaptureControlServer::Listen(QString* error) {
  return Listen(ServerName(), error);
}

bool CaptureControlServer::Listen(const QString& name, QString* error) {
  if (server_->isListening()) {
    return true;
  }

  const auto refuse = [error] {
    if (error != nullptr) {
      *error = QStringLiteral(
          "Another Domesday Duplicator is already running. Only one can use "
          "the device at a time.");
    }
    return false;
  };

  // Knocked on before it is listened on, rather than the other way about, and
  // this order is what makes the check mean anything on Windows.
  //
  // On Unix a second listen() on a socket in use is refused by the operating
  // system, so asking first is only tidier. On Windows a QLocalServer is a
  // named pipe, and Qt creates it with PIPE_UNLIMITED_INSTANCES and without
  // FILE_FLAG_FIRST_PIPE_INSTANCE — so a second process listening on a name
  // its neighbour already holds *succeeds*, adding another instance of the
  // same pipe. Both would then believe they were the only one, and a client's
  // stop would be answered by whichever of them the kernel handed the
  // connection to, which may be the one that is not capturing.
  if (SomethingIsAnswering(name)) {
    return refuse();
  }

  if (server_->listen(name)) {
    RestrictToOwner();
    return true;
  }

  // Nothing answered a moment ago and the name is still taken, which is either
  // the socket a killed application left behind or an instance that started in
  // the time between the two. Knocking again is what tells those apart, and
  // without it the recovery below would delete a live application's socket and
  // take its place.
  if (SomethingIsAnswering(name)) {
    return refuse();
  }

  QLocalServer::removeServer(name);
  if (server_->listen(name)) {
    RestrictToOwner();
    if (logger_ != nullptr) {
      logger_->Info(
          "Removed a control socket left behind by an application that did "
          "not shut down.");
    }
    return true;
  }

  if (error != nullptr) {
    *error = QStringLiteral("The control socket could not be created: %1")
                 .arg(server_->errorString());
  }
  return false;
}

void CaptureControlServer::RestrictToOwner() {
  const QString path = server_->fullServerName();
  if (path.isEmpty()) {
    return;
  }

  // The access bits the default socket options did not set: owner only, so
  // that nothing belonging to another account on this machine can stop a
  // capture. The name is per-user as well — see ServerName() — so this is the
  // second of two locks rather than the only one.
  //
  // A Unix socket is a file and this applies to it. A Windows named pipe is not
  // a file and this does nothing, which is why the failure is only reported for
  // a name that exists in the filesystem: on Windows there is nothing here to
  // report, and a warning every run would be noise about a platform difference
  // rather than about a problem.
  if (!QFile::setPermissions(
          path, QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
      QFileInfo::exists(path) && logger_ != nullptr) {
    logger_->Warning(
        "The control socket could not be restricted to this user: " +
        path.toStdString());
  }
}

bool CaptureControlServer::listening() const { return server_->isListening(); }

QString CaptureControlServer::name() const {
  return server_->isListening() ? server_->serverName() : QString();
}

void CaptureControlServer::OnNewConnection() {
  while (QLocalSocket* socket = server_->nextPendingConnection()) {
    partial_.insert(socket, QByteArray());

    connect(socket, &QLocalSocket::readyRead, this,
            [this, socket] { OnReadyRead(socket); });

    connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
      Forget(socket);
      socket->deleteLater();
    });
  }
}

void CaptureControlServer::OnReadyRead(QLocalSocket* socket) {
  QByteArray& buffered = partial_[socket];
  buffered.append(socket->readAll());

  qsizetype newline = buffered.indexOf('\n');
  while (newline >= 0) {
    const QByteArray line = buffered.left(newline);
    buffered.remove(0, newline + 1);
    HandleRequest(socket, line);
    newline = buffered.indexOf('\n');
  }

  if (buffered.size() > kMaximumRequestBytes) {
    Reply(socket,
          FormatControlReplyError(QStringLiteral("That is not a request.")));
    socket->disconnectFromServer();
  }
}

void CaptureControlServer::HandleRequest(QLocalSocket* socket,
                                         const QByteArray& line) {
  const std::optional<QString> verb = ParseControlVerb(line);
  if (!verb.has_value()) {
    Reply(socket,
          FormatControlReplyError(QStringLiteral("That is not a request.")));
    return;
  }

  if (*verb == QLatin1String(kStopVerb)) {
    HandleStop(socket);
    return;
  }

  // Answered rather than ignored: a client asking for something this build does
  // not do should be told so, not left waiting for a reply that is not coming.
  Reply(socket, FormatControlReplyError(
                    QStringLiteral("Unknown request '%1'.").arg(*verb)));
}

void CaptureControlServer::HandleStop(QLocalSocket* socket) {
  if (controller_ == nullptr || !controller_->capturing()) {
    Reply(socket,
          FormatControlReplyError(QStringLiteral("No capture is running.")));
    return;
  }

  // Connected before the capture is asked to stop, and to CaptureFinished
  // rather than to CapturingChanged. Stopping detaches the writer immediately
  // and says so, but the file is not finished at that point: the encoder has
  // yet to close it, the capture may still be renamed to carry its duration,
  // and the sidecar has not been written. The path in the reply is the one that
  // is actually on disk, so it is the later signal that carries it.
  //
  // The socket is the context object, so a client that gave up and closed the
  // connection takes this connection with it rather than leaving a reply
  // addressed to nothing.
  connect(
      controller_, &CaptureController::CaptureFinished, socket,
      [this, socket](const QString& file_path, quint64 bytes) {
        Reply(socket, FormatControlReplyStopped(file_path, bytes));
      },
      Qt::SingleShotConnection);

  if (logger_ != nullptr) {
    logger_->Info("Stopping the capture: asked over the control socket.");
  }

  controller_->StopCapture();
}

void CaptureControlServer::Forget(QLocalSocket* socket) {
  partial_.remove(socket);
}

void CaptureControlServer::Reply(QLocalSocket* socket, const QString& line) {
  if (socket == nullptr || socket->state() != QLocalSocket::ConnectedState) {
    return;
  }

  socket->write(line.toUtf8());
  socket->flush();

  // A headless run quits as soon as the capture it was asked to stop has
  // finished, which can be the same turn of the event loop this reply is
  // written in. Pushed out here, rather than left in a buffer belonging to a
  // socket that is about to be destroyed along with everything else.
  socket->waitForBytesWritten(kFlushTimeoutMilliseconds);
}

}  // namespace ddd::gui
