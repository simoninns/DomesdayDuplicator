/************************************************************************

    usb_blaster_cable.cpp

    The USB-Blaster's wire protocol, over a byte pipe
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "usb_blaster_cable.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "logger.h"

namespace ddd::capture {
namespace {

// Bit-bang command bits.
constexpr uint8_t kBitBangTck = 0x01;
constexpr uint8_t kBitBangTms = 0x02;
constexpr uint8_t kBitBangTdi = 0x10;

// Output enable, and the cable's activity LED on the same bit. Held set for
// as long as the cable is in use: on the boards where this bit gates the
// output buffers, clearing it stops the cable driving JTAG at all, and on
// the rest it only means the light is on while work is happening.
constexpr uint8_t kBitBangOutputEnable = 0x20;

// Set on a bit-bang command to ask for TDO back.
//
// Byte-shift mode defines the same bit and this cable does not honour it —
// see Shift(). Every read this driver performs is therefore a bit-bang one.
constexpr uint8_t kReadFlag = 0x40;

// Set to select byte-shift mode; clear for bit-bang.
constexpr uint8_t kByteShiftFlag = 0x80;

// How much command stream is allowed to pile up before it is sent.
//
// Every transfer costs a round trip, and an EPCS write is tens of millions
// of command bytes, so the difference between sending 64 bytes at a time and
// sending 16 KiB at a time is the difference between an afternoon and a few
// minutes. Nothing is lost by the delay: everything that needs the cable to
// have caught up asks for TDO, and asking for TDO flushes.
constexpr size_t kWriteBufferBytes = size_t{16} << 10;

bool BitAt(std::span<const uint8_t> bits, size_t index) {
  const size_t byte = index / 8;
  if (byte >= bits.size()) {
    return false;
  }
  return ((bits[byte] >> (index % 8)) & 1U) != 0;
}

void SetBitAt(std::vector<uint8_t>& bits, size_t index, bool value) {
  if (!value) {
    return;
  }
  bits[index / 8] |= static_cast<uint8_t>(1U << (index % 8));
}

class UsbBlasterCable : public IJtagCable {
 public:
  UsbBlasterCable(IFtdiTransport& transport, ILogger* logger)
      : transport_(transport), logger_(logger) {}

  const char* Name() const override { return "USB-Blaster"; }

  bool Shift(std::span<const uint8_t> tms, std::span<const uint8_t> tdi,
             size_t bit_count, std::vector<uint8_t>* tdo) override {
    if (tdo != nullptr) {
      tdo->assign((bit_count + 7) / 8, 0);
    }

    size_t bit = 0;
    while (bit < bit_count) {
      // Byte-shift whenever the next eight cycles hold TMS low and nothing
      // is being read back, which is every cycle of a scan except the last.
      // A megabit of flash image therefore costs a megabit of USB traffic
      // rather than sixteen.
      //
      // **Not when TDO is wanted**, and that is a bench finding rather than
      // a design preference (B-V1, 2026-08-17). Asked for a read, this cable
      // returns FF for every byte-shifted byte — no information at all,
      // rather than the wrong information — while the same bits read
      // correctly one cycle at a time. What is not in doubt is byte-shift
      // *shifting*: reading a Cyclone IV's IDCODE with the first 24 bits
      // byte-shifted and the last 8 bit-banged returns the correct top byte,
      // so the shift had advanced exactly 24 places. Only the answer is
      // missing.
      //
      // Nothing is lost by avoiding it. In this project's own provisioning
      // file 103 bits of 73,297,811 are read — one ten-thousandth of one per
      // cent — so the fast path still carries everything that takes time,
      // and a read costs sixteen command bytes a byte instead of two.
      const size_t whole_bytes =
          tdo == nullptr ? ByteShiftableBytes(tms, bit, bit_count) : 0;
      if (whole_bytes > 0) {
        if (!ShiftBytes(tdi, bit, whole_bytes)) {
          return false;
        }
        bit += whole_bytes * 8;
        continue;
      }

      if (!ShiftOneBit(BitAt(tms, bit), BitAt(tdi, bit), bit, tdo)) {
        return false;
      }
      ++bit;
    }

    return true;
  }

  bool RunClock(size_t count) override {
    // TMS is low throughout, so this is byte-shift mode's own case: whatever
    // is on TDI is ignored by a TAP sitting in Run-Test/Idle, and the cable
    // clocks eight cycles per byte sent.
    size_t remaining = count;
    while (remaining >= 8) {
      const size_t bytes = std::min(remaining / 8, kMaximumShiftBytes);
      Queue(static_cast<uint8_t>(kByteShiftFlag | bytes));
      for (size_t index = 0; index < bytes; ++index) {
        Queue(0x00);
      }
      remaining -= bytes * 8;
      if (!FlushIfFull()) {
        return false;
      }
    }

    for (size_t index = 0; index < remaining; ++index) {
      if (!ShiftOneBit(false, false, 0, nullptr)) {
        return false;
      }
    }
    return true;
  }

  bool Flush() override {
    if (pending_.empty()) {
      return true;
    }
    const bool sent = transport_.Write(pending_);
    pending_.clear();
    if (!sent) {
      Fail("Sending to the USB-Blaster failed.");
      return false;
    }
    return true;
  }

 private:
  // How many whole bytes from `bit` can be clocked in byte-shift mode: TMS
  // must be low for all eight cycles of each, and the count is capped at
  // what one command carries.
  size_t ByteShiftableBytes(std::span<const uint8_t> tms, size_t bit,
                            size_t bit_count) const {
    size_t bytes = 0;
    while (bytes < kMaximumShiftBytes && bit + (bytes + 1) * 8 <= bit_count) {
      bool tms_low = true;
      for (size_t index = 0; index < 8 && tms_low; ++index) {
        tms_low = !BitAt(tms, bit + bytes * 8 + index);
      }
      if (!tms_low) {
        break;
      }
      ++bytes;
    }
    return bytes;
  }

  // Eight cycles a byte, TDI only. There is deliberately no read here: the
  // caller never byte-shifts a scan it is reading, for the reason given in
  // Shift(), and a path this cable has been shown not to answer is better
  // deleted than left for somebody to reach for.
  bool ShiftBytes(std::span<const uint8_t> tdi, size_t bit, size_t bytes) {
    Queue(static_cast<uint8_t>(kByteShiftFlag | bytes));
    for (size_t index = 0; index < bytes; ++index) {
      uint8_t value = 0;
      for (size_t offset = 0; offset < 8; ++offset) {
        if (BitAt(tdi, bit + index * 8 + offset)) {
          value |= static_cast<uint8_t>(1U << offset);
        }
      }
      Queue(value);
    }

    return FlushIfFull();
  }

  // One TCK cycle in bit-bang mode: the pins are set with TCK low, then the
  // same state is sent again with TCK high to make the edge.
  //
  // TDO is asked for on the low half, deliberately. A TAP updates TDO on the
  // falling edge and the host samples it on the rising one, so the value
  // belonging to this cycle is the one standing while TCK is low — reading
  // after the edge would return the *next* bit and shift every answer along
  // by one. This is the one detail of this file that a bench run has to
  // confirm rather than a test (B-V1).
  bool ShiftOneBit(bool tms, bool tdi, size_t bit, std::vector<uint8_t>* tdo) {
    uint8_t pins = kBitBangOutputEnable;
    if (tms) {
      pins |= kBitBangTms;
    }
    if (tdi) {
      pins |= kBitBangTdi;
    }

    const bool reading = tdo != nullptr;
    Queue(static_cast<uint8_t>(pins | (reading ? kReadFlag : 0)));
    Queue(static_cast<uint8_t>(pins | kBitBangTck));

    if (!reading) {
      return FlushIfFull();
    }

    std::vector<uint8_t> answer;
    if (!Read(1, answer)) {
      return false;
    }
    SetBitAt(*tdo, bit, (answer[0] & 1U) != 0);
    return true;
  }

  void Queue(uint8_t byte) { pending_.push_back(byte); }

  bool FlushIfFull() {
    if (pending_.size() < kWriteBufferBytes) {
      return true;
    }
    return Flush();
  }

  // Read exactly `wanted` payload bytes, dropping the two status bytes from
  // the front of every packet the chip sends.
  bool Read(size_t wanted, std::vector<uint8_t>& payload) {
    if (!Flush()) {
      return false;
    }

    const size_t packet =
        std::max<size_t>(transport_.packet_bytes(), kFtdiStatusBytes + 1);
    payload.clear();
    payload.reserve(wanted);

    std::vector<uint8_t> buffer(packet * 8);
    size_t empty_reads = 0;
    while (payload.size() < wanted) {
      size_t received = 0;
      if (!transport_.Read(buffer, received)) {
        Fail("Reading from the USB-Blaster failed.");
        return false;
      }

      const size_t before = payload.size();
      for (size_t offset = 0; offset < received; offset += packet) {
        const size_t chunk = std::min(packet, received - offset);
        if (chunk <= kFtdiStatusBytes) {
          continue;
        }
        payload.insert(
            payload.end(),
            buffer.begin() + static_cast<ptrdiff_t>(offset + kFtdiStatusBytes),
            buffer.begin() + static_cast<ptrdiff_t>(offset + chunk));
      }

      // A packet of nothing but status is what the chip sends while it has
      // no data to give, and a few of them are ordinary. An unbroken run of
      // them is a cable that has stopped shifting, and waiting for it
      // forever would hang the flow that is driving this.
      if (payload.size() == before) {
        if (++empty_reads > kEmptyReadLimit) {
          Fail("The USB-Blaster stopped answering.");
          return false;
        }
      } else {
        empty_reads = 0;
      }
    }

    // More than was asked for means the cable sent a byte nothing requested,
    // and there is exactly one read outstanding at a time — so the stream is
    // out of step and every answer from here on would be one byte early.
    //
    // Said rather than silently dropped, which is what this used to do. A
    // dropped byte turns into a scan that reads back plausible rubbish,
    // fails a TDO comparison somewhere unrelated, and clears on the next
    // run: the hardest possible shape of bug to find from a screenshot, and
    // one this cable's unexplained read behaviour makes worth naming
    // (TESTING.md, B-V1).
    if (payload.size() > wanted) {
      Fail(
          "The USB-Blaster sent more than it was asked for, so its answers "
          "are no longer in step with the cycles they belong to.");
      return false;
    }

    return true;
  }

  void Fail(const std::string& message) {
    if (logger_ != nullptr) {
      logger_->Error(message);
    }
  }

  // How many status-only reads in a row are tolerated before the cable is
  // declared silent. The transport's own timeout is the first defence; this
  // is the one that catches a cable answering promptly with nothing.
  static constexpr size_t kEmptyReadLimit = 32;

  IFtdiTransport& transport_;
  ILogger* logger_ = nullptr;
  std::vector<uint8_t> pending_;
};

}  // namespace

std::unique_ptr<IJtagCable> MakeUsbBlasterCableOver(IFtdiTransport& transport,
                                                    ILogger* logger) {
  return std::make_unique<UsbBlasterCable>(transport, logger);
}

}  // namespace ddd::capture
