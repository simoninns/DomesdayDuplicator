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

#include "analysis_worker.h"
#include "capture_metatypes.h"
#include "capture_pipeline.h"
#include "capture_settings.h"
#include "device_monitor.h"
#include "flac_sink.h"
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

  // Whether a writer is attached. Always implies monitoring(): a capture is the
  // same stream with a sink on the end of it.
  bool capturing() const { return capturing_; }

  // The file the current capture is being written to, or the last one written.
  // Empty until the first capture of the session.
  QString capture_path() const { return capture_path_; }

  std::vector<capture::DeviceInfo> devices() const { return devices_; }

  const CaptureSettings& settings() const { return settings_; }

  // The signal panels' source of waveform and spectrum frames. Owned here
  // rather than by a panel because it is tied to the run rather than to any one
  // display: it is attached when a run starts and detached when it ends, and
  // three panels share what it produces.
  AnalysisWorker* analysis() { return analysis_.get(); }

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

  // Attach a writer, so the stream starts reaching a file.
  //
  // Starts monitoring first if nothing is running, so that a user who has not
  // been monitoring gets one action rather than two. From an existing monitor
  // session the sink is attached at the next buffer boundary and the stream is
  // not interrupted — the device never knows a capture began.
  void StartCapture();

  // Detach the writer and finalise the file, leaving the stream running.
  //
  // Stop returns to monitoring rather than to idle, which is what makes taking
  // several captures from one setup session possible without reopening the
  // device between them.
  void StopCapture();

 signals:
  void DevicesChanged(const std::vector<ddd::capture::DeviceInfo>& devices);
  void MonitoringChanged(bool monitoring);
  void StatsUpdated(const ddd::capture::CaptureStats& stats);

  // A writer was attached or detached. The path is the file being written, or
  // empty when the capture has just ended.
  void CapturingChanged(bool capturing, const QString& file_path);

  // A capture finished and its file is closed. `bytes` is what reached the
  // disk, which is not derivable from the sample count once a compressor is in
  // the path.
  void CaptureFinished(const QString& file_path, quint64 bytes);

  // The destination volume has less space left than the warning threshold.
  // Raised once per capture: a warning that repeated every two seconds for the
  // rest of a disc side would be ignored, which is worse than not warning.
  void LowSpaceWarning(const QString& message);

  // The settings changed. Emitted for the panels rather than for the engine:
  // the front-end gain declaration is a display calibration, so a panel that
  // has already drawn a level in converter codes has to be told to draw it
  // again in millivolts without anything being re-acquired.
  void SettingsChanged(const CaptureSettings& settings);

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

  // Build and open the file for a new capture. Returns null with the reason
  // already reported through Failed().
  std::unique_ptr<capture::FlacSink> OpenCaptureFile();

  // Notice that the writer has been detached and the file closed, and report
  // it. Called from Tick() rather than from StopCapture(), because finalising a
  // FLAC stream happens on the processing thread and takes as long as it takes.
  void CollectFinishedCapture();

  // Stop the capture because the duration limit has been reached.
  void CheckDurationLimit(const capture::CaptureStats& stats);

  // Warn once if the destination volume is running out.
  void CheckFreeSpace();

  capture::IUsbDevice* device_ = nullptr;
  capture::ILogger* logger_ = nullptr;

  CaptureSettings settings_;

  std::unique_ptr<capture::DeviceMonitor> monitor_;
  std::unique_ptr<capture::CapturePipeline> pipeline_;
  std::unique_ptr<capture::ISampleSource> source_;

  // Declared after the pipeline so that it is destroyed before it: the worker
  // reads through a publisher the pipeline owns, and the reverse order would
  // free the publisher first.
  std::unique_ptr<AnalysisWorker> analysis_;

  QTimer stats_timer_;

  std::vector<capture::DeviceInfo> devices_;
  bool monitoring_ = false;
  bool capturing_ = false;

  QString capture_path_;

  // The sink change this controller is waiting to see completed, so that the
  // finished file is collected at the moment the processing thread actually
  // swapped it rather than at the moment the swap was asked for.
  uint64_t pending_sink_change_ = 0;

  // Ticks until the next free-space check. The volume is interrogated about
  // once every two seconds rather than at the statistics rate: it is a
  // filesystem call, and nothing it reports changes twenty times a second.
  int ticks_until_space_check_ = 0;
  bool low_space_warned_ = false;

  static constexpr int kSpaceCheckIntervalTicks =
      2000 / kStatsIntervalMilliseconds;

  // The device the firmware warning has already been shown for. Cleared when
  // that device goes away, so re-plugging it warns again — which is right,
  // because re-plugging is what a user does after updating the firmware.
  QString warned_device_path_;
  QString warned_device_product_;
};

}  // namespace ddd::gui
