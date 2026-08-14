/************************************************************************

    update_page.h

    The staged update flow, inside the Firmware dialog
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>
#include <QWidget>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "device_programmer.h"
#include "device_updater.h"
#include "update_key.h"
#include "update_manifest.h"
#include "update_orchestrator.h"
#include "usb_device_info.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QThread;

namespace ddd::gui {

class UpdateWorker;

// The update half of the Firmware dialog: a staged, wizard-like flow that at
// every moment says what is happening, how long it will take, and what the
// user should not do.
//
// The stages, and each one has its own progress and its own line of plain
// language:
//
//   1. *Checking*   — versions found, what will change, release notes;
//   2. *Verifying*  — the signature and every digest, with a tick when they
//                     pass. (The online *Downloading* stage joins this one
//                     when the fetcher arrives; the file picker is the whole
//                     of the offline path and verifies identically, because
//                     it all lives inside the bundle.)
//   3. *Updating*   — transfer, write and verify shown distinctly, because
//                     they move at very different speeds and one bar over
//                     all three would appear to stop;
//   4. *Restarting* — "the device will disconnect and reconnect by itself";
//   5. *Confirming* — "your device now reports firmware X."
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

 signals:
  // Raised when an update starts and again when it ends, so the window can
  // keep a capture and an update out of each other's way and refuse to close
  // while one is running.
  void BusyChanged(bool busy);

 private slots:
  void ChooseBundle();
  void StartUpdate();
  void CancelUpdate();
  void HandleProgress(int stage, quint64 done, quint64 total,
                      const QString& message);
  void HandleFinished(bool succeeded, const QString& problem,
                      const QString& product_string,
                      const QString& gateware_commit);

 private:
  void RefreshVersions();
  void RefreshButtons();

  // "Update", or "Program this device" for a device with no firmware. The
  // mechanism is the same either way; the words are not, because somebody
  // holding a kit they have just soldered has not broken anything.
  QString InstallButtonLabel() const;

  bool in_recovery() const {
    return device_.personality == capture::DevicePersonality::kRecovery;
  }
  void ShowStage(capture::UpdateStage stage, const QString& message);
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

  QLabel* versions_ = nullptr;
  QLabel* bundle_ = nullptr;
  QLabel* banner_ = nullptr;
  QLabel* status_ = nullptr;
  QLabel* instruction_ = nullptr;
  QProgressBar* progress_ = nullptr;
  QPushButton* choose_ = nullptr;
  QPushButton* install_ = nullptr;
  QPushButton* cancel_ = nullptr;

  QThread* thread_ = nullptr;
  UpdateWorker* worker_ = nullptr;
};

}  // namespace ddd::gui
