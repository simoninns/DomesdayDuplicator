/************************************************************************

    test_usb_blaster_cable.cpp

    T1 unit test for the USB-Blaster's wire protocol
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "fake_ftdi_transport.h"
#include "usb_blaster_cable.h"

namespace ddd::capture {
namespace {

// The command bits, spelled out here as well as in the cable, deliberately.
// This is a wire protocol nobody can look up in a specification: the second
// copy is what makes a change to the first one a change a test notices
// rather than a change a bench discovers.
constexpr uint8_t kTck = 0x01;
constexpr uint8_t kTms = 0x02;
constexpr uint8_t kTdi = 0x10;
constexpr uint8_t kOutputEnable = 0x20;
constexpr uint8_t kRead = 0x40;
constexpr uint8_t kByteShift = 0x80;

// A run of bits, packed the way the cable takes them: first bit clocked in
// the least significant bit of the first byte.
std::vector<uint8_t> Packed(const std::string& bits) {
  std::vector<uint8_t> packed((bits.size() + 7) / 8, 0);
  for (size_t index = 0; index < bits.size(); ++index) {
    if (bits[index] == '1') {
      packed[index / 8] |= static_cast<uint8_t>(1U << (index % 8));
    }
  }
  return packed;
}

std::string Unpacked(const std::vector<uint8_t>& packed, size_t bit_count) {
  std::string bits;
  for (size_t index = 0; index < bit_count; ++index) {
    bits.push_back(((packed[index / 8] >> (index % 8)) & 1U) != 0 ? '1' : '0');
  }
  return bits;
}

class UsbBlasterCableTest : public ::testing::Test {
 protected:
  bool Shift(const std::string& tms, const std::string& tdi,
             std::vector<uint8_t>* tdo = nullptr) {
    return cable_->Shift(Packed(tms), Packed(tdi), tms.size(), tdo);
  }

  FakeFtdiTransport transport_;
  std::unique_ptr<IJtagCable> cable_ =
      MakeUsbBlasterCableOver(transport_, nullptr);
};

// --- Bit-bang --------------------------------------------------------------

// One TCK cycle is two command bytes carrying the same pin state, the second
// with the clock high. Everything else in this file is an optimisation of
// this.
TEST_F(UsbBlasterCableTest, ACycleIsTheSamePinsTwiceWithTheClockRaised) {
  ASSERT_TRUE(Shift("0", "1"));
  ASSERT_TRUE(cable_->Flush());

  const std::vector<uint8_t> expected{
      static_cast<uint8_t>(kOutputEnable | kTdi),
      static_cast<uint8_t>(kOutputEnable | kTdi | kTck)};
  EXPECT_EQ(transport_.written(), expected);
}

TEST_F(UsbBlasterCableTest, TmsIsCarriedOnItsOwnBit) {
  ASSERT_TRUE(Shift("1", "0"));
  ASSERT_TRUE(cable_->Flush());

  const std::vector<uint8_t> expected{
      static_cast<uint8_t>(kOutputEnable | kTms),
      static_cast<uint8_t>(kOutputEnable | kTms | kTck)};
  EXPECT_EQ(transport_.written(), expected);
}

// The read is asked for on the half of the cycle with the clock low. A TAP
// updates TDO on the falling edge and the host samples it on the rising one,
// so that half is the value belonging to this cycle; asking on the other
// half would return the next bit and shift every answer along by one.
TEST_F(UsbBlasterCableTest, TdoIsAskedForBeforeTheRisingEdge) {
  transport_.AnswerWith({0x01});

  std::vector<uint8_t> tdo;
  ASSERT_TRUE(Shift("1", "0", &tdo));

  ASSERT_GE(transport_.written().size(), 2u);
  EXPECT_EQ(transport_.written()[0], kOutputEnable | kTms | kRead);
  EXPECT_EQ(transport_.written()[1], kOutputEnable | kTms | kTck);
  EXPECT_EQ(Unpacked(tdo, 1), "1");
}

// --- Byte-shift ------------------------------------------------------------

// Eight cycles with TMS low become one command and one data byte: eight
// times less traffic than bit-banging them, which across a flash image is
// the difference between minutes and an afternoon.
TEST_F(UsbBlasterCableTest, EightCyclesWithTmsLowBecomeOneShiftedByte) {
  ASSERT_TRUE(Shift("00000000", "10100101"));
  ASSERT_TRUE(cable_->Flush());

  ASSERT_EQ(transport_.written().size(), 2u);
  EXPECT_EQ(transport_.written()[0], kByteShift | 1);

  // The data byte is the same bits in the same order: first clocked in the
  // least significant bit.
  EXPECT_EQ(transport_.written()[1], 0xA5);
}

// A scan raises TMS on its final bit to leave the shift state as it shifts,
// so that one bit cannot go in a byte-shift command and drops back to
// bit-bang. Everything before it still goes the fast way.
TEST_F(UsbBlasterCableTest, TheLastBitOfAScanFallsBackToBitBang) {
  ASSERT_TRUE(Shift("000000001", "111111110"));
  ASSERT_TRUE(cable_->Flush());

  ASSERT_EQ(transport_.written().size(), 4u);
  EXPECT_EQ(transport_.written()[0], kByteShift | 1);
  EXPECT_EQ(transport_.written()[1], 0xFF);
  EXPECT_EQ(transport_.written()[2], kOutputEnable | kTms);
  EXPECT_EQ(transport_.written()[3], kOutputEnable | kTms | kTck);
}

// A shift that is being read back does not use byte-shift mode at all, and
// this is the one rule in this file that came from hardware rather than from
// the protocol (B-V1, 2026-08-17). Asked for a read, the cable returns FF for
// every byte-shifted byte — no answer at all — while the same bits read
// correctly one cycle at a time. The cost of avoiding it is nothing that
// matters: 103 bits of this project's 73,297,811 are read.
TEST_F(UsbBlasterCableTest, AShiftThatIsReadBackNeverUsesByteShift) {
  // One payload byte to a packet, which is what a cable answering one
  // bit-bang cycle at a time looks like.
  FakeFtdiTransport one_at_a_time(kFtdiStatusBytes + 1);
  std::unique_ptr<IJtagCable> cable =
      MakeUsbBlasterCableOver(one_at_a_time, nullptr);
  one_at_a_time.AnswerWith(std::vector<uint8_t>(16, 0x01));

  std::vector<uint8_t> tdo;
  const std::string cycles(16, '0');
  ASSERT_TRUE(cable->Shift(Packed(cycles), Packed(cycles), 16, &tdo));

  for (uint8_t byte : one_at_a_time.written()) {
    EXPECT_EQ(byte & kByteShift, 0)
        << "a byte-shift command was used for a shift that reads TDO";
  }

  // And every bit came back, one bit-bang cycle each.
  EXPECT_EQ(Unpacked(tdo, 16), "1111111111111111");
}

// The fast path is still the fast path for everything that carries data: a
// write of the same length is one command and two data bytes.
TEST_F(UsbBlasterCableTest, AShiftThatIsNotReadStillUsesByteShift) {
  ASSERT_TRUE(Shift("0000000000000000", "0000000000000000"));
  ASSERT_TRUE(cable_->Flush());

  ASSERT_EQ(transport_.written().size(), 3u);
  EXPECT_EQ(transport_.written()[0], kByteShift | 2);
}

// One command can carry sixty-three data bytes, so a longer run is split.
TEST_F(UsbBlasterCableTest, ALongRunIsSplitIntoWholeCommands) {
  const size_t bits = (kMaximumShiftBytes + 1) * 8;
  ASSERT_TRUE(Shift(std::string(bits, '0'), std::string(bits, '0')));
  ASSERT_TRUE(cable_->Flush());

  ASSERT_EQ(transport_.written().size(), kMaximumShiftBytes + 1 + 1 + 1);
  EXPECT_EQ(transport_.written()[0], kByteShift | kMaximumShiftBytes);
  EXPECT_EQ(transport_.written()[kMaximumShiftBytes + 1], kByteShift | 1);
}

// --- Waiting ---------------------------------------------------------------

// An EPCS write spends more cycles waiting than shifting, and a wait is
// TMS-low cycles with nothing to read — which is byte-shift mode's own case.
TEST_F(UsbBlasterCableTest, WaitingUsesWholeBytesAndBitBangsTheRemainder) {
  ASSERT_TRUE(cable_->RunClock(20));
  ASSERT_TRUE(cable_->Flush());

  // Two shifted bytes for sixteen cycles, then four cycles bit-banged.
  ASSERT_EQ(transport_.written().size(), 1u + 2u + 8u);
  EXPECT_EQ(transport_.written()[0], kByteShift | 2);
  EXPECT_EQ(transport_.written()[3], kOutputEnable);
  EXPECT_EQ(transport_.written()[4], kOutputEnable | kTck);
}

TEST_F(UsbBlasterCableTest, WaitingNeverAsksForAnythingBack) {
  ASSERT_TRUE(cable_->RunClock(4500));
  ASSERT_TRUE(cable_->Flush());

  for (uint8_t byte : transport_.written()) {
    EXPECT_EQ(byte & kRead, 0) << "a wait asked the cable to answer";
  }
  EXPECT_EQ(transport_.reads(), 0u);
}

// --- The pipe --------------------------------------------------------------

// Commands pile up until something needs the cable to have caught up, which
// is what makes the whole path fast enough to be usable.
TEST_F(UsbBlasterCableTest, CommandsAreHeldBackUntilThereIsAReasonToSend) {
  ASSERT_TRUE(Shift("00000000", "00000000"));
  EXPECT_TRUE(transport_.written().empty());

  ASSERT_TRUE(cable_->Flush());
  EXPECT_FALSE(transport_.written().empty());
}

TEST_F(UsbBlasterCableTest, AskingForAnAnswerSendsWhatIsWaitingFirst) {
  transport_.AnswerWith({0x00});

  ASSERT_TRUE(Shift("00000000", "11111111"));

  std::vector<uint8_t> tdo;
  ASSERT_TRUE(Shift("1", "0", &tdo));

  // The queued byte-shift went out with the read, not after it.
  ASSERT_GE(transport_.written().size(), 3u);
  EXPECT_EQ(transport_.written()[0], kByteShift | 1);
  EXPECT_EQ(transport_.written()[1], 0xFF);
}

// The chip puts two status bytes at the front of every packet it sends, and
// they are not data. Getting this wrong reads every answer two bytes late,
// which looks like a cable that works and returns rubbish.
TEST_F(UsbBlasterCableTest, TheStatusBytesOnEveryPacketAreNotData) {
  FakeFtdiTransport small(kFtdiStatusBytes + 1);  // One payload byte a packet.
  std::unique_ptr<IJtagCable> cable = MakeUsbBlasterCableOver(small, nullptr);

  // Three cycles, so three answer bytes, each behind its own pair of status
  // bytes. A reader that took the status bytes for data would report 0x31.
  small.AnswerWith({0x01, 0x00, 0x01});

  std::vector<uint8_t> tdo;
  const std::string cycles(3, '0');
  ASSERT_TRUE(cable->Shift(Packed(cycles), Packed(cycles), 3, &tdo));

  EXPECT_EQ(Unpacked(tdo, 3), "101")
      << "the payload was reassembled with status bytes in it";
  EXPECT_GE(small.reads(), 3u) << "the answers arrived in one packet, so the "
                                  "framing was never exercised";
}

TEST_F(UsbBlasterCableTest, ASendThatFailsFailsTheShift) {
  transport_.FailWrites();

  EXPECT_TRUE(Shift("00000000", "00000000"));  // Queued, nothing sent yet.
  EXPECT_FALSE(cable_->Flush());
}

TEST_F(UsbBlasterCableTest, AReadThatFailsFailsTheShift) {
  transport_.FailReads();

  std::vector<uint8_t> tdo;
  EXPECT_FALSE(Shift("1", "0", &tdo));
}

// A cable that answers promptly with nothing is a cable that has stopped
// shifting, and waiting for it forever would hang whatever is driving this.
TEST_F(UsbBlasterCableTest, ACableThatOnlySendsStatusIsGivenUpOn) {
  transport_.AnswerWith({});

  std::vector<uint8_t> tdo;
  EXPECT_FALSE(Shift("1", "0", &tdo));
}

// And a cable that answers with *more* than it was asked for is refused too,
// which is the less obvious half of the same rule.
//
// One read is outstanding at a time, so a surplus byte is one nothing asked
// for and the stream is out of step: every answer after it belongs to the
// cycle before. Dropping the surplus — which is what this used to do — turns
// that into a scan that reads back plausible rubbish and fails a comparison
// somewhere unrelated, clears on the next run, and cannot be diagnosed from
// the message it produces.
TEST_F(UsbBlasterCableTest, ACableThatSaysMoreThanItWasAskedForIsRefused) {
  transport_.AnswerWith({0x01, 0x00});

  std::vector<uint8_t> tdo;
  EXPECT_FALSE(Shift("1", "0", &tdo));
}

}  // namespace
}  // namespace ddd::capture
