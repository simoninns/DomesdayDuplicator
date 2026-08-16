/************************************************************************

    player_remote_dialog.h

    The player's remote control
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QDialog>
#include <QString>
#include <cstdint>
#include <vector>

#include "player_command.h"
#include "player_connection.h"
#include "player_request.h"
#include "player_status.h"

class QBoxLayout;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QValidator;

namespace ddd::gui {

class PlayerController;

// Driving the player by hand.
//
// **Non-modal**, and that is the point of it rather than a detail: setting up a
// capture consists of moving the player about while watching the spectrum and
// the waveform, and a window that blocked the rest of the application would
// make the one thing it is for impossible.
//
// Three departures from the old application's remote:
//
//   **The buttons come from the player.** A control the connected model does
//   not have is disabled, with a tooltip naming the models that do have it. The
//   old dialog offered every button to every player, so a control the player
//   lacked was present, enabled, and silently did nothing — which from the
//   user's side is indistinguishable from a broken cable.
//
//   **The addressing follows the disc.** With a CLV disc loaded, frame entry is
//   not offered, because a frame number means nothing on one. This is the same
//   principle the examine flow is built on, applied here.
//
//   **Repeat is gone.** It was never functional in the old application.
//
// The manual command field is kept deliberately, and kept for unrecognised
// players in particular: it is the tool that lets somebody work out what an
// undocumented player does, and therefore the tool that lets the next
// definition header be written. Its reply is shown exactly as it arrived.
//
// Thread-safety: NOT thread-safe. GUI thread only. Nothing here touches the
// player; every control submits a request to the controller and waits.
class PlayerRemoteDialog : public QDialog {
  Q_OBJECT

 public:
  // The controller may be null, and the widget tests pass null deliberately:
  // the dialog then builds, lays out and drives nothing, exactly as the panels
  // do. Its state can still be set through the two slots below, which is what
  // lets the capability gating be tested against models that are not on
  // anybody's bench.
  explicit PlayerRemoteDialog(PlayerController* controller,
                              QWidget* parent = nullptr);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kHeadlineLabelName = "remote_headline";

  static constexpr const char* kTrayOpenButtonName = "remote_tray_open";
  static constexpr const char* kTrayCloseButtonName = "remote_tray_close";
  static constexpr const char* kStopButtonName = "remote_stop";
  static constexpr const char* kPlayButtonName = "remote_play";
  static constexpr const char* kPlayIgnoringStopCodesButtonName =
      "remote_play_ignoring_stop_codes";
  static constexpr const char* kPauseButtonName = "remote_pause";
  static constexpr const char* kStillButtonName = "remote_still";
  static constexpr const char* kStepReverseButtonName = "remote_step_reverse";
  static constexpr const char* kStepForwardButtonName = "remote_step_forward";
  static constexpr const char* kScanReverseButtonName = "remote_scan_reverse";
  static constexpr const char* kScanForwardButtonName = "remote_scan_forward";
  static constexpr const char* kMultiSpeedReverseButtonName =
      "remote_multi_speed_reverse";
  static constexpr const char* kMultiSpeedForwardButtonName =
      "remote_multi_speed_forward";
  static constexpr const char* kSpeedComboName = "remote_speed";

  static constexpr const char* kAddressModeComboName = "remote_address_mode";
  static constexpr const char* kAddressEditName = "remote_address";
  static constexpr const char* kSearchButtonName = "remote_search";
  static constexpr const char* kSearchNoteLabelName = "remote_search_note";

  static constexpr const char* kDisplayOnButtonName = "remote_display_on";
  static constexpr const char* kDisplayOffButtonName = "remote_display_off";
  static constexpr const char* kAudioComboName = "remote_audio";
  static constexpr const char* kKeyLockOnButtonName = "remote_key_lock_on";
  static constexpr const char* kKeyLockOffButtonName = "remote_key_lock_off";

  static constexpr const char* kStandardUserCodeButtonName =
      "remote_standard_user_code";
  static constexpr const char* kPioneerUserCodeButtonName =
      "remote_pioneer_user_code";
  static constexpr const char* kUserCodeViewName = "remote_user_code";

  static constexpr const char* kManualEditName = "remote_manual";
  static constexpr const char* kManualSendButtonName = "remote_manual_send";
  static constexpr const char* kManualReplyViewName = "remote_manual_reply";

 public slots:
  // Wired to the controller when there is one, and called directly by the
  // widget tests when there is not.
  void SetConnection(const ddd::gui::PlayerConnection& connection);
  void SetStatus(const ddd::player::PlayerStatus& status);

 private:
  QWidget* BuildTransport();
  QWidget* BuildSearch();
  QWidget* BuildPresentation();
  QWidget* BuildUserCodes();
  QWidget* BuildManualCommand();

  // Make a button that sends one command, and enrol it in the capability
  // gating. Every transport, display and key-lock button goes through here, so
  // there is one rule about when a control is offered rather than one per
  // button.
  QPushButton* AddCommandButton(QWidget* parent, QBoxLayout* into,
                                const char* object_name, const QString& label,
                                player::PlayerCommand command,
                                const QString& help);

  // Submit a request, or do nothing at all when there is no controller.
  uint64_t Send(const PlayerRequest& request);

  void OnRequestCompleted(const PlayerReply& reply);
  void OnSearch();
  void OnManualSend();

  // Offer exactly what this player can do. Called whenever the connection
  // changes, which includes losing it.
  void ApplyControls();

  // Offer exactly the ways this disc can be addressed. Called when the disc
  // type changes, which is the only thing it depends on.
  void ApplyAddressModes();

  PlayerController* controller_ = nullptr;

  PlayerConnection connection_;
  player::PlayerStatus status_;

  // Which manual command the reply box is waiting for, so an answer lands in
  // the box that asked for it. Zero when nothing is outstanding — somebody may
  // well press Play while a manual command is still in flight.
  uint64_t manual_request_ = 0;

  struct GatedButton {
    QPushButton* button = nullptr;
    player::PlayerCommand command = player::PlayerCommand::kPlay;
    QString help;
  };
  std::vector<GatedButton> command_buttons_;

  QLabel* headline_ = nullptr;

  QComboBox* speed_ = nullptr;

  QComboBox* address_mode_ = nullptr;
  QLineEdit* address_ = nullptr;
  QPushButton* search_ = nullptr;
  QLabel* search_note_ = nullptr;

  // One per addressing mode, made once and swapped in. A QLineEdit does not own
  // its validator, so making a fresh one on every change would accumulate them
  // for as long as the window is open.
  QValidator* frame_validator_ = nullptr;
  QValidator* time_validator_ = nullptr;
  QValidator* chapter_validator_ = nullptr;

  QComboBox* audio_ = nullptr;

  // Both readouts are text views rather than labels, because a reply here may
  // be two hundred bytes of fixed-width record: it wants a monospace font, its
  // own scrollbar, and to be selectable so it can be copied into whatever is
  // being written about the disc.
  QPlainTextEdit* user_code_ = nullptr;

  QLineEdit* manual_ = nullptr;
  QPushButton* manual_send_ = nullptr;
  QPlainTextEdit* manual_reply_ = nullptr;
};

}  // namespace ddd::gui
