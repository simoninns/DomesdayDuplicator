/************************************************************************

    bringup_orchestrator.h

    Bringing a board up to current firmware and gateware, in the one order
    that is safe
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "device_recovery.h"
#include "jtag_cable.h"
#include "svf_player.h"
#include "update_bundle.h"
#include "update_orchestrator.h"

namespace ddd::capture {

class ILogger;

// The device work a bring-up does, and the order it does it in.
//
// **The order is the point of this class.** The FX3 and the FPGA share one
// interconnect, and one line changed direction between the legacy design and
// the current one: `CTL_07`/`GPIO_24` is a push-pull output of the legacy
// firmware and the MISO output of the modern gateware, so a board running
// legacy firmware under modern gateware has two drivers on one net. The
// Explorer Kit puts 22 Ω in series on every CTL line, which makes that
// contention tens of milliamps rather than a bare short — source-series
// termination for a 100 MHz bus rather than protection, an order of magnitude
// below what would make the pairing acceptable, and sustained rather than
// transient because the legacy firmware claims the pin and holds it.
//
// What keeps the board out of that pairing is not a sequence of pages. It is
// where the FX3 is while the FPGA changes:
//
//     the FX3 sits in its boot ROM before the FPGA is touched, and only ever
//     runs the new firmware afterwards.
//
// In the boot ROM the FX3's GPIF and GPIO pins are unconfigured and undriven,
// so the FPGA can become modern underneath it with nothing to contend with.
// By the time any firmware runs again it is the firmware out of the bundle,
// and there is no path back to the legacy image — so the bad pairing is not
// avoided by care, it is unreachable.
//
// That is a hardware property rather than a presentation choice, and it is
// machine-checked rather than trusted to the order somebody laid pages out in:
// ProgramDevice refuses until ConfigureFpga has succeeded, whatever calls it
// and in whatever order.
//
// The write ordering *inside* ProgramDevice carries the same kind of weight
// and is documented there.
//
// Qt-free and synchronous. Both halves block — one for seconds and one for
// minutes — so this belongs on a worker thread; the same object is called
// twice, from the same thread.

// How the two halves reach their hardware.
//
// Factories rather than objects, for the same reason DeviceAccess uses them:
// the device that is programmed does not exist when the wizard starts, and the
// cable should not be held open across the minutes of flash writing that
// follow — Quartus's jtagd would like it too.
struct BringUpAccess {
  // The FX3, sitting in its boot ROM.
  DeviceAccess fx3;

  // Opens the JTAG cable, or returns nothing having written a sentence into
  // `problem`.
  std::function<std::unique_ptr<IJtagCable>(std::string* problem)> open_cable;
};

// What configuring the FPGA over JTAG did.
//
// Volatile from beginning to end: it puts the factory image into the FPGA's
// configuration memory and writes nothing to the board. Stopping it, or
// failing it, leaves a board exactly as it was — which is why this is the half
// that runs while the FX3 has no firmware.
struct BringUpConfigureOutcome {
  bool succeeded = false;

  // Set when the caller asked for the run to stop rather than anything going
  // wrong.
  bool stopped = false;

  // Written for a user, and empty on success.
  std::string problem;

  // What the player counted, for the log and for TESTING.md's bench records.
  SvfPlayResult play;

  // How many times the file was played. More than one means an attempt
  // failed and was made again — see ConfigureFpga for why that is allowed
  // here and nowhere else.
  int attempts = 0;
};

// Roughly how long playing this many bytes of SVF will take, in seconds.
//
// Measured rather than derived. The measurement: 1,450,426 bytes — 5,749,532
// bits, 17 statements — took **2.6 seconds** through the DE0-Nano's on-board
// Blaster on 2026-08-17. That is 558 KB of file per second, and the rate below
// is rounded down to 500 KB for the same reason the update path rounds its own
// estimate up: an estimate that is too long makes a user wait and one that is
// too short makes them unplug a cable in the middle of something.
//
// The writes that follow are estimated by EstimateUpdateSeconds, like every
// other write to those media, because that is exactly what they are.
int EstimateConfigureSeconds(uint64_t svf_bytes);

class BringUpOrchestrator {
 public:
  BringUpOrchestrator(BringUpAccess access, ILogger* logger);

  BringUpOrchestrator(const BringUpOrchestrator&) = delete;
  BringUpOrchestrator& operator=(const BringUpOrchestrator&) = delete;
  BringUpOrchestrator(BringUpOrchestrator&&) = delete;
  BringUpOrchestrator& operator=(BringUpOrchestrator&&) = delete;

  void SetTimings(const DeviceRecoveryTimings& timings) { timings_ = timings; }
  void SetUpdateTimings(const UpdateTimings& timings) {
    update_timings_ = timings;
  }

  void SetProgressCallback(UpdateProgressCallback callback) {
    progress_ = std::move(callback);
  }

  // Polled repeatedly; return true to stop.
  void SetCancelCallback(std::function<bool()> cancel) {
    cancel_ = std::move(cancel);
  }

  // Play the bundle's vectors into the FPGA, giving it the factory image out
  // of its own configuration memory.
  //
  // Nothing is written to the board. What this buys is the flash bridge: from
  // here on the FPGA answers register reads and can have its own flash written
  // through it, which is what makes every step after this one an ordinary
  // update.
  //
  // Runs while the FX3 is in its boot ROM, and that is the safety property —
  // see the note at the top of this file.
  BringUpConfigureOutcome ConfigureFpga(const UpdateBundle& bundle);

  // Program every permanent thing on the board, from a device sitting in its
  // boot ROM.
  //
  // Refused until ConfigureFpga has succeeded. That refusal is the ordering
  // rule, and it is here rather than in the interface so that it holds for
  // every caller.
  //
  // The bundle must carry all four payloads; a file that could do half the job
  // is refused before any of it is done rather than discovered part way
  // through.
  UpdateOutcome ProgramDevice(const UpdateBundle& bundle);

  // Whether the FPGA has been configured, which is the only thing that unlocks
  // the half that writes.
  bool fpga_configured() const { return fpga_configured_; }

 private:
  BringUpConfigureOutcome ConfigureFailure(std::string problem) const;

  // One attempt at the vectors: open the cable, play the file, close it
  // again. ConfigureFpga decides how many of these there are, and needs to
  // know whether the cable opened at all to decide whether a second is worth
  // anything.
  BringUpConfigureOutcome PlayVectors(std::string_view text, int attempt,
                                      bool& cable_opened);

  BringUpAccess access_;
  ILogger* logger_ = nullptr;
  DeviceRecoveryTimings timings_;
  UpdateTimings update_timings_;
  UpdateProgressCallback progress_;
  std::function<bool()> cancel_;

  bool fpga_configured_ = false;
};

}  // namespace ddd::capture
