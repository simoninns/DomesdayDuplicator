/************************************************************************

    fake_usb_device.h

    A USB backend with no USB in it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "synthetic_source.h"
#include "usb_device.h"
#include "usb_device_info.h"

namespace ddd::capture {

// An IUsbDevice whose device list the test decides and whose streaming source
// is the synthetic one.
//
// This is the piece that makes the device-handling code testable at all. With
// it, "a device is plugged in", "a device is plugged into a USB 2 port", "the
// device is running different firmware" and "the device is pulled out
// mid-capture" are all things a test can simply state, on a machine with
// nothing attached, in milliseconds. Without it they are things that can only
// be arranged by hand with a cable.
//
// Thread-safety: SetDevices() and Enumerate() may be called concurrently, since
// the device monitor enumerates from its own thread while a test changes what
// is attached from the main one.
class FakeUsbDevice : public IUsbDevice {
 public:
  FakeUsbDevice() = default;

  const char* Name() const override { return "fake"; }

  bool Enumerate(std::vector<DeviceInfo>& devices) override {
    const std::lock_guard<std::mutex> guard(mutex_);
    ++enumerate_count_;
    if (enumeration_fails_) {
      return false;
    }
    devices = devices_;
    return true;
  }

  bool SendConfiguration(const std::string& path, bool test_mode) override {
    const std::lock_guard<std::mutex> guard(mutex_);
    ++configuration_count_;
    configured_path_ = path;
    configured_test_mode_ = test_mode;
    return !configuration_fails_;
  }

  std::unique_ptr<ISampleSource> OpenSource(const std::string& path,
                                            const UsbSourceOptions& options,
                                            TransferResult& result) override {
    {
      const std::lock_guard<std::mutex> guard(mutex_);
      opened_path_ = path;
      opened_options_ = options;
      ++open_count_;

      if (open_fails_) {
        result = open_failure_;
        return nullptr;
      }
    }

    result = TransferResult::kSuccess;
    return std::make_unique<SyntheticSource>(source_options_);
  }

  // --- What the test decides ----------------------------------------------

  void SetDevices(std::vector<DeviceInfo> devices) {
    const std::lock_guard<std::mutex> guard(mutex_);
    devices_ = std::move(devices);
  }

  // One device at a path, at a speed, running firmware built from a commit.
  void SetSingleDevice(const std::string& path, DeviceSpeed speed,
                       const std::string& product_string) {
    DeviceInfo info;
    info.path = path;
    info.speed = speed;
    info.product_string = product_string;
    SetDevices({info});
  }

  void SetEnumerationFails(bool fails) {
    const std::lock_guard<std::mutex> guard(mutex_);
    enumeration_fails_ = fails;
  }

  void SetConfigurationFails(bool fails) {
    const std::lock_guard<std::mutex> guard(mutex_);
    configuration_fails_ = fails;
  }

  void SetOpenFails(
      bool fails, TransferResult failure = TransferResult::kConnectionFailure) {
    const std::lock_guard<std::mutex> guard(mutex_);
    open_fails_ = fails;
    open_failure_ = failure;
  }

  // How the synthetic source that OpenSource() hands back will behave.
  void SetSourceOptions(const SyntheticSource::Options& options) {
    source_options_ = options;
  }

  // --- What the test can check --------------------------------------------

  uint64_t enumerate_count() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return enumerate_count_;
  }
  uint64_t open_count() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return open_count_;
  }
  uint64_t configuration_count() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return configuration_count_;
  }
  std::string opened_path() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return opened_path_;
  }
  std::string configured_path() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return configured_path_;
  }
  bool configured_test_mode() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return configured_test_mode_;
  }
  UsbSourceOptions opened_options() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return opened_options_;
  }

 private:
  mutable std::mutex mutex_;

  std::vector<DeviceInfo> devices_;
  bool enumeration_fails_ = false;
  bool configuration_fails_ = false;
  bool open_fails_ = false;
  TransferResult open_failure_ = TransferResult::kConnectionFailure;

  SyntheticSource::Options source_options_;

  uint64_t enumerate_count_ = 0;
  uint64_t open_count_ = 0;
  uint64_t configuration_count_ = 0;
  std::string opened_path_;
  std::string configured_path_;
  bool configured_test_mode_ = false;
  UsbSourceOptions opened_options_;
};

}  // namespace ddd::capture
