/************************************************************************

    player_discovery.cpp

    Which ports to try, in what order, and how long to wait before trying again
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_discovery.h"

#include <algorithm>

namespace ddd::gui {

std::vector<DiscoveryAttempt> PlanDiscovery(
    const PlayerSettings& settings,
    const std::vector<SerialPortCandidate>& available) {
  std::vector<DiscoveryAttempt> attempts;

  const auto is_present = [&available](const QString& path) {
    return std::any_of(available.begin(), available.end(),
                       [&path](const SerialPortCandidate& candidate) {
                         return candidate.path == path;
                       });
  };

  // A fixed port is an instruction, not a preference. It is tried whether or
  // not the system currently lists it — a port that has gone away produces
  // "could not be opened", which is the true and useful answer — and nothing
  // else is tried at all.
  if (!settings.port_path.isEmpty()) {
    attempts.push_back(
        DiscoveryAttempt{settings.port_path, settings.baud_rate});
    return attempts;
  }

  // The remembered port only if it is still there. Probing a path that has
  // disappeared would spend the connect timeout finding out what the
  // enumeration already said.
  const bool remember =
      !settings.remembered_port.isEmpty() && settings.remembered_baud != 0 &&
      !settings.excluded_ports.contains(settings.remembered_port) &&
      is_present(settings.remembered_port);

  if (remember) {
    attempts.push_back(
        DiscoveryAttempt{settings.remembered_port, settings.remembered_baud});
  }

  for (const SerialPortCandidate& candidate :
       RankSerialPorts(available, settings.excluded_ports)) {
    // Not twice. The remembered port has already had its cheap attempt; giving
    // it a second, slower one at the end of the queue would only delay finding
    // out that the player has moved.
    if (remember && candidate.path == settings.remembered_port) {
      continue;
    }
    attempts.push_back(DiscoveryAttempt{candidate.path, 0});
  }

  return attempts;
}

std::chrono::milliseconds SearchRetryDelay(int consecutive_failures) {
  if (consecutive_failures <= 1) {
    return kFirstSearchRetry;
  }

  // Doubling, with the shift bounded before it is applied rather than after —
  // a caller that has been failing all afternoon would otherwise shift a
  // 64-bit count past its width, which is undefined behaviour rather than a
  // large number.
  constexpr int kMaximumDoublings = 16;
  const int doublings = std::min(consecutive_failures - 1, kMaximumDoublings);

  const std::chrono::milliseconds delay{kFirstSearchRetry.count() << doublings};
  return std::min(delay, kMaximumSearchRetry);
}

}  // namespace ddd::gui
