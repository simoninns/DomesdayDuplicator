/************************************************************************

    fake_ftdi_transport.h

    The byte pipe a USB-Blaster rides on, without the cable
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "usb_blaster_cable.h"

namespace ddd::capture {

// Everything the cable puts on the wire, kept for a test to read, and
// everything the chip would send back, framed the way it frames it.
//
// The framing is the point of the read half: an FT245 puts two status bytes
// at the front of every packet it sends, so a payload of a hundred bytes
// arrives as two packets with four bytes of something else mixed in. Getting
// that wrong shifts every answer along by two and produces a cable that
// appears to work and reads rubbish, which is precisely the class of bug
// that is impossible to find on a bench and trivial to find here.
class FakeFtdiTransport : public IFtdiTransport {
 public:
  explicit FakeFtdiTransport(size_t packet_bytes = 64)
      : packet_bytes_(packet_bytes) {}

  bool Write(std::span<const uint8_t> bytes) override {
    if (fail_writes_) {
      return false;
    }
    ++writes_;
    written_.insert(written_.end(), bytes.begin(), bytes.end());
    return true;
  }

  bool Read(std::span<uint8_t> buffer, size_t& received) override {
    received = 0;
    if (fail_reads_) {
      return false;
    }
    ++reads_;

    // One packet at a time, which is the least convenient thing a real chip
    // can do and therefore the thing worth doing here: a reader that only
    // works when everything arrives at once does not work.
    if (buffer.size() < packet_bytes_) {
      return false;
    }

    buffer[0] = 0x31;  // Modem status, as an FT245 reports it when idle.
    buffer[1] = 0x60;
    received = kFtdiStatusBytes;

    const size_t available =
        std::min(packet_bytes_ - kFtdiStatusBytes, answers_.size() - position_);
    for (size_t index = 0; index < available; ++index) {
      buffer[kFtdiStatusBytes + index] = answers_[position_ + index];
    }
    position_ += available;
    received += available;
    return true;
  }

  size_t packet_bytes() const override { return packet_bytes_; }

  // The bytes the cable sent, in order.
  const std::vector<uint8_t>& written() const { return written_; }
  size_t writes() const { return writes_; }
  size_t reads() const { return reads_; }

  // What the chip has to say, as payload — the status bytes are added by the
  // read above, because they are the chip's and not the caller's.
  void AnswerWith(std::vector<uint8_t> payload) {
    answers_ = std::move(payload);
    position_ = 0;
  }

  void FailWrites() { fail_writes_ = true; }
  void FailReads() { fail_reads_ = true; }

 private:
  size_t packet_bytes_;
  std::vector<uint8_t> written_;
  std::vector<uint8_t> answers_;
  size_t position_ = 0;
  size_t writes_ = 0;
  size_t reads_ = 0;
  bool fail_writes_ = false;
  bool fail_reads_ = false;
};

}  // namespace ddd::capture
