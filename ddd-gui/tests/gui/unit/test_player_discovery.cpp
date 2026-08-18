/************************************************************************

    test_player_discovery.cpp

    T1 tests for which serial ports get written to, and when
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QString>
#include <QStringList>
#include <vector>

#include "player_discovery.h"
#include "player_settings.h"
#include "serial_port_scanner.h"

namespace ddd::gui {
namespace {

SerialPortCandidate Port(const char* path, bool usb_adapter = true,
                         bool busy = false) {
  SerialPortCandidate candidate;
  candidate.path = QLatin1String(path);
  candidate.usb_adapter = usb_adapter;
  candidate.busy = busy;
  return candidate;
}

std::vector<QString> Paths(const std::vector<DiscoveryAttempt>& attempts) {
  std::vector<QString> paths;
  paths.reserve(attempts.size());
  for (const DiscoveryAttempt& attempt : attempts) {
    paths.push_back(attempt.port_path);
  }
  return paths;
}

TEST(SerialPortScannerTest, AdaptersComeBeforeBuiltInPorts) {
  // A LaserDisc player is on a USB adapter on every machine this will run on,
  // and a built-in port is far more likely to be something the user would
  // rather was not interrupted.
  const std::vector<SerialPortCandidate> ports = {
      Port("/dev/ttyS0", false),
      Port("/dev/ttyUSB0"),
      Port("/dev/ttyS1", false),
      Port("/dev/ttyUSB1"),
  };

  const std::vector<SerialPortCandidate> ranked = RankSerialPorts(ports, {});

  ASSERT_EQ(ranked.size(), 4U);
  EXPECT_EQ(ranked[0].path, QLatin1String("/dev/ttyUSB0"));
  EXPECT_EQ(ranked[1].path, QLatin1String("/dev/ttyUSB1"));

  // And within each group the system's own order survives, which is what makes
  // the ordering explicable rather than merely deterministic.
  EXPECT_EQ(ranked[2].path, QLatin1String("/dev/ttyS0"));
  EXPECT_EQ(ranked[3].path, QLatin1String("/dev/ttyS1"));
}

TEST(SerialPortScannerTest, ExcludedAndBusyPortsAreNeverOffered) {
  const std::vector<SerialPortCandidate> ports = {
      Port("/dev/ttyUSB0"),
      Port("/dev/ttyUSB1"),
      Port("/dev/ttyUSB2", true, /*busy=*/true),
  };

  const std::vector<SerialPortCandidate> ranked =
      RankSerialPorts(ports, QStringList{QLatin1String("/dev/ttyUSB1")});

  ASSERT_EQ(ranked.size(), 1U);
  EXPECT_EQ(ranked[0].path, QLatin1String("/dev/ttyUSB0"));
}

TEST(PlayerDiscoveryTest, TheRememberedPortIsTriedFirstAndOnlyOnce) {
  // The whole point of remembering: a machine that has connected before spends
  // one probe, and every other port on it is left alone unless that one has
  // stopped answering.
  PlayerSettings settings;
  settings.remembered_port = QLatin1String("/dev/ttyUSB1");
  settings.remembered_baud = 4800;

  const std::vector<DiscoveryAttempt> attempts =
      PlanDiscovery(settings, {Port("/dev/ttyUSB0"), Port("/dev/ttyUSB1")});

  ASSERT_EQ(attempts.size(), 2U);
  EXPECT_EQ(attempts[0].port_path, QLatin1String("/dev/ttyUSB1"));
  EXPECT_EQ(attempts[0].baud_rate, 4800U);

  // Not visited a second time at the back of the queue, which would only delay
  // finding out that the player has moved.
  EXPECT_EQ(attempts[1].port_path, QLatin1String("/dev/ttyUSB0"));
  EXPECT_EQ(attempts[1].baud_rate, 0U);
}

