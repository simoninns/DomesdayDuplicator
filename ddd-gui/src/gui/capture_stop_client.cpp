/************************************************************************

    capture_stop_client.cpp

    --stop-capture, the other end of the control socket
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_stop_client.h"

#include <QByteArray>
#include <QEventLoop>
#include <QLocalSocket>
#include <QTextStream>
#include <QTimer>
#include <optional>

#include "capture_cli.h"
#include "capture_control_server.h"

namespace ddd::gui {

int RunStopCapture(QTextStream& out, QTextStream& error,
                   const StopCaptureOptions& options) {
  const QString name = options.server_name.isEmpty()
                           ? CaptureControlServer::ServerName()
                           : options.server_name;

  QLocalSocket socket;
  QEventLoop loop;
  QTimer deadline;
  deadline.setSingleShot(true);

  QByteArray buffered;
  int code = kExitNoRunningInstance;
  bool settled = false;
  bool connected = false;

  // Everything below reaches an answer exactly once. The socket has several
  // ways to end — a reply, a close, an error, a timeout — and more than one of
  // them can happen: a server that replies and then closes would otherwise
  // overwrite a success with the disconnection that followed it.
  //
  // That is not a rare case. A headless run exits the moment the capture it was
  // asked to stop has finished, which is the moment after this reply, so the
  // disconnection arrives in the same turn of the event loop as the answer
  // nearly every time.
  const auto settle = [&](int result) {
    if (settled) {
      return;
    }
    settled = true;
    code = result;
    loop.quit();
  };

  // Whether an answer has already been reached, checked before *saying*
  // anything rather than only before recording it. Quitting the loop does not
  // take effect until control returns to exec(), so the handlers for whatever
  // else the socket does still run — and a message printed from one of them
  // would tell a user their successful stop had failed.
  const auto answered = [&settled] { return settled; };

  QObject::connect(&socket, &QLocalSocket::connected, &socket, [&] {
    connected = true;
    deadline.start(options.reply_timeout_milliseconds);
    socket.write(FormatControlRequest(QLatin1String(kStopVerb)).toUtf8());
    socket.flush();
  });

  QObject::connect(&socket, &QLocalSocket::readyRead, &socket, [&] {
    if (answered()) {
      return;
    }
    buffered.append(socket.readAll());
    const qsizetype newline = buffered.indexOf('\n');
    if (newline < 0) {
      return;
    }

    const std::optional<ControlReply> reply =
        ParseControlReply(buffered.left(newline));
    if (!reply.has_value()) {
      error << "Error: the running application answered with something that "
               "could not be understood.\n";
      settle(kExitCaptureFailed);
      return;
    }

    if (!reply->ok) {
      error << "Error: " << reply->error << "\n";
      settle(kExitCaptureFailed);
      return;
    }

    // The path alone, so that a script can use what it reads without having to
    // take it apart. The size is worth saying to whoever is watching, and it
    // goes to the other stream for exactly that reason.
    out << reply->file_path << "\n";
    error << "Stopped. " << QString::number(reply->bytes)
          << " bytes written.\n";
    settle(kExitSuccess);
  });

  QObject::connect(&socket, &QLocalSocket::errorOccurred, &socket,
                   [&](QLocalSocket::LocalSocketError) {
                     if (answered()) {
                       return;
                     }
                     if (!connected) {
                       error << "There is no Domesday Duplicator running to "
                                "stop.\n";
                       settle(kExitNoRunningInstance);
                       return;
                     }
                     error << "Error: the connection to the running "
                              "application failed before it answered.\n";
                     settle(kExitCaptureFailed);
                   });

  QObject::connect(&socket, &QLocalSocket::disconnected, &socket, [&] {
    if (answered()) {
      return;
    }
    error << "Error: the running application closed the connection without "
             "answering.\n";
    settle(kExitCaptureFailed);
  });

  QObject::connect(&deadline, &QTimer::timeout, &deadline, [&] {
    if (answered()) {
      return;
    }
    if (!connected) {
      error << "There is no Domesday Duplicator running to stop.\n";
      settle(kExitNoRunningInstance);
      return;
    }
    error << "Error: the capture was asked to stop, but the application did "
             "not say it had finished the file.\n";
    settle(kExitCaptureFailed);
  });

  deadline.start(options.connect_timeout_milliseconds);
  socket.connectToServer(name);

  // A connection that failed before there was an event loop to report it in has
  // already settled, and exec() would then wait for a quit() that has been and
  // gone.
  if (!settled) {
    loop.exec();
  }

  out.flush();
  error.flush();
  return code;
}

}  // namespace ddd::gui
