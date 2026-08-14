/************************************************************************

    capture_controller.cpp

    The bridge between the GUI and the capture engine
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_controller.h"

#include <QDir>
#include <QMetaObject>
#include <ctime>

#include "capture_failure_presenter.h"
#include "capture_format.h"
#include "capture_naming.h"
#include "capture_provenance.h"
#include "firmware_version.h"
#include "free_space.h"
#include "gain_choices.h"
#include "logger.h"
#include "sample_format.h"
#include "sample_sink.h"
#include "statistics_presenter.h"
#include "version.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

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

void CaptureController::SetDeviceMonitorSuspended(bool suspended) {
  // Null-checked because Start() may never have been called, or may have
  // failed, and neither is a reason to refuse an update.
  if (monitor_ != nullptr) {
    monitor_->SetSuspended(suspended);
  }
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
    fpga_version_ = capture::FpgaVersion{};
    return;
  }

  const QString path = QString::fromStdString(selected->path);
  const QString product = QString::fromStdString(selected->product_string);
  if (path == warned_device_path_ && product == warned_device_product_) {
    return;
  }
  warned_device_path_ = path;
  warned_device_product_ = product;

  // Read the gateware's identity while the device is being looked at anyway,
  // so the Firmware dialog can show all three versions without opening the
  // device itself. A device that cannot answer leaves this default
  // constructed, which reads as "not known" and is not an error — gateware
  // predating the register interface, or an FPGA that was never configured,
  // both land here and both capture perfectly well.
  fpga_version_ = ReadFpgaVersion(selected->path);

  const capture::FirmwareVersionCheck check = capture::CheckFirmwareVersion(
      selected->product_string, capture::Version());

  if (logger_ != nullptr && !check.device_commit.empty()) {
    logger_->Info("Device firmware commit " + check.device_commit +
                  ", application commit " + check.application_commit);
  }

  if (logger_ != nullptr && fpga_version_.present) {
    logger_->Info("Device gateware commit " +
                  (fpga_version_.commit.empty() ? std::string("unknown")
                                                : fpga_version_.commit) +
                  (fpga_version_.dirty ? " (modified)" : ""));
  }

  if (check.ShouldWarn()) {
    emit FirmwareWarning(QString::fromStdString(check.message));
  }
}

capture::FpgaVersion CaptureController::ReadFpgaVersion(
    const std::string& path) {
  if (device_ == nullptr) {
    return {};
  }

  std::vector<uint8_t> identity;
  if (!device_->ReadRegisters(path, capture::kRegisterId,
                              capture::kIdentityLength, identity)) {
    return {};
  }

  return capture::ParseFpgaIdentity(identity);
}

void CaptureController::StartMonitoring() {
  if (monitoring_ || device_ == nullptr) {
    return;
  }

  const std::string path = settings_.preferred_device_path.toStdString();

  // Written before the device is opened for streaming rather than after. The
  // gateware applies it immediately and there is no acknowledgement, so doing
  // it while data is already flowing would put the mode change somewhere
  // unpredictable in the stream.
  if (!device_->WriteRegister(path, capture::kRegisterTestMode,
                              settings_.test_mode ? 1 : 0)) {
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

  // A capture still running when monitoring stops is finalised by the pipeline
  // on its way out — the sink is finished after both workers have joined. What
  // has to happen here is the bookkeeping: the file is no longer being written
  // to, and the panels have to be told before the run ends rather than after,
  // or a capture that ended with the stream would leave a button saying "stop".
  pipeline_->RequestStop();
}

std::unique_ptr<capture::FlacSink> CaptureController::OpenCaptureFile() {
  const std::time_t now = std::time(nullptr);

  const QString directory = settings_.ResolvedCaptureDirectory();

  // Created rather than required. A user who types a folder name that does not
  // exist yet means "put it there", and refusing would be a dialog for
  // something the application can simply do.
  QDir().mkpath(directory);

  const std::filesystem::path wanted = capture::BuildCapturePath(
      std::filesystem::path(directory.toStdString()),
      settings_.capture_name.toStdString(), settings_.test_mode, now);

  const std::filesystem::path path = capture::MakeUniqueCapturePath(wanted);

  capture::FlacWriter::Options options;
  options.compression_level = settings_.compression_level;
  options.sample_rate_label = capture::kFlacSampleRateLabel;

  capture::CaptureProvenance provenance;
  provenance.title = path.filename().string();
  provenance.application_version = std::string(capture::Version());
  provenance.test_mode = settings_.test_mode;
  provenance.started = now;

  // Written only when a declaration was actually made. DescribeFrontEndGain
  // returns a sentence saying nothing has been declared for the undeclared
  // pattern, and putting that in a metadata field would be worse than leaving
  // the field out: it would read as calibration data.
  if (settings_.DeclaredGain().declared()) {
    provenance.front_end_gain =
        DescribeFrontEndGain(settings_.front_end_gain_switches).toStdString();
  }

  options.tags = capture::BuildProvenanceTags(provenance);

  auto sink = std::make_unique<capture::FlacSink>();
  if (!sink->Open(path, options)) {
    const CaptureFailureView view = PresentCaptureFailure(
        capture::TransferResult::kFileCreationError,
        QString::fromStdString(sink->LastError()), QString());
    emit Failed(view.title, view.ToMessage());
    return nullptr;
  }

  capture_path_ = QString::fromStdString(path.string());
  if (logger_ != nullptr) {
    logger_->Info("Capturing to " + path.string());
  }
  return sink;
}

void CaptureController::StartCapture() {
  if (capturing_) {
    return;
  }

  // One action rather than two. Someone who has not been monitoring and presses
  // Start capture means "capture", and making them start the stream first would
  // be ceremony.
  if (!monitoring_) {
    StartMonitoring();
    if (!monitoring_) {
      // StartMonitoring has already said why through Failed().
      return;
    }
  }

  std::unique_ptr<capture::FlacSink> sink = OpenCaptureFile();
  if (sink == nullptr) {
    return;
  }

  low_space_warned_ = false;
  ticks_until_space_check_ = 0;

  pipeline_->AttachSink(std::move(sink));

  capturing_ = true;
  emit CapturingChanged(true, capture_path_);
}

void CaptureController::StopCapture() {
  if (!capturing_) {
    return;
  }

  // Detach rather than stop. The stream keeps running and the display keeps
  // moving while the encoder writes out its last frames and patches the header,
  // which is what makes taking several captures from one setup session possible
  // without reopening the device between them.
  pending_sink_change_ = pipeline_->DetachSink();

  capturing_ = false;
  emit CapturingChanged(false, QString());
}

void CaptureController::CollectFinishedCapture() {
  if (pending_sink_change_ == 0) {
    return;
  }
  if (pipeline_->SinkChangeCount() < pending_sink_change_) {
    return;
  }

  pending_sink_change_ = 0;

  const std::unique_ptr<capture::ISampleSink> retired =
      pipeline_->TakeRetiredSink();
  const quint64 bytes =
      retired != nullptr ? static_cast<quint64>(retired->BytesWritten()) : 0;

  if (logger_ != nullptr) {
    logger_->Info("Capture finished: " + capture_path_.toStdString() + ", " +
                  std::to_string(bytes) + " bytes");
  }

  emit CaptureFinished(capture_path_, bytes);
}

void CaptureController::CheckDurationLimit(const capture::CaptureStats& stats) {
  if (!capturing_ || settings_.duration_limit_seconds <= 0) {
    return;
  }

  // Read every tick rather than latched at the start, so that changing the
  // limit mid-capture takes effect. Unlike the engine settings beside it there
  // is nothing here that cannot be changed under a running stream: this is a
  // number compared against a counter, not a ring that would have to be
  // reallocated.
  const uint64_t limit_samples =
      static_cast<uint64_t>(settings_.duration_limit_seconds) *
      capture::kSampleRateHz;

  if (stats.samples_written < limit_samples) {
    return;
  }

  // Checked here rather than on the processing thread, so the overshoot is
  // bounded by the statistics interval and the buffer in flight — about 50 ms,
  // or 4 MB at the device's rate — rather than being exact. That is the right
  // trade: an exact limit would mean putting a GUI policy decision on the
  // real-time path, and the whole design of this application is that nothing
  // the GUI does can cost a sample.

  // Counted in samples the sink accepted, and a sink only ever receives whole
  // buffers — so the stop lands on a buffer boundary by construction rather
  // than by a timer firing somewhere in the middle of one.
  if (logger_ != nullptr) {
    logger_->Info("Duration limit reached after " +
                  std::to_string(stats.samples_written) + " samples");
  }
  StopCapture();
}

void CaptureController::CheckFreeSpace() {
  if (!capturing_ || low_space_warned_ ||
      settings_.low_space_warning_minutes <= 0) {
    return;
  }

  if (ticks_until_space_check_ > 0) {
    --ticks_until_space_check_;
    return;
  }
  ticks_until_space_check_ = kSpaceCheckIntervalTicks;

  const capture::FreeSpace space = capture::AvailableSpace(
      settings_.ResolvedCaptureDirectory().toStdString());
  if (!space.known) {
    return;
  }

  const double seconds_left =
      capture::CaptureSecondsRemaining(space.bytes_available);
  const double threshold =
      static_cast<double>(settings_.low_space_warning_minutes) * 60.0;
  if (seconds_left >= threshold) {
    return;
  }

  low_space_warned_ = true;

  // A warning, and only a warning. The capture is not stopped and the estimate
  // is an estimate — real RF compresses better than the figure used here, so
  // an application that halted on this prediction would sometimes end a good
  // capture early.
  emit LowSpaceWarning(
      tr("The destination volume has about %1 of capture left. The capture is "
         "still running; free some space or it will stop when the volume "
         "fills.")
          .arg(FormatElapsed(seconds_left)));
}

void CaptureController::Tick() {
  const capture::CaptureStats stats = pipeline_->stats().Read();
  emit StatsUpdated(stats);

  CheckDurationLimit(stats);
  CheckFreeSpace();
  CollectFinishedCapture();

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

  // A capture that was still running when the stream stopped. The pipeline has
  // already finished the sink — it does that after joining both workers,
  // however the run ended — so the file on disk is closed and readable. What is
  // left is to say so, and to say it before the failure below, so that the
  // panels are consistent by the time a message box takes over the event loop.
  const bool was_capturing = capturing_;
  const QString finished_path = capture_path_;
  const capture::CaptureStats final_stats = pipeline_->stats().Read();

  if (was_capturing) {
    capturing_ = false;
    emit CapturingChanged(false, QString());
  }
  pending_sink_change_ = 0;

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

  if (was_capturing) {
    emit CaptureFinished(finished_path, final_stats.bytes_written);
  }

  if (capture::TransferFailed(result)) {
    const CaptureFailureView view = PresentCaptureFailure(
        result, ToQString(detail), was_capturing ? finished_path : QString());
    emit Failed(view.title, view.ToMessage());
  }
}

}  // namespace ddd::gui
