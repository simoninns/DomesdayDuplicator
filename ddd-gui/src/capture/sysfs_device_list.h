/************************************************************************

    sysfs_device_list.h

    What the kernel says is attached, read straight from sysfs
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace ddd::capture {

// A second opinion on which devices are attached, used to catch libusb's
// first one being out of date.
//
// libusb_get_device_list does not rescan on Linux. It returns the list the
// backend maintains, which is seeded by one sysfs scan at libusb_init and
// updated thereafter only by hot-plug events arriving over a udev netlink
// monitor. Where those events cannot arrive, the list is frozen at whatever
// was attached when the application started — and nothing reports an error,
// because from libusb's point of view nothing has happened.
//
// A Flatpak is exactly that environment unless it is given the host's network
// namespace: the netlink socket is created and bound without complaint, and
// the kernel does not broadcast untagged uevents into a network namespace
// outside the initial user namespace. Plain containers can be the same. The
// manifest asks for what libusb needs; this is the belt to that pair of
// braces, and the thing that makes the failure visible rather than silent.
//
// sysfs is the right second opinion because it is not a cache. Every attached
// USB device has a directory under /sys/bus/usb/devices whose idVendor and
// idProduct files are read from the device's descriptor, so listing them says
// what is on the bus now, needs no device to be opened, and costs a handful of
// small reads.

// One device's identifiers, as the kernel reports them.
struct UsbIdentity {
  uint16_t vendor = 0;
  uint16_t product = 0;

  bool operator==(const UsbIdentity& other) const {
    return vendor == other.vendor && product == other.product;
  }

  // Sorted so that two lists of the same identifiers compare equal whatever
  // order the bus enumerated them in.
  bool operator<(const UsbIdentity& other) const {
    return vendor != other.vendor ? vendor < other.vendor
                                  : product < other.product;
  }
};

// The identifiers this application recognises, as the kernel currently lists
// them, or nothing where the question cannot be asked.
//
// Nothing rather than an empty list, and the difference carries the platform
// split: macOS has no sysfs, so the answer there is "no second opinion
// available" and never "no devices attached". An empty list is a real answer
// meaning the bus has none of ours on it, which is exactly what should be
// noticed when libusb still thinks there is one.
//
// Only the identifiers PersonalityFromDescriptor would recognise are
// returned. Every other device on the bus is somebody else's business, and
// counting them would have a mouse being plugged in look like news.
std::optional<std::vector<UsbIdentity>> ReadSysfsDeviceIdentities(
    const std::filesystem::path& root = "/sys/bus/usb/devices");

// Would this application recognise a device with these identifiers?
//
// The same four pairs PersonalityFromDescriptor matches on, stated here so the
// scan above can filter without a libusb descriptor to hand. The two must
// agree: a pair recognised by one and not the other would read as a permanent
// disagreement between the kernel and libusb, and produce a rescan on every
// poll for ever.
bool IsRecognisedUsbIdentity(const UsbIdentity& identity);

// Do the kernel and libusb disagree about what is attached?
//
// True when one lists something the other does not, in either direction: a
// missed attach and a missed detach are both a stale cache, and both are
// mended the same way. The comparison counts duplicates, so a second
// Duplicator appearing is a disagreement even though its identifiers were
// already in both lists.
//
// Takes libusb's side as the *unfiltered* set of recognised identifiers it
// enumerated, before any probe drops one. A device wearing the flash
// programmer's identifiers that fails the 0xB0 probe is deliberately left out
// of the device list, and comparing against that filtered list would report a
// disagreement that no rescan can ever resolve.
bool DeviceViewsDisagree(std::vector<UsbIdentity> kernel,
                         std::vector<UsbIdentity> libusb);

}  // namespace ddd::capture
