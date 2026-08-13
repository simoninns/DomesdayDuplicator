/************************************************************************

    analysis_worker.h

    Snapshot analysis, off the thread that has to stay responsive
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QObject>
#include <QThread>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "capture_metatypes.h"
#include "monitor_tap.h"
#include "spectrum_analyser.h"

class QTimer;

namespace ddd::gui {

// Where the transforms happen, and why not on the GUI thread.
//
// A 4,096-point FFT is a millisecond or so. Thirty a second on the GUI thread
// would be three per cent of it — not enough to stutter, until the machine is
// also compositing, encoding and moving 80 MB/s off a USB device, which is
// precisely when a user is watching the display to find out whether it is
// coping. The measurement must not be part of what it is measuring.
//
// Nothing here can slow the capture down. The snapshots come from the
// triple-buffered publisher in monitor_tap.h, which never makes its writer wait
// for anything: a poll that finds no new snapshot returns immediately, and a
// consumer too slow to keep up misses snapshots rather than delaying the
// pipeline. Frames are dropped, never queued — an old picture of a live signal
// is of no interest, and a backlog of them would be worse than useless.

// The half that lives on the worker thread. Created on the GUI thread, moved
// once, and never touched from anywhere else except through the small guarded
// surface below.
class SnapshotAnalyser : public QObject {
  Q_OBJECT

 public:
  SnapshotAnalyser() = default;

  // Attach to a running pipeline's publisher, or detach with nullptr.
  //
  // Blocks until the worker is out of the publisher, so a caller may destroy it
  // the moment this returns — which matters, because the pipeline builds a new
  // publisher for every run. The wait is a snapshot copy at most, and it is the
  // GUI thread waiting on the worker rather than anything waiting on the
  // capture.
  void SetSource(capture::SnapshotPublisher* snapshots);

  void SetSpectrumAveraging(double averaging);
  void RequestPeakHoldReset();

 public slots:
  // Builds the poll timer. Connected to the thread's started() signal so that
  // the timer is created on the thread it will fire on.
  void Begin();

  void Poll();

 signals:
  void WaveformReady(const std::vector<uint16_t>& codes);
  void SpectrumReady(const std::vector<double>& magnitudes_db,
                     const std::vector<double>& peak_hold_db);

 private:
  // Guards the source pointer and the read through it, and nothing else. Held
  // for a memcpy, never for a transform.
  std::mutex source_mutex_;
  capture::SnapshotPublisher* source_ = nullptr;

  QTimer* timer_ = nullptr;

  analysis::SpectrumAnalyser spectrum_;
  std::atomic<double> requested_averaging_{analysis::kDefaultAveraging};
  std::atomic<bool> averaging_changed_{false};
  std::atomic<bool> peak_hold_reset_requested_{false};

  // Worker-thread scratch. Reused rather than reallocated per frame.
  std::vector<uint8_t> wire_;
  std::vector<uint16_t> codes_;
};

// The GUI-side handle. Owns the thread and the object on it, and re-emits what
// that object produces so that nothing outside this file has to know there is a
// second thread at all.
class AnalysisWorker : public QObject {
  Q_OBJECT

 public:
  explicit AnalysisWorker(QObject* parent = nullptr);
  ~AnalysisWorker() override;

  // About 30 Hz. Ahead of the pipeline's own snapshot rate of roughly 9 Hz, so
  // a snapshot is picked up in the frame after it is published rather than
  // waiting out a slower poll.
  static constexpr int kPollIntervalMilliseconds = 33;

  void Start();
  void Stop();

  bool running() const { return thread_.isRunning(); }

  // All three are no-ops before Start() and after Stop(): there is no thread to
  // carry the request to, and a caller should not have to check.
  void SetSource(capture::SnapshotPublisher* snapshots);
  void SetSpectrumAveraging(double averaging);
  void ResetPeakHold();

 signals:
  void WaveformReady(const std::vector<uint16_t>& codes);
  void SpectrumReady(const std::vector<double>& magnitudes_db,
                     const std::vector<double>& peak_hold_db);

 private:
  QThread thread_;

  // Raw rather than a unique_ptr: once the thread is running this object
  // belongs to it, and deleting it from here would be deleting an object on
  // another thread. It is destroyed by a deleteLater connected to the thread's
  // finished signal, which runs on the thread that owns it.
  SnapshotAnalyser* analyser_ = nullptr;
};

}  // namespace ddd::gui
