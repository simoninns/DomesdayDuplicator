/************************************************************************

    device_monitor.h

    Noticing that a device has been plugged in or pulled out
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "usb_device_info.h"

namespace ddd::capture {

class ILogger;
class IUsbDevice;

// Watches for devices appearing and disappearing, and says so when the set
// changes.
//
// Polling rather than a hot-plug callback, and that is a choice rather than an
// omission. libusb has libusb_hotplug_register_callback, but it is unsupported
// on Windows and its macOS behaviour has historically depended on the libusb
// build; WinUSB has no equivalent at all and would need a window handle and a
// device-notification message pump. Polling is the one mechanism that behaves
// identically on all three platforms, and at 200 ms it costs a device
// enumeration five times a second — measured in microseconds — to meet a
// 500 ms detection requirement with room to spare.
//
// It runs on its own thread because enumeration is not instant: reading a
// product string means opening the device and doing a control transfer, and on
// a machine with a misbehaving USB device that can take tens of milliseconds.
// On the GUI thread that would be a visible stutter five times a second.
//
// Thread-safety: Start, Stop and SetSuspended are for one controlling thread.
// The callback arrives on the monitor's own thread and must be safe there —
// the Qt layer marshals it across with a queued connection.
class DeviceMonitor {
 public:
  using Callback = std::function<void(const std::vector<DeviceInfo>&)>;

  // Fast enough that an attach is reported well inside the 500 ms the plan
  // asks for, even if a poll lands just after the device appears.
  static constexpr std::chrono::milliseconds kDefaultInterval{200};

  DeviceMonitor(IUsbDevice* device, ILogger* logger);
  ~DeviceMonitor();

  DeviceMonitor(const DeviceMonitor&) = delete;
  DeviceMonitor& operator=(const DeviceMonitor&) = delete;
  DeviceMonitor(DeviceMonitor&&) = delete;
  DeviceMonitor& operator=(DeviceMonitor&&) = delete;

  // Begin polling. The callback fires once with the initial set and then only
  // when the set changes — a panel that redrew five times a second whether or
  // not anything had happened would be a needless waste on a machine that is
  // otherwise trying to sustain 80 MB/s.
  void Start(Callback on_change,
             std::chrono::milliseconds interval = kDefaultInterval);

  void Stop();

  // Stop enumerating without stopping the monitor.
  //
  // Set while a capture is running. Enumeration opens devices to read their
  // descriptors, and doing that to a device that is streaming means control
  // transfers competing with the bulk endpoint for no reason at all — the
  // device plainly has not been unplugged, because data is arriving from it.
  // Resuming polls immediately rather than waiting out the interval, so a
  // device pulled during a capture is reported as soon as the capture ends.
  void SetSuspended(bool suspended);

  bool suspended() const { return suspended_.load(); }

  // The most recent set of devices seen.
  std::vector<DeviceInfo> Devices() const;

  // Polls completed since Start(). Only of interest to a test that needs to
  // know a poll has definitely happened.
  uint64_t PollCount() const { return poll_count_.load(); }

 private:
  void Loop(std::chrono::milliseconds interval);

  IUsbDevice* device_ = nullptr;
  ILogger* logger_ = nullptr;

  Callback on_change_;

  std::thread thread_;
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  std::atomic<bool> running_{false};
  std::atomic<bool> suspended_{false};

  // Cuts the current sleep short rather than waiting out the interval. Needed
  // because a condition variable woken by a notification whose predicate is
  // still false simply goes back to sleep for the remaining time.
  std::atomic<bool> wake_requested_{false};
  std::atomic<uint64_t> poll_count_{0};

  std::vector<DeviceInfo> devices_;
};

}  // namespace ddd::capture
