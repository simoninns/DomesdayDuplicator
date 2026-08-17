/************************************************************************

    provisioning_orchestrator.h

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

#include "device_recovery.h"
#include "jtag_cable.h"
#include "svf_player.h"
#include "update_bundle.h"
#include "update_orchestrator.h"

namespace ddd::capture {

class ILogger;

// The device work a first bring-up does, and the order it does it in.
//
// **The order is the point of this class.** The FX3 and the FPGA share one
// interconnect, and one line changed direction between the legacy design and
// the current one: `CTL_07`/`GPIO_24` is an output of the legacy firmware and
// an output of the modern gateware, so a board running legacy firmware under
// modern gateware has two drivers on one net. The reverse mixture — modern
// firmware over legacy gateware — leaves the net undriven from both ends,
// which is a floating input and not a fault.
//
// The Explorer Kit puts 22 Ω in series on every CTL line, so that contention
// is tens of milliamps rather than a bare short. That is source-series
// termination for a 100 MHz bus rather than protection, and it is an order of
// magnitude below what would make the pairing acceptable — it buys time, not
// safety, and the pairing is sustained rather than transient.
//
// So there is exactly one safe ordering for bringing a board up:
//
//     the FX3 becomes modern first, and the FPGA second.
//
// That is a hardware property rather than a presentation choice, and it is
// worth machine-checking rather than trusting to the order somebody laid pages
// out in. ProgramGateware refuses until InstallFirmware has succeeded, whatever
// calls it and in whatever order — so a wizard page wired up wrongly is a
// refused operation rather than two devices driven out of specification.
// (See the plan's *Two constraints*, and verification item B-V0, which is what
// confirms the reading of the two source trees this rests on.)
//
// Qt-free and synchronous. Both halves block for minutes, so this belongs on a
// worker thread; the same object is called twice, from the same thread, with a
// power-cycle-free gap in between while the user takes a jumper off.

// How the two halves reach their hardware.
//
// Factories rather than objects, for the same reason DeviceAccess uses them:
// the device the FX3 half updates does not exist when the wizard starts, and
// the cable the FPGA half drives should not be held open across the minutes in
// between — Quartus's jtagd would like it too.
struct ProvisioningAccess {
  // The FX3, sitting in its boot ROM with the PMODE jumper fitted.
  DeviceAccess fx3;

  // Opens the JTAG cable, or returns nothing having written a sentence into
  // `problem`.
  std::function<std::unique_ptr<IJtagCable>(std::string* problem)> open_cable;
};

// What the FPGA half did.
struct ProvisioningGatewareOutcome {
  bool succeeded = false;

  // Set when the caller asked for the run to stop rather than anything going
  // wrong. The flash is then partly written, which is safe: the same vectors
  // can simply be played again, and nothing boots from a half-written EPCS
  // until it is power-cycled.
  bool stopped = false;

  // Written for a user, and empty on success.
  std::string problem;

  // What the player counted, for the log and for TESTING.md's bench records.
  SvfPlayResult play;
};

// Roughly how long playing this many bytes of SVF will take, in seconds.
//
// Derived rather than measured, and deliberately pessimistic, exactly like
// EstimateUpdateSeconds: an estimate that is too long makes a user wait and an
// estimate that is too short makes them unplug the cable in the middle of a
// flash write.
//
// The derivation, from this project's own provisioning file (18.4 MB of SVF,
// 37,140 statements, 73.3 Mbit shifted, 471.9 M idle clocks, declared at
// 4.5 MHz): the clocks alone stand for about 105 seconds the flash genuinely
// needs, the shifted bits for another 16, and the byte-shift traffic for those
// is about 68 MB over a full-speed FT245 — which overlaps the waiting rather
// than adding to it. Rounded up to five minutes for that file, which is
// 60 KB of file per second.
//
// Replaced with a measurement when B-V1 is taken on the bench.
int EstimateProvisioningSeconds(uint64_t svf_bytes);

class ProvisioningOrchestrator {
 public:
  ProvisioningOrchestrator(ProvisioningAccess access, ILogger* logger);

  ProvisioningOrchestrator(const ProvisioningOrchestrator&) = delete;
  ProvisioningOrchestrator& operator=(const ProvisioningOrchestrator&) = delete;
  ProvisioningOrchestrator(ProvisioningOrchestrator&&) = delete;
  ProvisioningOrchestrator& operator=(ProvisioningOrchestrator&&) = delete;

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

  // Write the bundle's firmware to a device sitting in its boot ROM.
  //
  // The ordinary recovery path, with one difference: the restart and the
  // confirmation are deferred, because the PMODE jumper that put the device in
  // its boot ROM is still fitted and a reset would land it back there. The
  // caller owns the power cycle and the check afterwards — which the bring-up
  // flow was going to own anyway, since the FPGA half needs one too.
  //
  // The bundle must carry firmware; a set that carries only provisioning
  // gateware is refused with a sentence rather than attempted.
  UpdateOutcome InstallFirmware(const UpdateBundle& bundle);

  // Play the bundle's provisioning vectors into the FPGA's configuration
  // flash.
  //
  // Refused until InstallFirmware has succeeded — see the note at the top of
  // this file. That refusal is the ordering rule, and it is here rather than
  // in the interface so that it holds for every caller.
  ProvisioningGatewareOutcome ProgramGateware(const UpdateBundle& bundle);

  // Whether the FX3 half has been done, which is the only thing that unlocks
  // the FPGA half.
  bool firmware_installed() const { return firmware_installed_; }

 private:
  ProvisioningGatewareOutcome GatewareFailure(std::string problem) const;

  ProvisioningAccess access_;
  ILogger* logger_ = nullptr;
  DeviceRecoveryTimings timings_;
  UpdateTimings update_timings_;
  UpdateProgressCallback progress_;
  std::function<bool()> cancel_;

  bool firmware_installed_ = false;
};

}  // namespace ddd::capture