TEST(PlayerDiscoveryTest, ARememberedPortThatHasGoneIsNotProbed) {
  // Spending the connect timeout finding out what the enumeration already said.
  PlayerSettings settings;
  settings.remembered_port = QLatin1String("/dev/ttyUSB9");
  settings.remembered_baud = 9600;

  const std::vector<DiscoveryAttempt> attempts =
      PlanDiscovery(settings, {Port("/dev/ttyUSB0")});

  ASSERT_EQ(attempts.size(), 1U);
  EXPECT_EQ(attempts[0].port_path, QLatin1String("/dev/ttyUSB0"));
}

TEST(PlayerDiscoveryTest, AFixedPortIsTheWholeSearch) {
  // Quietly succeeding on a different port would leave the user with a setting
  // that does not describe their hardware and a fault that reappears on the
  // next machine.
  PlayerSettings settings;
  settings.port_path = QLatin1String("/dev/ttyUSB1");
  settings.baud_rate = 1200;

  const std::vector<DiscoveryAttempt> attempts =
      PlanDiscovery(settings, {Port("/dev/ttyUSB0"), Port("/dev/ttyUSB1")});

  ASSERT_EQ(attempts.size(), 1U);
  EXPECT_EQ(attempts[0].port_path, QLatin1String("/dev/ttyUSB1"));
  EXPECT_EQ(attempts[0].baud_rate, 1200U);
}

TEST(PlayerDiscoveryTest, AFixedPortIsTriedEvenWhenNothingListsIt) {
  // "That port could not be opened" is the true and useful answer for an
  // adapter that has been unplugged. Falling back to a scan would answer a
  // question the user did not ask.
  PlayerSettings settings;
  settings.port_path = QLatin1String("/dev/ttyUSB9");

  const std::vector<DiscoveryAttempt> attempts =
      PlanDiscovery(settings, {Port("/dev/ttyUSB0")});

  ASSERT_EQ(attempts.size(), 1U);
  EXPECT_EQ(attempts[0].port_path, QLatin1String("/dev/ttyUSB9"));
}

TEST(PlayerDiscoveryTest, AnExcludedPortIsNeverPlanned) {
  // The only protection available for equipment this application knows nothing
  // about, so it holds even for a port that would otherwise be tried first.
  PlayerSettings settings;
  settings.excluded_ports = QStringList{QLatin1String("/dev/ttyUSB0")};
  settings.remembered_port = QLatin1String("/dev/ttyUSB0");
  settings.remembered_baud = 9600;

  const std::vector<DiscoveryAttempt> attempts =
      PlanDiscovery(settings, {Port("/dev/ttyUSB0"), Port("/dev/ttyUSB1")});

  const std::vector<QString> paths = Paths(attempts);
  EXPECT_EQ(paths, std::vector<QString>{QLatin1String("/dev/ttyUSB1")});
}

TEST(PlayerDiscoveryTest, NoPortsMeansNothingToTry) {
  EXPECT_TRUE(PlanDiscovery(PlayerSettings{}, {}).empty());
}

TEST(PlayerDiscoveryTest, TheRetryDelayGrowsAndIsBounded) {
  // A player that is switched off must not produce a write to every serial
  // port on the machine once a second for the rest of the afternoon.
  EXPECT_EQ(SearchRetryDelay(0), kFirstSearchRetry);
  EXPECT_EQ(SearchRetryDelay(1), kFirstSearchRetry);
  EXPECT_EQ(SearchRetryDelay(2), 2 * kFirstSearchRetry);
  EXPECT_EQ(SearchRetryDelay(3), 4 * kFirstSearchRetry);

  EXPECT_EQ(SearchRetryDelay(10), kMaximumSearchRetry);

  // Still bounded after an afternoon of failures, rather than shifting a count
  // past its own width.
  EXPECT_EQ(SearchRetryDelay(100000), kMaximumSearchRetry);
}

}  // namespace
}  // namespace ddd::gui
