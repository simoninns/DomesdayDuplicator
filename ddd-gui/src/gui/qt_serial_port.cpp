/************************************************************************

    qt_serial_port.cpp

    A real serial port, behind the protocol layer's interface
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "qt_serial_port.h"

#include <QByteArray>
#include <QSerialPort>
#include <QString>
#include <algorithm>

namespace ddd::gui {

QtSerialPort::QtSerialPort() : port_(std::make_unique<QSerialPort>()) {}

QtSerialPort::~QtSerialPort() { Close(); }

bool QtSerialPort::Open(const std::string& path,
                        const player::SerialSettings& settings) {
  Close();
  abort_requested_.store(false);

  port_->setPortName(QString::fromStdString(path));
  port_->setBaudRate(static_cast<qint32>(settings.baud_rate));
  port_->setDataBits(QSerialPort::Data8);
  port_->setParity(QSerialPort::NoParity);
  port_->setStopBits(QSerialPort::OneStop);
  port_->setFlowControl(QSerialPort::NoFlowControl);

  if (!port_->open(QIODevice::ReadWrite)) {
    return false;
  }

  // Whatever was in the buffers belongs to whoever had the port before.
  port_->clear();
  return true;
}

void QtSerialPort::Close() {
  if (port_->isOpen()) {
    port_->close();
  }
}

bool QtSerialPort::IsOpen() const { return port_->isOpen(); }

void QtSerialPort::DiscardBuffers() {
  if (port_->isOpen()) {
    port_->clear();
  }
}

bool QtSerialPort::Write(std::string_view bytes) {
  if (!port_->isOpen()) {
    return false;
  }

  const qint64 written =
      port_->write(bytes.data(), static_cast<qint64>(bytes.size()));
  if (written != static_cast<qint64>(bytes.size())) {
    return false;
  }

  // Waited for rather than left to the event loop, because there is no event
  // loop on this thread while a command is in flight: the session writes and
  // then blocks reading, and an unflushed command is a command the player
  // never sees.
  //
  // A command is a handful of bytes, so even at 1200 baud this is tens of
  // milliseconds. The slice length is reused as the bound because it is the
  // same question — how long to wait before looking at the abort flag again.
  while (port_->bytesToWrite() > 0) {
    if (abort_requested_.load()) {
      return false;
    }
    if (!port_->waitForBytesWritten(static_cast<int>(kWaitSlice.count()))) {
      // A timeout with bytes still queued on a link this slow is not evidence
      // of a fault, so it is only a failure if the port says so.
      if (port_->error() != QSerialPort::NoError &&
          port_->error() != QSerialPort::TimeoutError) {
        port_->clearError();
        return false;
      }
      port_->clearError();
    }
  }

  return true;
}

bool QtSerialPort::Read(std::string& into, std::chrono::milliseconds timeout) {
  if (!port_->isOpen()) {
    return false;
  }

  // The interface's contract is "block until something arrives or the whole
  // timeout has elapsed". That is done in slices so the abort flag is looked at
  // in between — see the class comment for why that matters at all.
  std::chrono::milliseconds remaining = timeout;

  while (remaining.count() > 0) {
    if (abort_requested_.load()) {
      return false;
    }

    const std::chrono::milliseconds slice = std::min(remaining, kWaitSlice);

    if (port_->waitForReadyRead(static_cast<int>(slice.count()))) {
      const QByteArray data = port_->readAll();
      into.append(data.constData(), static_cast<size_t>(data.size()));
      return true;
    }

    // A slice that expired with nothing on the line is the ordinary case and
    // says nothing about the port. Anything else is a port that has gone away —
    // the cable pulled, the adapter unplugged — which is a disconnection rather
    // than a slow player.
    const QSerialPort::SerialPortError error = port_->error();
    if (error != QSerialPort::NoError && error != QSerialPort::TimeoutError) {
      port_->clearError();
      return false;
    }
    port_->clearError();

    remaining -= slice;
  }

  return true;
}

void QtSerialPort::RequestAbort() { abort_requested_.store(true); }

}  // namespace ddd::gui
