/************************************************************************

    firmware_dialog.h

    The Firmware box: which build each half of the device is running
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <optional>

#include "firmware_text.h"
#include "update_page.h"

class QLabel;
class QTabWidget;

namespace ddd::gui {

// Shows the application's build alongside the FX3 firmware's and the FPGA
// gateware's, says whether they agree, and — on its second page — installs a
// new one.
//
// Reachable from the Help menu rather than raised on its own, because a version
// difference is a note and not a fault — the application monitors and captures
// normally whatever this says. What it exists for is the moment when something
// is behaving oddly and the first question is whether the three halves of the
// device match; and, now, the moment when the answer to that is "update it".
//
// The versions page is passed everything at construction and reads no device,
// so opening the dialog can never block and never changes what the device is
// doing. The update page reaches a device only when a user has chosen a file
// and confirmed.
//
// Thread-safety: NOT thread-safe. GUI thread only.
class FirmwareDialog : public QDialog {
  Q_OBJECT

 public:
  // Versions only: the dialog the Help menu opened before there was anything
  // to install, and what a build with no device handling constructs.
  explicit FirmwareDialog(const FirmwareVersions& versions,
                          QWidget* parent = nullptr);

  // With an update page.
  FirmwareDialog(const FirmwareVersions& versions, UpdatePage::Device device,
                 QWidget* parent = nullptr);

  // The width the text is laid out at, matching the About dialog's.
  static constexpr int kTextWidthPixels = 520;

  // Object names, so a widget test can reach the text without depending on the
  // order things were added to the layout.
  static constexpr const char* kTextLabelName = "firmware_text";
  static constexpr const char* kScrollAreaName = "firmware_scroll";
  static constexpr const char* kTabsName = "firmware_tabs";
  static constexpr const char* kUpdatePageName = "firmware_update_page";

  // The update page, or nothing when the dialog was built without one.
  UpdatePage* update_page() const { return update_; }

 protected:
  // Refused while an update is running, because closing this window is how a
  // user would try to stop one — and stopping one by closing the window that
  // is driving it is the one way to leave a device half-written.
  void closeEvent(QCloseEvent* event) override;
  void reject() override;

 private:
  void Build(const FirmwareVersions& versions,
             std::optional<UpdatePage::Device> device);

  // True while an update is running, so the two refusals above have one
  // answer between them.
  bool CanClose();

  QLabel* text_ = nullptr;
  QTabWidget* tabs_ = nullptr;
  UpdatePage* update_ = nullptr;
};

}  // namespace ddd::gui
