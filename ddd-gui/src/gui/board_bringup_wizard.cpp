/************************************************************************

    board_bringup_wizard.cpp

    Programming both halves of a board from nothing, in nine pages
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "board_bringup_wizard.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
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

#include "bringup_worker.h"
#include "update_bundle.h"
#include "update_text.h"
#include "wire_protocol.h"

namespace ddd::gui {
namespace {

// The size this window opens at. Chosen rather than derived, for the reason
// AutoCaptureWizard's is: every page is a scroll area, and a scroll area
// reports a small fixed size hint whatever it contains.
//
// Tall enough to hold a portrait photograph of a jumper and the sentence that
// says what to do with it, because those two together are the whole of the
// pages that matter most.
constexpr int kWantedWidthPixels = 720;
constexpr int kWantedHeightPixels = 800;
constexpr double kMostOfTheScreen = 0.9;

// How often the pages that are waiting for hardware look at the bus.
//
// Half a second: fast enough that plugging a cable back in feels immediate,
// and slow enough that enumerating the bus — which on some machines means
// opening devices — is nowhere near a cost.
constexpr int kPollMilliseconds = 500;

// How long the power-cycle page stays merely patient before it starts being
// helpful. Twenty polls is ten seconds, which is comfortably longer than a
// device takes to enumerate and comfortably shorter than somebody's patience.
constexpr int kPowerCyclePatiencePolls = 20;

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

// A photograph and its caption, or nothing at all when the build has no
// resources compiled in — which is what a test binary that links the library
// without the qrc looks like, and is not worth failing over.
QWidget* MakePhotograph(QWidget* parent, const QString& path,
                        const QString& caption, const char* object_name) {
  auto* holder = new QWidget(parent);
  auto* layout = new QVBoxLayout(holder);
  layout->setContentsMargins(0, 0, 0, 0);

  auto* image = new QLabel(holder);
  image->setObjectName(QLatin1String(object_name));
  image->setAlignment(Qt::AlignCenter);

  const QPixmap pixmap(path);
  if (!pixmap.isNull()) {
    image->setPixmap(pixmap);
  }

  // The caption is the alt text as well as the caption: it says fitted or
  // removed in words, so a page whose picture did not load still says what to
  // do rather than showing an empty box.
  image->setAccessibleName(caption);
  layout->addWidget(image);

  QLabel* const words = MakeBody(holder, caption);
  words->setAlignment(Qt::AlignCenter);
  layout->addWidget(words);

  return holder;
}

// Green tick, amber dash, red cross — as text, because this application does
// not carry an icon set and a coloured word is legible in every theme.
QString RowMarkup(const BringUpStatusRow& row) {
  const char* mark = "✕";
  const char* colour = "#c0392b";
  if (row.state == BringUpRowState::kReady) {
    mark = "✓";
    colour = "#27ae60";
  } else if (row.state == BringUpRowState::kWaiting) {
    mark = "•";
    colour = "#d68910";
  }

  return QStringLiteral("<p><b><span style=\"color:%1\">%2</span> %3</b><br>%4")
             .arg(QLatin1String(colour), QLatin1String(mark),
                  row.title.toHtmlEscaped(), row.detail) +
         QStringLiteral("</p>");
}

}  // namespace

BoardBringUpWizard::BoardBringUpWizard(Access access, QWidget* parent)
    : QDialog(parent),
      access_(std::move(access)),
      policy_(capture::DefaultUpdateKeyPolicy()) {
  setWindowTitle(tr("Bring up a new or legacy board"));

  // Modeless, like the other wizards. Somebody part way through this may well
  // want the log panel open beside it.
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
  pages_->addWidget(BuildJumperPage());
  pages_->addWidget(BuildFirmwarePage());
  pages_->addWidget(BuildRemoveJumperPage());
  pages_->addWidget(BuildGatewarePage());
  pages_->addWidget(BuildPowerCyclePage());
  pages_->addWidget(BuildVerifyPage());
  layout->addWidget(pages_, 1);

  // Close on the left with the width of the window between it and Next, for
  // the reason AutoCaptureWizard does it: the button that leaves and the
  // button that carries on must not be a few pixels apart on a flow where
  // leaving in the middle means starting the physical part again.
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
          &BoardBringUpWizard::GoPrevious);
  connect(next_button_, &QPushButton::clicked, this,
          &BoardBringUpWizard::GoNext);
  connect(close, &QPushButton::clicked, this, &QWidget::close);

  timer_ = new QTimer(this);
  timer_->setInterval(kPollMilliseconds);
  connect(timer_, &QTimer::timeout, this, &BoardBringUpWizard::Poll);
  timer_->start();

  ReadDevices();
  ShowPage(BringUpPage::kOverview);
}

BoardBringUpWizard::~BoardBringUpWizard() {
  if (thread_ != nullptr) {
    // A run in flight when the window is torn down: ask it to stop and wait.
    // Detaching would leave a thread writing to a device — or clocking a
    // cable — through an orchestrator whose owner had gone.
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

namespace {

// Every page is a scroll area: several of them carry a photograph and several
// paragraphs, and a window that could not scroll would be worse on a short
// screen than one that opens large on a tall one.
QScrollArea* MakeScrollingPage(QWidget* parent, QWidget** page_out) {
  auto* scroll = new QScrollArea(parent);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* page = new QWidget(scroll);
  scroll->setWidget(page);
  *page_out = page;
  return scroll;
}

}  // namespace

QWidget* BoardBringUpWizard::BuildOverviewPage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  QLabel* const text = MakeBody(page, BringUpOverviewText());
  text->setObjectName(QLatin1String(kOverviewTextName));
  layout->addWidget(text);

  layout->addWidget(MakeBody(page, BringUpDurationText()));
  layout->addWidget(
      MakePhotograph(page, BringUpPhotographPath(BringUpPage::kOverview),
                     BringUpPhotographCaption(BringUpPage::kOverview),
                     kOverviewPhotographName));

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildConnectPage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeBody(
      page,
      tr("<p>Both boards have to be connected and reachable before anything "
         "starts. Each is <b>opened</b> here rather than merely noticed, so "
         "that a permissions problem turns up now — while there is still "
         "nothing half-programmed — rather than in the middle of writing a "
         "flash.</p>")));

  fx3_row_ = MakeBody(page);
  fx3_row_->setObjectName(QLatin1String(kFx3RowName));
  layout->addWidget(fx3_row_);

  fpga_row_ = MakeBody(page);
  fpga_row_->setObjectName(QLatin1String(kFpgaRowName));
  layout->addWidget(fpga_row_);

  auto* again = new QPushButton(tr("Check again"), page);
  again->setObjectName(QLatin1String(kCheckAgainButtonName));
  connect(again, &QPushButton::clicked, this, [this] {
    ReadDevices();
    ProbeCable(true);
    Refresh();
  });

  auto* row = new QHBoxLayout();
  row->addWidget(again);
  row->addStretch(1);
  layout->addLayout(row);

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildImagePage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeBody(
      page,
      tr("<p>A <b>provisioning set</b> carries the FX3's firmware and the "
         "FPGA's gateware as JTAG vectors. It is an ordinary signed update "
         "file — the same format, the same signature check and the same "
         "digests — and it is published beside the release it belongs to.</p>"
         "<p>Its signature and every digest are checked here, before anything "
         "is programmed.</p>")));

  image_ = MakeBody(page, tr("No provisioning set chosen."));
  image_->setObjectName(QLatin1String(kImageLabelName));
  layout->addWidget(image_);

  image_banner_ = MakeBody(page);
  image_banner_->setObjectName(QLatin1String(kImageBannerName));
  image_banner_->hide();
  layout->addWidget(image_banner_);

  auto* choose = new QPushButton(tr("Choose file…"), page);
  choose->setObjectName(QLatin1String(kChooseButtonName));
  connect(choose, &QPushButton::clicked, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose a provisioning set"), QString(),
        tr("Domesday Duplicator update files (*%1);;All files (*)")
            .arg(QString::fromUtf8(
                capture::kUpdateBundleExtension.data(),
                static_cast<qsizetype>(
                    capture::kUpdateBundleExtension.size()))));
    if (!path.isEmpty()) {
      LoadProvisioningSet(path);
    }
  });

  auto* row = new QHBoxLayout();
  row->addWidget(choose);
  row->addStretch(1);
  layout->addLayout(row);

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildJumperPage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  QLabel* const text = MakeBody(page, BringUpFitJumperText());
  text->setObjectName(QLatin1String(kJumperTextName));
  layout->addWidget(text);

  layout->addWidget(MakePhotograph(
      page, BringUpPhotographPath(BringUpPage::kJumper),
      BringUpPhotographCaption(BringUpPage::kJumper), kJumperPhotographName));

  jumper_status_ = MakeBody(page);
  jumper_status_->setObjectName(QLatin1String(kJumperStatusName));
  layout->addWidget(jumper_status_);

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildFirmwarePage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  QLabel* const text = MakeBody(page, BringUpFirmwareText());
  text->setObjectName(QLatin1String(kFirmwareTextName));
  layout->addWidget(text);

  firmware_status_ = MakeBody(page);
  firmware_status_->setObjectName(QLatin1String(kFirmwareStatusName));
  layout->addWidget(firmware_status_);

  firmware_progress_ = new QProgressBar(page);
  firmware_progress_->setObjectName(QLatin1String(kFirmwareProgressName));
  firmware_progress_->setRange(0, 100);
  firmware_progress_->setValue(0);
  layout->addWidget(firmware_progress_);

  firmware_start_ = new QPushButton(tr("Program the FX3"), page);
  firmware_start_->setObjectName(QLatin1String(kFirmwareStartButtonName));
  connect(firmware_start_, &QPushButton::clicked, this,
          &BoardBringUpWizard::StartFirmware);

  auto* row = new QHBoxLayout();
  row->addWidget(firmware_start_);
  row->addStretch(1);
  layout->addLayout(row);

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildRemoveJumperPage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  QLabel* const text = MakeBody(page, BringUpRemoveJumperText());
  text->setObjectName(QLatin1String(kRemoveJumperTextName));
  layout->addWidget(text);

  // The same board in the same framing as the previous photograph, so the
  // difference the user has to make is the only difference between the two
  // pictures.
  layout->addWidget(
      MakePhotograph(page, BringUpPhotographPath(BringUpPage::kRemoveJumper),
                     BringUpPhotographCaption(BringUpPage::kRemoveJumper),
                     kRemoveJumperPhotographName));

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildGatewarePage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  gateware_text_ = MakeBody(page, BringUpGatewareText(0));
  gateware_text_->setObjectName(QLatin1String(kGatewareTextName));
  layout->addWidget(gateware_text_);

  gateware_status_ = MakeBody(page);
  gateware_status_->setObjectName(QLatin1String(kGatewareStatusName));
  layout->addWidget(gateware_status_);

  gateware_progress_ = new QProgressBar(page);
  gateware_progress_->setObjectName(QLatin1String(kGatewareProgressName));
  gateware_progress_->setRange(0, 100);
  gateware_progress_->setValue(0);
  layout->addWidget(gateware_progress_);

  gateware_start_ = new QPushButton(tr("Program the FPGA"), page);
  gateware_start_->setObjectName(QLatin1String(kGatewareStartButtonName));
  connect(gateware_start_, &QPushButton::clicked, this,
          &BoardBringUpWizard::StartGateware);

  stop_button_ = new QPushButton(tr("Stop"), page);
  stop_button_->setObjectName(QLatin1String(kStopButtonName));
  stop_button_->setEnabled(false);
  connect(stop_button_, &QPushButton::clicked, this, &BoardBringUpWizard::Stop);

  auto* row = new QHBoxLayout();
  row->addWidget(gateware_start_);
  row->addWidget(stop_button_);
  row->addStretch(1);
  layout->addLayout(row);

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildPowerCyclePage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  QLabel* const text = MakeBody(page, BringUpPowerCycleText());
  text->setObjectName(QLatin1String(kPowerCycleTextName));
  layout->addWidget(text);

  power_cycle_status_ = MakeBody(page);
  power_cycle_status_->setObjectName(QLatin1String(kPowerCycleStatusName));
  layout->addWidget(power_cycle_status_);

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildVerifyPage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  verify_ = MakeBody(page);
  verify_->setObjectName(QLatin1String(kVerifyTextName));
  layout->addWidget(verify_);

  verify_summary_ = MakeBody(page);
  verify_summary_->setObjectName(QLatin1String(kVerifySummaryName));
  layout->addWidget(verify_summary_);

  update_now_ = new QPushButton(tr("Update firmware now…"), page);
  update_now_->setObjectName(QLatin1String(kUpdateNowButtonName));
  connect(update_now_, &QPushButton::clicked, this,
          &BoardBringUpWizard::OpenUpdateRequested);

  auto* row = new QHBoxLayout();
  row->addWidget(update_now_);
  row->addStretch(1);
  layout->addLayout(row);

  layout->addStretch(1);
  return scroll;
}

// --- navigation -----------------------------------------------------------

std::vector<BringUpPage> BoardBringUpWizard::Steps() const {
  std::vector<BringUpPage> steps;
  for (BringUpPage page :
       {BringUpPage::kOverview, BringUpPage::kConnect, BringUpPage::kImage,
        BringUpPage::kJumper, BringUpPage::kFirmware,
        BringUpPage::kRemoveJumper, BringUpPage::kGateware,
        BringUpPage::kPowerCycle, BringUpPage::kVerify}) {
    const bool jumper_page =
        page == BringUpPage::kJumper || page == BringUpPage::kRemoveJumper;
    if (jumper_page && !jumper_needed_) {
      continue;
    }
    steps.push_back(page);
  }
  return steps;
}

std::optional<BringUpPage> BoardBringUpWizard::After(BringUpPage page) const {
  const std::vector<BringUpPage> steps = Steps();
  for (size_t index = 0; index + 1 < steps.size(); ++index) {
    if (steps[index] == page) {
      return steps[index + 1];
    }
  }
  return std::nullopt;
}

std::optional<BringUpPage> BoardBringUpWizard::Before(BringUpPage page) const {
  const std::vector<BringUpPage> steps = Steps();
  for (size_t index = 1; index < steps.size(); ++index) {
    if (steps[index] == page) {
      return steps[index - 1];
    }
  }
  return std::nullopt;
}

void BoardBringUpWizard::GoNext() {
  if (busy() || !PageIsSatisfied(page_)) {
    return;
  }

  // Decided here and nowhere else: a board already waiting in its boot ROM
  // needs no jumper, and asking for one would be asking somebody to fit a
  // jumper so that the wizard could ask them to take it off again. Decided on
  // the way *out* of the connectivity page, because that is the last moment
  // the answer is about a board nothing has been done to yet.
  if (page_ == BringUpPage::kConnect) {
    const std::optional<capture::DeviceInfo> fx3 = Fx3();
    jumper_needed_ = !fx3.has_value() ||
                     fx3->personality != capture::DevicePersonality::kRecovery;
  }

  const std::optional<BringUpPage> next = After(page_);
  if (next.has_value()) {
    ShowPage(*next);
  }
}

void BoardBringUpWizard::GoPrevious() {
  if (busy()) {
    return;
  }
  const std::optional<BringUpPage> previous = Before(page_);
  if (previous.has_value()) {
    ShowPage(*previous);
  }
}

void BoardBringUpWizard::ShowPage(BringUpPage page) {
  page_ = page;

  const std::vector<BringUpPage> steps = Steps();
  if (std::find(steps.begin(), steps.end(), page) == steps.end()) {
    // A page this run does not visit, reached by something that should not
    // have asked for it. Sent to the first page it does.
    page_ = steps.front();
    page = page_;
  }

  // The stack holds all nine in the enumeration's order, whichever ones this
  // run visits, so a skipped page is a page nothing navigates to rather than a
  // page that has to be built differently.
  pages_->setCurrentIndex(static_cast<int>(page));

  heading_->setText(BringUpPageHeading(page));

  // Numbered out of nine even on a run that visits seven of them, so that two
  // runs of one procedure can be talked about in the same words.
  step_->setText(BringUpPageTitle(page));

  if (page == BringUpPage::kPowerCycle) {
    power_cycle_polls_ = 0;
  }
  if (page == BringUpPage::kVerify) {
    Verify();
  }

  ProbeCable(page == BringUpPage::kConnect);
  Refresh();
}

bool BoardBringUpWizard::PageIsSatisfied(BringUpPage page) const {
  switch (page) {
    case BringUpPage::kOverview:
      return true;

    case BringUpPage::kConnect: {
      const std::optional<capture::DeviceInfo> fx3 = Fx3();
      const BringUpStatusRow fx3_row = BringUpFx3Row(
          fx3.has_value(),
          fx3.has_value() ? fx3->personality
                          : capture::DevicePersonality::kApplication,
          capture::UsbPresence::kUnknown);
      return fx3_row.usable() && cable_opened_;
    }

    case BringUpPage::kImage:
      return manifest_.has_value() && BringUpImageProblem(*manifest_).isEmpty();

    case BringUpPage::kJumper: {
      const std::optional<capture::DeviceInfo> fx3 = Fx3();
      return fx3.has_value() &&
             fx3->personality == capture::DevicePersonality::kRecovery;
    }

    case BringUpPage::kFirmware:
      return firmware_done_;

    case BringUpPage::kRemoveJumper:
      // Nothing to detect: a jumper is invisible to software. The next page
      // does not touch the FX3 at all, and the power cycle after it is where
      // a jumper left on would show up — with a page that names it.
      return true;

    case BringUpPage::kGateware:
      return gateware_done_;

    case BringUpPage::kPowerCycle:
      return returned_;

    case BringUpPage::kVerify:
      return false;
  }
  return false;
}

void BoardBringUpWizard::Refresh() {
  const bool running = busy();

  previous_button_->setEnabled(!running && Before(page_).has_value());
  next_button_->setEnabled(!running && PageIsSatisfied(page_) &&
                           After(page_).has_value());
  next_button_->setVisible(page_ != BringUpPage::kVerify);

  if (firmware_start_ != nullptr) {
    firmware_start_->setEnabled(!running && !firmware_done_ &&
                                manifest_.has_value());
  }
  if (gateware_start_ != nullptr) {
    gateware_start_->setEnabled(!running && !gateware_done_ && firmware_done_ &&
                                manifest_.has_value());
  }
  if (stop_button_ != nullptr) {
    stop_button_->setEnabled(running);
  }

  // --- what each waiting page is showing ---------------------------------

  const std::optional<capture::DeviceInfo> fx3 = Fx3();

  // Only while the connectivity page is the page in hand. Asking the bus about
  // the kit's debug bridge is an enumeration, and doing one twice a second for
  // a row nobody is looking at would be a cost for nothing — and a cost paid
  // next to a device that is being written to.
  if (fx3_row_ != nullptr && page_ == BringUpPage::kConnect) {
    const capture::UsbPresence bridge =
        access_.presence
            ? access_.presence(capture::kCypressDebugBridgeVendorId,
                               capture::kCypressDebugBridgeProductId)
            : capture::UsbPresence::kUnknown;
    fx3_row_->setText(RowMarkup(BringUpFx3Row(
        fx3.has_value(),
        fx3.has_value() ? fx3->personality
                        : capture::DevicePersonality::kApplication,
        bridge)));
  }

  if (fpga_row_ != nullptr && page_ == BringUpPage::kConnect) {
    fpga_row_->setText(RowMarkup(
        BringUpFpgaRow(cable_opened_, cable_presence_, cable_problem_)));
  }

  if (jumper_status_ != nullptr) {
    jumper_status_->setText(
        PageIsSatisfied(BringUpPage::kJumper)
            ? tr("<p><b>The FX3 is in its boot ROM.</b> Ready for the next "
                 "step.</p>")
            : tr("<p>Waiting for the board to come back in its boot ROM…</p>"));
  }

  if (power_cycle_status_ != nullptr) {
    if (returned_) {
      power_cycle_status_->setText(
          tr("<p><b>The Duplicator is back.</b> Ready for the last step.</p>"));
    } else if (power_cycle_polls_ > kPowerCyclePatiencePolls) {
      power_cycle_status_->setText(BringUpPowerCycleTimeoutText());
    } else {
      power_cycle_status_->setText(tr("<p>Waiting for it to come back…</p>"));
    }
  }
}

// --- what is on the bus ---------------------------------------------------

std::optional<capture::DeviceInfo> BoardBringUpWizard::Fx3() const {
  // Any personality, unlike almost everywhere else in the application: a
  // device that can do nothing at all is exactly the device this window
  // exists to repair.
  const capture::DeviceInfo* const found = capture::SelectDevice(
      devices_, std::string(), capture::DeviceSelection::kAny);
  if (found == nullptr) {
    return std::nullopt;
  }
  return *found;
}

void BoardBringUpWizard::ReadDevices() {
  devices_ =
      access_.devices ? access_.devices() : std::vector<capture::DeviceInfo>{};
}

void BoardBringUpWizard::ProbeCable(bool force) {
  const capture::UsbPresence presence =
      access_.presence ? access_.presence(capture::kAlteraVendorId,
                                          capture::kUsbBlasterProductId)
                       : capture::UsbPresence::kUnknown;
  cable_presence_ = presence;

  // Opening the cable claims an interface and resets an FTDI chip, so it is
  // not something to do five times a second on the off chance. It is done
  // when the answer might have changed — the presence moved, or somebody
  // pressed Check again — and remembered in between.
  if (!force && presence == probed_presence_) {
    return;
  }
  probed_presence_ = presence;

  cable_opened_ = false;
  cable_problem_.clear();

  if (!access_.open_cable) {
    return;
  }

  std::string problem;
  const std::unique_ptr<capture::IJtagCable> cable =
      access_.open_cable(&problem);
  cable_opened_ = cable != nullptr;
  cable_problem_ = QString::fromStdString(problem);

  // And closed again immediately. Holding it open for the minutes between
  // this page and the one that programs would keep Quartus's jtagd — and
  // anything else on the bench — locked out for no reason.
}

void BoardBringUpWizard::Poll() {
  if (busy()) {
    // The device is being written to. Enumerating it in the middle of that is
    // exactly what DeviceMonitor::SetSuspended exists to prevent.
    return;
  }

  ReadDevices();

  if (page_ == BringUpPage::kConnect) {
    ProbeCable(false);
  }

  if (page_ == BringUpPage::kPowerCycle) {
    ++power_cycle_polls_;
    const std::optional<capture::DeviceInfo> fx3 = Fx3();
    if (fx3.has_value() &&
        fx3->personality == capture::DevicePersonality::kApplication) {
      returned_ = true;
    }
  }

  Refresh();
}

// --- the two halves -------------------------------------------------------

void BoardBringUpWizard::StartFirmware() {
  if (busy() || firmware_done_ || !manifest_.has_value()) {
    return;
  }
  StartTask(static_cast<int>(BringUpWorker::Task::kFirmware));
}

void BoardBringUpWizard::StartGateware() {
  if (busy() || gateware_done_ || !firmware_done_ || !manifest_.has_value()) {
    return;
  }
  StartTask(static_cast<int>(BringUpWorker::Task::kGateware));
}

void BoardBringUpWizard::StartTask(int task) {
  const auto kind = static_cast<BringUpWorker::Task>(task);
  running_task_ = task;

  if (orchestrator_ == nullptr) {
    // Built once, at the moment the first half starts, and kept: it is what
    // carries the ordering rule across the page on which the user takes the
    // jumper off.
    const std::optional<capture::DeviceInfo> fx3 = Fx3();
    const std::string path = fx3.has_value() ? fx3->path : std::string();

    capture::ProvisioningAccess access;
    if (access_.open_programmer) {
      access.fx3.open_programmer = [this, path] {
        return access_.open_programmer(path);
      };
    }
    if (access_.open_updater) {
      access.fx3.open_updater = [this, path](const std::string& found) {
        // An empty path means the device this started on; a path is given
        // when the device has just been woken, because on Windows it comes
        // back at a different one.
        return access_.open_updater(found.empty() ? path : found);
      };
    }
    access.open_cable = access_.open_cable;

    orchestrator_ = std::make_unique<capture::ProvisioningOrchestrator>(
        std::move(access), nullptr);
  }

  QLabel* const status = kind == BringUpWorker::Task::kFirmware
                             ? firmware_status_
                             : gateware_status_;
  QProgressBar* const bar = kind == BringUpWorker::Task::kFirmware
                                ? firmware_progress_
                                : gateware_progress_;

  status->setText(tr("<p>Starting…</p>"));
  bar->setValue(0);

  thread_ = new QThread(this);
  worker_ = new BringUpWorker(orchestrator_.get(), kind, archive_, policy_);
  worker_->moveToThread(thread_);

  connect(thread_, &QThread::started, worker_, &BringUpWorker::Run);
  connect(worker_, &BringUpWorker::Progress, this,
          [this, status, bar](int, int, quint64 done, quint64 total,
                              const QString& message) {
            if (total > 0) {
              bar->setValue(static_cast<int>((done * 100) / total));
            }
            status->setText(
                QStringLiteral("<p>%1</p>").arg(message.toHtmlEscaped()));
          });
  connect(worker_, &BringUpWorker::Finished, this,
          &BoardBringUpWizard::FinishTask);

  Refresh();
  emit BusyChanged(true);
  thread_->start();
}

void BoardBringUpWizard::FinishTask(bool succeeded, bool stopped,
                                    const QString& problem) {
  const bool firmware =
      running_task_ == static_cast<int>(BringUpWorker::Task::kFirmware);

  if (thread_ != nullptr) {
    thread_->quit();
    thread_->wait();
    thread_->deleteLater();
    thread_ = nullptr;
  }
  worker_->deleteLater();
  worker_ = nullptr;

  QLabel* const status = firmware ? firmware_status_ : gateware_status_;
  QProgressBar* const bar = firmware ? firmware_progress_ : gateware_progress_;

  if (succeeded) {
    if (firmware) {
      firmware_done_ = true;
      status->setText(
          tr("<p><b>The FX3's firmware is written and checked.</b> It has "
             "deliberately not been restarted — the jumper is still "
             "fitted.</p>"));
    } else {
      gateware_done_ = true;
      status->setText(
          tr("<p><b>The FPGA's flash is written.</b> It will start using it "
             "at the next power cycle, which is the next page.</p>"));
    }
    bar->setValue(100);
  } else if (stopped) {
    status->setText(BringUpStoppedText());
  } else {
    status->setText(BringUpFailureText(problem));
  }

  Refresh();
  emit BusyChanged(false);
}

void BoardBringUpWizard::Stop() {
  if (worker_ != nullptr) {
    worker_->Cancel();
    stop_button_->setEnabled(false);
  }
}

// --- the last page --------------------------------------------------------

void BoardBringUpWizard::Verify() {
  const std::optional<capture::DeviceInfo> fx3 = Fx3();

  capture::DeviceIdentity identity;
  if (fx3.has_value() && access_.open_updater) {
    const std::unique_ptr<capture::IDeviceUpdater> updater =
        access_.open_updater(fx3->path);
    if (updater != nullptr) {
      identity = updater->ReadIdentity().value_or(capture::DeviceIdentity{});
    }
  }

  const QString expected =
      manifest_.has_value() && manifest_->firmware.has_value()
          ? QString::fromStdString(manifest_->firmware->identity)
          : QString();

  const std::vector<BringUpCheck> checks = BringUpVerification(
      fx3.has_value(),
      fx3.has_value() ? fx3->personality
                      : capture::DevicePersonality::kApplication,
      identity, expected);

  QString text = QStringLiteral("<p>");
  bool all_passed = true;
  for (const BringUpCheck& check : checks) {
    all_passed = all_passed && check.passed;
    text += QStringLiteral("<span style=\"color:%1\">%2</span> %3<br>")
                .arg(QLatin1String(check.passed ? "#27ae60" : "#c0392b"),
                     QLatin1String(check.passed ? "✓" : "✕"),
                     check.description.toHtmlEscaped());
  }
  text += QStringLiteral("</p>");

  verify_->setText(text);
  verify_summary_->setText(all_passed ? BringUpCompleteText()
                                      : BringUpIncompleteText());
}

// --- the provisioning set -------------------------------------------------

void BoardBringUpWizard::LoadProvisioningSet(const QString& path) {
  archive_.clear();
  manifest_.reset();
  image_banner_->hide();

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    image_->setText(tr("That file could not be read."));
    Refresh();
    return;
  }

  const QByteArray bytes = file.readAll();
  archive_.assign(bytes.begin(), bytes.end());

  std::string error;
  const std::optional<capture::UpdateBundle> bundle =
      capture::OpenUpdateBundleForPolicy(archive_, policy_, &error);
  if (!bundle.has_value()) {
    archive_.clear();
    image_->setText(QStringLiteral("<p>%1</p>")
                        .arg(QString::fromStdString(error).toHtmlEscaped()));
    Refresh();
    return;
  }

  manifest_ = bundle->manifest;

  QString summary = BringUpImageSummary(*manifest_);
  const QString problem = BringUpImageProblem(*manifest_);
  if (!problem.isEmpty()) {
    summary += QStringLiteral("<br><br><b>") + problem + QStringLiteral("</b>");
  }
  image_->setText(summary);

  // The same banner the update page shows, from the same function: a
  // development signature proves the file is well formed and proves nothing
  // whatever about where it came from, and that has to be said every time.
  if (manifest_->channel == capture::UpdateChannel::kDevelopment) {
    image_banner_->setText(DevelopmentBundleBanner());
    image_banner_->show();
  }

  if (manifest_->provisioning.has_value()) {
    gateware_text_->setText(BringUpGatewareText(
        capture::EstimateProvisioningSeconds(manifest_->provisioning->length)));
  }

  Refresh();
}

// --- leaving --------------------------------------------------------------

bool BoardBringUpWizard::RefuseWhileRunning() {
  if (!busy()) {
    return false;
  }

  QMessageBox::information(
      this, tr("Programming in progress"),
      tr("A board is being programmed. Press Stop and wait for it to finish "
         "before closing this window."));
  return true;
}

void BoardBringUpWizard::closeEvent(QCloseEvent* event) {
  if (RefuseWhileRunning()) {
    event->ignore();
    return;
  }
  QDialog::closeEvent(event);
}

void BoardBringUpWizard::reject() {
  if (RefuseWhileRunning()) {
    return;
  }
  QDialog::reject();
}

}  // namespace ddd::gui
