/************************************************************************

    jtag_cable.h

    The seam between JTAG vectors and the cable that clocks them out
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ddd::capture {

class ILogger;

// A JTAG cable, at the level of TCK cycles and nothing above it.
//
// This is the third device seam in the engine, beside IDeviceUpdater and
// IDeviceProgrammer, and it is deliberately the smallest of the three: a
// cable clocks TMS and TDI out and reads TDO back, and every idea above that
// — TAP states, instruction registers, what a Cyclone IV wants — belongs to
// the player that drives it (svf_player.h) or to Quartus, which decided all
// of it at build time.
//
// Keeping the seam this low costs nothing and buys two things. A fake
// records the vector stream exactly as the cable would see it, so the whole
// of the player is testable with no hardware; and the one place that knows
// how to make a particular cable go fast — the USB-Blaster's byte-shift mode
// — is the implementation, not the caller.
//
// Bit packing, used by every method here: bit `i` of a run lives in bit
// `i % 8` of byte `i / 8`, so the first bit clocked is the least significant
// bit of the first byte. That is the order JTAG shifts in and the order the
// USB-Blaster's byte-shift mode expects, so nothing reverses anything.
//
// Every call is blocking. This is driven from a worker thread, never from a
// user interface thread: writing a flash image takes minutes.
//
// Thread-safety: NOT thread-safe. One thread owns a cable for its lifetime.
class IJtagCable {
 public:
  IJtagCable() = default;
  virtual ~IJtagCable() = default;

  IJtagCable(const IJtagCable&) = delete;
  IJtagCable& operator=(const IJtagCable&) = delete;
  IJtagCable(IJtagCable&&) = delete;
  IJtagCable& operator=(IJtagCable&&) = delete;

  // Clock `bit_count` TCK cycles, driving TMS and TDI from the packed runs
  // given, and capture TDO into `tdo` if it is not null.
  //
  // `tms` and `tdi` must each hold at least `bit_count` bits. `tdo` is
  // resized to hold `bit_count` bits, packed the same way, and the value
  // captured for cycle `i` is the one TDO held while TCK was low — which is
  // the edge the target updates it on and therefore the value belonging to
  // that cycle.
  //
  // Asking for TDO is what forces a round trip to the cable, so a caller
  // that does not need it should not ask: a scan with nothing to compare is
  // queued and the next one is queued behind it, and neither waits for the
  // bus.
  virtual bool Shift(std::span<const uint8_t> tms, std::span<const uint8_t> tdi,
                     size_t bit_count, std::vector<uint8_t>* tdo) = 0;

  // Clock `count` cycles with TMS and TDI held low, capturing nothing.
  //
  // This is SVF's RUNTEST, and it is not a convenience: an EPCS write spends
  // more time here than anywhere else — the provisioning file for this
  // project asks for around twenty million idle cycles against seventy
  // million shifted bits — and a cable that can clock a byte per command
  // does it in an eighth of the traffic that clocking one bit at a time
  // would take. Expressing it as a shift of zeros would hide that.
  virtual bool RunClock(size_t count) = 0;

  // Send anything still buffered and wait for the cable to have taken it.
  //
  // Called at the end of a run, and by the implementation itself whenever a
  // caller asks for TDO. A player that never reads anything back must still
  // flush, or the last few vectors of a programming pass are never sent.
  virtual bool Flush() = 0;

  // A short name for logs and for the interface: "USB-Blaster", "none".
  virtual const char* Name() const = 0;
};

// A cable that goes nowhere, counting what it was asked to do.
//
// The point of it is checking a programming file without a board: a file
// that plays cleanly against this one is a file this application can read,
// parse and drive, and the only thing left unproved is the hardware. That is
// exactly what the shell tool's dry run offers, and it is the only mode of
// this whole path that CI is allowed anywhere near (AGENTS.md §4).
//
// TDO reads back as all-ones, which is what an unloaded JTAG input floats to
// and is deliberately *not* an attempt to look like a device: a file with
// expected responses in it will fail against this cable, and it should.
class NullJtagCable : public IJtagCable {
 public:
  bool Shift(std::span<const uint8_t> tms, std::span<const uint8_t> tdi,
             size_t bit_count, std::vector<uint8_t>* tdo) override {
    (void)tms;
    (void)tdi;
    shifted_bits_ += bit_count;
    if (tdo != nullptr) {
      tdo->assign((bit_count + 7) / 8, 0xFF);
    }
    return true;
  }

  bool RunClock(size_t count) override {
    run_clocks_ += count;
    return true;
  }

  bool Flush() override { return true; }

  const char* Name() const override { return "none"; }

  uint64_t shifted_bits() const { return shifted_bits_; }
  uint64_t run_clocks() const { return run_clocks_; }

 private:
  uint64_t shifted_bits_ = 0;
  uint64_t run_clocks_ = 0;
};

// The USB identifiers of the cables this project meets.
//
// The DE0-Nano carries an on-board USB-Blaster on its mini-USB connector,
// which is the only electrical route to a blank FPGA's configuration flash
// (docs: EPCS layout and boot flow). It is 09fb:6001; the rest of the family
// is listed because recognising a cable this code cannot drive and saying so
// is worth far more than "no cable found" to somebody holding one.
inline constexpr uint16_t kAlteraVendorId = 0x09fb;
inline constexpr uint16_t kUsbBlasterProductId = 0x6001;
inline constexpr uint16_t kUsbBlasterRevisionBProductId = 0x6002;
inline constexpr uint16_t kUsbBlasterRevisionCProductId = 0x6003;
inline constexpr uint16_t kUsbBlasterTwoProductId = 0x6010;
inline constexpr uint16_t kUsbBlasterTwoAlternateProductId = 0x6810;

// Open the DE0-Nano's on-board USB-Blaster.
//
// Returns nothing if no cable is attached, if one is attached that this code
// does not drive, or if it could not be opened — each of which is logged as
// itself, because on Linux the third is nearly always the udev rules
// (fpga/configs/70-altera-usb-blaster.rules) and on Windows the driver
// binding, and neither is diagnosable from "failed".
//
// `problem` takes the same sentence the log gets, for a caller that has to put
// it on the screen rather than in a file. The bring-up wizard's connectivity
// page is that caller: the whole value of the page is that it names which of
// those three happened while the user is still in a position to fix it.
std::unique_ptr<IJtagCable> MakeUsbBlasterCable(ILogger* logger,
                                                std::string* problem = nullptr);

}  // namespace ddd::capture
