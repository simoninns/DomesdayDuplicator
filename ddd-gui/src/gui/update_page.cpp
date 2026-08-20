/************************************************************************

    update_page.cpp

    The staged update flow, inside the Firmware dialog
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_page.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <string>
#include <utility>

#include "log_format.h"
#include "update_bundle.h"
#include "update_gate.h"
#include "update_step_list.h"
#include "update_text.h"
#include "update_worker.h"

namespace ddd::gui {
namespace {

// How many lines the details window keeps. Rolling rather than unbounded: a
// long update on a slow medium produces thousands of reports, and the lines
// worth reading are always the recent ones. A hundred is several minutes of
// an update at the rate distinct messages actually change.
constexpr int kLogLines = 200;

QLabel* MakeLabel(QWidget* parent, const char* object_name) {
  auto* label = new QLabel(parent);
  label->setObjectName(QLatin1String(object_name));
  label->setTextFormat(Qt::RichText);
  label->setWordWrap(true);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

}  // namespace

UpdatePage::UpdatePage(QString application_commit, Device device,
                       QWidget* parent)
    : QWidget(parent),
      application_commit_(std::move(application_commit)),
      device_(std::move(device)),
      policy_(capture::DefaultUpdateKeyPolicy()) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);

  versions_ = MakeLabel(this, kVersionsLabelName);
  layout->addWidget(versions_);

  choose_ = new QPushButton(tr("Choose update file…"), this);
  choose_->setObjectName(QLatin1String(kChooseButtonName));
  connect(choose_, &QPushButton::clicked, this, &UpdatePage::ChooseBundle);
  layout->addWidget(choose_);

  bundle_ = MakeLabel(this, kBundleLabelName);
  layout->addWidget(bundle_);

  banner_ = MakeLabel(this, kBannerLabelName);
  banner_->setVisible(false);
  layout->addWidget(banner_);

  // The procedure, above the bar that measures it. Greyed until there is a
  // bundle to plan from, and then greyed until the update starts: what it
  // shows before the button is pressed is what *will* happen, and the heading
  // says so in those words rather than leaving a numbered list to be read as
  // either a plan or a report depending on which the reader assumed.
  steps_heading_ = MakeLabel(this, kStepsHeadingName);
  steps_heading_->setVisible(false);
  SetStepsHeading(tr("What will happen"));
  layout->addWidget(steps_heading_);

  steps_ = new UpdateStepList(this);
  steps_->setObjectName(QLatin1String(kStepListName));
  steps_->setVisible(false);
  layout->addWidget(steps_);

  // One bar, over the whole update. Its text names the step it is inside, so
  // that a glance at the bar alone still answers "which of these am I on".
  progress_ = new QProgressBar(this);
  progress_->setObjectName(QLatin1String(kProgressBarName));
  progress_->setRange(0, 100);
  progress_->setValue(0);
  progress_->setTextVisible(true);
  progress_->setVisible(false);
  layout->addWidget(progress_);

  // What the current step is doing right now, under the bar that says how far
  // through it is.
  status_ = MakeLabel(this, kStatusLabelName);
  layout->addWidget(status_);

  // The one instruction that matters, and it is on the screen before the
  // first byte moves rather than appearing when it is already too late to
  // act on.
  instruction_ = MakeLabel(this, kInstructionLabelName);
  instruction_->setText(UpdateHoldStillInstruction());
  instruction_->setVisible(false);
  layout->addWidget(instruction_);

  // The rolling log, and the button that reveals it. Closed by default and
  // remembered for the life of the dialog: the step list and the one line
  // under the bar are the whole of what most users need, and somebody
  // diagnosing a failure — or reporting one — needs every line the engine
  // produced. Both are served, and neither is in the other's way.
  details_ = new QPushButton(tr("Show details"), this);
  details_->setObjectName(QLatin1String(kDetailsButtonName));
  details_->setCheckable(true);
  details_->setVisible(false);
  connect(details_, &QPushButton::toggled, this, &UpdatePage::ShowDetails);

  auto* details_row = new QHBoxLayout;
  details_row->addWidget(details_);
  details_row->addStretch(1);
  layout->addLayout(details_row);

  log_ = new QPlainTextEdit(this);
  log_->setObjectName(QLatin1String(kLogViewName));
  log_->setReadOnly(true);
  log_->setMaximumBlockCount(kLogLines);
  log_->setVisible(false);

  // A fixed-pitch font, because the lines are timestamped and a column that
  // does not line up is a column nobody scans.
  log_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  // Tall enough to show the last several lines and no taller. This page sits
  // inside a scroll area, and a details window that grew without limit would
  // push the buttons off the bottom of it.
  constexpr int kLogHeightPixels = 140;
  log_->setMaximumHeight(kLogHeightPixels);
  layout->addWidget(log_);

  auto* buttons = new QHBoxLayout;
  buttons->addStretch(1);

  cancel_ = new QPushButton(tr("Stop"), this);
  cancel_->setObjectName(QLatin1String(kCancelButtonName));
  cancel_->setVisible(false);
  connect(cancel_, &QPushButton::clicked, this, &UpdatePage::CancelUpdate);
  buttons->addWidget(cancel_);

  install_ = new QPushButton(InstallButtonLabel(), this);
  install_->setObjectName(QLatin1String(kInstallButtonName));
  install_->setDefault(true);
  connect(install_, &QPushButton::clicked, this, &UpdatePage::StartUpdate);
  buttons->addWidget(install_);

  layout->addLayout(buttons);
  layout->addStretch(1);

  RefreshVersions();
  SetBundleState(
      tr("No update file chosen. Updates are published on the project's "
         "releases page; download one and choose it here."),
      QString());
  RefreshButtons();
}

UpdatePage::~UpdatePage() {
  if (thread_ != nullptr) {
    // An update in flight when the window is torn down: ask it to stop and
    // wait. Detaching instead would leave a thread writing to a device
    // through an updater whose owner had gone.
    if (worker_ != nullptr) {
      worker_->Cancel();
    }
    thread_->quit();
    thread_->wait();
  }

  delete worker_;
  worker_ = nullptr;
}

void UpdatePage::RefreshVersions() {
  const capture::UpdateManifest* const manifest =
      manifest_.has_value() ? &manifest_.value() : nullptr;

  QString text = UpdateVersionTable(
      UpdateVersionRows(application_commit_, device_.identity, device_.attached,
                        manifest, device_.personality));

  // Above the table rather than below it: what state the device is in is the
  // thing that explains why the table says what it says, and a user reading
  // "None installed" needs the explanation before the surprise, not after it.
  //
  // The two states are mutually exclusive in practice — a device with no
  // firmware cannot report which gateware image it is running — but both are
  // asked about rather than one being assumed to rule the other out.
  if (device_.attached) {
    const QString state = DevicePersonalityText(device_.personality);
    if (!state.isEmpty()) {
      text = state + QStringLiteral("<br><br>") + text;
    }

    const QString gateware = GatewareRecoveryText(device_.identity);
    if (!gateware.isEmpty()) {
      text = gateware + QStringLiteral("<br><br>") + text;
    }
  }

  versions_->setText(text);
}

QString UpdatePage::InstallButtonLabel() const {
  return InstallActionLabel(device_.personality, in_gateware_recovery());
}

void UpdatePage::SetBundleState(const QString& summary, const QString& banner) {
  bundle_->setText(summary);
  banner_->setText(banner);
  banner_->setVisible(!banner.isEmpty());
}

void UpdatePage::RefreshButtons() {
  const bool running = busy();

  choose_->setEnabled(!running);
  install_->setVisible(!running);
  install_->setEnabled(!running && bundle_installable_ && device_.attached);
  cancel_->setVisible(running);

  // The plan appears as soon as there is one, before the button is pressed:
  // an update cannot be interrupted safely, so what a user needs in order to
  // decide whether to start one is what starting it commits them to.
  //
  // The list and the bar then stay after a run rather than vanishing with the
  // buttons — the bar ends full on a success and stopped part way on a
  // failure, with a tick against every step that did finish, and all of that
  // is worth still being able to see.
  const bool planned = steps_->count() > 0;
  steps_heading_->setVisible(planned);
  steps_->setVisible(planned);
  progress_->setVisible(planned || running || attempted_);

  instruction_->setVisible(running);

  // The details button appears with the first attempt rather than with the
  // plan: before anything has run there is nothing in the log to show.
  details_->setVisible(attempted_);
  log_->setVisible(attempted_ && details_->isChecked());
}

void UpdatePage::ShowDetails(bool shown) {
  details_->setText(shown ? tr("Hide details") : tr("Show details"));
  log_->setVisible(shown && attempted_);
}

void UpdatePage::PlanSteps() {
  if (!manifest_.has_value() || !bundle_installable_) {
    tracker_ = UpdateProgressTracker();
    steps_->SetSteps({});
    progress_->setValue(0);
    progress_->setFormat(QStringLiteral("%p%"));
    return;
  }

  tracker_ = UpdateProgressTracker(PlanUpdateSteps(*manifest_, in_recovery()));

  std::vector<QString> titles;
  titles.reserve(tracker_.steps().size());
  for (const UpdateStep& step : tracker_.steps()) {
    titles.push_back(step.title);
  }

  steps_->SetSteps(titles);
  SetStepsHeading(tr("What will happen"));
  progress_->setValue(0);
  progress_->setFormat(tr("Not started"));
}

void UpdatePage::SetStepsHeading(const QString& heading) {
  steps_heading_->setText(
      QStringLiteral("<b>%1</b>").arg(heading.toHtmlEscaped()));
}

void UpdatePage::ShowPosition(const UpdateProgressTracker::Position& position) {
  progress_->setValue(position.percent);

  if (position.step >= 0 && position.step < steps_->count()) {
    steps_->SetCurrent(position.step);

    // "Step 2 of 5 — 42%". The bar carries the count because the bar is what
    // the eye goes to, and a percentage on its own does not say how much of
    // the procedure is left to sit through.
    progress_->setFormat(
        tr("Step %1 of %2 — %p%").arg(position.step + 1).arg(steps_->count()));
  }
}

void UpdatePage::AppendLog(const QString& line) {
  const qint64 seconds = (clock_.isValid() ? clock_.elapsed() : 0) / 1000;

  log_->appendPlainText(QStringLiteral("%1:%2  %3")
                            .arg(seconds / 60)
                            .arg(seconds % 60, 2, 10, QLatin1Char('0'))
                            .arg(line));

  // The same line into the application's log. This is the cheapest useful
  // thing this page does for an investigation: the rolling log above is
  // already one line per change of phase rather than one per report, and it
  // closes with the dialog — so mirroring it costs nothing and is the
  // difference between a fault report that carries the update's own story and
  // one that says "it failed".
  if (device_.logger != nullptr) {
    device_.logger->Debug("Update page: " + line.toStdString());
  }
}

void UpdatePage::ChooseBundle() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Choose an update file"), QString(),
      tr("Domesday Duplicator updates (*%1);;All files (*)")
          .arg(QString::fromUtf8(
              capture::kUpdateBundleExtension.data(),
              static_cast<qsizetype>(capture::kUpdateBundleExtension.size()))));

  if (!path.isEmpty()) {
    LoadBundle(path);
  }
}

void UpdatePage::LoadBundle(const QString& path) {
  archive_.clear();
  manifest_.reset();
  bundle_installable_ = false;
  status_->clear();

  // The plan belonged to the file that was chosen before this one. Cleared
  // first so that a file which turns out to be unreadable cannot leave the
  // previous file's steps on the screen.
  PlanSteps();

  if (device_.logger != nullptr) {
    device_.logger->Debug("Update page: opening " + path.toStdString());
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (device_.logger != nullptr) {
      device_.logger->Warning(
          "The update file could not be read: " + path.toStdString() + " — " +
          file.errorString().toStdString());
    }
    SetBundleState(tr("That file could not be read."), QString());
    RefreshVersions();
    RefreshButtons();
    return;
  }

  const QByteArray bytes = file.readAll();
  archive_.assign(bytes.begin(), bytes.end());

  std::string error;
  const std::optional<capture::UpdateBundle> bundle =
      capture::OpenUpdateBundleForPolicy(archive_, policy_, &error);
  if (!bundle.has_value()) {
    if (device_.logger != nullptr) {
      // The file was read and refused, which is a different thing from a file
      // that could not be read at all — and the two have completely different
      // remedies, so the log says which happened rather than "it did not
      // work".
      device_.logger->Warning("The update file was refused: " + error);
    }
    archive_.clear();
    SetBundleState(QString::fromStdString(error).toHtmlEscaped(), QString());
    RefreshVersions();
    RefreshButtons();
    return;
  }

  manifest_ = bundle->manifest;

  // The signature and every payload digest have now passed, so the file is
  // what its signer signed for. Said plainly, because "verified" is the word
  // that answers the question a user has about a file they downloaded.
  QString summary = UpdateBundleSummary(*manifest_);
  summary += QStringLiteral("<br><br>") +
             tr("✓ This update is verified as intact and correctly signed.");

  const QString banner =
      manifest_->channel == capture::UpdateChannel::kDevelopment
          ? DevelopmentBundleBanner()
          : QString();

  // Verified is not the same as installable. That is the compatibility gate,
  // and its whole purpose is that a user cannot drive the device past what
  // the application driving it understands.
  capture::UpdateGateInput input;
  input.device_attached = device_.attached;
  input.device = device_.identity;
  input.device_personality = device_.personality;

  const capture::UpdateGateResult gate =
      capture::CheckUpdateGate(*manifest_, input);
  bundle_installable_ = gate.allowed();

  if (device_.logger != nullptr) {
    device_.logger->Debug("Update file verified: version " +
                          manifest_->version + ", " +
                          capture::UpdateChannelName(manifest_->channel) +
                          " channel, " + capture::FormatBytes(archive_.size()) +
                          ", " + std::to_string(bytes.size()) +
                          " bytes read from " + path.toStdString());

    // What the device looked like when the gate was asked, beside what the
    // gate said. The verdict on its own cannot be argued with; the verdict
    // beside its inputs can be checked.
    device_.logger->Debug(
        std::string("Compatibility gate: ") +
        capture::UpdateGateVerdictName(gate.verdict) + " (device " +
        (device_.attached ? "attached" : "not attached") + ", personality " +
        capture::DevicePersonalityName(device_.personality) +
        ", firmware protocol " +
        std::to_string(device_.identity.protocol_version) + ", register map " +
        std::to_string(device_.identity.register_map_version) + ")");

    for (const std::string& reason : gate.reasons) {
      device_.logger->Debug("  " + reason);
    }
  }

  const QString gate_text = UpdateGateText(gate);
  if (!gate_text.isEmpty()) {
    summary += QStringLiteral("<br><br>") + gate_text;
  }

  // The time estimate is offered only where there is something to install,
  // because "this will take about a minute" under a refusal is an invitation
  // to look for the button that starts it.
  if (gate.allowed()) {
    summary += QStringLiteral("<br><br>") +
               tr("This will take %1. %2")
                   .arg(FormatUpdateEstimate(
                            capture::EstimateUpdateSeconds(*manifest_)),
                        UpdateHoldStillInstruction());
  }

  SetBundleState(summary, banner);

  // Built from the manifest that has just been verified and gated, so the
  // list is the truth about this file on this device: a bundle with no
  // gateware in it shows no gateware step, and a device in recovery shows the
  // step that wakes it.
  PlanSteps();

  RefreshVersions();
  RefreshButtons();
}

void UpdatePage::StartUpdate() {
  if (busy() || !bundle_installable_ || archive_.empty()) {
    return;
  }

  UpdateDevice target;
  target.in_recovery = in_recovery();

  if (target.in_recovery) {
    // Nothing is opened here. The device this update ends on does not exist
    // yet — it is the one that will appear once the boot ROM has been handed
    // the bundle's firmware — so what the worker is given is the two
    // factories that will reach it, and both are called on the worker
    // thread where the waiting happens.
    target.recovery.open_programmer = device_.open_programmer;
    target.recovery.open_updater = device_.open;

    if (!target.recovery.open_programmer || !target.recovery.open_updater) {
      status_->setText(UpdateFailureText(
          tr("This device cannot be reached for programming. Unplug it, plug "
             "it back in, and try again.")));
      return;
    }
  } else {
    target.updater = device_.open ? device_.open(std::string()) : nullptr;
    if (target.updater == nullptr) {
      status_->setText(UpdateFailureText(
          tr("The device could not be opened for updating. Unplug it, plug it "
             "back in, and try again.")));
      return;
    }
  }

  attempted_ = true;
  thread_ = new QThread(this);
  worker_ =
      new UpdateWorker(std::move(target), archive_, policy_, device_.logger);
  worker_->moveToThread(thread_);

  connect(thread_, &QThread::started, worker_, &UpdateWorker::Run);
  connect(worker_, &UpdateWorker::Progress, this, &UpdatePage::HandleProgress);
  connect(worker_, &UpdateWorker::Finished, this, &UpdatePage::HandleFinished);

  // A second attempt starts from the beginning of the same plan rather than
  // from where the first one stopped: the engine does, so the list has to.
  tracker_.Reset();
  logged_stage_ = -1;
  logged_target_ = -1;
  logged_message_.clear();

  clock_.start();

  if (log_->blockCount() > 1 || !log_->toPlainText().isEmpty()) {
    log_->appendPlainText(QString());
  }
  AppendLog(tr("Update started. %1")
                .arg(manifest_.has_value()
                         ? tr("Installing version %1.")
                               .arg(QString::fromStdString(manifest_->version))
                         : QString()));

  progress_->setRange(0, 100);
  progress_->setValue(0);

  SetStepsHeading(tr("What is happening"));

  // The first step is lit here rather than waiting for the worker's first
  // report, so that pressing the button has a visible effect on the frame
  // after it is pressed rather than whenever a thread gets round to starting.
  ShowStage(capture::UpdateStage::kChecking, capture::UpdateTarget::kFirmware,
            QString());
  ShowPosition(tracker_.Fold(capture::UpdateStage::kChecking,
                             capture::UpdateTarget::kFirmware, 0, 0));

  RefreshButtons();
  emit BusyChanged(true);

  thread_->start();
}

void UpdatePage::CancelUpdate() {
  if (worker_ != nullptr) {
    worker_->Cancel();
    cancel_->setEnabled(false);

    const QString stopping =
        tr("Stopping at the next safe point. Leave the device plugged in.");
    status_->setText(QStringLiteral("<b>%1</b>").arg(stopping));
    AppendLog(stopping);
  }
}

void UpdatePage::ShowStage(capture::UpdateStage stage,
                           capture::UpdateTarget target,
                           const QString& message) {
  // The stage's own title is not repeated here during a run: the step list
  // above the bar is already showing it, in bold, with the steps either side
  // of it for context. What this line adds is the sentence underneath — what
  // the device is doing this second, and why it is about to pause.
  const QString text =
      message.isEmpty() ? UpdateStageTitle(stage, target) : message;
  status_->setText(text.toHtmlEscaped());
}

void UpdatePage::HandleProgress(int stage, int target, quint64 done,
                                quint64 total, const QString& message) {
  const auto update_stage = static_cast<capture::UpdateStage>(stage);
  const auto update_target = static_cast<capture::UpdateTarget>(target);

  ShowStage(update_stage, update_target, message);
  ShowPosition(tracker_.Fold(update_stage, update_target, done, total));

  // One line per change of phase, not one per report. A transfer produces a
  // report per chunk — thousands of them, all saying the same sentence — and
  // a log that scrolled at that rate would be unreadable, which is the same
  // as not being there at all.
  if (stage != logged_stage_ || target != logged_target_ ||
      message != logged_message_) {
    logged_stage_ = stage;
    logged_target_ = target;
    logged_message_ = message;

    AppendLog(message.isEmpty() ? UpdateStageTitle(update_stage, update_target)
                                : message);
  }
}

void UpdatePage::HandleFinished(bool succeeded, const QString& problem,
                                const capture::DeviceIdentity& identity) {
  if (thread_ != nullptr) {
    thread_->quit();
    thread_->wait();
  }

  // Deleted rather than deleteLater()'d: the worker lives on a thread whose
  // event loop has just stopped, so a deferred delete posted to it would
  // never be delivered and the updater would stay holding the device open.
  delete worker_;
  worker_ = nullptr;

  if (thread_ != nullptr) {
    thread_->deleteLater();
    thread_ = nullptr;
  }

  SetStepsHeading(tr("What happened"));

  if (succeeded) {
    // The device is read back rather than assumed, so what is shown here is
    // what the device says it is running and not what the update intended.
    // That includes which gateware image answered, which is how a unit that
    // was in gateware recovery stops being described as one.
    device_.identity = identity;
    device_.attached = true;

    // A device that was in recovery is not any more: it has just been read
    // back, by name, running the firmware that was written to it. Recording
    // that is what makes the table below say the new version rather than
    // "None installed", and what stops the button offering to program a
    // device that has been programmed.
    device_.personality = capture::DevicePersonality::kApplication;

    status_->setText(QStringLiteral("<b>%1</b><br>%2")
                         .arg(UpdateStageTitle(capture::UpdateStage::kComplete)
                                  .toHtmlEscaped(),
                              UpdateCompleteText(identity)));

    // Every step ticked and the bar full. The list is then a record of what
    // was done rather than a plan of what was going to be.
    tracker_.Complete();
    steps_->MarkComplete();
    progress_->setRange(0, 100);
    progress_->setValue(100);
    progress_->setFormat(UpdateStageTitle(capture::UpdateStage::kComplete));

    // What the device reported when it came back, rather than the bare word
    // the engine has already logged: the log's last line is the one somebody
    // pastes into a bug report, and this is the line worth having there.
    AppendLog(tr("Update complete. The device reports %1.")
                  .arg(QString::fromStdString(identity.product_string)));

    // Nothing left to install from this file, so the button does not invite
    // a second run of an update that has already happened, and it stops
    // saying "Try again" about an attempt that has now succeeded.
    bundle_installable_ = false;
    install_->setText(InstallButtonLabel());
  } else {
    status_->setText(
        QStringLiteral("<b>%1</b><br>%2")
            .arg(
                UpdateStageTitle(capture::UpdateStage::kFailed).toHtmlEscaped(),
                UpdateFailureText(problem)));

    // The step it stopped on is crossed, and the ones before it keep their
    // ticks. On a bundle carrying both halves that distinction is the whole
    // story: "the firmware went in and the gateware did not" is a different
    // situation from "nothing happened", and the list is where a user can see
    // which of them they are in.
    steps_->MarkFailed();

    const int stopped = tracker_.position().step;
    progress_->setFormat(stopped >= 0 && stopped < steps_->count()
                             ? tr("Stopped at step %1 of %2")
                                   .arg(stopped + 1)
                                   .arg(steps_->count())
                             : UpdateStageTitle(capture::UpdateStage::kFailed));

    AppendLog(problem.isEmpty()
                  ? UpdateStageTitle(capture::UpdateStage::kFailed)
                  : problem);

    // The install button comes back, because "Try again" is the next step for
    // almost every failure this can produce and a user should not have to
    // choose the file a second time to reach it.
    install_->setText(tr("Try again"));
  }

  RefreshVersions();
  RefreshButtons();
  cancel_->setEnabled(true);
  emit BusyChanged(false);
}

}  // namespace ddd::gui
