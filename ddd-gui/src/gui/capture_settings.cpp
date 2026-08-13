/************************************************************************

    capture_settings.cpp

    The settings a capture runs with, and where they are kept
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_settings.h"

#include <QSettings>
#include <algorithm>

namespace ddd::gui {
namespace {

constexpr const char* kPreferredDeviceKey = "capture/preferred_device";
constexpr const char* kQueueSizeKey = "capture/queue_size_bytes";
constexpr const char* kSmallTransfersKey = "capture/small_transfers";
constexpr const char* kTransferQueueKey = "capture/transfer_queue_bytes";

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
}

}  // namespace ddd::gui
