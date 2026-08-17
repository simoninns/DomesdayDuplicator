/************************************************************************

    player_settings.cpp

    How the application is told to find the player, and where that is kept
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_settings.h"

#include <QSettings>
#include <algorithm>

#include "player_definition.h"
#include "player_registry.h"

namespace ddd::gui {
namespace {

constexpr const char* kEnabledKey = "player/enabled";
constexpr const char* kModelKey = "player/model_id_code";
constexpr const char* kPortKey = "player/port";
constexpr const char* kBaudKey = "player/baud_rate";
constexpr const char* kExcludedKey = "player/excluded_ports";
constexpr const char* kRememberedPortKey = "player/remembered_port";
constexpr const char* kRememberedBaudKey = "player/remembered_baud_rate";
constexpr const char* kStopPlayerKey = "player/stop_player_with_capture";
constexpr const char* kStopCaptureKey = "player/stop_capture_with_player";

// A stored rate that no player family uses reads as "work it out".
uint32_t ReadBaudRate(const QSettings& settings, const char* key) {
  const uint32_t stored = settings.value(QLatin1String(key), 0).toUInt();
  return IsSupportedPlayerBaudRate(stored) ? stored : 0;
}

}  // namespace

bool IsSupportedPlayerBaudRate(uint32_t baud_rate) {
  if (baud_rate == 0) {
    return false;
  }

  for (const player::ProbeSpec* probe : player::RegisteredProbes()) {
    for (const uint32_t rate : probe->baud_rates) {
      if (rate == baud_rate) {
        return true;
      }
    }
  }

  return false;
}

PlayerSettings LoadPlayerSettings() {
  const QSettings settings;
  PlayerSettings loaded;

  loaded.enabled =
      settings.value(QLatin1String(kEnabledKey), loaded.enabled).toBool();

  // A model this build does not know reads as "whatever answers". The
  // alternative — keeping the string and never matching it — would put the
  // application permanently in the model-mismatch state against a player that
  // is working perfectly well.
  const QString model = settings.value(QLatin1String(kModelKey)).toString();
  if (player::FindPlayerByIdCode(model.toStdString()) != nullptr) {
    loaded.model_id_code = model;
  }

  loaded.port_path = settings.value(QLatin1String(kPortKey)).toString();
  loaded.baud_rate = ReadBaudRate(settings, kBaudKey);

  loaded.excluded_ports =
      settings.value(QLatin1String(kExcludedKey)).toStringList();
  loaded.excluded_ports.removeAll(QString());

  loaded.remembered_port =
      settings.value(QLatin1String(kRememberedPortKey)).toString();
  loaded.remembered_baud = ReadBaudRate(settings, kRememberedBaudKey);

  loaded.stop_player_with_capture =
      settings
          .value(QLatin1String(kStopPlayerKey), loaded.stop_player_with_capture)
          .toBool();
  loaded.stop_capture_with_player = settings
                                        .value(QLatin1String(kStopCaptureKey),
                                               loaded.stop_capture_with_player)
                                        .toBool();

  // A remembered pair is only useful as a pair: a port with no rate would be
  // probed by searching every rate, which is what the scan does anyway, and a
  // rate with no port names nothing.
  if (loaded.remembered_port.isEmpty() || loaded.remembered_baud == 0) {
    loaded.remembered_port.clear();
    loaded.remembered_baud = 0;
  }

  return loaded;
}

void SavePlayerSettings(const PlayerSettings& settings) {
  QSettings store;
  store.setValue(QLatin1String(kEnabledKey), settings.enabled);
  store.setValue(QLatin1String(kModelKey), settings.model_id_code);
  store.setValue(QLatin1String(kPortKey), settings.port_path);
  store.setValue(QLatin1String(kBaudKey), settings.baud_rate);
  // Removed rather than written empty. Qt stores an empty QStringList as
  // "@Invalid()", which round-trips correctly and looks like a fault to anybody
  // reading the settings file — and this file is meant to be readable.
  if (settings.excluded_ports.isEmpty()) {
    store.remove(QLatin1String(kExcludedKey));
  } else {
    store.setValue(QLatin1String(kExcludedKey), settings.excluded_ports);
  }
  store.setValue(QLatin1String(kRememberedPortKey), settings.remembered_port);
  store.setValue(QLatin1String(kRememberedBaudKey), settings.remembered_baud);
  store.setValue(QLatin1String(kStopPlayerKey),
                 settings.stop_player_with_capture);
  store.setValue(QLatin1String(kStopCaptureKey),
                 settings.stop_capture_with_player);
}

}  // namespace ddd::gui
