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
#include "wire_protocol.h"

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
// A control channel that stalls everything, which is what a device with no
// update agent in its firmware does.
class SilentControlChannel : public IUsbControlChannel {
 public:
  int Transfer(uint8_t, uint8_t, uint16_t, uint16_t, std::span<uint8_t>,
               unsigned int) override {
    return -1;
  }
};

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

  bool WriteRegister(const std::string& path, uint8_t address,
                     uint8_t value) override {
    const std::lock_guard<std::mutex> guard(mutex_);
    ++configuration_count_;
    configured_path_ = path;
    written_register_ = address;
    written_value_ = value;
    return !configuration_fails_;
  }

  bool ReadRegisters(const std::string& path, uint8_t address, uint8_t length,
                     std::vector<uint8_t>& data) override {
    const std::lock_guard<std::mutex> guard(mutex_);
    ++register_read_count_;
    read_path_ = path;

    // No register bank is the default, and it is the case a test gets without
    // asking: a device whose FPGA never answered stalls the request, and the
    // application has to carry on regardless. A test that wants a gateware
    // version says so with SetGatewareCommit().
    if (registers_.empty() ||
        static_cast<size_t>(address) + length > registers_.size()) {
      return false;
    }

    data.assign(registers_.begin() + address,
                registers_.begin() + address + length);
    return true;
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

  // A control channel is offered only for a path this fake is presenting, so
  // that "the device went away" is a state a test can simply arrange.
  //
  // The channel itself answers nothing: the update flow is tested against
  // FakeDeviceUpdater, which models the protocol, rather than against a
  // synthetic byte stream that would have to reimplement the firmware to be
  // any use. What this exists for is the code that only wants to know
  // whether a device could be opened at all.
  std::unique_ptr<IUsbControlChannel> OpenControlChannel(
      const std::string& path) override {
    const std::lock_guard<std::mutex> guard(mutex_);
    ++control_channel_count_;

    for (const DeviceInfo& info : devices_) {
      if (info.path == path) {
        return std::make_unique<SilentControlChannel>();
      }
    }
    return nullptr;
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

  // Give the fake a gateware identity block, as the register-read request
  // would return it. An empty commit is a gateware that cannot name the build
  // it came from, which is what one compiled outside a checkout reports.
  void SetGatewareCommit(const std::string& commit, bool dirty = false,
                         uint8_t map_version = kIdentityMapVersion) {
    const std::lock_guard<std::mutex> guard(mutex_);

    registers_.assign(kFakeRegisterCount, 0);
    registers_[kRegisterId] = kIdentityValue;
    registers_[kRegisterMapVersion] = map_version;

    uint8_t flags = 0;
    if (dirty) {
      flags |= kBuildFlagDirty;
    }
    if (!commit.empty()) {
      flags |= kBuildFlagCommit;
    }
    registers_[kRegisterBuildFlags] = flags;

    for (size_t index = 0; index < commit.size() && index < kCommitLength;
         ++index) {
      registers_[kRegisterCommit + index] = static_cast<uint8_t>(commit[index]);
    }
  }

  // Take the register bank away again — a device whose FPGA is unconfigured,
  // or whose firmware predates the register interface.
  void SetGatewareUnavailable() {
    const std::lock_guard<std::mutex> guard(mutex_);
    registers_.clear();
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
  uint8_t written_register() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return written_register_;
  }
  uint8_t written_value() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return written_value_;
  }
  bool configured_test_mode() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return written_register_ == kRegisterTestMode && written_value_ != 0;
  }
  uint64_t register_read_count() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return register_read_count_;
  }
  UsbSourceOptions opened_options() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return opened_options_;
  }
  uint64_t control_channel_count() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return control_channel_count_;
  }

 private:
  mutable std::mutex mutex_;

  std::vector<DeviceInfo> devices_;
  bool enumeration_fails_ = false;
  bool configuration_fails_ = false;
  bool open_fails_ = false;
  TransferResult open_failure_ = TransferResult::kConnectionFailure;

  SyntheticSource::Options source_options_;

  // The seven-bit register address space the gateware implements
  static constexpr size_t kFakeRegisterCount = 128;
  std::vector<uint8_t> registers_;

  uint64_t enumerate_count_ = 0;
  uint64_t open_count_ = 0;
  uint64_t configuration_count_ = 0;
  uint64_t register_read_count_ = 0;
  uint64_t control_channel_count_ = 0;
  std::string opened_path_;
  std::string configured_path_;
  std::string read_path_;
  uint8_t written_register_ = 0;
  uint8_t written_value_ = 0;
  UsbSourceOptions opened_options_;
};

}  // namespace ddd::capture
