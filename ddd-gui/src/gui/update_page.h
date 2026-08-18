/************************************************************************

    update_page.h

    The staged update flow, inside the Firmware dialog
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QElapsedTimer>
#include <QString>
#include <QWidget>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "device_programmer.h"
#include "device_updater.h"
#include "update_key.h"
#include "update_manifest.h"
#include "update_orchestrator.h"
#include "update_steps.h"
#include "usb_device_info.h"

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QThread;

namespace ddd::gui {

class UpdateStepList;
class UpdateWorker;

// The update half of the Firmware dialog: a procedure the user can see the
// whole of, that at every moment says what is happening, how long it will
// take, and what they should not do.
//
// **The shape of this is the point of it.** An update is the one operation in
// this application that cannot be interrupted safely, and the interface is
// arranged around the two questions somebody in front of it is asking — *what
// is it doing* and *how much longer*:
//
//   - the **steps** are listed and greyed before the first byte moves, so the
//     whole procedure is visible in advance rather than arriving one stage
//     name at a time. The step in hand is picked out and the finished ones get
//     a tick, so "where am I" is answered by looking rather than by reading;
//   - **one bar** fills once across all of them. Not a bar per stage: a bar
//     that restarts at each boundary cannot answer "how far through the whole
//     thing am I", which is the only question a progress bar is for. What
//     stopped the earlier arrangement from misleading — that the stages move
//     at wildly different speeds — is handled by weighting each step's share
//     of the bar by how long it is expected to take, in update_steps.h;
//   - the **detail line** under the bar says what the current step is doing
//     right now, in the engine's own words;
//   - the **rolling log** is behind a *Show details* button, closed by
//     default. Everything the engine reported, timestamped, for the user who
//     wants it and out of the way of the user who does not.
//
// Everything the page needs of a device comes through a factory it is given,
// so the complete flow — including every error and every rescue branch — is
// drivable in a widget test against a fake, with nothing plugged in. That is
// not a convenience: half of these branches cannot be arranged on real
// hardware at all.
//
// Thread-safety: NOT thread-safe. Interface thread only. The update itself
// runs on a worker thread and reports back through queued signals.
class UpdatePage : public QWidget {
  Q_OBJECT

 public:
  // How the page reaches a device, and what it may say about it.
  struct Device {
    // Opens an updater for the device at `path`, or for the selected device
    // when `path` is empty. Called once per attempt, on the interface thread
    // for a device that is already working, and on the worker thread for one
    // that has just been woken from recovery — which is why the path is a
    // parameter rather than something the factory remembers: on Windows a
    // device's path changes when its personality does.
    std::function<std::unique_ptr<capture::IDeviceUpdater>(
        const std::string& path)>
        open;

    // Opens a programmer for a device sitting in its boot ROM. Only used
    // when `personality` says so, and null everywhere else.
    std::function<std::unique_ptr<capture::IDeviceProgrammer>()>
        open_programmer;

    // The device's identity as the application already knows it, so opening
    // this page reads nothing and cannot block. Empty throughout for a device
    // in recovery, which has nothing running on it to report one.
    capture::DeviceIdentity identity;

    bool attached = false;

    capture::DevicePersonality personality =
        capture::DevicePersonality::kApplication;
  };

  UpdatePage(QString application_version, Device device,
             QWidget* parent = nullptr);
  ~UpdatePage() override;

  // Which signatures this page accepts. Defaults to DefaultUpdateKeyPolicy(),
  // which is the release key alone in a build that pins one; the application's
  // --dev-update-key is what widens it, and a widget test sets it so that the
  // flow can be driven with a development-signed fixture whatever key the
  // build was given.
  //
  // Set it before a bundle is loaded. A bundle already verified against the
  // previous policy is not re-verified by this, and the worker takes whatever
  // is set when the install starts.
  void SetKeyPolicy(capture::UpdateKeyPolicy policy) {
    policy_ = std::move(policy);
  }

  // Load a bundle from a file, exactly as the Choose button does. Separate
  // from the button so a test can drive the flow without a file dialog, which
  // is modal and native and cannot be driven at all.
  void LoadBundle(const QString& path);

  // Whether an update is running. The dialog refuses to close on this, and
  // the window refuses to start a capture on it.
  bool busy() const { return worker_ != nullptr; }

  // Object names, so a widget test can reach a control without depending on
  // the order things were added to the layout.
  static constexpr const char* kVersionsLabelName = "update_versions";
  static constexpr const char* kBundleLabelName = "update_bundle";
  static constexpr const char* kBannerLabelName = "update_banner";
  static constexpr const char* kStatusLabelName = "update_status";
  static constexpr const char* kInstructionLabelName = "update_instruction";
  static constexpr const char* kProgressBarName = "update_progress";
  static constexpr const char* kChooseButtonName = "update_choose";
  static constexpr const char* kInstallButtonName = "update_install";
  static constexpr const char* kCancelButtonName = "update_cancel";
  static constexpr const char* kStepsHeadingName = "update_steps_heading";
  static constexpr const char* kStepListName = "update_steps";
  static constexpr const char* kDetailsButtonName = "update_details";
  static constexpr const char* kLogViewName = "update_log";

 signals:
  // Raised when an update starts and again when it ends, so the window can
  // keep a capture and an update out of each other's way and refuse to close
  // while one is running.
  void BusyChanged(bool busy);

 private slots:
  void ChooseBundle();
  void StartUpdate();
  void CancelUpdate();
  void ShowDetails(bool shown);
  void HandleProgress(int stage, int target, quint64 done, quint64 total,
                      const QString& message);
  void HandleFinished(bool succeeded, const QString& problem,
                      const capture::DeviceIdentity& identity);

 private:
  void RefreshVersions();
  void RefreshButtons();

  // Rebuild the step list from what the chosen bundle carries and what state
  // the device is in, every step greyed. Called when a bundle is loaded, which
  // is the first moment there is a truthful plan to show.
  void PlanSteps();

  // Move the highlight and the bar to where the tracker says the update is.
  void ShowPosition(const UpdateProgressTracker::Position& position);

  // The heading over the step list. It changes tense as the update goes —
  // *what will happen*, *what is happening*, *what happened* — because the
  // same list is a plan, a report and a record in turn, and which of the three
  // it is is the thing a reader has to know before reading it.
  void SetStepsHeading(const QString& heading);

  // One line into the rolling log, stamped with how long the update has been
  // running. Elapsed rather than clock time: what matters when reading a log
  // back is how long a step took, and a wall-clock time makes that a
  // subtraction.
  void AppendLog(const QString& line);

  // "Update", or "Program this device" for a device with no firmware. The
  // mechanism is the same either way; the words are not, because somebody
  // holding a kit they have just soldered has not broken anything.
  QString InstallButtonLabel() const;

  bool in_recovery() const {
    return device_.personality == capture::DevicePersonality::kRecovery;
  }

  // Whether the FPGA is running its resident factory image. A working
  // device with no capture path, which is a different state from a device
  // with no firmware and is repaired by a different half of the bundle.
  bool in_gateware_recovery() const {
    return device_.attached &&
           device_.personality == capture::DevicePersonality::kApplication &&
           device_.identity.GatewareIsRecovery();
  }

  void ShowStage(capture::UpdateStage stage, capture::UpdateTarget target,
                 const QString& message);
  void SetBundleState(const QString& summary, const QString& banner);

  QString application_version_;
  Device device_;
  capture::UpdateKeyPolicy policy_;

  // The chosen bundle's bytes, kept so the worker can re-verify them rather
  // than trusting this page's verification.
  std::vector<uint8_t> archive_;
  std::optional<capture::UpdateManifest> manifest_;
  bool bundle_installable_ = false;

  // Whether an update has been run in this session, which is what keeps the
  // progress bar on screen after one has finished.
  bool attempted_ = false;

  // The plan, and where in it the update has got to.
  UpdateProgressTracker tracker_;

  // What was last written to the log, so that the thousands of reports one
  // transfer produces become one line rather than thousands. A rolling log
  // that scrolled a screenful a second would be unreadable, which is the same
  // as not being there.
  int logged_stage_ = -1;
  int logged_target_ = -1;
  QString logged_message_;

  // Started when an update starts, and what the log's timestamps are counted
  // from.
  QElapsedTimer clock_;

  QLabel* versions_ = nullptr;
  QLabel* bundle_ = nullptr;
  QLabel* banner_ = nullptr;
  QLabel* status_ = nullptr;
  QLabel* instruction_ = nullptr;
  QLabel* steps_heading_ = nullptr;
  UpdateStepList* steps_ = nullptr;
  QProgressBar* progress_ = nullptr;
  QPushButton* choose_ = nullptr;
  QPushButton* install_ = nullptr;
  QPushButton* cancel_ = nullptr;
  QPushButton* details_ = nullptr;
  QPlainTextEdit* log_ = nullptr;

  QThread* thread_ = nullptr;
  UpdateWorker* worker_ = nullptr;
};

}  // namespace ddd::gui
