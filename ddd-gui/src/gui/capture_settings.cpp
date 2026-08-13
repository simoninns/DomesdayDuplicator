/************************************************************************

    capture_settings.cpp

    The settings a capture runs with, and where they are kept
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_settings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>

namespace ddd::gui {
namespace {

constexpr const char* kPreferredDeviceKey = "capture/preferred_device";
constexpr const char* kQueueSizeKey = "capture/queue_size_bytes";
constexpr const char* kSmallTransfersKey = "capture/small_transfers";
constexpr const char* kTransferQueueKey = "capture/transfer_queue_bytes";
constexpr const char* kFrontEndGainKey = "hardware/front_end_gain_switches";
constexpr const char* kCaptureDirectoryKey = "capture/directory";
constexpr const char* kCaptureNameKey = "capture/name";
constexpr const char* kCompressionLevelKey = "capture/compression_level";
constexpr const char* kDurationLimitKey = "capture/duration_limit_seconds";
constexpr const char* kLowSpaceKey = "capture/low_space_warning_minutes";

// The transfer queue is bounded on both sides. Below one slot there is never
// more than a single transfer outstanding, which defeats the point of it; above
// the usbfs default there is nothing to gain and a submission failure to lose.
constexpr size_t kMinimumTransferQueueBytes = size_t{2} << 20;
constexpr size_t kMaximumTransferQueueBytes = size_t{12} << 20;

}  // namespace

capture::UsbSourceOptions CaptureSettings::UsbOptions() const {
  capture::UsbSourceOptions options;
  options.small_transfers = small_transfers;
  options.transfer_queue_bytes = transfer_queue_bytes;
  return options;
}

analysis::FrontEndGain CaptureSettings::DeclaredGain() const {
  return analysis::FrontEndGain::FromSwitchPattern(front_end_gain_switches);
}

QString DefaultCaptureDirectory() {
  const QString movies =
      QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
  if (!movies.isEmpty()) {
    return movies;
  }

  // Movies is not defined on every platform Qt supports. Home is, and a capture
  // landing in the home folder is at least somewhere the user can find it —
  // unlike the working directory, which for an application started from a
  // desktop shortcut may be anywhere at all.
  const QString home =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  return home.isEmpty() ? QDir::currentPath() : home;
}

QString CaptureSettings::ResolvedCaptureDirectory() const {
  return capture_directory.isEmpty() ? DefaultCaptureDirectory()
                                     : capture_directory;
}

CaptureSettings LoadCaptureSettings() {
  const QSettings settings;
  CaptureSettings loaded;

  loaded.preferred_device_path =
      settings.value(QLatin1String(kPreferredDeviceKey)).toString();

  loaded.queue_size_bytes = std::clamp(
      static_cast<size_t>(
          settings
              .value(QLatin1String(kQueueSizeKey),
                     static_cast<qulonglong>(loaded.queue_size_bytes))
              .toULongLong()),
      capture::DiskBufferRing::kMinimumQueueSizeBytes,
      capture::DiskBufferRing::kMaximumQueueSizeBytes);

  loaded.small_transfers =
      settings.value(QLatin1String(kSmallTransfersKey), loaded.small_transfers)
          .toBool();

  loaded.transfer_queue_bytes = std::clamp(
      static_cast<size_t>(
          settings
              .value(QLatin1String(kTransferQueueKey),
                     static_cast<qulonglong>(loaded.transfer_queue_bytes))
              .toULongLong()),
      kMinimumTransferQueueBytes, kMaximumTransferQueueBytes);

  // Not clamped like the others, because there is no nearest sensible value to
  // clamp a switch pattern to: FromSwitchPattern reads anything outside 1..15
  // as undeclared, which is the only safe reading of a setting that would
  // otherwise put a wrong voltage on the screen.
  loaded.front_end_gain_switches = static_cast<uint8_t>(
      settings.value(QLatin1String(kFrontEndGainKey), 0).toUInt() & 0xFFU);

  // Stored as typed and resolved when it is used, so that a settings file
  // written on one machine and read on another does not pin captures to a home
  // directory that is not there.
  loaded.capture_directory =
      settings.value(QLatin1String(kCaptureDirectoryKey)).toString();
  loaded.capture_name =
      settings.value(QLatin1String(kCaptureNameKey)).toString();

  loaded.compression_level = std::clamp(
      settings
          .value(QLatin1String(kCompressionLevelKey), loaded.compression_level)
          .toInt(),
      0, 8);

  loaded.duration_limit_seconds =
      std::clamp(settings.value(QLatin1String(kDurationLimitKey), 0).toInt(), 0,
                 CaptureSettings::kMaximumDurationLimitSeconds);

  loaded.low_space_warning_minutes = std::clamp(
      settings
          .value(QLatin1String(kLowSpaceKey), loaded.low_space_warning_minutes)
          .toInt(),
      0, CaptureSettings::kMaximumLowSpaceWarningMinutes);

  return loaded;
}

void SaveCaptureSettings(const CaptureSettings& settings) {
  QSettings store;
  store.setValue(QLatin1String(kPreferredDeviceKey),
                 settings.preferred_device_path);
  store.setValue(QLatin1String(kQueueSizeKey),
                 static_cast<qulonglong>(settings.queue_size_bytes));
  store.setValue(QLatin1String(kSmallTransfersKey), settings.small_transfers);
  store.setValue(QLatin1String(kTransferQueueKey),
                 static_cast<qulonglong>(settings.transfer_queue_bytes));
  store.setValue(QLatin1String(kFrontEndGainKey),
                 static_cast<uint>(settings.front_end_gain_switches));
  store.setValue(QLatin1String(kCaptureDirectoryKey),
                 settings.capture_directory);
  store.setValue(QLatin1String(kCaptureNameKey), settings.capture_name);
  store.setValue(QLatin1String(kCompressionLevelKey),
                 settings.compression_level);
  store.setValue(QLatin1String(kDurationLimitKey),
                 settings.duration_limit_seconds);
  store.setValue(QLatin1String(kLowSpaceKey),
                 settings.low_space_warning_minutes);
}

}  // namespace ddd::gui
