/************************************************************************

    serial_port.h

    The seam between the player protocol and a serial port
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace ddd::player {

// How the line is configured.
//
// Only the rate, because everything else is fixed for every player this schema
// describes: eight data bits, no parity, one stop bit, no flow control. Stated
// here rather than assumed silently in a backend, so that a family needing
// something else has an obvious place to put it.
struct SerialSettings {
  uint32_t baud_rate = 9600;
};

// A serial port, as the protocol layer needs one.
//
// This interface is why the protocol above it links no Qt and opens nothing:
// with it, a whole connect-probe-command sequence can be driven from a test in
// microseconds, with the player answering late, answering wrongly, answering at
// the wrong rate or going silent mid-command — all things that are otherwise
// arranged by hand with a cable and a player that has to be persuaded to
// misbehave.
//
// Thread-safety: none. One session owns one port and drives it from one thread.
class ISerialPort {
 public:
  ISerialPort() = default;
  virtual ~ISerialPort() = default;

  ISerialPort(const ISerialPort&) = delete;
  ISerialPort& operator=(const ISerialPort&) = delete;
  ISerialPort(ISerialPort&&) = delete;
  ISerialPort& operator=(ISerialPort&&) = delete;

  // False if the port could not be opened — busy, absent, or not permitted.
  virtual bool Open(const std::string& path,
                    const SerialSettings& settings) = 0;

  virtual void Close() = 0;

  virtual bool IsOpen() const = 0;

  // Throw away anything buffered in either direction.
  //
  // Called before every exchange. Without it, a reply that arrived after its
  // command timed out is still sitting there waiting to be misread as the
  // answer to the next one — which is how a seek reports the tray state.
  virtual void DiscardBuffers() = 0;

  // False if the port failed. A short write is the implementation's problem,
  // not the caller's.
  virtual bool Write(std::string_view bytes) = 0;

  // Append whatever arrives, waiting up to `timeout`.
  //
  // The contract that matters: this blocks until at least one byte is available
  // or the whole timeout has elapsed, and returns true either way. Returning
  // true having appended nothing therefore means the timeout expired, which is
  // what lets the session's read loop terminate without polling.
  //
  // False means the port failed, which is a disconnection rather than a
  // timeout.
  virtual bool Read(std::string& into, std::chrono::milliseconds timeout) = 0;

  // Abandon a wait in progress. **The one method callable from another
  // thread.**
  //
  // It exists because the waits are long: a seek is allowed thirty seconds, and
  // an application asked to quit in the middle of one would otherwise take
  // thirty seconds to close — which is how a user learns to force-quit it. A
  // read that is abandoned returns as a port failure, which the session already
  // knows how to report as a disconnection.
  //
  // Implementations must not touch anything the waiting thread owns; setting an
  // atomic flag the wait looks at is the whole of what is expected. The default
  // does nothing, which is right for any implementation whose waits are not
  // long enough to be worth interrupting.
  virtual void RequestAbort() {}
};

}  // namespace ddd::player
