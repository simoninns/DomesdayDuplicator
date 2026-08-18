/************************************************************************

    player_discovery.h

    Which ports to try, in what order, and how long to wait before trying again
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <chrono>
#include <cstdint>
#include <vector>

#include "player_settings.h"
#include "serial_port_scanner.h"

namespace ddd::gui {

// One probe: a port, and either a rate to use or zero for "try them all".
struct DiscoveryAttempt {
  QString port_path;
  uint32_t baud_rate = 0;

  bool operator==(const DiscoveryAttempt& other) const {
    return port_path == other.port_path && baud_rate == other.baud_rate;
  }
};

// The ports to try, in order, given what the user has said and what is there.
//
// A pure function, and the most important one in this feature to be able to
// test: it decides which serial ports get written to. Everything about the
// order is a decision about somebody else's equipment.
//
//   1. The remembered port, at its remembered rate. On a machine that has
//      connected before this is the whole list — one probe, well under a
//      second, and no other port on the machine is touched at all.
//
//   2. A port the user fixed. Then that and nothing else, whether or not it is
//      currently there: a fixed port that has gone away is worth saying so
//      about, and searching elsewhere would quietly leave the user with a
//      setting that does not describe their hardware.
//
//   3. Otherwise every remaining candidate, ranked by RankSerialPorts, at every
//      rate.
//
// Excluded and busy ports never appear — except a port the user fixed, which is
// an explicit instruction and outranks a list they also wrote.
std::vector<DiscoveryAttempt> PlanDiscovery(
    const PlayerSettings& settings,
    const std::vector<SerialPortCandidate>& available);

// How long to wait after this many consecutive failed searches.
//
// Doubling from two seconds to a thirty-second ceiling. A player that is
// switched off, or a machine with no adapter plugged in, must not produce a
// probe every second for the rest of the session — that is a write to every
// serial port on the machine, once a second, for hours. Thirty seconds is
// still soon enough that somebody who turns their player on notices the
// application find it without having pressed anything.
std::chrono::milliseconds SearchRetryDelay(int consecutive_failures);

inline constexpr std::chrono::milliseconds kFirstSearchRetry{2000};
inline constexpr std::chrono::milliseconds kMaximumSearchRetry{30000};

}  // namespace ddd::gui
