/************************************************************************

    board_bringup_wizard.h

    Programming a board from nothing to fully up to date, in nine pages
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

#include "bringup_orchestrator.h"
#include "bringup_text.h"
#include "device_programmer.h"
#include "device_updater.h"
#include "jtag_cable.h"
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

// Bringing a board up: the FX3's firmware and both images in the FPGA's
// configuration flash, programmed from nothing, ending with a device that is
// ready to capture.
//
// **The shape of this is decided by two facts about the hardware**, and
// neither is negotiable.
//
// The first is about the case. The FX3's PMODE header can be reached with the
// unit assembled; the DE0-Nano's mini-USB connector cannot. So this needs the
// case off, and it says so on the first page rather than discovering it later:
// both cables on, case off, nothing physical asked for twice.
//
// The second is about a wire. `CTL_07`/`GPIO_24` is driven by the original
// firmware and driven by the current gateware, so those two must never be
// running together — while the reverse mixture leaves the net undriven from
// both ends, which is merely a floating input. What keeps a board out of the
// bad pairing is where the FX3 is while the FPGA changes: in its boot ROM,
// with every shared pin idle. So the configure page comes before the page that
// writes, and no firmware runs again until it is the firmware out of the
// bundle. That rule is not enforced here — it is enforced in
// BringUpOrchestrator, which refuses to write anything until the FPGA has been
// configured, so a page wired up wrongly is a refused operation rather than
// two outputs on one wire. What this class owes it is the page order, and
// there is a widget test that asserts exactly that.
//
// **Nothing here diagnoses the board.** Fitting the jumper reaches the FX3's
// boot ROM from any state whatever, so the flow is the same for a bare kit, a
// legacy unit, a current one, and one left half-programmed by a run somebody
// stopped. The one conditional is a courtesy: a board already in its boot ROM
// skips the two jumper pages.
//
// **Every page tells the user where it has got to, in the same two shapes.**
// A page that is not finished says what it is waiting for; a page that is
// finished leads with *All done* and names the button to press. Both come from
// bringup_text so they cannot drift, and both sit at the bottom of the page,
// under whatever control does the work — which is where somebody is looking
// after they press it. A step that is finished also has its start button
// disabled and relabelled, so that "this has happened" is legible from the
// buttons alone.
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

    // Where this build's own update file is, or an empty string when it
    // carries none. A function rather than a path so that the search — which
    // is about how the application was installed — stays out of this class,
    // and so that a test can state what a build carries instead of installing
    // one.
    std::function<QString()> bundled_file;
  };

  explicit BoardBringUpWizard(Access access, QWidget* parent = nullptr);
  ~BoardBringUpWizard() override;

  // Which signatures this wizard accepts. Defaults to DefaultUpdateKeyPolicy()
  // — the same policy, from the same place, as the update page, because this
  // is the same file the update page installs and there is no second set of
  // rules for it.
  void SetKeyPolicy(capture::UpdateKeyPolicy policy) {
    policy_ = std::move(policy);
  }

  // Load an update file, exactly as the Choose button does. Separate from the
  // button so a test can drive the flow without a file dialog, which is modal
  // and native and cannot be driven at all.
  void LoadUpdateFile(const QString& path);

  // The file this build came with, or empty when it came with none. What the
  // image page starts on.
  const QString& bundled_path() const { return bundled_path_; }

  // Whether the file in hand is that one, rather than one somebody chose.
  bool using_bundled_file() const {
    return !bundled_path_.isEmpty() && chosen_path_ == bundled_path_;
  }

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
  static constexpr const char* kConnectLegendName = "bringup_connect_legend";
  static constexpr const char* kCheckAgainButtonName = "bringup_check_again";
  static constexpr const char* kConnectStatusName = "bringup_connect_status";

  static constexpr const char* kChooseButtonName = "bringup_choose";
  static constexpr const char* kUseBundledButtonName = "bringup_use_bundled";

  static constexpr const char* kImageSourceName = "bringup_image_source";
  static constexpr const char* kImageLabelName = "bringup_image";
  static constexpr const char* kImageBannerName = "bringup_image_banner";
  static constexpr const char* kImageStatusName = "bringup_image_status";

  static constexpr const char* kJumperTextName = "bringup_jumper";
  static constexpr const char* kJumperPhotographName = "bringup_jumper_photo";
  static constexpr const char* kJumperStatusName = "bringup_jumper_status";

  static constexpr const char* kConfigureTextName = "bringup_configure";
  static constexpr const char* kConfigureStatusName =
      "bringup_configure_status";
  static constexpr const char* kConfigureProgressName =
      "bringup_configure_progress";
  static constexpr const char* kConfigureStartButtonName =
      "bringup_configure_go";
  static constexpr const char* kConfigureStopButtonName =
      "bringup_configure_stop";

  static constexpr const char* kProgramTextName = "bringup_program";
  static constexpr const char* kProgramStatusName = "bringup_program_status";
  static constexpr const char* kProgramProgressName =
      "bringup_program_progress";
  static constexpr const char* kProgramStartButtonName = "bringup_program_go";
  static constexpr const char* kProgramStopButtonName = "bringup_program_stop";

  static constexpr const char* kRemoveJumperTextName = "bringup_remove_jumper";
  static constexpr const char* kRemoveJumperPhotographName =
      "bringup_remove_jumper_photo";
  static constexpr const char* kRemoveJumperStatusName =
      "bringup_remove_jumper_status";

  static constexpr const char* kPowerCycleTextName = "bringup_power_cycle";
  static constexpr const char* kPowerCycleStatusName =
      "bringup_power_cycle_status";

  static constexpr const char* kVerifyTextName = "bringup_verify";
  static constexpr const char* kVerifySummaryName = "bringup_verify_summary";

 signals:
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
  void StartConfigure();
  void StartProgram();

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
  QWidget* BuildConfigurePage();
  QWidget* BuildProgramPage();
  QWidget* BuildRemoveJumperPage();
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

  // What the power-cycle page has been able to establish, in the order it
  // establishes it.
  //
  // **This page cannot go by what is on the bus**, which is the mistake it
  // used to make. The step before it hands the firmware to the FX3's boot ROM
  // and runs it out of RAM, so a fully working Duplicator is enumerating
  // before anybody touches a cable — and a page that asked "is a Duplicator
  // attached?" answered yes and reported a power cycle that had not happened.
  //
  // So the page waits for two things it can actually observe. The device has
  // to **go away**, which is what proves a cable came out. And it has to come
  // back **on the application image**, which is what proves the board lost
  // power: the gateware step put the *factory* image into the FPGA over JTAG,
  // and only a power cycle makes it reload from flash. That second check is
  // what catches the failure this whole page warns about — pulling the USB 3.0
  // cable alone makes the device vanish and return while the mini-USB keeps
  // the board alive, so the firmware in RAM and the gateware in the FPGA both
  // survive it.
  enum class PowerCycleState {
    // Still on the bus. Nothing has happened yet.
    kStillHere,

    // Observed absent, so at least one cable is out.
    kGone,

    // Back, in its boot ROM: jumper J4 is still fitted.
    kBootRom,

    // Back, running its firmware, and the FPGA is still on the image JTAG put
    // there — so the board never lost power.
    kNotReloaded,

    // Back, and running out of its own flash. The power cycle happened.
    kBack,
  };

  // Whether the device that has just come back is running from its own flash,
  // read off the device rather than assumed.
  //
  // Only a positive reading of the factory image counts as "this did not
  // happen": no updater, no identity, or gateware too old to report a role are
  // all things this page cannot tell, and the verification page checks
  // properly. A wizard that trapped somebody on step 8 over something it could
  // not read would be worse than one that let them reach the page whose whole
  // job is to say what the device is running.
  bool ReturnedOnItsOwnFlash();

  // The FX3 as it is right now, or nothing if none is attached.
  std::optional<capture::DeviceInfo> Fx3() const;

  // Start one half on a worker thread.
  void StartTask(int task);
  void FinishTask(bool succeeded, bool stopped, const QString& problem);

  // Read the finished device back and fill the last page in.
  void Verify();

  // Load the file this build came with, the first time the image page is
  // reached and only if nothing has been chosen.
  void LoadBundledFileOnce();

  bool RefuseWhileRunning();

  Access access_;
  capture::UpdateKeyPolicy policy_;

  BringUpPage page_ = BringUpPage::kOverview;

  // Decided once, on the way out of the connectivity page: a board already in
  // its boot ROM needs no jumper, and asking for one would be asking somebody
  // to fit a jumper so that the wizard could ask them to take it off again.
  bool jumper_needed_ = true;

  // The chosen update file: the bytes, so the worker can verify them again
  // rather than trusting this window's verification, and the manifest, for
  // what the page shows.
  std::vector<uint8_t> archive_;
  std::optional<capture::UpdateManifest> manifest_;

  // Where this build's own file is, asked for once at construction, and which
  // file is in hand — the two together are what the image page's first
  // paragraph is about.
  QString bundled_path_;
  QString chosen_path_;
  bool bundled_tried_ = false;

  // What the bus last looked like, and what the cable last said.
  std::vector<capture::DeviceInfo> devices_;
  capture::UsbPresence cable_presence_ = capture::UsbPresence::kUnknown;
  capture::UsbPresence probed_presence_ = capture::UsbPresence::kUnknown;
  bool cable_opened_ = false;
  QString cable_problem_;

  // Whether each half has been done, so that going back and forth between
  // pages does not offer to do one of them twice.
  bool configured_ = false;
  bool programmed_ = false;
  bool returned_ = false;

  // Whether a run has left a sentence on one of the two working pages that
  // Refresh() must not write over — a failure, or a stop. Everything else on
  // those pages is derived state that Refresh() owns, and a step that finished
  // badly is the one thing it cannot derive: the page would go back to saying
  // it was waiting to be started, which is true and is not what somebody who
  // has just watched a run fail needs to read.
  bool configure_reported_ = false;
  bool program_reported_ = false;

  // How long the power-cycle page has been waiting, in polls, so that it can
  // stop being patient and start being helpful.
  int power_cycle_polls_ = 0;

  // How far that page has got. Reset every time it is arrived at, so that a
  // run somebody walked backwards through does not carry an observation about
  // a cable that came out several minutes ago.
  PowerCycleState power_cycle_state_ = PowerCycleState::kStillHere;

  std::unique_ptr<capture::BringUpOrchestrator> orchestrator_;

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
  QLabel* connect_status_ = nullptr;

  QLabel* image_source_ = nullptr;
  QLabel* image_ = nullptr;
  QLabel* image_banner_ = nullptr;
  QLabel* image_status_ = nullptr;
  QPushButton* use_bundled_ = nullptr;

  QLabel* jumper_status_ = nullptr;

  QLabel* configure_text_ = nullptr;
  QLabel* configure_status_ = nullptr;
  QProgressBar* configure_progress_ = nullptr;
  QPushButton* configure_start_ = nullptr;
  QPushButton* configure_stop_ = nullptr;

  QLabel* program_text_ = nullptr;
  QLabel* program_status_ = nullptr;
  QProgressBar* program_progress_ = nullptr;
  QPushButton* program_start_ = nullptr;
  QPushButton* program_stop_ = nullptr;

  QLabel* power_cycle_status_ = nullptr;

  QLabel* verify_ = nullptr;
  QLabel* verify_summary_ = nullptr;
};

}  // namespace ddd::gui
