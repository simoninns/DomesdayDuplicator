/************************************************************************

    board_bringup_wizard.h

    Programming both halves of a board from nothing, in nine pages
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

#include "bringup_text.h"
#include "device_programmer.h"
#include "device_updater.h"
#include "jtag_cable.h"
#include "provisioning_orchestrator.h"
#include "update_key.h"
#include "update_manifest.h"
#include "usb_device_info.h"
#include "usb_presence.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QThread;
class QTimer;

namespace ddd::gui {

class BringUpWorker;

// Bringing a board up: the FX3's firmware and the FPGA's configuration flash,
// programmed from nothing.
//
// **The shape of this is decided by two facts about the hardware**, and
// neither is negotiable.
//
// The first is about the case. The FX3's PMODE header can be reached with the
// unit assembled; the DE0-Nano's mini-USB connector cannot. So *any* FPGA work
// means opening the case and no FX3 work does — and a wizard that did the FX3
// first, asked the resulting firmware whether the gateware needed anything and
// only then sent the user for a screwdriver would send them for one nearly
// every time, after telling them they would not need it. Firmware and gateware
// are built and released together, so a board that is out of date is out of
// date in both halves. This wizard therefore assumes the whole job from the
// first page: both cables on, case off, both halves programmed, nothing
// physical asked for twice.
//
// The second is about a wire. `CTL_07`/`GPIO_24` is driven by the original
// firmware and driven by the current gateware, so those two must never be
// running together — while the reverse mixture leaves the net undriven from
// both ends, which is merely a floating input. The FX3 is therefore always the
// first thing to become modern. That rule is not enforced here: it is enforced
// in ProvisioningOrchestrator, which refuses the FPGA until the FX3 is done,
// so a page wired up wrongly is a refused operation rather than two outputs on
// one wire. What this class owes it is the page order, and there is a widget
// test that asserts exactly that.
//
// Hand-built on the AutoCaptureWizard pattern rather than QWizard, and for the
// same reasons: an object name on every control, a window that builds with
// every factory null so the whole flow is drivable in a widget test with
// nothing attached, and pages that refuse to navigate while a run is going.
//
// Thread-safety: NOT thread-safe. Interface thread only. Each half runs on a
// worker thread and reports back through queued signals.
class BoardBringUpWizard : public QDialog {
  Q_OBJECT

 public:
  // How the wizard reaches hardware. Every one of these may be null, and the
  // widget tests pass a fake for each: the pages then build, poll, navigate
  // and report exactly as they do in the application, and program nothing.
  struct Access {
    // What the application currently sees on the bus. Polled at human speed
    // while a page is waiting for something to change.
    std::function<std::vector<capture::DeviceInfo>()> devices;

    // Whether something with these identifiers is attached at all, without
    // opening it. What tells "no cable" apart from "a cable this machine will
    // not let me open", which have completely different remedies.
    std::function<capture::UsbPresence(uint16_t, uint16_t)> presence;

    // Opens the DE0-Nano's USB-Blaster, or returns nothing having written a
    // sentence into `problem`.
    std::function<std::unique_ptr<capture::IJtagCable>(std::string* problem)>
        open_cable;

    // Opens a programmer for the FX3 sitting in its boot ROM at `path`.
    std::function<std::unique_ptr<capture::IDeviceProgrammer>(
        const std::string& path)>
        open_programmer;

    // Opens an updater for the device at `path`. Used twice: by the FX3 step,
    // for the firmware that has just been loaded into RAM, and by the last
    // page, to read back what the finished device says it is running.
    std::function<std::unique_ptr<capture::IDeviceUpdater>(
        const std::string& path)>
        open_updater;
  };

  explicit BoardBringUpWizard(Access access, QWidget* parent = nullptr);
  ~BoardBringUpWizard() override;

  // Which signatures this wizard accepts. Defaults to DefaultUpdateKeyPolicy()
  // — the same policy, from the same place, as the update page, because a
  // provisioning set is an ordinary signed bundle and there is no second set
  // of rules for it.
  void SetKeyPolicy(capture::UpdateKeyPolicy policy) {
    policy_ = std::move(policy);
  }

  // Load a provisioning set from a file, exactly as the Choose button does.
  // Separate from the button so a test can drive the flow without a file
  // dialog, which is modal and native and cannot be driven at all.
  void LoadProvisioningSet(const QString& path);

  BringUpPage page() const { return page_; }
  bool busy() const { return worker_ != nullptr; }

  // Whether this run has to send the user to the jumper. False for a board
  // already waiting in its boot ROM, which is what a freshly soldered kit
  // does — the two jumper pages are then not in the flow at all.
  bool jumper_needed() const { return jumper_needed_; }

  // The pages this run will visit, in order. The property a hardware-safety
  // rule deserves stated as data rather than inferred from a sequence of
  // button presses.
  std::vector<BringUpPage> Steps() const;

  // Object names, so a widget test can reach a control without depending on
  // the order things were added to a layout.
  static constexpr const char* kPagesName = "bringup_pages";
  static constexpr const char* kPreviousButtonName = "bringup_previous";
  static constexpr const char* kNextButtonName = "bringup_next";
  static constexpr const char* kCloseButtonName = "bringup_close";
  static constexpr const char* kHeadingName = "bringup_heading";
  static constexpr const char* kStepLabelName = "bringup_step";

  static constexpr const char* kOverviewTextName = "bringup_overview";
  static constexpr const char* kOverviewPhotographName =
      "bringup_overview_photo";

  static constexpr const char* kFx3RowName = "bringup_fx3_row";
  static constexpr const char* kFpgaRowName = "bringup_fpga_row";
  static constexpr const char* kCheckAgainButtonName = "bringup_check_again";

  static constexpr const char* kChooseButtonName = "bringup_choose";
  static constexpr const char* kImageLabelName = "bringup_image";
  static constexpr const char* kImageBannerName = "bringup_image_banner";

  static constexpr const char* kJumperTextName = "bringup_jumper";
  static constexpr const char* kJumperPhotographName = "bringup_jumper_photo";
  static constexpr const char* kJumperStatusName = "bringup_jumper_status";

  static constexpr const char* kFirmwareTextName = "bringup_firmware";
  static constexpr const char* kFirmwareStatusName = "bringup_firmware_status";
  static constexpr const char* kFirmwareProgressName =
      "bringup_firmware_progress";
  static constexpr const char* kFirmwareStartButtonName = "bringup_firmware_go";

  static constexpr const char* kRemoveJumperTextName = "bringup_remove_jumper";
  static constexpr const char* kRemoveJumperPhotographName =
      "bringup_remove_jumper_photo";

  static constexpr const char* kGatewareTextName = "bringup_gateware";
  static constexpr const char* kGatewareStatusName = "bringup_gateware_status";
  static constexpr const char* kGatewareProgressName =
      "bringup_gateware_progress";
  static constexpr const char* kGatewareStartButtonName = "bringup_gateware_go";

  static constexpr const char* kStopButtonName = "bringup_stop";

  static constexpr const char* kPowerCycleTextName = "bringup_power_cycle";
  static constexpr const char* kPowerCycleStatusName =
      "bringup_power_cycle_status";

  static constexpr const char* kVerifyTextName = "bringup_verify";
  static constexpr const char* kVerifySummaryName = "bringup_verify_summary";
  static constexpr const char* kUpdateNowButtonName = "bringup_update_now";

 signals:
  // The last page's button. The wizard does not open the update dialog
  // itself: it is a window this one knows nothing about, and the window that
  // owns both is the one place that should decide what opening one means.
  void OpenUpdateRequested();

  // Raised when a half starts and again when it ends, so the window can keep
  // the device monitor out of a programming run's way and know not to start a
  // capture in the middle of one.
  void BusyChanged(bool busy);

 public slots:
  void GoNext();
  void GoPrevious();

  // Look at the bus now, rather than waiting out the poll interval. What the
  // *Check again* button does, and what the widget tests call to advance the
  // flow one observation at a time.
  void Poll();

  // Begin each half. Both do nothing when one is already running or when the
  // page is not ready — the buttons are disabled in both cases, and this is
  // the second half of that rule for anything that reaches them another way.
  void StartFirmware();
  void StartGateware();

  // Ask the run in hand to stop at its next safe point.
  void Stop();

  void reject() override;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  QWidget* BuildOverviewPage();
  QWidget* BuildConnectPage();
  QWidget* BuildImagePage();
  QWidget* BuildJumperPage();
  QWidget* BuildFirmwarePage();
  QWidget* BuildRemoveJumperPage();
  QWidget* BuildGatewarePage();
  QWidget* BuildPowerCyclePage();
  QWidget* BuildVerifyPage();

  void ShowPage(BringUpPage page);

  // Set every button and every gate from the state in hand. One function,
  // called after anything changes, because a wizard whose Next button is only
  // correct on the page it was drawn on is a wizard that offers a step it will
  // refuse.
  void Refresh();

  // Whether the page in hand has finished what it is for.
  bool PageIsSatisfied(BringUpPage page) const;

  // The next and previous pages this run visits, honouring the jumper skip.
  std::optional<BringUpPage> After(BringUpPage page) const;
  std::optional<BringUpPage> Before(BringUpPage page) const;

  // Look at the bus and update whichever page cares. Called by the timer and
  // by Poll().
  void ReadDevices();
  void ProbeCable(bool force);

  // The FX3 as it is right now, or nothing if none is attached.
  std::optional<capture::DeviceInfo> Fx3() const;

  // Start one half on a worker thread.
  void StartTask(int task);
  void FinishTask(bool succeeded, bool stopped, const QString& problem);

  // Read the finished device back and fill the last page in.
  void Verify();

  bool RefuseWhileRunning();

  Access access_;
  capture::UpdateKeyPolicy policy_;

  BringUpPage page_ = BringUpPage::kOverview;

  // Decided once, on the way out of the connectivity page: a board already in
  // its boot ROM needs no jumper, and asking for one would be asking somebody
  // to fit a jumper so that the wizard could ask them to take it off again.
  bool jumper_needed_ = true;

  // The chosen provisioning set: the bytes, so the worker can verify them
  // again rather than trusting this window's verification, and the manifest,
  // for what the page shows.
  std::vector<uint8_t> archive_;
  std::optional<capture::UpdateManifest> manifest_;

  // What the bus last looked like, and what the cable last said.
  std::vector<capture::DeviceInfo> devices_;
  capture::UsbPresence cable_presence_ = capture::UsbPresence::kUnknown;
  capture::UsbPresence probed_presence_ = capture::UsbPresence::kUnknown;
  bool cable_opened_ = false;
  QString cable_problem_;

  // Whether each half has been done, so that going back and forth between
  // pages does not offer to do one of them twice.
  bool firmware_done_ = false;
  bool gateware_done_ = false;
  bool returned_ = false;

  // How long the power-cycle page has been waiting, in polls, so that it can
  // stop being patient and start being helpful.
  int power_cycle_polls_ = 0;

  std::unique_ptr<capture::ProvisioningOrchestrator> orchestrator_;

  QTimer* timer_ = nullptr;
  QThread* thread_ = nullptr;
  BringUpWorker* worker_ = nullptr;

  // Which half is running, so the report that arrives when one finishes goes
  // to the page that started it. Inferring it from what is already done would
  // be a guess at exactly the moment the answer matters.
  int running_task_ = 0;

  QStackedWidget* pages_ = nullptr;
  QLabel* heading_ = nullptr;
  QLabel* step_ = nullptr;
  QPushButton* previous_button_ = nullptr;
  QPushButton* next_button_ = nullptr;

  QLabel* fx3_row_ = nullptr;
  QLabel* fpga_row_ = nullptr;

  QLabel* image_ = nullptr;
  QLabel* image_banner_ = nullptr;

  QLabel* jumper_status_ = nullptr;

  QLabel* firmware_status_ = nullptr;
  QProgressBar* firmware_progress_ = nullptr;
  QPushButton* firmware_start_ = nullptr;

  QLabel* gateware_text_ = nullptr;
  QLabel* gateware_status_ = nullptr;
  QProgressBar* gateware_progress_ = nullptr;
  QPushButton* gateware_start_ = nullptr;

  QPushButton* stop_button_ = nullptr;

  QLabel* power_cycle_status_ = nullptr;

  QLabel* verify_ = nullptr;
  QLabel* verify_summary_ = nullptr;
  QPushButton* update_now_ = nullptr;
};

}  // namespace ddd::gui
