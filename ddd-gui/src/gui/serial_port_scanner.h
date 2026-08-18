/************************************************************************

    serial_port_scanner.h

    Which serial ports might have a player on the end of them
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <QStringList>
#include <vector>

namespace ddd::gui {

// One serial port, as the system describes it.
struct SerialPortCandidate {
  // What to open: /dev/ttyUSB0, /dev/cu.usbserial-1420, COM3.
  QString path;

  // What the system calls it, for a user choosing between several. Often empty
  // on ports that are not USB adapters.
  QString description;

  QString manufacturer;

  // The port is already open in this process or another. Skipped by the scan
  // rather than contended for: a port somebody else is using is not a port a
  // player is waiting on, and probing it would interrupt whatever is.
  //
  // EnumerateSerialPorts() cannot fill this in on Qt 6, which removed
  // QSerialPortInfo::isBusy() — a busy port is then found out about by failing
  // to open, which is reported honestly as "that port could not be opened". The
  // field and the rule stay because the rule is right and because a test, or a
  // platform-specific lister added later, can supply it.
  bool busy = false;

  // The system supplied USB vendor and product identifiers for it, which in
  // practice means it is a USB serial adapter rather than a built-in port.
  bool usb_adapter = false;
};

// The ports worth probing, best candidate first.
//
// A pure function over a supplied list, so the whole ordering can be tested
// without opening anything — which is the point, because the ordering is what
// decides which port gets written to first, and writing to the wrong port is
// the one thing this feature can do that reaches outside the application.
//
// The rules, in order:
//
//   Excluded ports are dropped. A user who has said "not this one" has
//   usually said it because something else is on the end of it.
//
//   Busy ports are dropped, for the same reason stated on the field above.
//
//   USB adapters come first. A LaserDisc player is connected through one on
//   every machine this will run on; a built-in port is far more likely to be
//   something the user cares about not being interrupted.
//
// Within those groups the system's own order is kept. It is stable, which
// matters more than any cleverness about which port is "newest" — the system
// does not report when a port appeared, so a claim to sort by that would be a
// guess dressed up as an ordering.
std::vector<SerialPortCandidate> RankSerialPorts(
    const std::vector<SerialPortCandidate>& ports,
    const QStringList& excluded_paths);

// Every serial port the system currently reports.
//
// Enumerated on demand rather than watched. There is no port-arrival
// notification that behaves the same way on Linux, macOS and Windows — the same
// reason device_monitor.h gives for polling USB — and a serial adapter being
// plugged in is not a time-critical event: it is followed by a person plugging
// in a second cable and turning a player on.
std::vector<SerialPortCandidate> EnumerateSerialPorts();

}  // namespace ddd::gui
