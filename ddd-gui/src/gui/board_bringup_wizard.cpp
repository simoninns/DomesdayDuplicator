/************************************************************************

    board_bringup_wizard.cpp

    Programming a board from nothing to fully up to date, in nine pages
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

// Green tick, amber dot, red cross — as text, because this application does
// not carry an icon set and a coloured word is legible in every theme. Which
// mark and which colour is decided in bringup_text, beside the legend that
// tells a user what they mean, so the two cannot drift apart.
QString RowMarkup(const BringUpStatusRow& row) {
  return QStringLiteral("<p><b><span style=\"color:%1\">%2</span> %3</b><br>%4")
             .arg(BringUpMarkColour(row.state), BringUpMark(row.state),
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
  pages_->addWidget(BuildConfigurePage());
  pages_->addWidget(BuildProgramPage());
  pages_->addWidget(BuildRemoveJumperPage());
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

  // Where this build's own update file is, asked for once. A packaged build
  // carries one so that a board can be brought up on a machine with no network
  // — the ordinary case, since a board being brought up is one that cannot be
  // updated over USB.
  //
  // Only *where*, here. Reading and verifying it waits until the image page,
  // because the key policy this window judges signatures by is set after it is
  // constructed, and a file checked against the default policy and then never
  // rechecked would be judged by rules the application had not finished
  // choosing.
  if (access_.bundled_file) {
    bundled_path_ = access_.bundled_file();
  }

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
      tr("<p><b>Connect both boards now</b> — the kit's USB 3.0 cable and the "
         "DE0-Nano's mini-USB — and leave both connected until the end.</p>"
         "<p>Each is <b>opened</b> here rather than merely noticed, so that a "
         "permissions problem turns up now rather than in the middle of "
         "writing a flash.</p>")));

  fx3_row_ = MakeBody(page);
  fx3_row_->setObjectName(QLatin1String(kFx3RowName));
  layout->addWidget(fx3_row_);

  fpga_row_ = MakeBody(page);
  fpga_row_->setObjectName(QLatin1String(kFpgaRowName));
  layout->addWidget(fpga_row_);

  // Under the rows rather than above them: it explains marks somebody has
  // already looked at, and the amber one in particular, which reads as a fault
  // and usually is not.
  QLabel* const legend = MakeBody(page, BringUpConnectLegend());
  legend->setObjectName(QLatin1String(kConnectLegendName));
  layout->addWidget(legend);

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

  connect_status_ = MakeBody(page);
  connect_status_->setObjectName(QLatin1String(kConnectStatusName));
  layout->addWidget(connect_status_);

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildImagePage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(MakeBody(
      page,
      tr("<p><b>Choose the update file to program the board with.</b> It is "
         "the same signed file <b>Tools ▸ Firmware ▸ Update firmware…</b> "
         "installs, published with the release it belongs to.</p>"
         "<p>Bringing a board up needs all four of its payloads, rather than "
         "the two a working device takes, so a file that can update but not "
         "bring up is refused here and named below. Its signature and every "
         "digest are checked before anything is programmed.</p>")));

  // Which of the three states the page is in — bundled, chosen over a bundled
  // one, or nothing bundled at all — said in words above the set itself.
  image_source_ = MakeBody(page);
  image_source_->setObjectName(QLatin1String(kImageSourceName));
  layout->addWidget(image_source_);

  image_ = MakeBody(page, tr("No update file chosen."));
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
        this, tr("Choose an update file"), QString(),
        tr("Domesday Duplicator update files (*%1);;All files (*)")
            .arg(QString::fromUtf8(
                capture::kUpdateBundleExtension.data(),
                static_cast<qsizetype>(
                    capture::kUpdateBundleExtension.size()))));
    if (!path.isEmpty()) {
      LoadUpdateFile(path);
    }
  });

  // Only ever shown when there is one to go back to, so it is never an offer
  // to do something this build cannot do.
  use_bundled_ = new QPushButton(tr("Use the bundled file"), page);
  use_bundled_->setObjectName(QLatin1String(kUseBundledButtonName));
  use_bundled_->hide();
  connect(use_bundled_, &QPushButton::clicked, this, [this] {
    if (!bundled_path_.isEmpty()) {
      LoadUpdateFile(bundled_path_);
    }
  });

  auto* row = new QHBoxLayout();
  row->addWidget(choose);
  row->addWidget(use_bundled_);
  row->addStretch(1);
  layout->addLayout(row);

  image_status_ = MakeBody(page);
  image_status_->setObjectName(QLatin1String(kImageStatusName));
  layout->addWidget(image_status_);

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

  // Above the photograph rather than below it. The picture is portrait and
  // fills the page, so a status line under it is a status line somebody has to
  // scroll to find — on the one page where what they are waiting for is a
  // board that has just been unplugged and plugged back in.
  jumper_status_ = MakeBody(page);
  jumper_status_->setObjectName(QLatin1String(kJumperStatusName));
  layout->addWidget(jumper_status_);

  layout->addWidget(MakePhotograph(
      page, BringUpPhotographPath(BringUpPage::kJumper),
      BringUpPhotographCaption(BringUpPage::kJumper), kJumperPhotographName));

  layout->addStretch(1);
  return scroll;
}

QWidget* BoardBringUpWizard::BuildConfigurePage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  configure_text_ = MakeBody(page, BringUpConfigureText(0));
  configure_text_->setObjectName(QLatin1String(kConfigureTextName));
  layout->addWidget(configure_text_);

  configure_progress_ = new QProgressBar(page);
  configure_progress_->setObjectName(QLatin1String(kConfigureProgressName));
  configure_progress_->setRange(0, 100);
  configure_progress_->setValue(0);
  layout->addWidget(configure_progress_);

  configure_start_ = new QPushButton(tr("Load the gateware"), page);
  configure_start_->setObjectName(QLatin1String(kConfigureStartButtonName));
  connect(configure_start_, &QPushButton::clicked, this,
          &BoardBringUpWizard::StartConfigure);

  configure_stop_ = new QPushButton(tr("Stop"), page);
  configure_stop_->setObjectName(QLatin1String(kConfigureStopButtonName));
  configure_stop_->setEnabled(false);
  connect(configure_stop_, &QPushButton::clicked, this,
          &BoardBringUpWizard::Stop);

  auto* row = new QHBoxLayout();
  row->addWidget(configure_start_);
  row->addWidget(configure_stop_);
  row->addStretch(1);
  layout->addLayout(row);

  // Under the buttons, which is where somebody is looking a moment after
  // pressing one — and where the answer to "did that work?" belongs.
  configure_status_ = MakeBody(page);
  configure_status_->setObjectName(QLatin1String(kConfigureStatusName));
  layout->addWidget(configure_status_);

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

  // The one page with nothing to wait for, and it says so rather than leaving
  // a live Next button to mean either "carry on" or "the wizard has seen
  // something". A jumper is invisible to software: there is nothing here that
  // could confirm it, and pretending otherwise would be worse than saying so.
  QLabel* const status = MakeBody(
      page, BringUpWaitingText(
                tr("you to remove the jumper. Nothing here can detect it, so "
                   "click <b>Next ›</b> once it is off.")));
  status->setObjectName(QLatin1String(kRemoveJumperStatusName));
  layout->addWidget(status);

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

QWidget* BoardBringUpWizard::BuildProgramPage() {
  QWidget* page = nullptr;
  QScrollArea* const scroll = MakeScrollingPage(this, &page);
  auto* layout = new QVBoxLayout(page);

  program_text_ = MakeBody(page, BringUpProgramText(0));
  program_text_->setObjectName(QLatin1String(kProgramTextName));
  layout->addWidget(program_text_);

  program_progress_ = new QProgressBar(page);
  program_progress_->setObjectName(QLatin1String(kProgramProgressName));
  program_progress_->setRange(0, 100);
  program_progress_->setValue(0);
  layout->addWidget(program_progress_);

  program_start_ = new QPushButton(tr("Program the board"), page);
  program_start_->setObjectName(QLatin1String(kProgramStartButtonName));
  connect(program_start_, &QPushButton::clicked, this,
          &BoardBringUpWizard::StartProgram);

  program_stop_ = new QPushButton(tr("Stop"), page);
  program_stop_->setObjectName(QLatin1String(kProgramStopButtonName));
  program_stop_->setEnabled(false);
  connect(program_stop_, &QPushButton::clicked, this,
          &BoardBringUpWizard::Stop);

  auto* row = new QHBoxLayout();
  row->addWidget(program_start_);
  row->addWidget(program_stop_);
  row->addStretch(1);
  layout->addLayout(row);

  program_status_ = MakeBody(page);
  program_status_->setObjectName(QLatin1String(kProgramStatusName));
  layout->addWidget(program_status_);

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

  // No button. There is nothing to hand over to: this page is reached with the
  // board already running what the file carried, and offering to open the
  // update window here would suggest one more step that does not exist.
  layout->addStretch(1);
  return scroll;
}

// --- navigation -----------------------------------------------------------

std::vector<BringUpPage> BoardBringUpWizard::Steps() const {
  std::vector<BringUpPage> steps;
  for (BringUpPage page :
       {BringUpPage::kOverview, BringUpPage::kConnect, BringUpPage::kImage,
        BringUpPage::kJumper, BringUpPage::kConfigure, BringUpPage::kProgram,
        BringUpPage::kRemoveJumper, BringUpPage::kPowerCycle,
        BringUpPage::kVerify}) {
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

  if (page == BringUpPage::kImage) {
    LoadBundledFileOnce();
  }
  if (page == BringUpPage::kPowerCycle) {
    power_cycle_polls_ = 0;
    power_cycle_state_ = PowerCycleState::kStillHere;
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
      const BringUpStatusRow fx3_row =
          BringUpFx3Row(fx3, capture::UsbPresence::kUnknown);
      return fx3_row.usable() && cable_opened_;
    }

    case BringUpPage::kImage:
      return manifest_.has_value() && BringUpImageProblem(*manifest_).isEmpty();

    case BringUpPage::kJumper: {
      const std::optional<capture::DeviceInfo> fx3 = Fx3();
      return fx3.has_value() &&
             fx3->personality == capture::DevicePersonality::kRecovery;
    }

    case BringUpPage::kConfigure:
      return configured_;

    case BringUpPage::kProgram:
      return programmed_;

    case BringUpPage::kRemoveJumper:
      // Nothing to detect: a jumper is invisible to software. The power cycle
      // on the next page is where a jumper left on shows up — with a page that
      // names it.
      return true;

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

  // A step that has been done takes its own button away and says what it now
  // is. Greyed out on its own reads as "not yet"; greyed out and relabelled
  // reads as "already", which is the difference somebody is trying to work out
  // when they look at this page a second time.
  if (configure_start_ != nullptr) {
    configure_start_->setEnabled(!running && !configured_ &&
                                 manifest_.has_value());
    configure_start_->setText(configured_ ? tr("Gateware loaded")
                                          : tr("Load the gateware"));
  }

  // The ordering rule, in the one place a user meets it: the button that
  // writes is dead until the FPGA has been configured. The orchestrator
  // refuses it too — this is the half that stops it being offered.
  if (program_start_ != nullptr) {
    program_start_->setEnabled(!running && !programmed_ && configured_ &&
                               manifest_.has_value());
    program_start_->setText(programmed_ ? tr("Board programmed")
                                        : tr("Program the board"));
  }
  if (configure_stop_ != nullptr) {
    configure_stop_->setEnabled(running);
  }
  if (program_stop_ != nullptr) {
    program_stop_->setEnabled(running);
  }

  if (image_source_ != nullptr) {
    if (bundled_path_.isEmpty()) {
      image_source_->setText(BringUpNoBundledFileText());
    } else if (using_bundled_file()) {
      image_source_->setText(manifest_.has_value()
                                 ? BringUpBundledFileText()
                                 : BringUpBundledFileUnusableText());
    } else {
      image_source_->setText(BringUpChosenFileText());
    }
  }
  if (use_bundled_ != nullptr) {
    use_bundled_->setVisible(!bundled_path_.isEmpty() && !using_bundled_file());
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
    fx3_row_->setText(RowMarkup(BringUpFx3Row(fx3, bridge)));
  }

  if (fpga_row_ != nullptr && page_ == BringUpPage::kConnect) {
    fpga_row_->setText(RowMarkup(
        BringUpFpgaRow(cable_opened_, cable_presence_, cable_problem_)));
  }

  if (connect_status_ != nullptr) {
    connect_status_->setText(
        PageIsSatisfied(BringUpPage::kConnect)
            ? BringUpStepDoneText(tr("Both boards were found and opened."))
            // Named as the red cross rather than "whatever is marked": an
            // amber mark above is a board the wizard is happy with and will
            // come back to, and telling somebody to put it right would send
            // them looking for a fault that is not there.
            : BringUpWaitingText(
                  tr("both boards to be found and opened. Put right anything "
                     "marked with a red cross above, then press <b>Check "
                     "again</b>.")));
  }

  if (image_status_ != nullptr) {
    image_status_->setText(
        PageIsSatisfied(BringUpPage::kImage)
            ? BringUpStepDoneText(
                  tr("The file is signed, intact, and carries everything a "
                     "bring-up needs."))
            : BringUpWaitingText(
                  tr("an update file that can bring a board up. Press "
                     "<b>Choose file…</b> below.")));
  }

  if (jumper_status_ != nullptr) {
    jumper_status_->setText(
        PageIsSatisfied(BringUpPage::kJumper)
            ? BringUpStepDoneText(tr("The FX3 is in its boot ROM."))
            : BringUpWaitingText(
                  tr("the board to come back in its boot ROM, once the jumper "
                     "is fitted and both cables have been out and back in.")));
  }

  // The two working pages. Refresh() owns everything on them except the
  // sentence a failed or stopped run leaves behind, which it would otherwise
  // replace with a perfectly true "waiting for you to press this" a fraction
  // of a second after somebody watched the run stop.
  if (configure_status_ != nullptr && !running) {
    if (configured_) {
      configure_status_->setText(BringUpStepDoneText(
          tr("The FPGA is running the gateware — held in memory only, and "
             "nothing has been written to the board yet.")));
    } else if (!configure_reported_) {
      configure_status_->setText(BringUpWaitingText(
          tr("you to press <b>Load the gateware</b> below.")));
    }
  }

  if (program_status_ != nullptr && !running) {
    if (programmed_) {
      program_status_->setText(BringUpStepDoneText(
          tr("The firmware, the factory image and the application image are "
             "all written and checked. Nothing has been restarted.")));
    } else if (!program_reported_) {
      program_status_->setText(BringUpWaitingText(
          configured_
              ? tr("you to press <b>Program the board</b> below.")
              : tr("the gateware to be loaded on the previous page, which is "
                   "what gives this step a way to reach the flash.")));
    }
  }

  // Five states, and none of them is "a Duplicator is attached". Two of them
  // are diagnoses rather than guesses, so they are shown as soon as they are
  // read rather than after the page has run out of patience.
  if (power_cycle_status_ != nullptr) {
    const bool waited_long_enough =
        power_cycle_polls_ > kPowerCyclePatiencePolls;
    switch (power_cycle_state_) {
      case PowerCycleState::kBack:
        power_cycle_status_->setText(BringUpStepDoneText(
            tr("The Duplicator has restarted and is running from its own "
               "flash.")));
        break;

      case PowerCycleState::kBootRom:
        power_cycle_status_->setText(BringUpStillInBootRomText());
        break;

      case PowerCycleState::kNotReloaded:
        power_cycle_status_->setText(BringUpNotReloadedText());
        break;

      case PowerCycleState::kGone:
        power_cycle_status_->setText(
            waited_long_enough
                ? BringUpDeviceNotBackText()
                : BringUpWaitingText(
                      tr("the Duplicator to come back. Plug both cables in "
                         "again.")));
        break;

      case PowerCycleState::kStillHere:
        power_cycle_status_->setText(
            waited_long_enough
                ? BringUpPowerCycleTimeoutText()
                : BringUpWaitingText(
                      tr("both cables to come out. The board is still on the "
                         "bus, so nothing has happened yet.")));
        break;
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

bool BoardBringUpWizard::ReturnedOnItsOwnFlash() {
  const std::optional<capture::DeviceInfo> fx3 = Fx3();
  if (!fx3.has_value() || !access_.open_updater) {
    return true;
  }

  const std::unique_ptr<capture::IDeviceUpdater> updater =
      access_.open_updater(fx3->path);
  if (updater == nullptr) {
    return true;
  }

  const std::optional<capture::DeviceIdentity> identity =
      updater->ReadIdentity();
  if (!identity.has_value()) {
    return true;
  }

  // The factory image, positively read, is the only thing that means "the
  // board did not lose power" — because the factory image is what the gateware
  // step put into the FPGA over JTAG, and only a power cycle replaces it with
  // the application image out of flash.
  const bool still_on_the_jtag_image =
      identity->gateware_present &&
      identity->image_role == capture::kImageRoleFactory;
  return !still_on_the_jtag_image;
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

  if (page_ == BringUpPage::kPowerCycle && !returned_) {
    ++power_cycle_polls_;

    const std::optional<capture::DeviceInfo> fx3 = Fx3();
    if (!fx3.has_value()) {
      // The one observation that proves a cable came out. Everything after
      // this is about a device that has come back.
      power_cycle_state_ = PowerCycleState::kGone;
    } else if (power_cycle_state_ == PowerCycleState::kGone ||
               power_cycle_state_ == PowerCycleState::kBootRom) {
      switch (fx3->personality) {
        case capture::DevicePersonality::kRecovery:
          power_cycle_state_ = PowerCycleState::kBootRom;
          break;
        case capture::DevicePersonality::kApplication:
          // Read once per return rather than every poll: leaving kNotReloaded
          // needs another power cycle, and that is another disappearance.
          power_cycle_state_ = ReturnedOnItsOwnFlash()
                                   ? PowerCycleState::kBack
                                   : PowerCycleState::kNotReloaded;
          break;
        case capture::DevicePersonality::kLegacy:
        case capture::DevicePersonality::kFlashProgrammer:
          // Neither is reachable from here — the EEPROM has just been written
          // and read back — and neither is evidence of anything, so the page
          // goes on waiting rather than inventing a diagnosis.
          break;
      }
    }

    returned_ = power_cycle_state_ == PowerCycleState::kBack;
  }

  Refresh();
}

// --- the two halves -------------------------------------------------------

void BoardBringUpWizard::StartConfigure() {
  if (busy() || configured_ || !manifest_.has_value()) {
    return;
  }
  StartTask(static_cast<int>(BringUpWorker::Task::kConfigure));
}

void BoardBringUpWizard::StartProgram() {
  if (busy() || programmed_ || !configured_ || !manifest_.has_value()) {
    return;
  }
  StartTask(static_cast<int>(BringUpWorker::Task::kProgram));
}

void BoardBringUpWizard::StartTask(int task) {
  const auto kind = static_cast<BringUpWorker::Task>(task);
  running_task_ = task;

  if (orchestrator_ == nullptr) {
    // Built once, at the moment the first half starts, and kept: it is what
    // carries the ordering rule across the page between the two halves.
    const std::optional<capture::DeviceInfo> fx3 = Fx3();
    const std::string path = fx3.has_value() ? fx3->path : std::string();

    capture::BringUpAccess access;
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

    orchestrator_ = std::make_unique<capture::BringUpOrchestrator>(
        std::move(access), nullptr);
  }

  QLabel* const status = kind == BringUpWorker::Task::kConfigure
                             ? configure_status_
                             : program_status_;
  QProgressBar* const bar = kind == BringUpWorker::Task::kConfigure
                                ? configure_progress_
                                : program_progress_;

  // Whatever the last run of this half left on the page goes now: a failure
  // somebody has just pressed the button again after is not what the page
  // should still be saying while the retry runs.
  if (kind == BringUpWorker::Task::kConfigure) {
    configure_reported_ = false;
  } else {
    program_reported_ = false;
  }

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
  const bool configure =
      running_task_ == static_cast<int>(BringUpWorker::Task::kConfigure);

  if (thread_ != nullptr) {
    thread_->quit();
    thread_->wait();
    thread_->deleteLater();
    thread_ = nullptr;
  }
  worker_->deleteLater();
  worker_ = nullptr;

  QLabel* const status = configure ? configure_status_ : program_status_;
  QProgressBar* const bar = configure ? configure_progress_ : program_progress_;

  if (succeeded) {
    // What the page then says is Refresh()'s to write, out of the same
    // function every other finished step uses. Two ways of announcing a
    // success is one way too many, and this was the way that put the good news
    // in the middle of a paragraph.
    if (configure) {
      configured_ = true;
    } else {
      programmed_ = true;
    }
    bar->setValue(100);
  } else {
    status->setText(stopped ? BringUpStoppedText()
                            : BringUpFailureText(problem));
    if (configure) {
      configure_reported_ = true;
    } else {
      program_reported_ = true;
    }
  }

  Refresh();
  emit BusyChanged(false);
}

void BoardBringUpWizard::Stop() {
  if (worker_ == nullptr) {
    return;
  }
  worker_->Cancel();

  // Both, rather than whichever page is showing: the run is what was stopped,
  // and a button that stayed live on the page nobody is looking at would offer
  // to stop something that is already stopping.
  if (configure_stop_ != nullptr) {
    configure_stop_->setEnabled(false);
  }
  if (program_stop_ != nullptr) {
    program_stop_->setEnabled(false);
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
    // The same two marks the connectivity rows use, from the same place: a
    // page that ticked in one shape and crossed in another would be two
    // vocabularies for one answer.
    const BringUpRowState state =
        check.passed ? BringUpRowState::kReady : BringUpRowState::kProblem;
    text += QStringLiteral("<span style=\"color:%1\">%2</span> %3<br>")
                .arg(BringUpMarkColour(state), BringUpMark(state),
                     check.description.toHtmlEscaped());
  }
  text += QStringLiteral("</p>");

  verify_->setText(text);
  verify_summary_->setText(all_passed ? BringUpCompleteText()
                                      : BringUpIncompleteText());
}

// --- the update file ------------------------------------------------------

void BoardBringUpWizard::LoadBundledFileOnce() {
  // Once, and only when nothing has been chosen: somebody who came back to
  // this page after picking a file would not expect the choice to be taken off
  // them, and somebody whose bundled file failed to verify does not want it
  // tried again every time they navigate.
  if (bundled_tried_ || bundled_path_.isEmpty() || !chosen_path_.isEmpty()) {
    return;
  }
  bundled_tried_ = true;
  LoadUpdateFile(bundled_path_);
}

void BoardBringUpWizard::LoadUpdateFile(const QString& path) {
  archive_.clear();
  manifest_.reset();
  image_banner_->hide();
  chosen_path_ = path;

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

  // Each page quotes what its own step costs, from the components that step
  // writes — so a bar that runs for four minutes is never introduced by a page
  // that said thirty seconds.
  if (manifest_->provisioning.has_value()) {
    configure_text_->setText(BringUpConfigureText(
        capture::EstimateConfigureSeconds(manifest_->provisioning->length)));
  }

  double programming = 0.0;
  if (manifest_->firmware.has_value()) {
    programming += capture::EstimateComponentSeconds(
        capture::UpdateTarget::kFirmware, *manifest_->firmware);
  }
  if (manifest_->factory_gateware.has_value()) {
    programming += capture::EstimateComponentSeconds(
        capture::UpdateTarget::kEpcsFactory, *manifest_->factory_gateware);
  }
  if (manifest_->gateware.has_value()) {
    programming += capture::EstimateComponentSeconds(
        capture::UpdateTarget::kGateware, *manifest_->gateware);
  }
  program_text_->setText(BringUpProgramText(static_cast<int>(programming) + 1));

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
