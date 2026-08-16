/************************************************************************

    serial_port_scanner.cpp

    Which serial ports might have a player on the end of them
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "serial_port_scanner.h"

#include <QSerialPortInfo>

namespace ddd::gui {

std::vector<SerialPortCandidate> RankSerialPorts(
    const std::vector<SerialPortCandidate>& ports,
    const QStringList& excluded_paths) {
  std::vector<SerialPortCandidate> adapters;
  std::vector<SerialPortCandidate> others;

  for (const SerialPortCandidate& port : ports) {
    if (port.path.isEmpty() || port.busy ||
        excluded_paths.contains(port.path)) {
      continue;
    }

    if (port.usb_adapter) {
      adapters.push_back(port);
    } else {
      others.push_back(port);
    }
  }

  // Two passes rather than a sort, so the system's order survives inside each
  // group. std::stable_sort would do the same thing and would invite somebody
  // to add a second criterion to the comparator, which is where an ordering
  // stops being explicable.
  adapters.insert(adapters.end(), others.begin(), others.end());
  return adapters;
}

std::vector<SerialPortCandidate> EnumerateSerialPorts() {
  std::vector<SerialPortCandidate> ports;

  const QList<QSerialPortInfo> available = QSerialPortInfo::availablePorts();
  ports.reserve(static_cast<size_t>(available.size()));

  for (const QSerialPortInfo& info : available) {
    SerialPortCandidate candidate;
    candidate.path = info.systemLocation();
    candidate.description = info.description();
    candidate.manufacturer = info.manufacturer();
    // Qt 6 removed QSerialPortInfo::isBusy(), so enumeration cannot say. A port
    // somebody else is using therefore reaches the probe and fails to open,
    // which is reported as "that port could not be opened" — the same words a
    // permission problem gets, and true of both. The field stays because the
    // rule it drives is right and a caller that does know may supply it; see
    // its comment in the header.
    candidate.busy = info.isNull();

    // Both identifiers, not either. A driver that reports a vendor and no
    // product is reporting something it inferred, and the point of the flag is
    // to say "the system knows this is a USB device" rather than "something
    // filled in a field".
    candidate.usb_adapter =
        info.hasVendorIdentifier() && info.hasProductIdentifier();

    ports.push_back(candidate);
  }

  return ports;
}

}  // namespace ddd::gui
