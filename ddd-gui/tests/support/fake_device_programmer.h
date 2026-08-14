/************************************************************************

    fake_device_programmer.h

    A device with no firmware, with no device in it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "device_programmer.h"

namespace ddd::capture {

// An IDeviceProgrammer that records what was downloaded into it and can be
// told to fail at each of the three points it can fail at.
//
// The recovery path's failure modes are all things that happen to a device
// nobody is holding: a download that stops half way, a device that does not
// run what it was given, a device that runs it and never re-enumerates. None
// of them can be arranged on a bench without unplugging a cable at exactly
// the right moment, and the last two cannot be arranged at all. Here they are
// one line each.
//
// It keeps every section it was given, indexed by load address, so a test can
// check that the bytes the boot ROM would have received are the bytes the
// image actually contains — which is the property the parser exists to
// preserve.
class FakeDeviceProgrammer : public IDeviceProgrammer {
 public:
  enum class Fault {
    kNone,

    // The download stops part way, at the section index in fail_at_section.
    kRefuseDownload,

    // The device takes the whole image and then will not start it.
    kRefuseStart,

    // It starts, and never comes back under an identity anybody recognises.
    kNeverReturns,
  };

  bool WriteRam(uint32_t address, std::span<const uint8_t> data) override {
    if (fault_ == Fault::kRefuseDownload &&
        sections_written_ == fail_at_section_) {
      return false;
    }

    ++sections_written_;
    bytes_written_ += data.size();
    written_[address].assign(data.begin(), data.end());
    return true;
  }

  bool Start(uint32_t entry_address) override {
    started_ = true;
    entry_address_ = entry_address;
    return fault_ != Fault::kRefuseStart;
  }

  std::optional<std::string> WaitForApplication(
      std::chrono::milliseconds timeout) override {
    (void)timeout;
    ++wait_count_;
    if (fault_ == Fault::kNeverReturns) {
      return std::nullopt;
    }
    return returned_path_;
  }

  // --- What the test tells it ----------------------------------------------

  void SetFault(Fault fault) { fault_ = fault; }
  void SetFailAtSection(size_t index) { fail_at_section_ = index; }
  void SetReturnedPath(std::string path) { returned_path_ = std::move(path); }

  // --- What the test reads back --------------------------------------------

  size_t sections_written() const { return sections_written_; }
  size_t bytes_written() const { return bytes_written_; }
  bool started() const { return started_; }
  uint32_t entry_address() const { return entry_address_; }
  size_t wait_count() const { return wait_count_; }

  const std::vector<uint8_t>& section_at(uint32_t address) {
    return written_[address];
  }

 private:
  Fault fault_ = Fault::kNone;
  size_t fail_at_section_ = 0;
  std::string returned_path_ = "/sys/bus/usb/devices/1-1";

  size_t sections_written_ = 0;
  size_t bytes_written_ = 0;
  bool started_ = false;
  uint32_t entry_address_ = 0;
  size_t wait_count_ = 0;
  std::map<uint32_t, std::vector<uint8_t>> written_;
};

}  // namespace ddd::capture
