/************************************************************************

    qt_serial_port.h

    A real serial port, behind the protocol layer's interface
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>

#include "serial_port.h"

class QSerialPort;

namespace ddd::gui {

// ISerialPort over QSerialPort, and the only file in this project that includes
// a QtSerialPort header.
//
// Eight data bits, no parity, one stop bit, no flow control — the line settings
// every player the schema describes uses, applied here rather than asked for,
// because they are not a choice anybody can make correctly by being offered
// them.
//
// Thread-affinity: the port belongs to the thread that opened it, and every
// method but RequestAbort() must be called from there. RequestAbort() is the
// exception and exists for one reason: a read may be waiting out a thirty-
// second seek when the application is asked to quit, and a window that takes
// half a minute to close is a window somebody force-quits. It sets a flag the
// waiting read notices between slices, so no other thread ever touches the
// QSerialPort itself.
class QtSerialPort : public player::ISerialPort {
 public:
  QtSerialPort();
  ~QtSerialPort() override;

  bool Open(const std::string& path,
            const player::SerialSettings& settings) override;
  void Close() override;
  bool IsOpen() const override;
  void DiscardBuffers() override;
  bool Write(std::string_view bytes) override;
  bool Read(std::string& into, std::chrono::milliseconds timeout) override;

  // Abandon whatever is being waited for, from any thread. The read in
  // progress returns as a link failure, which the session already knows how to
  // report; the flag is cleared by the next Open().
  void RequestAbort() override;

  // How long a single wait lasts before the abort flag is looked at again.
  // Short enough that quitting feels immediate, long enough that a five-second
  // command timeout is fifty waits rather than five thousand.
  static constexpr std::chrono::milliseconds kWaitSlice{100};

 private:
  std::unique_ptr<QSerialPort> port_;
  std::atomic<bool> abort_requested_{false};
};

}  // namespace ddd::gui
