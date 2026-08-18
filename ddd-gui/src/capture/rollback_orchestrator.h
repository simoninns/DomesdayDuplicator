/************************************************************************

    rollback_orchestrator.h

    Returning a unit to the original firmware and gateware, in the one
    order that is safe
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "device_updater.h"
#include "update_bundle.h"
#include "update_orchestrator.h"

namespace ddd::capture {

class ILogger;

// The device work a rollback does, and the order it does it in.
//
// **The order is the point of this class, and it is the opposite of
// ProvisioningOrchestrator's.** The FX3 and the FPGA share one interconnect,
// and `CTL_07`/`GPIO_24` is an output of the original firmware and an output
// of the current gateware — so those two must never run together, while the
// reverse mixture leaves the net undriven from both ends and is merely a
// floating input.
//
//     the FPGA becomes legacy first, and the FX3 second.
//
// Bring-up runs the other way for the same reason: the FX3 is always the
// first thing to become modern and the last thing to become legacy. Written
// down here as well as there because the two orderings look like a
// presentation choice and are not, and because a reader who has just read the
// bring-up file will expect this one to agree with it.
//
// InstallFirmware refuses until ProgramGateware has succeeded, whatever calls
// it and in whatever order, so a wizard page wired up wrongly is a refused
// operation rather than two outputs on one wire.
//
// **No JTAG, and no cable.** A unit that can be rolled back is by definition
// running this application's own firmware over gateware with a flash bridge,
// so both images go over the ordinary USB link: the legacy gateware to the
// EPCS at address 0 through the bridge, and the legacy firmware to the boot
// EEPROM by the update protocol. Nothing here opens a USB-Blaster and nothing
// here needs the case off — which is the one way rollback is easier than the
// bring-up that undoes it, and worth saying because a user will ask.
//
// Qt-free and synchronous. Both halves block, so this belongs on a worker
// thread.
class RollbackOrchestrator {
 public:
  // The updater is the device this rolls back. It has to be open before this
  // class exists, unlike bring-up's — where the device does not exist yet
  // because it is sitting in a boot ROM waiting to be given firmware.
  RollbackOrchestrator(IDeviceUpdater& device, ILogger* logger);

  RollbackOrchestrator(const RollbackOrchestrator&) = delete;
  RollbackOrchestrator& operator=(const RollbackOrchestrator&) = delete;
  RollbackOrchestrator(RollbackOrchestrator&&) = delete;
  RollbackOrchestrator& operator=(RollbackOrchestrator&&) = delete;

  void SetTimings(const UpdateTimings& timings) { timings_ = timings; }

  void SetProgressCallback(UpdateProgressCallback callback) {
    progress_ = std::move(callback);
  }

  // Polled repeatedly; return true to stop.
  void SetCancelCallback(std::function<bool()> cancel) {
    cancel_ = std::move(cancel);
  }

  // Write the legacy gateware over the factory image at address 0.
  //
  // The factory region rather than the application one, because that is where
  // an EPCS boots from and the legacy gateware is a single image with no boot
  // loader in it: it does not read a boot block and nothing reads one on its
  // behalf. What is at the application address is left exactly as it is —
  // harmless, unread, and there for a later bring-up to come back to.
  UpdateOutcome ProgramGateware(const UpdateBundle& bundle);

  // Write the legacy firmware to the boot EEPROM.
  //
  // An ordinary target-0 transfer, and simpler than bring-up's counterpart in
  // every way: the device doing the writing is the modern firmware, which is
  // its own flasher, so there is no jumper, no RAM load and no boot ROM.
  //
  // The restart is deferred and must be: resetting the FX3 here would start
  // the legacy firmware while the FPGA is still running the modern gateware
  // it was configured with, which is the one pairing that must not happen.
  // Both halves change identity together, at the power cycle the caller owns,
  // or not at all.
  UpdateOutcome InstallFirmware(const UpdateBundle& bundle);

  // Whether the FPGA half has been done, which is the only thing that unlocks
  // the FX3 half.
  bool gateware_installed() const { return gateware_installed_; }

 private:
  UpdateOutcome Failure(std::string problem) const;

  // The shared setup for both halves, so that neither can quietly be given
  // different timings, a different bar or a different stop button.
  void Configure(UpdateOrchestrator& orchestrator) const;

  IDeviceUpdater& device_;
  ILogger* logger_ = nullptr;
  UpdateTimings timings_;
  UpdateProgressCallback progress_;
  std::function<bool()> cancel_;

  bool gateware_installed_ = false;
};

}  // namespace ddd::capture
