/************************************************************************

    legacy_rollback_wizard.h

    Putting the original firmware and gateware back, in seven pages
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <QString>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "device_updater.h"
#include "rollback_orchestrator.h"
#include "rollback_text.h"
#include "update_key.h"
#include "update_manifest.h"
#include "usb_device_info.h"

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QThread;
class QTimer;

namespace ddd::gui {

class RollbackWorker;

// Returning a unit to the original Duplicator firmware and gateware.
//
// The counterpart of BoardBringUpWizard, and shorter than it in every
// dimension for one reason: **a unit that can be rolled back is a working
// unit**. It is running this application's own firmware over gateware with a
// flash bridge, so it can write both images itself, over the cable it is
// already using. No JTAG, no jumper, no screwdriver, one power cycle.
//
// What it does share with the bring-up wizard is the thing that matters:
// `CTL_07`/`GPIO_24` is driven by the original firmware and by the current
// gateware, so those two must never run together. The ordering that avoids it
// is the reverse of bring-up's — **the FPGA becomes legacy first** — and, as
// there, this class does not enforce it. RollbackOrchestrator does, by
// refusing the FX3 until the FPGA is done, so a page wired up wrongly is a
// refused operation rather than two outputs on one wire. What this class owes
// it is the page order, and there is a widget test that asserts exactly that.
//
// Hand-built on the AutoCaptureWizard pattern rather than QWizard, for the
// same reasons: an object name on every control, a window that builds with
// every factory null so the whole flow is drivable in a widget test with
// nothing attached, and pages that refuse to navigate while a run is going.
//
// Thread-safety: NOT thread-safe. Interface thread only. Each half runs on a
// worker thread and reports back through queued signals.
class LegacyRollbackWizard : public QDialog {
  Q_OBJECT

 public:
  // How the wizard reaches hardware. Every one of these may be null, and the
  // widget tests pass a fake for each.
  struct Access {
    // What the application currently sees on the bus, polled at human speed.
    std::function<std::vector<capture::DeviceInfo>()> devices;

    // Opens an updater for the device at `path`. This flow's only route to
    // hardware: both images go through it, and the last page reads the device
    // back through it too.
    std::function<std::unique_ptr<capture::IDeviceUpdater>(
        const std::string& path)>
        open_updater;
  };

  explicit LegacyRollbackWizard(Access access, QWidget* parent = nullptr);
  ~LegacyRollbackWizard() override;

  // Which signatures this wizard accepts. The same policy, from the same
  // place, as the update page: a rollback set is an ordinary signed bundle and
  // there is no second set of rules for it.
  void SetKeyPolicy(capture::UpdateKeyPolicy policy) {
    policy_ = std::move(policy);
  }

  // Load a rollback file, exactly as the Choose button does. Separate from the
  // button so a test can drive the flow without a file dialog, which is modal
  // and native and cannot be driven at all.
  void LoadRollbackSet(const QString& path);

  RollbackPage page() const { return page_; }
  bool busy() const { return worker_ != nullptr; }

  // The pages this run visits, in order. The property a hardware-safety rule
  // deserves stated as data rather than inferred from a sequence of button
  // presses.
  std::vector<RollbackPage> Steps() const;

  // Object names, so a widget test can reach a control without depending on
  // the order things were added to a layout.
  static constexpr const char* kPagesName = "rollback_pages";
  static constexpr const char* kPreviousButtonName = "rollback_previous";
  static constexpr const char* kNextButtonName = "rollback_next";
  static constexpr const char* kCloseButtonName = "rollback_close";
  static constexpr const char* kHeadingName = "rollback_heading";
  static constexpr const char* kStepLabelName = "rollback_step";

  static constexpr const char* kOverviewTextName = "rollback_overview";
  static constexpr const char* kConfirmPromptName = "rollback_confirm_prompt";
  static constexpr const char* kConfirmFieldName = "rollback_confirm";

  static constexpr const char* kDeviceRowName = "rollback_device_row";
  static constexpr const char* kCheckAgainButtonName = "rollback_check_again";

  static constexpr const char* kChooseButtonName = "rollback_choose";
  static constexpr const char* kImageTextName = "rollback_image_text";
  static constexpr const char* kImageLabelName = "rollback_image";
  static constexpr const char* kImageBannerName = "rollback_image_banner";

  static constexpr const char* kGatewareTextName = "rollback_gateware";
  static constexpr const char* kGatewareStatusName = "rollback_gateware_status";
  static constexpr const char* kGatewareProgressName =
      "rollback_gateware_progress";
  static constexpr const char* kGatewareStartButtonName =
      "rollback_gateware_go";

  static constexpr const char* kFirmwareTextName = "rollback_firmware";
  static constexpr const char* kFirmwareStatusName = "rollback_firmware_status";
  static constexpr const char* kFirmwareProgressName =
      "rollback_firmware_progress";
  static constexpr const char* kFirmwareStartButtonName =
      "rollback_firmware_go";

  static constexpr const char* kStopButtonName = "rollback_stop";

  static constexpr const char* kPowerCycleTextName = "rollback_power_cycle";
  static constexpr const char* kPowerCycleStatusName =
      "rollback_power_cycle_status";

  static constexpr const char* kVerifyTextName = "rollback_verify";
  static constexpr const char* kVerifySummaryName = "rollback_verify_summary";

 signals:
  // Raised when a half starts and again when it ends, so the window can keep
  // the device monitor out of a programming run's way.
  void BusyChanged(bool busy);

 public slots:
  void GoNext();
  void GoPrevious();

  // Look at the bus now, rather than waiting out the poll interval.
  void Poll();

  // Begin each half. Both do nothing when one is already running or when the
  // page is not ready — the buttons are disabled in both cases, and this is
  // the second half of that rule for anything that reaches them another way.
  void StartGateware();
  void StartFirmware();

  void Stop();

  void reject() override;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  QWidget* BuildOverviewPage();
  QWidget* BuildConnectPage();
  QWidget* BuildImagePage();
  QWidget* BuildGatewarePage();
  QWidget* BuildFirmwarePage();
  QWidget* BuildPowerCyclePage();
  QWidget* BuildVerifyPage();

  void ShowPage(RollbackPage page);
  void Refresh();
  bool PageIsSatisfied(RollbackPage page) const;

  std::optional<RollbackPage> After(RollbackPage page) const;
  std::optional<RollbackPage> Before(RollbackPage page) const;

  void ReadDevices();

  // The Duplicator as it is right now, or nothing if none is attached. A
  // legacy device counts: it is what the last page is looking for.
  std::optional<capture::DeviceInfo> Device() const;

  // What the device says about itself, read through an updater. Empty for a
  // device that cannot be opened, which the connectivity row reports.
  std::optional<capture::DeviceIdentity> ReadIdentity() const;

  void StartTask(int task);
  void FinishTask(bool succeeded, bool stopped, const QString& problem);

  void Verify();

  bool RefuseWhileRunning();

  Access access_;
  capture::UpdateKeyPolicy policy_;

  RollbackPage page_ = RollbackPage::kOverview;

  // Typed on the first page. Not a checkbox, because this is the one thing
  // the application does that the application cannot undo.
  bool confirmed_ = false;

  // The chosen rollback file: the bytes, so the worker can verify them again
  // rather than trusting this window's verification, and the manifest, for
  // what the page shows.
  std::vector<uint8_t> archive_;
  std::optional<capture::UpdateManifest> manifest_;
  QString chosen_path_;

  // Where the device was when the run started. Held because a rollback writes
  // to one device and the bus may hold two.
  std::string device_path_;

  std::vector<capture::DeviceInfo> devices_;

  bool gateware_done_ = false;
  bool firmware_done_ = false;
  bool returned_ = false;

  int power_cycle_polls_ = 0;

  // Held open across both halves, because the device does not go away between
  // them: it is the same device, still running the same firmware, and opening
  // it twice would be opening it twice.
  std::unique_ptr<capture::IDeviceUpdater> updater_;
  std::unique_ptr<capture::RollbackOrchestrator> orchestrator_;

  QTimer* timer_ = nullptr;
  QThread* thread_ = nullptr;
  RollbackWorker* worker_ = nullptr;
  int running_task_ = 0;

  QStackedWidget* pages_ = nullptr;
  QLabel* heading_ = nullptr;
  QLabel* step_ = nullptr;
  QPushButton* previous_button_ = nullptr;
  QPushButton* next_button_ = nullptr;

  QLineEdit* confirm_field_ = nullptr;

  QLabel* device_row_ = nullptr;

  QLabel* image_ = nullptr;
  QLabel* image_banner_ = nullptr;

  QLabel* gateware_text_ = nullptr;
  QLabel* gateware_status_ = nullptr;
  QProgressBar* gateware_progress_ = nullptr;
  QPushButton* gateware_start_ = nullptr;

  QLabel* firmware_text_ = nullptr;
  QLabel* firmware_status_ = nullptr;
  QProgressBar* firmware_progress_ = nullptr;
  QPushButton* firmware_start_ = nullptr;

  QPushButton* stop_button_ = nullptr;

  QLabel* power_cycle_status_ = nullptr;

  QLabel* verify_ = nullptr;
  QLabel* verify_summary_ = nullptr;
};

}  // namespace ddd::gui
