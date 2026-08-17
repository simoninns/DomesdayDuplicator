/************************************************************************

    usb_blaster_cable.h

    The USB-Blaster's wire protocol, and the byte pipe it rides on
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "jtag_cable.h"

namespace ddd::capture {

class ILogger;

// The USB-Blaster's wire protocol, and where its description comes from.
//
// Altera never published it, but it has been described and independently
// implemented several times over two decades — urjtag, OpenOCD and
// openFPGALoader all drive this cable — and what is implemented here is
// written from those public descriptions of the *protocol*. No code was
// taken from any of them, deliberately: openFPGALoader is AGPL-3.0 and
// taking a line of it would move this project's licence position
// (AGENTS.md §10). They stay useful as something to check behaviour against,
// in the way minisign is used by the bundle tests.
//
// The cable is an FTDI FT245 in front of a small CPLD. The host writes a byte
// stream to the chip's bulk OUT endpoint and the CPLD reads it in one of two
// modes, chosen by the top bit of each command byte:
//
//   Bit-bang (bit 7 clear) — the remaining bits are driven straight onto the
//   JTAG pins, so one command byte is one pin state and a TCK cycle costs two
//   of them: one with TCK low, one with TCK high. Bit 6 asks the cable to
//   send back one byte carrying TDO.
//
//   Byte-shift (bit 7 set) — the low six bits are a count N of data bytes
//   that follow, each shifted out on TDI least significant bit first, with
//   TMS held low and the cable generating the clock itself. Bit 6 asks for
//   the N bytes of TDO back. Eight bits per byte of USB traffic against two
//   bytes per bit, so everything that can use it does.
//
// Reads carry the FT245's own framing: the chip puts two modem-status bytes
// at the front of every USB IN packet, and they are not part of the data.
//
// The transport below is a seam for one reason: it makes all of that
// testable. A fake byte pipe sees exactly what the cable would put on the
// wire, so the mode choices, the bit packing and the status stripping are
// checked against fixtures rather than against a board — leaving the bench
// only the question a bench can answer, which is whether the far end agrees
// (TESTING.md, B-V1).
class IFtdiTransport {
 public:
  IFtdiTransport() = default;
  virtual ~IFtdiTransport() = default;

  IFtdiTransport(const IFtdiTransport&) = delete;
  IFtdiTransport& operator=(const IFtdiTransport&) = delete;
  IFtdiTransport(IFtdiTransport&&) = delete;
  IFtdiTransport& operator=(IFtdiTransport&&) = delete;

  // Send every byte, or fail.
  virtual bool Write(std::span<const uint8_t> bytes) = 0;

  // Read whatever has arrived, up to the size of `buffer`, exactly as the
  // chip sends it — status bytes and all. `received` is set to how much
  // arrived, which may be nothing but status.
  virtual bool Read(std::span<uint8_t> buffer, size_t& received) = 0;

  // The chip's IN packet size, which is the interval the status bytes
  // repeat at.
  virtual size_t packet_bytes() const = 0;
};

// The two status bytes at the front of every IN packet.
inline constexpr size_t kFtdiStatusBytes = 2;

// The most data bytes one byte-shift command can carry: six bits of count,
// and a count of zero would be a command that does nothing.
inline constexpr size_t kMaximumShiftBytes = 63;

// A cable over any byte pipe. The libusb factory in jtag_cable.h is this
// over a real one.
std::unique_ptr<IJtagCable> MakeUsbBlasterCableOver(IFtdiTransport& transport,
                                                    ILogger* logger);

}  // namespace ddd::capture
