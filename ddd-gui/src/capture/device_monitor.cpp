/************************************************************************

    device_monitor.cpp

    Noticing that a device has been plugged in or pulled out
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "device_monitor.h"

#include <utility>

#include "logger.h"
#include "usb_device.h"

namespace ddd::capture {

DeviceMonitor::DeviceMonitor(IUsbDevice* device, ILogger* logger)
    : device_(device), logger_(logger) {}

DeviceMonitor::~DeviceMonitor() { Stop(); }

void DeviceMonitor::Start(Callback on_change,
                          std::chrono::milliseconds interval) {
  Stop();

  on_change_ = std::move(on_change);
  poll_count_ = 0;
  running_ = true;
  thread_ = std::thread(&DeviceMonitor::Loop, this, interval);
}

void DeviceMonitor::Stop() {
  if (!running_.exchange(false)) {
    return;
  }

  wake_.notify_all();
  if (thread_.joinable()) {
    thread_.join();
  }
}

void DeviceMonitor::SetSuspended(bool suspended) {
  suspended_ = suspended;

  // Waking on resume rather than on both edges. A suspend can wait for the
  // current poll to finish — it has already decided not to care about the
  // answer — but a resume should produce an up-to-date list immediately,
  // because the reason for resuming is usually that a capture has just ended
  // and the user is looking at the device list again.
  if (!suspended) {
    wake_requested_ = true;
    wake_.notify_all();
  }
}

std::vector<DeviceInfo> DeviceMonitor::Devices() const {
  const std::lock_guard<std::mutex> guard(mutex_);
  return devices_;
}

void DeviceMonitor::Loop(std::chrono::milliseconds interval) {
  bool announced = false;

  while (running_.load()) {
    if (!suspended_.load() && device_ != nullptr) {
      std::vector<DeviceInfo> found;
      const bool enumerated = device_->Enumerate(found);
      poll_count_.fetch_add(1);

      // A failed enumeration is not an empty one. Reporting "no devices"
      // because the USB subsystem was momentarily unavailable would make the
      // device vanish from the panel and come back a fifth of a second later,
      // which reads as a flapping cable rather than as what it is.
      if (enumerated) {
        bool changed = false;
        {
          const std::lock_guard<std::mutex> guard(mutex_);
          changed = !announced || found != devices_;
          if (changed) {
            devices_ = found;
          }
        }

        if (changed) {
          announced = true;
          if (logger_ != nullptr) {
            logger_->Info("Attached devices: " + std::to_string(found.size()));
          }
          if (on_change_) {
            on_change_(found);
          }
        }
      }
    }

    std::unique_lock<std::mutex> lock(mutex_);
    wake_.wait_for(lock, interval, [this] {
      return !running_.load() || wake_requested_.load();
    });
    wake_requested_ = false;
  }
}

}  // namespace ddd::capture
