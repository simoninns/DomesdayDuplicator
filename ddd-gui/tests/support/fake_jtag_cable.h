/************************************************************************

    fake_jtag_cable.h

    A JTAG cable that records what it was asked to clock out
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "jtag_cable.h"

namespace ddd::capture {

// The whole of the SVF player's far end, in a form a test can read.
//
// It keeps the vector stream flat — one entry per TCK cycle, in the order
// they were clocked — because that is what the questions are about: did the
// player walk to Shift-DR by the shortest route, did it raise TMS on the last
// bit of the scan and not before, did it come back to the state the file
// asked it to end in. Reconstructing that from a list of calls would mean
// the test knowing how the player chose to batch its work, which is exactly
// what it should not care about.
//
// TDO comes from a stream the test primes: each bit read is taken from it in
// order, and anything past the end reads as zero.
class FakeJtagCable : public IJtagCable {
 public:
  bool Shift(std::span<const uint8_t> tms, std::span<const uint8_t> tdi,
             size_t bit_count, std::vector<uint8_t>* tdo) override {
    ++shift_calls_;
    if (fail_after_ >= 0 && shift_calls_ > fail_after_) {
      return false;
    }

    if (tdo != nullptr) {
      tdo->assign((bit_count + 7) / 8, 0);
    }

    for (size_t index = 0; index < bit_count; ++index) {
      tms_.push_back(BitAt(tms, index));
      tdi_.push_back(BitAt(tdi, index));
      read_.push_back(tdo != nullptr);

      if (tdo != nullptr) {
        const bool value =
            tdo_position_ < answers_.size() ? answers_[tdo_position_] : false;
        ++tdo_position_;
        if (value) {
          (*tdo)[index / 8] |= static_cast<uint8_t>(1U << (index % 8));
        }
      }
    }
    return true;
  }

  bool RunClock(size_t count) override {
    ++run_calls_;
    run_clocks_ += count;

    // Recorded in the same stream as everything else, because a wait is
    // TMS-low cycles in whatever state the TAP is in and the tests that
    // count cycles should not have to add two numbers together.
    for (size_t index = 0; index < count; ++index) {
      tms_.push_back(false);
      tdi_.push_back(false);
      read_.push_back(false);
    }
    return true;
  }

  bool Flush() override {
    ++flushes_;
    return true;
  }

  const char* Name() const override { return "fake"; }

  // What the cable was asked to clock, one entry per TCK cycle.
  const std::vector<bool>& tms() const { return tms_; }
  const std::vector<bool>& tdi() const { return tdi_; }
  const std::vector<bool>& read() const { return read_; }

  size_t clocks() const { return tms_.size(); }
  size_t run_clocks() const { return run_clocks_; }
  size_t shift_calls() const { return shift_calls_; }
  size_t flushes() const { return flushes_; }

  // The bits TDO will produce, in the order they are asked for.
  void AnswerWith(std::vector<bool> bits) { answers_ = std::move(bits); }

  // Answer with the bits of a hexadecimal value, least significant first —
  // the order a scan shifts them, and the order SVF's own values are read
  // in, so a test can prime this with the same text the file it is checking
  // would carry.
  void AnswerWithHex(const std::string& hex, size_t bit_count) {
    std::vector<bool> bits(bit_count, false);
    for (size_t index = 0; index < bit_count; ++index) {
      const size_t nibble = index / 4;
      if (nibble >= hex.size()) {
        break;
      }
      const char digit = hex[hex.size() - 1 - nibble];
      const int value =
          (digit >= '0' && digit <= '9')
              ? digit - '0'
              : (digit >= 'a' ? digit - 'a' + 10 : digit - 'A' + 10);
      bits[index] = ((value >> (index % 4)) & 1) != 0;
    }
    answers_ = std::move(bits);
  }

  // Fail every shift after this many have been made, so the player's
  // handling of a cable that stops answering can be exercised.
  void FailAfterShifts(int shifts) { fail_after_ = shifts; }

 private:
  static bool BitAt(std::span<const uint8_t> bits, size_t index) {
    const size_t byte = index / 8;
    if (byte >= bits.size()) {
      return false;
    }
    return ((bits[byte] >> (index % 8)) & 1U) != 0;
  }

  std::vector<bool> tms_;
  std::vector<bool> tdi_;
  std::vector<bool> read_;
  std::vector<bool> answers_;
  size_t tdo_position_ = 0;
  size_t run_clocks_ = 0;
  size_t shift_calls_ = 0;
  size_t run_calls_ = 0;
  size_t flushes_ = 0;
  int fail_after_ = -1;
};

}  // namespace ddd::capture
