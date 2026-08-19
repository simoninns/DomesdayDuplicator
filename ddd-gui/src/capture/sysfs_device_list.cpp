/************************************************************************

    sysfs_device_list.cpp

    What the kernel says is attached, read straight from sysfs
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "sysfs_device_list.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <system_error>

#include "wire_protocol.h"

namespace ddd::capture {
namespace {

// A sysfs identifier file: four lower-case hex digits and a newline.
//
// Parsed strictly rather than with strtol, because the point of this scan is
// to be a *reliable* second opinion. Anything that is not exactly four hex
// digits is a file this code does not understand, and a device it does not
// understand must be left out rather than guessed at — a wrong identifier
// here would look like a device appearing and disappearing, and produce a
// rescan every poll for ever.
std::optional<uint16_t> ReadIdentifierFile(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) {
    return std::nullopt;
  }

  std::string text;
  if (!(file >> text) || text.size() != 4) {
    return std::nullopt;
  }

  uint16_t value = 0;
  for (const char character : text) {
    int digit = 0;
    if (character >= '0' && character <= '9') {
      digit = character - '0';
    } else if (character >= 'a' && character <= 'f') {
      digit = character - 'a' + 10;
    } else if (character >= 'A' && character <= 'F') {
      digit = character - 'A' + 10;
    } else {
      return std::nullopt;
    }
    value = static_cast<uint16_t>((value << 4) | digit);
  }
  return value;
}

}  // namespace

bool IsRecognisedUsbIdentity(const UsbIdentity& identity) {
  if (identity.vendor == kVendorId && identity.product == kProductId) {
    return true;
  }
  if (identity.vendor == kLegacyVendorId &&
      identity.product == kLegacyProductId) {
    return true;
  }
  if (identity.vendor == kCypressVendorId) {
    return identity.product == kRecoveryProductId ||
           identity.product == kFlashProgrammerProductId;
  }
  return false;
}

std::optional<std::vector<UsbIdentity>> ReadSysfsDeviceIdentities(
    const std::filesystem::path& root) {
  std::error_code error;
  if (!std::filesystem::is_directory(root, error)) {
    return std::nullopt;
  }

  std::vector<UsbIdentity> found;

  // Errors are collected rather than thrown, and a directory that cannot be
  // walked at all is "no answer" rather than "nothing attached". The
  // distinction matters: a disagreement is acted on, so reporting an empty bus
  // because a read failed would recycle the libusb context on every poll.
  std::filesystem::directory_iterator entries(
      root, std::filesystem::directory_options::skip_permission_denied, error);
  if (error) {
    return std::nullopt;
  }

  const std::filesystem::directory_iterator end;
  for (; entries != end; entries.increment(error)) {
    if (error) {
      return std::nullopt;
    }

    // Interfaces sit beside devices in this directory and carry no
    // identifier files of their own, so they simply fail the reads below.
    const std::optional<uint16_t> vendor =
        ReadIdentifierFile(entries->path() / "idVendor");
    if (!vendor.has_value()) {
      continue;
    }
    const std::optional<uint16_t> product =
        ReadIdentifierFile(entries->path() / "idProduct");
    if (!product.has_value()) {
      continue;
    }

    const UsbIdentity identity{*vendor, *product};
    if (IsRecognisedUsbIdentity(identity)) {
      found.push_back(identity);
    }
  }

  return found;
}

bool DeviceViewsDisagree(std::vector<UsbIdentity> kernel,
                         std::vector<UsbIdentity> libusb) {
  std::sort(kernel.begin(), kernel.end());
  std::sort(libusb.begin(), libusb.end());
  return kernel != libusb;
}

}  // namespace ddd::capture
