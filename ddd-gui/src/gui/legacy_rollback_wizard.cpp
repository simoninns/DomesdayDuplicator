/************************************************************************

    legacy_rollback_wizard.cpp

    Putting the original firmware and gateware back, in seven pages
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "legacy_rollback_wizard.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

#include "rollback_worker.h"
#include "update_bundle.h"
#include "update_orchestrator.h"
#include "update_text.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

constexpr int kWantedWidthPixels = 720;
constexpr int kWantedHeightPixels = 640;
constexpr double kMostOfTheScreen = 0.9;

constexpr int kPollMilliseconds = 500;

// How long the power-cycle page stays merely patient before it starts being
// helpful, in polls. Ten seconds, as in the bring-up wizard.
constexpr int kPowerCyclePatiencePolls = 20;

// The pages, in order. One list, used for navigation and for Steps(), so the
// order a test asserts is the order the buttons walk.
constexpr RollbackPage kPages[] = {
    RollbackPage::kOverview, RollbackPage::kConnect,  RollbackPage::kImage,
    RollbackPage::kGateware, RollbackPage::kFirmware, RollbackPage::kPowerCycle,
    RollbackPage::kVerify,
};

QLabel* MakeHeading(QWidget* parent, const QString& text) {
  auto* label = new QLabel(text, parent);
  QFont font = label->font();
  font.setBold(true);
  label->setFont(font);
  label->setWordWrap(true);
  return label;
}

QLabel* MakeBody(QWidget* parent, const QString& text = QString()) {
  auto* label = new QLabel(text, parent);
  label->setWordWrap(true);
  label->setTextFormat(Qt::RichText);
  return label;
}

// Every page is a scroll area, so a long explanation on a short screen is
// scrollable rather than clipped.
QWidget* MakePage(QWidget** body_out) {
  auto* area = new QScrollArea();
  area->setWidgetResizable(true);
  area->setFrameShape(QFrame::NoFrame);

  auto* body = new QWidget(area);
  area->setWidget(body);
  *body_out = body;
  return area;
}

QString RowMarkup(const BringUpStatusRow& row) {
  return QStringLiteral("<p><b><span style=\"color:%1\">%2</span> %3</b><br>%4")
             .arg(BringUpMarkColour(row.state), BringUpMark(row.state),
                  row.title.toHtmlEscaped(), row.detail) +
         QStringLiteral("</p>");
}

int IndexOf(RollbackPage page) {
  for (int index = 0; index < static_cast<int>(std::size(kPages)); ++index) {
    if (kPages[index] == page) {
      return index;
    }
  }
  return 0;
}

}  // namespace

LegacyRollbackWizard::LegacyRollbackWizard(Access access, QWidget* parent)
    : QDialog(parent),
      access_(std::move(access)),
      policy_(capture::DefaultUpdateKeyPolicy()) {
  setWindowTitle(tr("Roll back to legacy firmware"));
  setWindowModality(Qt::NonModal);
  setSizeGripEnabled(true);

  const QScreen* const display = screen();
  const QSize available = display != nullptr
                              ? display->availableGeometry().size()
                              : QSize(kWantedWidthPixels, kWantedHeightPixels);
  resize(std::min(kWantedWidthPixels,
                  static_cast<int>(available.width() * kMostOfTheScreen)),
         std::min(kWantedHeightPixels,
                  static_cast<int>(available.height() * kMostOfTheScreen)));

  auto* layout = new QVBoxLayout(this);

  auto* title_row = new QHBoxLayout();
  heading_ = MakeHeading(this, QString());
  heading_->setObjectName(QLatin1String(kHeadingName));
  title_row->addWidget(heading_, 1);

  step_ = new QLabel(this);
  step_->setObjectName(QLatin1String(kStepLabelName));
  title_row->addWidget(step_);
  layout->addLayout(title_row);

  pages_ = new QStackedWidget(this);
  pages_->setObjectName(QLatin1String(kPagesName));
  pages_->addWidget(BuildOverviewPage());
  pages_->addWidget(BuildConnectPage());
  pages_->addWidget(BuildImagePage());
  pages_->addWidget(BuildGatewarePage());
  pages_->addWidget(BuildFirmwarePage());
  pages_->addWidget(BuildPowerCyclePage());
  pages_->addWidget(BuildVerifyPage());
  layout->addWidget(pages_, 1);

  auto* button_row = new QWidget(this);
  auto* button_layout = new QHBoxLayout(button_row);
  button_layout->setContentsMargins(0, 0, 0, 0);

  auto* close = new QPushButton(tr("Close"), button_row);
  close->setObjectName(QLatin1String(kCloseButtonName));
  button_layout->addWidget(close);
  button_layout->addStretch(1);

  previous_button_ = new QPushButton(tr("‹ Previous"), button_row);
  previous_button_->setObjectName(QLatin1String(kPreviousButtonName));
  button_layout->addWidget(previous_button_);

  next_button_ = new QPushButton(tr("Next ›"), button_row);
  next_button_->setObjectName(QLatin1String(kNextButtonName));
  button_layout->addWidget(next_button_);
  layout->addWidget(button_row);

  connect(previous_button_, &QPushButton::clicked, this,
          &LegacyRollbackWizard::GoPrevious);
  connect(next_button_, &QPushButton::clicked, this,
          &LegacyRollbackWizard::GoNext);
  connect(close, &QPushButton::clicked, this, &QWidget::close);

  timer_ = new QTimer(this);
  timer_->setInterval(kPollMilliseconds);
  connect(timer_, &QTimer::timeout, this, &LegacyRollbackWizard::Poll);
  timer_->start();

  ReadDevices();
  ShowPage(RollbackPage::kOverview);
}

LegacyRollbackWizard::~LegacyRollbackWizard() {
  if (thread_ != nullptr) {
    if (worker_ != nullptr) {
      worker_->Cancel();
    }
    thread_->quit();
    thread_->wait();
  }

  delete worker_;
  worker_ = nullptr;
}

// --- the pages ------------------------------------------------------------

QWidget* LegacyRollbackWizard::BuildOverviewPage() {
  QWidget* page = nullptr;
  QWidget* const area = MakePage(&page);
  auto* layout = new QVBoxLayout(page);

  QLabel* const text = MakeBody(page, RollbackOverviewText());
  text->setObjectName(QLatin1String(kOverviewTextName));
  layout->addWidget(text);

  QLabel* const prompt = MakeBody(page, RollbackConfirmPrompt());
  prompt->setObjectName(QLatin1String(kConfirmPromptName));
  layout->addWidget(prompt);

  confirm_field_ = new QLineEdit(page);
  confirm_field_->setObjectName(QLatin1String(kConfirmFieldName));
  layout->addWidget(confirm_field_);

  // Compared case-insensitively and with the edges trimmed: what is being
  // asked for is deliberation, not typing accuracy, and a user who typed the
  // right words with a trailing space has deliberated.
  connect(confirm_field_, &QLineEdit::textChanged, this,
          [this](const QString& typed) {
            confirmed_ = typed.trimmed().compare(RollbackConfirmWord(),
                                                 Qt::CaseInsensitive) == 0;
            Refresh();
          });

  layout->addStretch(1);
  return area;
}

QWidget* LegacyRollbackWizard::BuildConnectPage() {
  QWidget* page = nullptr;
  QWidget* const area = MakePage(&page);
  auto* layout = new QVBoxLayout(page);

  device_row_ = MakeBody(page);
  device_row_->setObjectName(QLatin1String(kDeviceRowName));
  layout->addWidget(device_row_);

  layout->addWidget(MakeBody(page, BringUpConnectLegend()));

  auto* again = new QPushButton(tr("Check again"), page);
  again->setObjectName(QLatin1String(kCheckAgainButtonName));
  connect(again, &QPushButton::clicked, this, &LegacyRollbackWizard::Poll);
  layout->addWidget(again);

  layout->addStretch(1);
  return area;
}

QWidget* LegacyRollbackWizard::BuildImagePage() {
  QWidget* page = nullptr;
  QWidget* const area = MakePage(&page);
  auto* layout = new QVBoxLayout(page);

  QLabel* const text = MakeBody(page, RollbackImageText());
  text->setObjectName(QLatin1String(kImageTextName));
  layout->addWidget(text);

  auto* choose = new QPushButton(tr("Choose a rollback file…"), page);
  choose->setObjectName(QLatin1String(kChooseButtonName));
  connect(choose, &QPushButton::clicked, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose a rollback file"), QString(),
        tr("Domesday Duplicator update files (*%1)")
            .arg(QString::fromUtf8(
                capture::kUpdateBundleExtension.data(),
                static_cast<int>(capture::kUpdateBundleExtension.size()))));
    if (!path.isEmpty()) {
      LoadRollbackSet(path);
    }
  });
  layout->addWidget(choose);

  image_ = MakeBody(page);
  image_->setObjectName(QLatin1String(kImageLabelName));
  layout->addWidget(image_);

  image_banner_ = MakeBody(page);
  image_banner_->setObjectName(QLatin1String(kImageBannerName));
  image_banner_->setVisible(false);
  layout->addWidget(image_banner_);

  layout->addStretch(1);
  return area;
}

QWidget* LegacyRollbackWizard::BuildGatewarePage() {
  QWidget* page = nullptr;
  QWidget* const area = MakePage(&page);
  auto* layout = new QVBoxLayout(page);

  gateware_text_ = MakeBody(page, RollbackGatewareText(0));
  gateware_text_->setObjectName(QLatin1String(kGatewareTextName));
  layout->addWidget(gateware_text_);

  gateware_start_ = new QPushButton(tr("Write the original gateware"), page);
  gateware_start_->setObjectName(QLatin1String(kGatewareStartButtonName));
  connect(gateware_start_, &QPushButton::clicked, this,
          &LegacyRollbackWizard::StartGateware);
  layout->addWidget(gateware_start_);

  gateware_progress_ = new QProgressBar(page);
  gateware_progress_->setObjectName(QLatin1String(kGatewareProgressName));
  gateware_progress_->setRange(0, 100);
  layout->addWidget(gateware_progress_);

  gateware_status_ = MakeBody(page);
  gateware_status_->setObjectName(QLatin1String(kGatewareStatusName));
  layout->addWidget(gateware_status_);

  stop_button_ = new QPushButton(tr("Stop"), page);
  stop_button_->setObjectName(QLatin1String(kStopButtonName));
  stop_button_->setEnabled(false);
  connect(stop_button_, &QPushButton::clicked, this,
          &LegacyRollbackWizard::Stop);
  layout->addWidget(stop_button_);

  layout->addStretch(1);
  return area;
}

QWidget* LegacyRollbackWizard::BuildFirmwarePage() {
  QWidget* page = nullptr;
  QWidget* const area = MakePage(&page);
  auto* layout = new QVBoxLayout(page);

  firmware_text_ = MakeBody(page, RollbackFirmwareText(0));
  firmware_text_->setObjectName(QLatin1String(kFirmwareTextName));
  layout->addWidget(firmware_text_);

  firmware_start_ = new QPushButton(tr("Write the original firmware"), page);
  firmware_start_->setObjectName(QLatin1String(kFirmwareStartButtonName));
  connect(firmware_start_, &QPushButton::clicked, this,
          &LegacyRollbackWizard::StartFirmware);
  layout->addWidget(firmware_start_);

  firmware_progress_ = new QProgressBar(page);
  firmware_progress_->setObjectName(QLatin1String(kFirmwareProgressName));
  firmware_progress_->setRange(0, 100);
  layout->addWidget(firmware_progress_);

  firmware_status_ = MakeBody(page);
  firmware_status_->setObjectName(QLatin1String(kFirmwareStatusName));
  layout->addWidget(firmware_status_);

  layout->addStretch(1);
  return area;
}

QWidget* LegacyRollbackWizard::BuildPowerCyclePage() {
  QWidget* page = nullptr;
  QWidget* const area = MakePage(&page);
  auto* layout = new QVBoxLayout(page);

  QLabel* const text = MakeBody(page, RollbackPowerCycleText());
  text->setObjectName(QLatin1String(kPowerCycleTextName));
  layout->addWidget(text);

  power_cycle_status_ = MakeBody(page);
  power_cycle_status_->setObjectName(QLatin1String(kPowerCycleStatusName));
  layout->addWidget(power_cycle_status_);

  layout->addStretch(1);
  return area;
}

QWidget* LegacyRollbackWizard::BuildVerifyPage() {
  QWidget* page = nullptr;
  QWidget* const area = MakePage(&page);
  auto* layout = new QVBoxLayout(page);

  verify_summary_ = MakeHeading(page, QString());
  verify_summary_->setObjectName(QLatin1String(kVerifySummaryName));
  layout->addWidget(verify_summary_);

  verify_ = MakeBody(page);
  verify_->setObjectName(QLatin1String(kVerifyTextName));
  layout->addWidget(verify_);

  layout->addStretch(1);
  return area;
}

// --- navigation -----------------------------------------------------------

std::vector<RollbackPage> LegacyRollbackWizard::Steps() const {
  return std::vector<RollbackPage>(std::begin(kPages), std::end(kPages));
}

std::optional<RollbackPage> LegacyRollbackWizard::After(
    RollbackPage page) const {
  const int index = IndexOf(page);
  if (index + 1 >= static_cast<int>(std::size(kPages))) {
    return std::nullopt;
  }
  return kPages[index + 1];
}

std::optional<RollbackPage> LegacyRollbackWizard::Before(
    RollbackPage page) const {
  const int index = IndexOf(page);
  if (index == 0) {
    return std::nullopt;
  }
  return kPages[index - 1];
}

void LegacyRollbackWizard::ShowPage(RollbackPage page) {
  page_ = page;
  pages_->setCurrentIndex(IndexOf(page));
  heading_->setText(RollbackPageHeading(page));
  step_->setText(RollbackPageTitle(page));

  if (page == RollbackPage::kPowerCycle) {
    power_cycle_polls_ = 0;
  }
  if (page == RollbackPage::kVerify) {
    Verify();
  }

  Refresh();
}

void LegacyRollbackWizard::GoNext() {
  if (RefuseWhileRunning() || !PageIsSatisfied(page_)) {
    return;
  }
  const std::optional<RollbackPage> next = After(page_);
  if (next.has_value()) {
    ShowPage(*next);
  }
}

void LegacyRollbackWizard::GoPrevious() {
  if (RefuseWhileRunning()) {
    return;
  }
  const std::optional<RollbackPage> previous = Before(page_);
  if (previous.has_value()) {
    ShowPage(*previous);
  }
}

bool LegacyRollbackWizard::PageIsSatisfied(RollbackPage page) const {
  switch (page) {
    case RollbackPage::kOverview:
      return confirmed_;
    case RollbackPage::kConnect:
      return RollbackDeviceIsReady(Device(), ReadIdentity());
    case RollbackPage::kImage:
      return manifest_.has_value() &&
             RollbackImageProblem(*manifest_).isEmpty();
    case RollbackPage::kGateware:
      return gateware_done_;
    case RollbackPage::kFirmware:
      return firmware_done_;
    case RollbackPage::kPowerCycle:
      return returned_;
    case RollbackPage::kVerify:
      return true;
  }
  return false;
}

void LegacyRollbackWizard::Refresh() {
  const bool running = busy();

  previous_button_->setEnabled(!running && Before(page_).has_value());
  next_button_->setEnabled(!running && PageIsSatisfied(page_) &&
                           After(page_).has_value());

  if (gateware_start_ != nullptr) {
    gateware_start_->setEnabled(!running && !gateware_done_ &&
                                manifest_.has_value());
  }
  if (firmware_start_ != nullptr) {
    // The ordering rule, in the interface as well as in the orchestrator: the
    // FPGA first, always. The orchestrator would refuse anyway — this is what
    // stops a user being offered something that will be refused.
    firmware_start_->setEnabled(!running && gateware_done_ && !firmware_done_);
  }
  if (stop_button_ != nullptr) {
    stop_button_->setEnabled(running);
  }
}

bool LegacyRollbackWizard::RefuseWhileRunning() { return busy(); }

// --- the bus --------------------------------------------------------------

void LegacyRollbackWizard::Poll() { ReadDevices(); }

std::optional<capture::DeviceInfo> LegacyRollbackWizard::Device() const {
  for (const capture::DeviceInfo& device : devices_) {
    if (device.personality == capture::DevicePersonality::kApplication ||
        device.personality == capture::DevicePersonality::kLegacy ||
        device.personality == capture::DevicePersonality::kRecovery) {
      return device;
    }
  }
  return std::nullopt;
}

std::optional<capture::DeviceIdentity> LegacyRollbackWizard::ReadIdentity()
    const {
  const std::optional<capture::DeviceInfo> device = Device();
  if (!device.has_value() || !access_.open_updater) {
    return std::nullopt;
  }

  // Only a device running its own firmware has an identity to read; asking a
  // legacy one would be opening a device this application cannot drive.
  if (device->personality != capture::DevicePersonality::kApplication) {
    return std::nullopt;
  }

  const std::unique_ptr<capture::IDeviceUpdater> updater =
      access_.open_updater(device->path);
  if (updater == nullptr) {
    return std::nullopt;
  }
  return updater->ReadIdentity();
}

void LegacyRollbackWizard::ReadDevices() {
  devices_ =
      access_.devices ? access_.devices() : std::vector<capture::DeviceInfo>();

  if (page_ == RollbackPage::kConnect && device_row_ != nullptr) {
    device_row_->setText(
        RowMarkup(RollbackDeviceRow(Device(), ReadIdentity())));
  }

  if (page_ == RollbackPage::kPowerCycle) {
    const std::optional<capture::DeviceInfo> device = Device();
    const bool legacy =
        device.has_value() &&
        device->personality == capture::DevicePersonality::kLegacy;

    if (legacy) {
      returned_ = true;
      power_cycle_status_->setText(
          tr("<p><b>The unit has come back as a legacy Duplicator.</b></p>"));
    } else {
      ++power_cycle_polls_;
      power_cycle_status_->setText(
          power_cycle_polls_ > kPowerCyclePatiencePolls
              ? RollbackPowerCycleTimeoutText()
              : tr("<p>Waiting for the unit to come back…</p>"));
    }
  }

  Refresh();
}

// --- the file -------------------------------------------------------------

void LegacyRollbackWizard::LoadRollbackSet(const QString& path) {
  archive_.clear();
  manifest_.reset();
  chosen_path_.clear();

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    image_->setText(tr("<p>That file could not be read.</p>"));
    Refresh();
    return;
  }

  const QByteArray bytes = file.readAll();
  std::vector<uint8_t> archive(bytes.begin(), bytes.end());

  std::string error;
  const std::optional<capture::UpdateBundle> bundle =
      capture::OpenUpdateBundleForPolicy(archive, policy_, &error);
  if (!bundle.has_value()) {
    image_->setText(QStringLiteral("<p>") +
                    QString::fromStdString(error).toHtmlEscaped() +
                    QStringLiteral("</p>"));
    Refresh();
    return;
  }

  const QString problem = RollbackImageProblem(bundle->manifest);
  if (!problem.isEmpty()) {
    image_->setText(QStringLiteral("<p>") + problem + QStringLiteral("</p>"));
    Refresh();
    return;
  }

  archive_ = std::move(archive);
  manifest_ = bundle->manifest;
  chosen_path_ = path;

  image_->setText(RollbackImageSummary(*manifest_));

  // The development-channel banner, said every time and never once: a
  // development signature proves the file's format and nothing about where it
  // came from, and this is the flow where that matters most.
  const bool development =
      manifest_->channel == capture::UpdateChannel::kDevelopment;
  image_banner_->setVisible(development);
  if (development) {
    image_banner_->setText(DevelopmentBundleBanner());
  }

  if (manifest_->factory_gateware.has_value()) {
    gateware_text_->setText(RollbackGatewareText(static_cast<int>(
        capture::EstimateComponentSeconds(capture::UpdateTarget::kEpcsFactory,
                                          *manifest_->factory_gateware))));
  }
  if (manifest_->firmware.has_value()) {
    firmware_text_->setText(
        RollbackFirmwareText(static_cast<int>(capture::EstimateComponentSeconds(
            capture::UpdateTarget::kFirmware, *manifest_->firmware))));
  }

  Refresh();
}

// --- the two halves -------------------------------------------------------

void LegacyRollbackWizard::StartGateware() {
  if (busy() || !manifest_.has_value() || gateware_done_) {
    return;
  }
  StartTask(static_cast<int>(RollbackWorker::Task::kGateware));
}

void LegacyRollbackWizard::StartFirmware() {
  if (busy() || !manifest_.has_value() || !gateware_done_ || firmware_done_) {
    return;
  }
  StartTask(static_cast<int>(RollbackWorker::Task::kFirmware));
}

void LegacyRollbackWizard::StartTask(int task) {
  const auto kind = static_cast<RollbackWorker::Task>(task);
  running_task_ = task;

  if (orchestrator_ == nullptr) {
    const std::optional<capture::DeviceInfo> device = Device();
    if (!device.has_value() || !access_.open_updater) {
      FinishTask(false, false, tr("No Domesday Duplicator is attached."));
      return;
    }

    // Opened once and held across both halves: it is the same device, still
    // running the same firmware, and the orchestrator that carries the
    // ordering rule from one half to the other holds a reference to it.
    device_path_ = device->path;
    updater_ = access_.open_updater(device_path_);
    if (updater_ == nullptr) {
      FinishTask(false, false,
                 tr("The Duplicator could not be opened. Something else may "
                    "have it, or this machine may not have permission."));
      return;
    }

    orchestrator_ =
        std::make_unique<capture::RollbackOrchestrator>(*updater_, nullptr);
  }

  QLabel* const status = kind == RollbackWorker::Task::kGateware
                             ? gateware_status_
                             : firmware_status_;
  QProgressBar* const bar = kind == RollbackWorker::Task::kGateware
                                ? gateware_progress_
                                : firmware_progress_;

  status->setText(tr("<p>Starting…</p>"));
  bar->setValue(0);

  thread_ = new QThread(this);
  worker_ = new RollbackWorker(orchestrator_.get(), kind, archive_, policy_);
  worker_->moveToThread(thread_);

  connect(thread_, &QThread::started, worker_, &RollbackWorker::Run);
  connect(worker_, &RollbackWorker::Progress, this,
          [this, bar, status](int, int, quint64 done, quint64 total,
                              const QString& message) {
            if (total > 0) {
              bar->setValue(static_cast<int>((done * 100) / total));
            }
            status->setText(QStringLiteral("<p>") + message.toHtmlEscaped() +
                            QStringLiteral("</p>"));
          });
  connect(worker_, &RollbackWorker::Finished, this,
          &LegacyRollbackWizard::FinishTask);

  thread_->start();
  emit BusyChanged(true);
  Refresh();
}

void LegacyRollbackWizard::FinishTask(bool succeeded, bool stopped,
                                      const QString& problem) {
  if (thread_ != nullptr) {
    thread_->quit();
    thread_->wait();
    thread_->deleteLater();
    thread_ = nullptr;
  }
  if (worker_ != nullptr) {
    worker_->deleteLater();
    worker_ = nullptr;
  }

  const bool gateware =
      running_task_ == static_cast<int>(RollbackWorker::Task::kGateware);
  QLabel* const status = gateware ? gateware_status_ : firmware_status_;
  QProgressBar* const bar = gateware ? gateware_progress_ : firmware_progress_;

  if (succeeded) {
    if (gateware) {
      gateware_done_ = true;
    } else {
      firmware_done_ = true;
    }
    bar->setValue(100);
    status->setText(
        gateware
            ? tr("<p><b>Written and checked.</b> The FPGA is still running the "
                 "current gateware until the power cycle.</p>")
            : tr("<p><b>Written and checked.</b> Both halves are now on the "
                 "device, and neither is running yet.</p>"));
  } else if (stopped) {
    status->setText(
        tr("<p><b>Stopped.</b> Nothing is running that was not running before: "
           "what has been written does not take effect until the unit is "
           "power-cycled, and this step can simply be run again.</p>"));
  } else {
    status->setText(QStringLiteral("<p><b>") + tr("It stopped.") +
                    QStringLiteral("</b> ") + problem.toHtmlEscaped() +
                    QStringLiteral("</p>"));
  }

  emit BusyChanged(false);
  Refresh();
}

void LegacyRollbackWizard::Stop() {
  if (worker_ != nullptr) {
    worker_->Cancel();
  }
}

// --- the end --------------------------------------------------------------

void LegacyRollbackWizard::Verify() {
  const std::optional<capture::DeviceInfo> device = Device();
  const bool legacy =
      device.has_value() &&
      device->personality == capture::DevicePersonality::kLegacy;

  verify_summary_->setText(RollbackVerifySummary(legacy));
  verify_->setText(RollbackVerifyText(legacy));
}

void LegacyRollbackWizard::reject() {
  if (busy()) {
    return;
  }
  QDialog::reject();
}

void LegacyRollbackWizard::closeEvent(QCloseEvent* event) {
  if (!busy()) {
    QDialog::closeEvent(event);
    return;
  }

  // Closing mid-write is refused rather than confirmed. There is no question
  // worth asking here: the Stop button exists, it stops at a safe point, and a
  // window torn down over a half-written flash is the one outcome this flow
  // must not offer.
  QMessageBox::information(
      this, tr("Rollback in progress"),
      tr("A write is in progress. Use Stop, which finishes what it is doing "
         "and leaves the device in a state you can run this from again."));
  event->ignore();
}

}  // namespace ddd::gui
