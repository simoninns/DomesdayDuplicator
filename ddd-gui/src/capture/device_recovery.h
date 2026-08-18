/************************************************************************

    device_recovery.h

    Installing onto a device that has no working firmware to install with
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "device_programmer.h"
#include "device_updater.h"
#include "update_orchestrator.h"

namespace ddd::capture {

class ILogger;

// The update path for a device that cannot run the update path.
//
// A Domesday Duplicator whose EEPROM the FX3's boot ROM will not accept
// enumerates as the boot ROM instead: 04b4:00f3, answering one command. There
// are exactly three ways to arrive there, and they are indistinguishable on
// the wire —
//
//   * the kit has never been programmed, which is how every SuperSpeed
//     Explorer Kit arrives from the shop;
//   * an update was interrupted before its last write, which is the state the
//     firmware's held-back first page is designed to produce;
//   * the PMODE jumper is fitted, which is a developer doing it on purpose.
//
// All three are repaired the same way, and the repair is this: hand the boot
// ROM the update bundle's own firmware, let it run that from RAM, and then
// use the ordinary update mechanism — running on the firmware that has just
// been loaded — to write the EEPROM. Nothing about the second half is
// special-cased. The device streams, hashes, writes, reads back and confirms
// exactly as it does for a routine update, so the whole integrity chain
// applies to a first-time programming of a bare board.
//
// What the interface calls this depends on which of the three it is talking
// to, and the interface cannot know: a kit somebody has just soldered is
// offered "program this device" and a kit whose update was interrupted is
// offered "repair", because the words a user needs are different even though
// the mechanism is not. See update_text.h.

// How everything the recovery path needs to reach a device is supplied.
//
// Two factories rather than two objects, because the second cannot exist
// until the first has done its work: the device that is updated is a device
// that does not exist yet when this starts.
struct DeviceAccess {
  // Opens a programmer for the device in its recovery personality.
  std::function<std::unique_ptr<IDeviceProgrammer>()> open_programmer;

  // Opens an updater for the device that appeared at `path` once the firmware
  // was running.
  std::function<std::unique_ptr<IDeviceUpdater>(const std::string& path)>
      open_updater;
};

// Timings the recovery prelude works to. Collected in a struct so a test can
// drive the whole flow in milliseconds rather than in seconds.
struct DeviceRecoveryTimings {
  // How long to wait for the device to re-enumerate running the firmware it
  // has just been given. Generous against a slow hub and a slow host, and
  // still far short of the point where a user would have given up.
  std::chrono::milliseconds return_timeout{30000};
};

// Install a bundle onto a device that is sitting in its boot ROM.
//
// Shaped like UpdateOrchestrator on purpose, because it is the same operation
// with a prelude: the same progress callback, the same cancellation
// behaviour, and the same UpdateOutcome at the end. The interface and the
// command-line tool both drive this, and neither has a second description of
// what an update looks like.
//
// Qt-free and synchronous: it blocks for the whole of an install, which is
// minutes.
class RecoveryInstaller {
 public:
  RecoveryInstaller(DeviceAccess access, ILogger* logger);

  RecoveryInstaller(const RecoveryInstaller&) = delete;
  RecoveryInstaller& operator=(const RecoveryInstaller&) = delete;
  RecoveryInstaller(RecoveryInstaller&&) = delete;
  RecoveryInstaller& operator=(RecoveryInstaller&&) = delete;

  void SetTimings(const DeviceRecoveryTimings& timings) { timings_ = timings; }

  // Passed through to the update that follows the prelude.
  void SetUpdateTimings(const UpdateTimings& timings) {
    update_timings_ = timings;
  }

  void SetProgressCallback(UpdateProgressCallback callback) {
    progress_ = std::move(callback);
  }

  void SetCancelCallback(std::function<bool()> cancel) {
    cancel_ = std::move(cancel);
  }

  // Passed through to the update that follows the prelude. See
  // UpdateOrchestrator::SetDeferRestart — the bring-up wizard sets it because
  // the PMODE jumper is still fitted, so a reset would land back in the boot
  // ROM rather than in the firmware that has just been written.
  void SetDeferRestart(bool defer) { defer_restart_ = defer; }

  // Wake the device, then install.
  //
  // The bundle must carry firmware; a gateware-only bundle cannot recover a
  // device, because there is nothing running on it to write gateware with.
  // That is refused with a sentence rather than attempted.
  UpdateOutcome Run(const UpdateBundle& bundle);

  // Where the device appeared once it was running the firmware it was given,
  // and empty until it has.
  //
  // Exposed because bring-up has a second thing to do to the same device
  // afterwards — write its FPGA's flash — and the device does not go away in
  // between: the jumper comes out with the power still on, so this path is
  // still the path. Finding the device a second time would be finding it by
  // guesswork on a bench that may have two of them.
  const std::string& device_path() const { return device_path_; }

 private:
  // The prelude on its own: parse, download, jump, wait. Returns the path the
  // device appeared at, or nothing with `outcome` filled in.
  std::optional<std::string> Wake(const UpdateBundle& bundle,
                                  UpdateOutcome& outcome);

  void Report(uint64_t done, uint64_t total, std::string message);

  bool Cancelled() const { return cancel_ && cancel_(); }

  DeviceAccess access_;
  ILogger* logger_ = nullptr;
  std::string device_path_;
  DeviceRecoveryTimings timings_;
  UpdateTimings update_timings_;
  UpdateProgressCallback progress_;
  std::function<bool()> cancel_;
  bool defer_restart_ = false;
};

}  // namespace ddd::capture
