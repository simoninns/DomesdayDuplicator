/************************************************************************

    capture_control_server.h

    The socket a script stops a running capture through
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <optional>

class QLocalServer;
class QLocalSocket;

namespace ddd::capture {
class ILogger;
}  // namespace ddd::capture

namespace ddd::gui {

class CaptureController;

// --- The protocol ---------------------------------------------------------
//
// One JSON object per line, in both directions:
//
//   request   {"verb":"stop"}
//   reply     {"ok":true,"file":"/captures/disc-1.ddd.flac","bytes":123456}
//             {"ok":false,"error":"No capture is running."}
//
// JSON rather than a bare word and a path, for one reason that matters: the
// reply carries a file name a user chose, and a protocol that separated fields
// by spaces would be wrong the first time somebody captured a disc whose title
// has a space in it. Escaping is the whole job, and QJsonDocument does it.
//
// Formatted and parsed by free functions so that both halves can be tested
// without a socket, a server or an event loop between them.

// The only thing a client can ask for. An unknown verb is answered with an
// error rather than ignored, so that a newer client talking to an older
// application is told, instead of waiting for a reply that is never coming.
inline constexpr const char* kStopVerb = "stop";

// A request line, terminated. Never empty.
QString FormatControlRequest(const QString& verb);

// The verb from a request line, or nothing when the line was not a request
// this application understands.
std::optional<QString> ParseControlVerb(const QByteArray& line);

QString FormatControlReplyStopped(const QString& file_path, quint64 bytes);
QString FormatControlReplyError(const QString& message);

// A reply, as the client reads it. `ok` false means the request was understood
// and refused, which is a different thing from a line that could not be parsed
// at all — that is a missing optional from ParseControlReply().
struct ControlReply {
  bool ok = false;
  QString file_path;
  quint64 bytes = 0;
  QString error;
};

std::optional<ControlReply> ParseControlReply(const QByteArray& line);

// --- The server -----------------------------------------------------------

// Listens for the one instruction a running application takes from outside
// itself: stop the capture.
//
// Every instance holds one of these, windowed or not, so that a capture started
// from a script and a capture started by hand are stopped the same way. The
// socket is local — a Unix domain socket or a named pipe, never a network one —
// so nothing off this machine can reach it, and it is created with user-only
// access so nothing belonging to another user can either.
//
// Listening doubles as the single-instance check. Two processes streaming from
// one device is not something either of them can do, so a scripted start that
// finds the socket taken says so and exits rather than racing for the USB
// claim. That is why Listen() reports failure rather than merely logging it.
class CaptureControlServer : public QObject {
  Q_OBJECT

 public:
  CaptureControlServer(CaptureController* controller, capture::ILogger* logger,
                       QObject* parent = nullptr);
  ~CaptureControlServer() override;

  // The socket every instance uses, and the one --stop-capture looks for.
  //
  // Per user, because the directory the socket is created in may be shared:
  // two people logged into one machine each have their own Duplicator session,
  // and neither should find the other's socket in the way.
  static QString ServerName();

  // Start listening. False means another instance is already running — or, for
  // any other failure, that the socket could not be created at all; `error`
  // says which, in words meant for a user.
  //
  // A socket left behind by an instance that was killed is recovered from
  // rather than treated as a live one: it is probed first, and only removed
  // when nothing answers.
  bool Listen(QString* error);

  // The same, on a name of the test's choosing. Two test processes running at
  // once must not share a socket, and neither may touch the one a real
  // application is listening on.
  bool Listen(const QString& name, QString* error);

  bool listening() const;

  // The name being listened on, or empty. For the tests and the log.
  QString name() const;

 private:
  void OnNewConnection();
  void OnReadyRead(QLocalSocket* socket);
  void HandleRequest(QLocalSocket* socket, const QByteArray& line);
  void HandleStop(QLocalSocket* socket);
  void Forget(QLocalSocket* socket);

  // Take the socket's access bits down to this user, once it exists.
  void RestrictToOwner();

  // Write one line and make sure it leaves. A headless run quits as soon as the
  // capture it was asked to stop has finished, which can be within the same
  // turn of the event loop as this reply — so the bytes are pushed out here
  // rather than left for a socket that is about to be destroyed.
  void Reply(QLocalSocket* socket, const QString& line);

  CaptureController* controller_ = nullptr;
  capture::ILogger* logger_ = nullptr;
  QLocalServer* server_ = nullptr;

  // What has arrived on each connection but is not yet a whole line. A request
  // is one short line and almost always arrives in one piece, but "almost
  // always" is not something a protocol may be written against.
  QHash<QLocalSocket*, QByteArray> partial_;
};

}  // namespace ddd::gui
