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
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QValidator;

namespace ddd::gui {

class PlayerController;

// Everything to do with the player, in one window.
//
// **Non-modal**, and that is the point of it rather than a detail: setting up a
// capture consists of moving the player about while watching the spectrum and
// the waveform, and a window that blocked the rest of the application would
// make the one thing it is for impossible.
//
// **Tabbed, because the four things it holds are wanted at very different
// rates.** Transport and seeking are used constantly; the connection is looked
// at when something is wrong; the user codes are read once a disc; and the
// manual command line exists to work out what an unrecognised player does, so
// that the next definition header can be written. Stacked in one column, as
// they were, the two rare ones cost every user a window half as tall again as
// it needs to be — and the byte views are most of that height.
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
// The Connection tab is also why this window can be opened with nothing
// connected, unlike every other player window. It is where a user goes to find
// out *why* nothing is connected, so refusing to open it until something is
// would withhold the answer at exactly the moment it is wanted.
//
// Thread-safety: NOT thread-safe. GUI thread only. Nothing here touches the
// player; every control submits a request to the controller and waits.
class PlayerRemoteDialog : public QDialog {
  Q_OBJECT

 public:
  // The controller may be null, and the widget tests pass null deliberately:
  // the dialog then builds, lays out and drives nothing. Its state can still be
  // set through the two slots below, which is what lets the capability gating
  // be tested against models that are not on anybody's bench.
  explicit PlayerRemoteDialog(PlayerController* controller,
                              QWidget* parent = nullptr);

  // Which page to show. Opening on kConnection is what the main window does
  // when there is no player: the tab that explains the silence is the one to
  // land on.
  enum class Tab {
    kControl,
    kConnection,
    kDiscCodes,
    kManual,
  };

  void ShowTab(Tab tab);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kHeadlineLabelName = "remote_headline";
  static constexpr const char* kTabsName = "remote_tabs";

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

  // The Connection tab, which was the Player dock until this window grew tabs
  // to put it in.
  static constexpr const char* kEnabledCheckName = "remote_enabled_check";
  static constexpr const char* kSummaryLabelName = "remote_summary_label";
  static constexpr const char* kDetailLabelName = "remote_detail_label";
  static constexpr const char* kSourceLabelName = "remote_source_label";
  static constexpr const char* kVerificationLabelName =
      "remote_verification_label";
  static constexpr const char* kSearchNowButtonName = "remote_search_now";
  static constexpr const char* kUseModelButtonName = "remote_use_model";
  static constexpr const char* kStateLabelName = "remote_state_label";
  static constexpr const char* kTrayLabelName = "remote_tray_label";
  static constexpr const char* kDiscLabelName = "remote_disc_label";
  static constexpr const char* kAddressLabelName = "remote_address_label";
  static constexpr const char* kPositionLabelName = "remote_position_label";

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
  // One page each. The order they are declared in is the order they are added
  // in, which is the order of how often they are wanted.
  QWidget* BuildControlPage();
  QWidget* BuildConnectionPage();
  QWidget* BuildDiscCodePage();
  QWidget* BuildManualPage();

  QWidget* BuildTransport(QWidget* page);
  QWidget* BuildSearch(QWidget* page);
  QWidget* BuildPresentation(QWidget* page);

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

  // Say whether there is a player, how it was reached, and what is wrong when
  // there is not. The Connection tab's half of the same connection change.
  void ApplyConnection();

  // Blank every reading. Called when the link goes away, because a stale
  // position left on screen beside "no player" reads as a live one.
  void ClearStatus();

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
  QTabWidget* tabs_ = nullptr;

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

  // --- The Connection tab ---------------------------------------------------

  QCheckBox* enabled_check_ = nullptr;
  QLabel* summary_ = nullptr;
  QLabel* detail_ = nullptr;
  QLabel* source_ = nullptr;
  QLabel* verification_ = nullptr;
  QPushButton* search_now_ = nullptr;
  QPushButton* use_model_ = nullptr;

  QLabel* state_ = nullptr;
  QLabel* tray_ = nullptr;
  QLabel* disc_ = nullptr;
  QLabel* address_readout_ = nullptr;
  QLabel* position_ = nullptr;

  // The row the position sits in, hidden on every model that cannot report one
  // rather than shown as unknown — which would be a row that is blank for
  // almost every user forever.
  QWidget* position_row_ = nullptr;

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
