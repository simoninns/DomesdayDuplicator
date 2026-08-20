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
#include "capture_metadata.h"
#include "capture_metatypes.h"
#include "capture_pipeline.h"
#include "capture_provenance.h"
#include "capture_settings.h"
#include "device_monitor.h"
#include "flac_sink.h"
#include "fpga_version.h"
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

  // What the selected device's gateware last said about itself.
  //
  // Read when a device appears rather than on demand, so that showing it costs
  // nothing and never blocks. Default constructed — present false — when no
  // device is selected or its gateware could not answer.
  const capture::FpgaVersion& fpga_version() const { return fpga_version_; }

  const CaptureSettings& settings() const { return settings_; }

  // The USB backend, borrowed. Exposed so that the update flow can open the
  // same device this is watching, through the same backend, rather than
  // starting a second one — two libusb contexts enumerating the same bus is
  // a way to get two different answers about what is attached.
  capture::IUsbDevice* usb_device() const { return device_; }

  // Stop enumerating while something else owns the device.
  //
  // The device monitor opens every attached device to read its identity, and
  // an update holds one open for minutes. Enumerating underneath that is at
  // best wasted work and at worst a second claim on an interface that is
  // being written to — and the device disappears and comes back during an
  // update anyway, so the monitor's report would be noise a user should not
  // be shown.
  void SetDeviceMonitorSuspended(bool suspended);

  // The signal panels' source of waveform and spectrum frames. Owned here
  // rather than by a panel because it is tied to the run rather than to any one
  // display: it is attached when a run starts and detached when it ends, and
  // three panels share what it produces.
  AnalysisWorker* analysis() { return analysis_.get(); }

  // What was in the player when this capture was set up.
  //
  // Set by the automatic-capture coupling and by nothing else, and cleared when
  // there is no longer a disc it describes. It reaches the file the next
  // capture opens rather than the one that is running: a file's tags are
  // written into its header when it is created, so a fact that arrived
  // afterwards has nowhere to go.
  void SetDiscProvenance(const capture::DiscProvenance& disc);
  const capture::DiscProvenance& disc_provenance() const {
    return disc_provenance_;
  }

  // Who the player was, for the sidecar.
  //
  // Set whenever the link comes up or goes down, and so present for a capture
  // taken by hand as well as for an automatic one: a manual capture of a disc
  // in a player is still a capture whose provenance includes which player it
  // came off.
  void SetPlayerIdentity(const capture::PlayerIdentity& player);
  const capture::PlayerIdentity& player_identity() const {
    return player_identity_;
  }

  // What the examination of the disc found, for the sidecar.
  //
  // Set by the automatic-capture coupling on the same terms as the disc
  // provenance above, and cleared with it. A capture taken by hand some time
  // after an examination carries no scan rather than the previous disc's — the
  // disc in the player is not necessarily the disc that was examined, and a
  // file asserting otherwise would be worse than one that says nothing.
  void SetDiscScan(const capture::DiscScan& disc);
  const capture::DiscScan& disc_scan() const { return disc_scan_; }

  // Applying settings while a capture is running changes what the next one will
  // do, not this one. Nothing here can be changed mid-stream without stopping,
  // and pretending otherwise would mean a ring that was resized underneath a
  // running transfer.
  void SetSettings(const CaptureSettings& settings);

  // The same, without saving them.
  //
  // What the command line names applies to the run it was given to and is then
  // forgotten: a script that captures one disc at 20 Msps has not asked for
  // every capture afterwards to be taken at 20 Msps, and SetSettings() above
  // would have made that the user's new saved answer. The window is populated
  // from what this sets, so a capture set up from a script and then taken by
  // hand still runs with what the script asked for — and if the user then edits
  // any of it in the panel, that edit saves in the ordinary way, because at
  // that point it is their choice rather than the script's.
  void ApplySessionSettings(const CaptureSettings& settings);

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

  // The requested name was already taken, so the capture was written under
  // another. Not a failure — nothing was overwritten, which the engine
  // guarantees by resolving the path before it opens anything — but a fact the
  // user has to be told, because the file is not the one they named.
  void CaptureRenamed(const QString& requested, const QString& written);

  // A capture finished and its file is closed. `bytes` is what reached the
  // disk, which is not derivable from the sample count once a compressor is in
  // the path.
  //
  // The path is where the file ended up, which is not necessarily where it was
  // opened: a capture whose naming asks for the duration in its name is renamed
  // at this point, since the duration is not a fact until the capture has
  // stopped.
  void CaptureFinished(const QString& file_path, quint64 bytes);

  // The sidecar could not be written. Not a capture failure — the recording is
  // on disk and complete — so it is reported separately and never as an error
  // box, which would send somebody looking for a fault in the wrong place.
  void MetadataWriteFailed(const QString& detail);

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

  // Read and parse the gateware identity block from the device at `path`.
  capture::FpgaVersion ReadFpgaVersion(const std::string& path);

  // What the device this capture is coming off was built from, for the file's
  // own tags and for the sidecar beside it.
  //
  // Assembled from what was already read when the device appeared rather than
  // by asking it again: a capture is about to start, and the device is about
  // to be opened for the stream. Whatever could not be established is left
  // empty, which is what both writers treat as "say nothing".
  capture::DeviceBuild CurrentDeviceBuild() const;
  void Tick();
  void FinishRun();

  // Build and open the file for a new capture, in whichever format the settings
  // ask for. Returns null with the reason already reported through Failed().
  std::unique_ptr<capture::ISampleSink> OpenCaptureFile();

  // Notice that the writer has been detached and the file closed, and report
  // it. Called from Tick() rather than from StopCapture(), because finalising a
  // FLAC stream happens on the processing thread and takes as long as it takes.
  void CollectFinishedCapture(const capture::CaptureStats& stats);

  // Everything that happens once a capture's file is closed: the duration
  // rename where the naming asks for one, the sidecar, and the signal that
  // says so.
  //
  // One function for the three because they have to happen in that order and
  // share the path they act on — a sidecar written before the rename would be
  // orphaned by it, and a signal carrying the old path would name a file that
  // is no longer there.
  void FinishCaptureFile(const capture::CaptureStats& stats, uint64_t bytes,
                         uint64_t samples);

  // Write the sidecar beside `capture_file`, and report a failure without
  // treating it as one.
  void WriteMetadataSidecar(const std::filesystem::path& capture_file,
                            const capture::CaptureStats& stats, uint64_t bytes,
                            uint64_t samples, double duration_seconds);

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

  // The gateware version that goes with warned_device_path_
  capture::FpgaVersion fpga_version_;

  // See SetDiscProvenance. Empty for every capture taken without a player.
  capture::DiscProvenance disc_provenance_;

  // See SetPlayerIdentity and SetDiscScan.
  capture::PlayerIdentity player_identity_;
  capture::DiscScan disc_scan_;

  // What the running capture will say about itself, filled in when its file is
  // opened and completed when the file is closed.
  //
  // Latched at the start rather than gathered at the end, for the facts that
  // describe the setup: the player and the disc are cleared by the
  // automatic-capture coupling the moment its run ends, which is before the
  // encoder has finished the file this describes. The naming fields are
  // deliberately *not* latched — they are read at the end, so that notes typed
  // while watching a capture reach that capture's own metadata.
  capture::CaptureMetadata pending_metadata_;

  // The device's loss counters as they stood when the capture started, so that
  // the sidecar reports what the device lost while writing this file rather
  // than what it has lost since monitoring began.
  //
  // The signal figures need no equivalent. A minimum and a maximum cannot be
  // differenced, so the engine measures the file's own span in its own right —
  // see SampleMetrics::BeginCaptureSpan.
  uint64_t device_overflows_at_start_ = 0;
  uint64_t device_drops_at_start_ = 0;
};

}  // namespace ddd::gui
