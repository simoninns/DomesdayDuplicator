/************************************************************************

    analysis_worker.cpp

    Snapshot analysis, off the thread that has to stay responsive
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "analysis_worker.h"

#include <QTimer>

#include "sample_format.h"

namespace ddd::gui {

void SnapshotAnalyser::SetSource(capture::SnapshotPublisher* snapshots) {
  const std::lock_guard<std::mutex> lock(source_mutex_);
  source_ = snapshots;

  // A new run is a new signal. Carrying the averaged spectrum and the peak hold
  // across would show the previous device's carrier for several seconds after
  // this one was attached.
  spectrum_.Reset();
}

void SnapshotAnalyser::SetSpectrumAveraging(double averaging) {
  requested_averaging_.store(averaging);
  options_changed_.store(true);
}

void SnapshotAnalyser::SetSpectrumTransformSize(size_t transform_size) {
  requested_transform_size_.store(transform_size);
  options_changed_.store(true);
}

void SnapshotAnalyser::RequestPeakHoldReset() {
  peak_hold_reset_requested_.store(true);
}

void SnapshotAnalyser::Begin() {
  // Created here rather than in the constructor because a QTimer fires on the
  // thread it was created on. Built in the constructor it would belong to the
  // GUI thread, and every transform would run there — which is the one thing
  // this class exists to prevent.
  timer_ = new QTimer(this);
  timer_->setInterval(AnalysisWorker::kPollIntervalMilliseconds);
  connect(timer_, &QTimer::timeout, this, &SnapshotAnalyser::Poll);
  timer_->start();
}

void SnapshotAnalyser::Poll() {
  if (options_changed_.exchange(false)) {
    // The analyser holds its window and its buffers sized to a transform, so
    // changing either option means building another one. It happens when a user
    // moves a control, not per frame.
    analysis::SpectrumAnalyser::Options options;
    options.averaging = requested_averaging_.load();
    options.transform_size = requested_transform_size_.load();
    spectrum_ = analysis::SpectrumAnalyser(options);
  }

  if (peak_hold_reset_requested_.exchange(false)) {
    spectrum_.ResetPeakHold();
  }

  {
    const std::lock_guard<std::mutex> lock(source_mutex_);
    if (source_ == nullptr) {
      return;
    }

    uint64_t generation = 0;
    if (!source_->TryRead(wire_, generation)) {
      // Nothing new since the last poll, which is the ordinary case: the
      // pipeline publishes about nine snapshots a second and this looks thirty
      // times.
      return;
    }
  }

  const size_t sample_count = wire_.size() / capture::kBytesPerSample;
  codes_.resize(sample_count);
  for (size_t index = 0; index < sample_count; ++index) {
    // Assembled from bytes rather than reinterpreted as uint16_t: the wire
    // format is little-endian regardless of what this machine is, and a cast
    // would be right on one architecture and silently wrong on another.
    const uint16_t word =
        static_cast<uint16_t>(wire_[index * capture::kBytesPerSample]) |
        static_cast<uint16_t>(
            static_cast<uint16_t>(wire_[(index * capture::kBytesPerSample) + 1])
            << 8);
    codes_[index] = capture::SampleValueFromWord(word);
  }

  emit WaveformReady(codes_);

  if (spectrum_.Analyse(codes_.data(), codes_.size())) {
    emit SpectrumReady(spectrum_.magnitudes_db(), spectrum_.peak_hold_db(),
                       spectrum_.snapshot_db(), spectrum_.segment_count());
  }
}

AnalysisWorker::AnalysisWorker(QObject* parent) : QObject(parent) {
  // Registered here rather than in main() so that the types are known to the
  // meta-object system before any connection is made, whatever creates this.
  qRegisterMetaType<std::vector<uint16_t>>();
  qRegisterMetaType<std::vector<double>>();
}

AnalysisWorker::~AnalysisWorker() {
  Stop();

  // Only reachable when Start() was never called; otherwise Stop() has already
  // let the thread delete it.
  delete analyser_;
}

void AnalysisWorker::Start() {
  if (thread_.isRunning()) {
    return;
  }

  analyser_ = new SnapshotAnalyser();

  // Auto connections, which become queued because the sender ends up on the
  // worker thread and the receiver stays here. That is what puts the vectors
  // back on the GUI thread as values rather than as references to something the
  // worker is about to overwrite.
  connect(analyser_, &SnapshotAnalyser::WaveformReady, this,
          &AnalysisWorker::WaveformReady);
  connect(analyser_, &SnapshotAnalyser::SpectrumReady, this,
          &AnalysisWorker::SpectrumReady);

  analyser_->moveToThread(&thread_);
  connect(&thread_, &QThread::started, analyser_, &SnapshotAnalyser::Begin);
  connect(&thread_, &QThread::finished, analyser_, &QObject::deleteLater);

  thread_.start();
}

void AnalysisWorker::Stop() {
  if (!thread_.isRunning()) {
    return;
  }

  // Detached before the thread is asked to stop, so that whatever the caller
  // does next to the publisher cannot race a poll already under way.
  if (analyser_ != nullptr) {
    analyser_->SetSource(nullptr);
  }

  thread_.quit();
  thread_.wait();

  // The deleteLater above has run by now: a thread's deferred deletions are
  // processed as its event loop exits.
  analyser_ = nullptr;
}

void AnalysisWorker::SetSource(capture::SnapshotPublisher* snapshots) {
  if (analyser_ != nullptr) {
    analyser_->SetSource(snapshots);
  }
}

void AnalysisWorker::SetSpectrumAveraging(double averaging) {
  if (analyser_ != nullptr) {
    analyser_->SetSpectrumAveraging(averaging);
  }
}

void AnalysisWorker::SetSpectrumTransformSize(size_t transform_size) {
  if (analyser_ != nullptr) {
    analyser_->SetSpectrumTransformSize(transform_size);
  }
}

void AnalysisWorker::ResetPeakHold() {
  if (analyser_ != nullptr) {
    analyser_->RequestPeakHoldReset();
  }
}

}  // namespace ddd::gui
