/************************************************************************

    update_page.cpp

    The staged update flow, inside the Firmware dialog
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_page.h"

#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <string>
#include <utility>

#include "update_bundle.h"
#include "update_gate.h"
#include "update_text.h"
#include "update_worker.h"

namespace ddd::gui {
namespace {

QLabel* MakeLabel(QWidget* parent, const char* object_name) {
  auto* label = new QLabel(parent);
  label->setObjectName(QLatin1String(object_name));
  label->setTextFormat(Qt::RichText);
  label->setWordWrap(true);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

}  // namespace

UpdatePage::UpdatePage(QString application_version, Device device,
                       QWidget* parent)
    : QWidget(parent),
      application_version_(std::move(application_version)),
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

  status_ = MakeLabel(this, kStatusLabelName);
  layout->addWidget(status_);

  progress_ = new QProgressBar(this);
  progress_->setObjectName(QLatin1String(kProgressBarName));
  progress_->setVisible(false);
  layout->addWidget(progress_);

  // The one instruction that matters, and it is on the screen before the
  // first byte moves rather than appearing when it is already too late to
  // act on.
  instruction_ = MakeLabel(this, kInstructionLabelName);
  instruction_->setText(UpdateHoldStillInstruction());
  instruction_->setVisible(false);
  layout->addWidget(instruction_);

  auto* buttons = new QHBoxLayout;
  buttons->addStretch(1);

  cancel_ = new QPushButton(tr("Stop"), this);
  cancel_->setObjectName(QLatin1String(kCancelButtonName));
  cancel_->setVisible(false);
  connect(cancel_, &QPushButton::clicked, this, &UpdatePage::CancelUpdate);
  buttons->addWidget(cancel_);

  install_ = new QPushButton(InstallActionLabel(device_.personality), this);
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
      UpdateVersionRows(application_version_, device_.identity,
                        device_.attached, manifest, device_.personality));

  // Above the table rather than below it: what state the device is in is the
  // thing that explains why the table says what it says, and a user reading
  // "None installed" needs the explanation before the surprise, not after it.
  if (device_.attached) {
    const QString state = DevicePersonalityText(device_.personality);
    if (!state.isEmpty()) {
      text = state + QStringLiteral("<br><br>") + text;
    }
  }

  versions_->setText(text);
}

QString UpdatePage::InstallButtonLabel() const {
  return InstallActionLabel(device_.personality);
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

  // The bar stays after a run rather than vanishing with the buttons: it
  // ends at 100% on a success and stopped part way on a failure, and both
  // are things worth still being able to see.
  progress_->setVisible(running || attempted_);

  instruction_->setVisible(running);
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

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
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
  input.application_version = application_version_.toStdString();
  input.device_attached = device_.attached;
  input.device = device_.identity;
  input.device_personality = device_.personality;

  const capture::UpdateGateResult gate =
      capture::CheckUpdateGate(*manifest_, input);
  bundle_installable_ = gate.allowed();

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
  worker_ = new UpdateWorker(std::move(target), archive_, policy_);
  worker_->moveToThread(thread_);

  connect(thread_, &QThread::started, worker_, &UpdateWorker::Run);
  connect(worker_, &UpdateWorker::Progress, this, &UpdatePage::HandleProgress);
  connect(worker_, &UpdateWorker::Finished, this, &UpdatePage::HandleFinished);

  progress_->setRange(0, 0);
  ShowStage(capture::UpdateStage::kChecking, QString());
  RefreshButtons();
  emit BusyChanged(true);

  thread_->start();
}

void UpdatePage::CancelUpdate() {
  if (worker_ != nullptr) {
    worker_->Cancel();
    cancel_->setEnabled(false);
    status_->setText(
        QStringLiteral("<b>%1</b>")
            .arg(tr("Stopping at the next safe point. Leave the device "
                    "plugged in.")));
  }
}

void UpdatePage::ShowStage(capture::UpdateStage stage, const QString& message) {
  QString text =
      QStringLiteral("<b>%1</b>").arg(UpdateStageTitle(stage).toHtmlEscaped());
  if (!message.isEmpty()) {
    text += QStringLiteral("<br>") + message.toHtmlEscaped();
  }
  status_->setText(text);
}

void UpdatePage::HandleProgress(int stage, quint64 done, quint64 total,
                                const QString& message) {
  const auto update_stage = static_cast<capture::UpdateStage>(stage);
  ShowStage(update_stage, message);

  // A stage with no meaningful proportion gets a busy indicator and a line
  // saying what it is waiting for, rather than a bar invented to fill the
  // space. Progress here is either real or absent.
  if (total == 0) {
    progress_->setRange(0, 0);
    return;
  }

  progress_->setRange(0, 100);
  progress_->setValue(
      static_cast<int>((done * 100) / (total == 0 ? 1 : total)));
}

void UpdatePage::HandleFinished(bool succeeded, const QString& problem,
                                const QString& product_string,
                                const QString& gateware_commit) {
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

  if (succeeded) {
    // The device is read back rather than assumed, so what is shown here is
    // what the device says it is running and not what the update intended.
    capture::DeviceIdentity identity;
    identity.product_string = product_string.toStdString();
    identity.gateware_commit = gateware_commit.toStdString();
    identity.gateware_present = !gateware_commit.isEmpty();
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

    progress_->setRange(0, 100);
    progress_->setValue(100);

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
