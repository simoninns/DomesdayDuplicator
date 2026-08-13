/************************************************************************

    capture_controller.cpp

    The bridge between the GUI and the capture engine
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_controller.h"

#include <QMetaObject>

#include "firmware_version.h"
#include "logger.h"
#include "sample_sink.h"
#include "version.h"

namespace ddd::gui {

CaptureController::CaptureController(capture::IUsbDevice* device,
                                     capture::ILogger* logger, QObject* parent)
    : QObject(parent),
      device_(device),
      logger_(logger),
      settings_(LoadCaptureSettings()),
      pipeline_(std::make_unique<capture::CapturePipeline>(logger)),
      analysis_(std::make_unique<AnalysisWorker>()) {
  qRegisterMetaType<capture::CaptureStats>();
  qRegisterMetaType<std::vector<capture::DeviceInfo>>();

  stats_timer_.setInterval(kStatsIntervalMilliseconds);
  connect(&stats_timer_, &QTimer::timeout, this, &CaptureController::Tick);
}

CaptureController::~CaptureController() {
  // Order matters. The monitor's thread calls back into this object, so it has
  // to be stopped and joined before anything it might touch is destroyed. The
  // pipeline is torn down after, and its own destructor aborts and joins.
  if (monitor_ != nullptr) {
    monitor_->Stop();
  }
  if (analysis_ != nullptr) {
    analysis_->Stop();
  }
  if (pipeline_ != nullptr) {
    pipeline_->Abort();
    pipeline_->Wait();
  }
}

void CaptureController::Start() {
  if (device_ == nullptr) {
    emit Failed(tr("No USB support"),
                tr("The USB subsystem could not be started, so no device can "
                   "be found. Check that libusb is installed."));
    return;
  }

  monitor_ = std::make_unique<capture::DeviceMonitor>(device_, logger_);
  monitor_->Start([this](const std::vector<capture::DeviceInfo>& devices) {
    // This runs on the monitor's thread. Everything past this point is on the
    // GUI thread, which is the whole reason the hop exists: a QWidget touched
    // from another thread is undefined behaviour that usually works.
    QMetaObject::invokeMethod(
        this, [this, devices] { OnDevicesChanged(devices); },
        Qt::QueuedConnection);
  });
}

void CaptureController::SetSettings(const CaptureSettings& settings) {
  settings_ = settings;
  SaveCaptureSettings(settings_);
  emit SettingsChanged(settings_);
}

void CaptureController::OnDevicesChanged(
    const std::vector<capture::DeviceInfo>& devices) {
  devices_ = devices;
  CheckFirmware(devices);
  emit DevicesChanged(devices_);
}

void CaptureController::CheckFirmware(
    const std::vector<capture::DeviceInfo>& devices) {
  const capture::DeviceInfo* const selected = capture::SelectDevice(
      devices, settings_.preferred_device_path.toStdString());

  if (selected == nullptr) {
    warned_device_path_.clear();
    warned_device_product_.clear();
    return;
  }

  const QString path = QString::fromStdString(selected->path);
  const QString product = QString::fromStdString(selected->product_string);
  if (path == warned_device_path_ && product == warned_device_product_) {
    return;
  }
  warned_device_path_ = path;
  warned_device_product_ = product;

  const capture::FirmwareVersionCheck check = capture::CheckFirmwareVersion(
      selected->product_string, capture::Version());

  if (logger_ != nullptr && !check.device_commit.empty()) {
    logger_->Info("Device firmware commit " + check.device_commit +
                  ", application commit " + check.application_commit);
  }

  if (check.ShouldWarn()) {
    emit FirmwareWarning(QString::fromStdString(check.message));
  }
}

void CaptureController::StartMonitoring() {
  if (monitoring_ || device_ == nullptr) {
    return;
  }

  const std::string path = settings_.preferred_device_path.toStdString();

  // Sent before the device is opened for streaming rather than after. The
  // gateware applies it immediately and there is no acknowledgement, so doing
  // it while data is already flowing would put the mode change somewhere
  // unpredictable in the stream.
  if (!device_->SendConfiguration(path, settings_.test_mode)) {
    emit Failed(tr("The device could not be configured"),
                tr("The device did not accept the configuration request. It "
                   "may have been unplugged, or another application may be "
                   "using it."));
    return;
  }

  capture::TransferResult opened = capture::TransferResult::kConnectionFailure;
  source_ = device_->OpenSource(path, settings_.UsbOptions(), opened);
  if (source_ == nullptr) {
    emit Failed(tr("The device could not be opened"),
                QString::fromUtf8(capture::TransferResultDescription(opened)));
    return;
  }

  capture::CapturePipeline::Options options;
  options.queue_size_bytes = settings_.queue_size_bytes;
  options.test_mode = settings_.test_mode;

  // Enumerating opens devices and does control transfers on them. Doing that to
  // a device that is streaming would put avoidable traffic on the bus for an
  // answer that is already obvious: data is arriving, so it is plainly still
  // attached.
  //
  // Null-checked because monitoring does not depend on the monitor: Start() may
  // never have been called, or may have failed, and neither is a reason to
  // refuse to stream from a device the caller has named.
  if (monitor_ != nullptr) {
    monitor_->SetSuspended(true);
  }

  if (!pipeline_->Start(source_.get(), std::make_unique<capture::NullSink>(),
                        options)) {
    if (monitor_ != nullptr) {
      monitor_->SetSuspended(false);
    }
    source_.reset();
    emit Failed(tr("Monitoring could not be started"),
                QString::fromStdString(pipeline_->ResultDetail()));
    return;
  }

  // Started only for the duration of a run. A worker thread polling thirty
  // times a second for snapshots that cannot arrive is nothing measurable, but
  // it is also nothing at all, and a thread that only exists while it has work
  // is one fewer thing to explain in a stack trace.
  analysis_->Start();
  analysis_->SetSource(&pipeline_->snapshots());

  monitoring_ = true;
  stats_timer_.start();
  emit MonitoringChanged(true);
}

void CaptureController::StopMonitoring() {
  if (!monitoring_) {
    return;
  }
  pipeline_->RequestStop();
}

void CaptureController::Tick() {
  emit StatsUpdated(pipeline_->stats().Read());

  // The pipeline stops on its own schedule: a requested stop still has to drain
  // the ring and finalise whatever sink is attached. Noticing here rather than
  // waiting for it is what keeps the window responsive while that happens.
  if (monitoring_ && !pipeline_->Running()) {
    FinishRun();
  }
}

void CaptureController::FinishRun() {
  stats_timer_.stop();
  monitoring_ = false;

  // First, and before the pipeline is touched: the next run builds a new
  // snapshot publisher, and this is what guarantees nothing is reading the old
  // one when it goes.
  analysis_->Stop();

  // Returns immediately — Running() is already false — and joins the threads.
  pipeline_->Wait();

  const capture::TransferResult result = pipeline_->Result();
  const std::string detail = pipeline_->ResultDetail();

  // Finish() has already been called by the pipeline; this only releases the
  // object.
  source_.reset();

  if (monitor_ != nullptr) {
    monitor_->SetSuspended(false);
  }

  emit StatsUpdated(pipeline_->stats().Read());
  emit MonitoringChanged(false);

  if (capture::TransferFailed(result)) {
    emit Failed(
        tr("Monitoring stopped: %1")
            .arg(QString::fromUtf8(capture::TransferResultName(result))),
        detail.empty()
            ? QString::fromUtf8(capture::TransferResultDescription(result))
            : QString::fromStdString(detail));
  }
}

}  // namespace ddd::gui
