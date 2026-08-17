/************************************************************************

    player_remote_dialog.cpp

    The player's remote control
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_remote_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVariant>
#include <optional>

#include "player_controller.h"
#include "player_text.h"

namespace ddd::gui {
namespace {

// One way of addressing a disc: what to send, what to call it, and what an
// empty entry field should say it wants.
struct AddressModeEntry {
  player::PlayerCommand command;
  QString label;
  QString placeholder;
};

// A readout for a reply that is data rather than a word: monospace, so a hex
// dump's columns line up; read-only but selectable, so it can be copied out;
// and with its own scrollbar, so a two-hundred-byte record does not push the
// rest of the remote off the screen.
QPlainTextEdit* MakeByteView(QWidget* parent, const char* object_name,
                             int visible_lines) {
  auto* view = new QPlainTextEdit(parent);
  view->setObjectName(QLatin1String(object_name));
  view->setReadOnly(true);
  view->setLineWrapMode(QPlainTextEdit::NoWrap);

  QFont monospace = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  view->setFont(monospace);

  // Sized in lines of its own font rather than in pixels, so it holds what it
  // says it holds whatever the platform's fixed font turns out to be.
  const QFontMetrics metrics(monospace);
  view->setFixedHeight(metrics.lineSpacing() * visible_lines +
                       (view->frameWidth() * 2) + metrics.descent());

  return view;
}

QLabel* MakeReadout(QWidget* parent, const char* object_name) {
  auto* label = new QLabel(parent);
  label->setObjectName(QLatin1String(object_name));
  label->setWordWrap(true);

  // Selectable, because the whole reason these exist is to be copied into a
  // report or into a new definition header.
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

// One row of the status block: a value with nothing in it yet, which is a
// different thing from a value of zero.
QLabel* MakeStatusReadout(QWidget* parent, const char* object_name) {
  auto* label = new QLabel(QStringLiteral("—"), parent);
  label->setObjectName(QLatin1String(object_name));
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

}  // namespace

PlayerRemoteDialog::PlayerRemoteDialog(PlayerController* controller,
                                       QWidget* parent)
    : QDialog(parent), controller_(controller) {
  setWindowTitle(tr("Player"));

  // The point of this window rather than a detail of it: the spectrum, the
  // waveform and the capture controls stay usable while the player is being
  // driven, because setting up a capture is exactly those two activities at
  // once.
  setWindowModality(Qt::NonModal);
  setSizeGripEnabled(true);

  auto* layout = new QVBoxLayout(this);

  // Above the tab bar rather than on a page, because it is the one thing every
  // page wants in view: what is connected and what it is doing.
  headline_ = MakeReadout(this, kHeadlineLabelName);
  QFont headline_font = headline_->font();
  headline_font.setBold(true);
  headline_->setFont(headline_font);
  layout->addWidget(headline_);

  tabs_ = new QTabWidget(this);
  tabs_->setObjectName(QLatin1String(kTabsName));
  tabs_->addTab(BuildControlPage(), tr("Control"));
  tabs_->addTab(BuildConnectionPage(), tr("Connection"));
  tabs_->addTab(BuildDiscCodePage(), tr("Disc codes"));
  tabs_->addTab(BuildManualPage(), tr("Manual command"));
  layout->addWidget(tabs_);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
  layout->addWidget(buttons);

  if (controller_ != nullptr) {
    connect(controller_, &PlayerController::ConnectionChanged, this,
            &PlayerRemoteDialog::SetConnection);
    connect(controller_, &PlayerController::StatusUpdated, this,
            &PlayerRemoteDialog::SetStatus);
    connect(controller_, &PlayerController::RequestCompleted, this,
            &PlayerRemoteDialog::OnRequestCompleted);

    // The checkbox is the settings, so anything else that changes them — the
    // Tools menu, the settings dialog — is reflected here rather than leaving
    // the two disagreeing.
    connect(controller_, &PlayerController::SettingsChanged, this,
            [this](const PlayerSettings& settings) {
              const QSignalBlocker blocker(enabled_check_);
              enabled_check_->setChecked(settings.enabled);
            });

    enabled_check_->setChecked(controller_->settings().enabled);

    connection_ = controller_->connection();
    status_ = controller_->status();
  }

  ApplyAddressModes();
  ApplyConnection();
  SetStatus(status_);
}

void PlayerRemoteDialog::ShowTab(Tab tab) {
  tabs_->setCurrentIndex(static_cast<int>(tab));
}

QWidget* PlayerRemoteDialog::BuildControlPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  layout->addWidget(BuildTransport(page));
  layout->addWidget(BuildSearch(page));
  layout->addWidget(BuildPresentation(page));
  layout->addStretch(1);

  return page;
}

QWidget* PlayerRemoteDialog::BuildTransport(QWidget* page) {
  auto* group = new QGroupBox(tr("Transport"), page);
  auto* rows = new QVBoxLayout(group);

  auto* disc_row = new QHBoxLayout();
  AddCommandButton(group, disc_row, kTrayOpenButtonName, tr("Open tray"),
                   player::PlayerCommand::kTrayOpen,
                   tr("Open the tray. The player spins the disc down first, so "
                      "this can take a few seconds."));
  AddCommandButton(group, disc_row, kTrayCloseButtonName, tr("Close tray"),
                   player::PlayerCommand::kTrayClose,
                   tr("Close the tray and load the disc."));
  AddCommandButton(group, disc_row, kStopButtonName, tr("Reject"),
                   player::PlayerCommand::kStop,
                   tr("Stop the disc and park the optical assembly."));
  disc_row->addStretch(1);
  rows->addLayout(disc_row);

  auto* play_row = new QHBoxLayout();
  AddCommandButton(group, play_row, kPlayButtonName, tr("Play"),
                   player::PlayerCommand::kPlay, tr("Play from here."));
  AddCommandButton(
      group, play_row, kPlayIgnoringStopCodesButtonName,
      tr("Play ignoring stop codes"),
      player::PlayerCommand::kPlayWithoutStopCodes,
      tr("Play without stopping at the disc's stop codes. A CAV disc with stop "
         "codes pauses part way through a side, which ends a whole-side "
         "capture early; this is how to get past that."));
  AddCommandButton(
      group, play_row, kPauseButtonName, tr("Pause"),
      player::PlayerCommand::kPause,
      tr("Pause with the picture blanked. The disc keeps spinning."));
  AddCommandButton(group, play_row, kStillButtonName, tr("Still"),
                   player::PlayerCommand::kStillFrame,
                   tr("Hold the current frame. CAV discs only — a CLV disc has "
                      "no frame to hold."));
  play_row->addStretch(1);
  rows->addLayout(play_row);

  auto* step_row = new QHBoxLayout();
  AddCommandButton(group, step_row, kStepReverseButtonName, tr("◀ Step"),
                   player::PlayerCommand::kStepReverse, tr("Back one frame."));
  AddCommandButton(group, step_row, kStepForwardButtonName, tr("Step ▶"),
                   player::PlayerCommand::kStepForward,
                   tr("Forward one frame."));
  AddCommandButton(group, step_row, kScanReverseButtonName, tr("◀◀ Scan"),
                   player::PlayerCommand::kScanReverse,
                   tr("Scan backwards at high speed."));
  AddCommandButton(group, step_row, kScanForwardButtonName, tr("Scan ▶▶"),
                   player::PlayerCommand::kScanForward,
                   tr("Scan forwards at high speed."));
  step_row->addStretch(1);
  rows->addLayout(step_row);

  auto* speed_row = new QHBoxLayout();
  AddCommandButton(group, speed_row, kMultiSpeedReverseButtonName,
                   tr("◀ Multi-speed"),
                   player::PlayerCommand::kMultiSpeedReverse,
                   tr("Play backwards at the rate chosen here."));
  AddCommandButton(group, speed_row, kMultiSpeedForwardButtonName,
                   tr("Multi-speed ▶"),
                   player::PlayerCommand::kMultiSpeedForward,
                   tr("Play forwards at the rate chosen here."));

  speed_ = new QComboBox(group);
  speed_->setObjectName(QLatin1String(kSpeedComboName));
  speed_->setToolTip(tr("The rate the multi-speed buttons play at."));
  speed_row->addWidget(speed_);
  speed_row->addStretch(1);
  rows->addLayout(speed_row);

  // activated rather than currentIndexChanged: rebuilding this list for a newly
  // connected player must not send that player a command.
  connect(speed_, &QComboBox::activated, this, [this](int index) {
    if (index < 0) {
      return;
    }
    Send(SpeedRequest(
        static_cast<player::PlaybackSpeed>(speed_->itemData(index).toInt())));
  });

  return group;
}

QWidget* PlayerRemoteDialog::BuildSearch(QWidget* page) {
  auto* group = new QGroupBox(tr("Go to"), page);
  auto* rows = new QVBoxLayout(group);

  auto* row = new QHBoxLayout();

  address_mode_ = new QComboBox(group);
  address_mode_->setObjectName(QLatin1String(kAddressModeComboName));
  address_mode_->setToolTip(
      tr("How to address this disc. What is offered follows the disc that is "
         "loaded: a frame number means nothing on a CLV disc."));
  row->addWidget(address_mode_);

  address_ = new QLineEdit(group);
  address_->setObjectName(QLatin1String(kAddressEditName));
  row->addWidget(address_, 1);

  search_ = new QPushButton(tr("Go"), group);
  search_->setObjectName(QLatin1String(kSearchButtonName));
  row->addWidget(search_);

  rows->addLayout(row);

  search_note_ = MakeReadout(group, kSearchNoteLabelName);
  search_note_->setVisible(false);
  rows->addWidget(search_note_);

  // Made once and swapped, rather than made afresh each time the mode changes:
  // a QLineEdit does not own its validator, and a new one per change would
  // accumulate for as long as the window is open.
  frame_validator_ = new QIntValidator(1, 99999, this);
  chapter_validator_ = new QIntValidator(1, 99, this);

  // Digits and colons. Whether they amount to a time is decided when Go is
  // pressed — "1:2" is a perfectly reasonable thing to be halfway through
  // typing, and a validator that rejected it would make the field unusable.
  time_validator_ = new QRegularExpressionValidator(
      QRegularExpression(QStringLiteral("[0-9:]{0,9}")), this);

  connect(search_, &QPushButton::clicked, this, &PlayerRemoteDialog::OnSearch);
  connect(address_, &QLineEdit::returnPressed, this,
          &PlayerRemoteDialog::OnSearch);
  connect(address_mode_, &QComboBox::activated, this, [this] {
    // An entry typed for one mode is not an address in another.
    address_->clear();
    search_note_->setVisible(false);
    ApplyAddressModes();
  });

  return group;
}

QWidget* PlayerRemoteDialog::BuildPresentation(QWidget* page) {
  auto* group = new QGroupBox(tr("Display and audio"), page);
  auto* rows = new QVBoxLayout(group);

  auto* display_row = new QHBoxLayout();
  AddCommandButton(group, display_row, kDisplayOnButtonName, tr("Display on"),
                   player::PlayerCommand::kDisplayOn,
                   tr("Show the player's own frame counter on the picture. It "
                      "is burned into the video, so it will be in the "
                      "capture."));
  AddCommandButton(group, display_row, kDisplayOffButtonName, tr("Display off"),
                   player::PlayerCommand::kDisplayOff,
                   tr("Take the player's frame counter off the picture."));

  audio_ = new QComboBox(group);
  audio_->setObjectName(QLatin1String(kAudioComboName));
  audio_->setToolTip(tr("Which audio the player presents."));
  display_row->addWidget(audio_);
  display_row->addStretch(1);
  rows->addLayout(display_row);

  connect(audio_, &QComboBox::activated, this, [this](int index) {
    if (index < 0) {
      return;
    }
    Send(AudioRequest(
        static_cast<player::AudioMode>(audio_->itemData(index).toInt())));
  });

  // Two buttons rather than one tick, because nothing asks the player whether
  // its front panel is locked: a tick that cannot be read back is a claim this
  // application is in no position to make. These are commands, and a checkbox
  // would look like a readout.
  auto* lock_row = new QHBoxLayout();
  AddCommandButton(group, lock_row, kKeyLockOnButtonName, tr("Lock player"),
                   player::PlayerCommand::kKeyLockOn,
                   tr("Ignore the player's own buttons and its handset, so a "
                      "capture cannot be interrupted by somebody pressing "
                      "Stop."));
  AddCommandButton(group, lock_row, kKeyLockOffButtonName, tr("Unlock player"),
                   player::PlayerCommand::kKeyLockOff,
                   tr("Let the player's own buttons and handset work again."));
  lock_row->addStretch(1);
  rows->addLayout(lock_row);

  return group;
}

QWidget* PlayerRemoteDialog::BuildConnectionPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  enabled_check_ = new QCheckBox(tr("Player control"), page);
  enabled_check_->setObjectName(QLatin1String(kEnabledCheckName));
  enabled_check_->setToolTip(
      tr("Look for a LaserDisc player on the serial ports, and control it from "
         "here. While this is off, no serial port on this machine is opened or "
         "written to."));
  layout->addWidget(enabled_check_);

  // A sentence rather than a status light, and that is the design: "no player
  // found" and "that port could not be opened" send somebody to different
  // places, and a red dot would send them to neither. Everything shown here
  // comes from player_text.cpp, so this tab, the status bar and the log cannot
  // drift apart.
  summary_ = new QLabel(page);
  summary_->setObjectName(QLatin1String(kSummaryLabelName));
  summary_->setWordWrap(true);
  QFont summary_font = summary_->font();
  summary_font.setBold(true);
  summary_->setFont(summary_font);
  layout->addWidget(summary_);

  detail_ = MakeReadout(page, kDetailLabelName);
  layout->addWidget(detail_);

  source_ = MakeReadout(page, kSourceLabelName);
  layout->addWidget(source_);

  verification_ = new QLabel(page);
  verification_->setObjectName(QLatin1String(kVerificationLabelName));
  verification_->setWordWrap(true);
  layout->addWidget(verification_);

  auto* buttons = new QWidget(page);
  auto* button_layout = new QHBoxLayout(buttons);
  button_layout->setContentsMargins(0, 0, 0, 0);

  search_now_ = new QPushButton(tr("Search now"), buttons);
  search_now_->setObjectName(QLatin1String(kSearchNowButtonName));
  search_now_->setToolTip(
      tr("Look for the player again straight away, rather than waiting for the "
         "next automatic attempt."));
  button_layout->addWidget(search_now_);

  use_model_ = new QPushButton(tr("Use this model"), buttons);
  use_model_->setObjectName(QLatin1String(kUseModelButtonName));
  use_model_->setToolTip(
      tr("Change the selected model to the one that actually answered."));
  use_model_->setVisible(false);
  button_layout->addWidget(use_model_);

  button_layout->addStretch(1);
  layout->addWidget(buttons);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignLeft);

  state_ = MakeStatusReadout(page, kStateLabelName);
  form->addRow(tr("State"), state_);

  tray_ = MakeStatusReadout(page, kTrayLabelName);
  form->addRow(tr("Tray"), tray_);

  disc_ = MakeStatusReadout(page, kDiscLabelName);
  form->addRow(tr("Disc"), disc_);

  address_readout_ = MakeStatusReadout(page, kAddressLabelName);
  form->addRow(tr("Position"), address_readout_);

  // Built and then hidden rather than not built: the row exists in every build
  // of the window, so a widget test can find it and check that it stays hidden
  // for a model that cannot report one.
  position_row_ = new QWidget(page);
  auto* position_layout = new QHBoxLayout(position_row_);
  position_layout->setContentsMargins(0, 0, 0, 0);
  position_ = MakeStatusReadout(position_row_, kPositionLabelName);
  position_layout->addWidget(position_);
  form->addRow(tr("Optical assembly"), position_row_);
  position_row_->setVisible(false);

  layout->addLayout(form);
  layout->addStretch(1);

  if (controller_ != nullptr) {
    connect(enabled_check_, &QCheckBox::toggled, controller_,
            &PlayerController::SetEnabled);
    connect(search_now_, &QPushButton::clicked, controller_,
            &PlayerController::SearchNow);
    connect(use_model_, &QPushButton::clicked, controller_,
            &PlayerController::UseConnectedModel);
  }

  return page;
}

QWidget* PlayerRemoteDialog::BuildDiscCodePage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  auto* explanation = new QLabel(
      tr("The codes the disc carries about itself. They are read from the disc "
         "rather than worked out, and nothing in this application derives a "
         "length, a start or an end from one."),
      page);
  explanation->setWordWrap(true);
  layout->addWidget(explanation);

  auto* row = new QHBoxLayout();
  QPushButton* const standard =
      AddCommandButton(page, row, kStandardUserCodeButtonName, tr("Standard"),
                       player::PlayerCommand::kQueryStandardUserCode,
                       tr("Read the disc's standard user code."));
  QPushButton* const pioneer = AddCommandButton(
      page, row, kPioneerUserCodeButtonName, tr("Pioneer"),
      player::PlayerCommand::kQueryPioneerUserCode,
      tr("Read the disc's Pioneer user code. This one moves the player: it "
         "searches to the lead-in to read it, so the disc will not be where "
         "you left it afterwards, and it takes up to ten seconds."));
  row->addStretch(1);
  layout->addLayout(row);

  // Fifteen lines holds the whole of a 200-byte Pioneer user code — thirteen
  // dump lines and its heading — without scrolling, which is the reply this
  // box exists for.
  user_code_ = MakeByteView(page, kUserCodeViewName, 15);
  user_code_->setPlaceholderText(
      tr("The disc's user code, as the player reports it, byte for byte."));
  layout->addWidget(user_code_);
  layout->addStretch(1);

  // These two are the only command buttons whose answer is worth showing, so
  // they say that they are waiting for one. Everything else is a movement, and
  // the Connection tab shows the result of a movement.
  //
  // The Pioneer code in particular is worth saying so about: the player
  // searches to the lead-in to read it and Pioneer allows ten seconds for the
  // whole business, and a box that stayed blank that long would look like a
  // button that had done nothing.
  connect(standard, &QPushButton::clicked, this,
          [this] { user_code_->setPlainText(tr("Reading…")); });

  connect(pioneer, &QPushButton::clicked, this, [this] {
    user_code_->setPlainText(
        tr("Searching to the lead-in and reading — this takes up to ten "
           "seconds."));
  });

  return page;
}

QWidget* PlayerRemoteDialog::BuildManualPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  auto* explanation = new QLabel(
      tr("Send a command straight to the player and see exactly what it "
         "answers. This is how to find out what a player this build does not "
         "recognise can do — and therefore what is needed to add it."),
      page);
  explanation->setWordWrap(true);
  layout->addWidget(explanation);

  auto* row = new QHBoxLayout();

  manual_ = new QLineEdit(page);
  manual_->setObjectName(QLatin1String(kManualEditName));
  manual_->setPlaceholderText(tr("for example: ?P"));

  // The terminator is added on the way out, so the field holds one character
  // fewer than a player will accept. Refusing the twentieth character here is
  // kinder than building a command and having it rejected for a reason the
  // user cannot see.
  manual_->setMaxLength(static_cast<int>(player::kMaximumCommandLength) - 1);
  row->addWidget(manual_, 1);

  manual_send_ = new QPushButton(tr("Send"), page);
  manual_send_->setObjectName(QLatin1String(kManualSendButtonName));
  row->addWidget(manual_send_);

  layout->addLayout(row);

  manual_reply_ = MakeByteView(page, kManualReplyViewName, 6);
  manual_reply_->setPlaceholderText(
      tr("Whatever the player answers, shown exactly as it arrived."));
  layout->addWidget(manual_reply_);
  layout->addStretch(1);

  connect(manual_send_, &QPushButton::clicked, this,
          &PlayerRemoteDialog::OnManualSend);
  connect(manual_, &QLineEdit::returnPressed, this,
          &PlayerRemoteDialog::OnManualSend);

  return page;
}

QPushButton* PlayerRemoteDialog::AddCommandButton(
    QWidget* parent, QBoxLayout* into, const char* object_name,
    const QString& label, player::PlayerCommand command, const QString& help) {
  auto* button = new QPushButton(label, parent);
  button->setObjectName(QLatin1String(object_name));
  into->addWidget(button);

  connect(button, &QPushButton::clicked, this,
          [this, command] { Send(CommandRequest(command)); });

  command_buttons_.push_back(GatedButton{button, command, help});
  return button;
}

uint64_t PlayerRemoteDialog::Send(const PlayerRequest& request) {
  if (controller_ == nullptr) {
    return 0;
  }
  return controller_->Send(request);
}

void PlayerRemoteDialog::SetConnection(
    const ddd::gui::PlayerConnection& connection) {
  connection_ = connection;

  if (!connection_.live()) {
    // A reading taken from a player that is no longer there is a reading from
    // nowhere, and leaving it on screen makes it look current.
    user_code_->clear();
    ClearStatus();
  }

  // Which addresses are offered depends on what this player can search by as
  // well as on the disc, so both are worked out again here.
  ApplyAddressModes();
  ApplyConnection();
}

void PlayerRemoteDialog::SetStatus(const ddd::player::PlayerStatus& status) {
  const player::DiscType previous = status_.disc_type;
  status_ = status;

  headline_->setText(PlayerStatusBarText(connection_, status_));

  if (!status_.valid) {
    ClearStatus();
  } else {
    state_->setText(PlayerStateName(status_.state));
    tray_->setText(TrayStateName(status_.tray));
    disc_->setText(DiscTypeName(status_.disc_type));
    address_readout_->setText(PlayerAddressText(status_));

    const QString position = PhysicalPositionText(status_);
    position_->setText(position);
    position_row_->setVisible(!position.isEmpty());
  }

  // Only when the disc has actually changed. Rebuilding four times a second
  // would throw away the mode the user had chosen, several times before they
  // reached the entry field.
  if (status_.disc_type != previous) {
    ApplyAddressModes();
  }
}

void PlayerRemoteDialog::ClearStatus() {
  const QString nothing = QStringLiteral("—");
  state_->setText(nothing);
  tray_->setText(nothing);
  disc_->setText(nothing);
  address_readout_->setText(nothing);
  position_->setText(nothing);
  position_row_->setVisible(false);
}

void PlayerRemoteDialog::ApplyConnection() {
  summary_->setText(PlayerConnectionSummary(connection_));

  const QString detail = PlayerConnectionDetail(connection_);
  detail_->setText(detail);
  detail_->setVisible(!detail.isEmpty());

  const QString source = PlayerConnectionSource(connection_);
  source_->setText(source);
  source_->setVisible(!source.isEmpty());

  const QString verification = PlayerVerificationNote(connection_);
  verification_->setText(verification);
  verification_->setVisible(!verification.isEmpty());

  use_model_->setVisible(connection_.state ==
                         PlayerConnectionState::kModelMismatch);

  // Nothing to switch on, search with or accept without a controller behind
  // them. Shown rather than hidden so this tab has the same shape in every
  // build of the window, and disabled rather than left to do nothing when
  // pressed.
  const bool live_application = controller_ != nullptr;
  enabled_check_->setEnabled(live_application);
  use_model_->setEnabled(live_application);

  // Searching now is only meaningful when it is looking and has not found
  // anything: there is nothing to search for while connected, and nothing to
  // search with while switched off.
  search_now_->setEnabled(live_application &&
                          connection_.state ==
                              PlayerConnectionState::kDisconnected);
}

void PlayerRemoteDialog::ApplyControls() {
  const player::PlayerControls& controls = connection_.controls;
  const bool live = connection_.live();

  headline_->setText(PlayerStatusBarText(connection_, status_));

  for (const GatedButton& gated : command_buttons_) {
    const bool available = live && controls.Has(gated.command);
    gated.button->setEnabled(available);

    // A control that is not offered says why. The old application's silence
    // here is what made a capability the player lacked look like a fault in the
    // cable.
    gated.button->setToolTip(
        available ? gated.help
                  : UnsupportedControlNote(connection_, gated.command));
  }

  // The two parameterised commands. Their lists are rebuilt rather than merely
  // disabled, because what a model can be set to is per-model data: offering a
  // rate the player has no parameter for would be exactly the dead control this
  // design exists to avoid.
  const int wanted_speed =
      speed_->currentData().isValid()
          ? speed_->currentData().toInt()
          : static_cast<int>(player::PlaybackSpeed::kNormal);
  speed_->clear();
  for (size_t index = 0; index < player::kPlaybackSpeedCount; ++index) {
    const auto speed = static_cast<player::PlaybackSpeed>(index);
    if (controls.Has(speed)) {
      speed_->addItem(PlaybackSpeedName(speed), static_cast<int>(index));
    }
  }
  const int speed_row = speed_->findData(wanted_speed);
  speed_->setCurrentIndex(speed_row >= 0 ? speed_row : 0);
  speed_->setEnabled(live && speed_->count() > 0);

  const int wanted_audio =
      audio_->currentData().isValid()
          ? audio_->currentData().toInt()
          : static_cast<int>(player::AudioMode::kAnalogStereo);
  audio_->clear();
  for (size_t index = 0; index < player::kAudioModeCount; ++index) {
    const auto mode = static_cast<player::AudioMode>(index);
    if (controls.Has(mode)) {
      audio_->addItem(AudioModeName(mode), static_cast<int>(index));
    }
  }
  const int audio_row = audio_->findData(wanted_audio);
  audio_->setCurrentIndex(audio_row >= 0 ? audio_row : 0);
  audio_->setEnabled(live && audio_->count() > 0);

  const bool can_search = live && address_mode_->count() > 0;
  address_mode_->setEnabled(can_search);
  address_->setEnabled(can_search);
  search_->setEnabled(can_search);

  manual_->setEnabled(live);
  manual_send_->setEnabled(live);
}

void PlayerRemoteDialog::ApplyAddressModes() {
  const player::PlayerControls& controls = connection_.controls;
  const player::DiscType disc = status_.disc_type;

  // What the disc allows, crossed with what the player can search by. A CLV
  // disc has no frame numbers and a CAV disc has no time codes, so offering
  // both would be offering one that is certain to be refused. With no disc
  // identified yet both are offered, because declining to guess is better than
  // guessing.
  const bool frames = disc != player::DiscType::kClv;
  const bool times = disc != player::DiscType::kCav;

  std::vector<AddressModeEntry> entries;
  if (frames) {
    entries.push_back(
        {player::PlayerCommand::kSeekFrame, tr("Frame"), tr("frame number")});
  }
  if (times) {
    entries.push_back(
        {player::PlayerCommand::kSeekTimeCode, tr("Time"), tr("h:mm:ss")});
  }
  entries.push_back(
      {player::PlayerCommand::kSeekChapter, tr("Chapter"), tr("chapter")});

  const QVariant wanted = address_mode_->currentData();

  address_mode_->clear();
  for (const AddressModeEntry& entry : entries) {
    // With nothing connected there is no command table to consult, so every
    // mode is listed and the whole group is disabled instead. An empty selector
    // reads as broken rather than as idle.
    if (controls.Has(entry.command) || !controls.any()) {
      address_mode_->addItem(entry.label, static_cast<int>(entry.command));
    }
  }

  const int row = wanted.isValid() ? address_mode_->findData(wanted) : -1;
  address_mode_->setCurrentIndex(row >= 0 ? row : 0);

  // The entry field belongs to the mode: what may be typed into it, and what it
  // says it wants when it is empty.
  const auto command =
      static_cast<player::PlayerCommand>(address_mode_->currentData().toInt());

  for (const AddressModeEntry& entry : entries) {
    if (entry.command == command) {
      address_->setPlaceholderText(entry.placeholder);
    }
  }

  switch (command) {
    case player::PlayerCommand::kSeekTimeCode:
      address_->setValidator(time_validator_);
      break;
    case player::PlayerCommand::kSeekChapter:
      address_->setValidator(chapter_validator_);
      break;
    default:
      address_->setValidator(frame_validator_);
      break;
  }

  ApplyControls();
}

void PlayerRemoteDialog::OnSearch() {
  if (address_mode_->currentIndex() < 0) {
    return;
  }

  const auto command =
      static_cast<player::PlayerCommand>(address_mode_->currentData().toInt());

  std::optional<int32_t> argument;
  if (command == player::PlayerCommand::kSeekTimeCode) {
    argument = ParseTimeCodeEntry(address_->text());
  } else {
    bool numeric = false;
    const int32_t value = address_->text().trimmed().toInt(&numeric);
    if (numeric) {
      argument = value;
    }
  }

  if (!argument.has_value()) {
    // Refused here rather than sent and refused by the player: a seek to a
    // number this application invented out of "1:99" would move the disc
    // somewhere nobody asked for.
    search_note_->setText(
        command == player::PlayerCommand::kSeekTimeCode
            ? tr("That is not a time. Try 1:23:45, or 23:45 on a disc under an "
                 "hour.")
            : tr("That is not a number."));
    search_note_->setVisible(true);
    return;
  }

  search_note_->setVisible(false);
  Send(CommandRequest(command, argument));
}

void PlayerRemoteDialog::OnManualSend() {
  const QString typed = manual_->text().trimmed();
  if (typed.isEmpty()) {
    return;
  }

  manual_request_ = Send(RawRequest(typed));
  manual_reply_->setPlainText(tr("Waiting…"));
}

void PlayerRemoteDialog::OnRequestCompleted(const PlayerReply& reply) {
  if (reply.request.kind == PlayerRequest::Kind::kRaw) {
    // Matched by id rather than by being the most recent: somebody may well
    // press Play while a manual command is still waiting for its answer.
    if (reply.request.id == manual_request_) {
      manual_reply_->setPlainText(PlayerReplyReport(reply));
      manual_request_ = 0;
    }
    return;
  }

  const bool user_code =
      reply.request.kind == PlayerRequest::Kind::kCommand &&
      (reply.request.command == player::PlayerCommand::kQueryStandardUserCode ||
       reply.request.command == player::PlayerCommand::kQueryPioneerUserCode);

  if (user_code) {
    user_code_->setPlainText(PlayerReplyReport(reply));
  }
}

}  // namespace ddd::gui
