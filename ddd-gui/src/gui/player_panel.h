/************************************************************************

    player_panel.h

    The player: whether there is one, and what it is doing
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QWidget>

#include "player_connection.h"
#include "player_status.h"

class QCheckBox;
class QLabel;
class QPushButton;

namespace ddd::gui {

class PlayerController;

// The dock the player is watched from.
//
// Two halves, and the split is the design: the top says whether there is a
// player and how it was reached, and the bottom says what it is doing. The
// first is the half a user reads when something is wrong, so it is a sentence
// rather than a status light — "no player found" and "that port could not be
// opened" send somebody to different places, and a panel that showed only a red
// dot would send them to neither.
//
// Everything it displays comes from player_text.cpp, so the panel, the status
// bar and the log cannot drift apart.
//
// Thread-safety: NOT thread-safe. GUI thread only — which is the whole reason
// the controller exists.
class PlayerPanel : public QWidget {
  Q_OBJECT

 public:
  // The controller may be null, and the widget tests pass null deliberately:
  // the panel then builds and lays out exactly as it does in the application
  // and drives nothing.
  explicit PlayerPanel(PlayerController* controller, QWidget* parent = nullptr);

  // Named so the widget tests can find them without depending on layout order.
  static constexpr const char* kEnabledCheckName = "player_enabled_check";
  static constexpr const char* kSummaryLabelName = "player_summary_label";
  static constexpr const char* kDetailLabelName = "player_detail_label";
  static constexpr const char* kSourceLabelName = "player_source_label";
  static constexpr const char* kVerificationLabelName =
      "player_verification_label";
  static constexpr const char* kStateLabelName = "player_state_label";
  static constexpr const char* kTrayLabelName = "player_tray_label";
  static constexpr const char* kDiscLabelName = "player_disc_label";
  static constexpr const char* kAddressLabelName = "player_address_label";
  static constexpr const char* kPositionLabelName = "player_position_label";
  static constexpr const char* kSearchButtonName = "player_search_button";
  static constexpr const char* kUseModelButtonName = "player_use_model_button";

 private:
  void OnConnectionChanged(const PlayerConnection& connection);
  void OnStatusUpdated(const player::PlayerStatus& status);

  // Blank every reading. Called when the link goes away, because a stale
  // position left on screen beside "no player" reads as a live one.
  void ClearStatus();

  PlayerController* controller_ = nullptr;

  QCheckBox* enabled_check_ = nullptr;
  QLabel* summary_ = nullptr;
  QLabel* detail_ = nullptr;
  QLabel* source_ = nullptr;
  QLabel* verification_ = nullptr;

  QLabel* state_ = nullptr;
  QLabel* tray_ = nullptr;
  QLabel* disc_ = nullptr;
  QLabel* address_ = nullptr;
  QLabel* position_ = nullptr;

  // The row the position sits in, hidden on every model that cannot report one
  // rather than shown as unknown — which would be a row that is blank for
  // almost every user forever.
  QWidget* position_row_ = nullptr;

  QPushButton* search_ = nullptr;
  QPushButton* use_model_ = nullptr;
};

}  // namespace ddd::gui
