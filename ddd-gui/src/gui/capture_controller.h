/************************************************************************

    capture_controller.h

    The bridge between the GUI and the capture engine
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>
#include <vector>

#include "capture_pipeline.h"
#include "capture_settings.h"
#include "device_monitor.h"
#include "monitor_tap.h"
#include "usb_device.h"
#include "usb_device_info.h"

namespace ddd::gui {

// Owns the engine on the GUI's behalf, and is the only place the two meet.
//
// Everything below it is Qt-free and everything above it is Qt. That is what
// keeps the rule in AGENTS.md honest rather than aspirational: there is exactly
// one file where a QObject and a std::thread are in scope together, and it is
// this one.
//
// Nothing here blocks. The pipeline runs on its own threads and is asked
// politely to stop; a timer notices when it has, which is what lets a stop take
// as long as finalising a file needs to take without the window freezing. The
// statistics come from the wait-free publisher, so polling them cannot slow the
// capture down however often this asks.
//
// The device backend is borrowed rather than owned, so a test can supply one
// that reports whatever devices it likes and hands back a synthetic source.
// With that, everything in this class — including the parts a user drives — is
// testable on a machine with nothing plugged in.
class CaptureController : public QObject {
  Q_OBJECT

 public:
  CaptureController(capture::IUsbDevice* device, capture::ILogger* logger,
                    QObject* parent = nullptr);
  ~CaptureController() override;

  // Begin watching for devices. Separate from the constructor so that a caller
  // can connect to the signals first and not miss the initial report.
  void Start();

  bool monitoring() const { return monitoring_; }

  std::vector<capture::DeviceInfo> devices() const { return devices_; }

  const CaptureSettings& settings() const { return settings_; }

  // Applying settings while a capture is running changes what the next one will
  // do, not this one. Nothing here can be changed mid-stream without stopping,
  // and pretending otherwise would mean a ring that was resized underneath a
  // running transfer.
  void SetSettings(const CaptureSettings& settings);

  // How often the statistics are republished to the panels. 20 Hz: fast enough
  // that a throughput reading looks live, slow enough that it is nowhere near
  // the cost of anything else the application does.
  static constexpr int kStatsIntervalMilliseconds = 50;

 public slots:
  // Open the device and start streaming with no sink attached. This is monitor
  // mode: the signal is validated, measured and published, and nothing is
  // written anywhere.
  void StartMonitoring();

  // Stop at the next buffer boundary. Returns immediately; MonitoringChanged
  // follows when the pipeline has actually stopped.
  void StopMonitoring();

 signals:
  void DevicesChanged(const std::vector<ddd::capture::DeviceInfo>& devices);
  void MonitoringChanged(bool monitoring);
  void StatsUpdated(const ddd::capture::CaptureStats& stats);

  // A device whose firmware build differs from this application's. Raised once
  // per connection and never blocking — see firmware_version.h.
  void FirmwareWarning(const QString& message);

  // Something went wrong, with a short title and the sentence explaining it.
  void Failed(const QString& title, const QString& detail);

 private:
  void OnDevicesChanged(const std::vector<capture::DeviceInfo>& devices);
  void CheckFirmware(const std::vector<capture::DeviceInfo>& devices);
  void Tick();
  void FinishRun();

  capture::IUsbDevice* device_ = nullptr;
  capture::ILogger* logger_ = nullptr;

  CaptureSettings settings_;

  std::unique_ptr<capture::DeviceMonitor> monitor_;
  std::unique_ptr<capture::CapturePipeline> pipeline_;
  std::unique_ptr<capture::ISampleSource> source_;

  QTimer stats_timer_;

  std::vector<capture::DeviceInfo> devices_;
  bool monitoring_ = false;

  // The device the firmware warning has already been shown for. Cleared when
  // that device goes away, so re-plugging it warns again — which is right,
  // because re-plugging is what a user does after updating the firmware.
  QString warned_device_path_;
  QString warned_device_product_;
};

}  // namespace ddd::gui

// Declared so that QSignalSpy can carry them, which is how the tests observe
// what this class emits without a window to look at.
Q_DECLARE_METATYPE(ddd::capture::CaptureStats)
Q_DECLARE_METATYPE(std::vector<ddd::capture::DeviceInfo>)
