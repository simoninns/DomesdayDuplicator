/************************************************************************

    player_remote_dialog.cpp

    The player's remote control
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_remote_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
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

}  // namespace

PlayerRemoteDialog::PlayerRemoteDialog(PlayerController* controller,
                                       QWidget* parent)
    : QDialog(parent), controller_(controller) {
  setWindowTitle(tr("Remote control"));

  // The point of this window rather than a detail of it: the spectrum, the
  // waveform and the capture controls stay usable while the player is being
  // driven, because setting up a capture is exactly those two activities at
  // once.
  setWindowModality(Qt::NonModal);
  setSizeGripEnabled(true);

  auto* layout = new QVBoxLayout(this);

  headline_ = MakeReadout(this, kHeadlineLabelName);
  QFont headline_font = headline_->font();
  headline_font.setBold(true);
  headline_->setFont(headline_font);
  layout->addWidget(headline_);

  layout->addWidget(BuildTransport());
  layout->addWidget(BuildSearch());
  layout->addWidget(BuildPresentation());
  layout->addWidget(BuildUserCodes());
  layout->addWidget(BuildManualCommand());
  layout->addStretch(1);

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

    connection_ = controller_->connection();
    status_ = controller_->status();
  }

  ApplyAddressModes();
}

QWidget* PlayerRemoteDialog::BuildTransport() {
  auto* group = new QGroupBox(tr("Transport"), this);
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

QWidget* PlayerRemoteDialog::BuildSearch() {
  auto* group = new QGroupBox(tr("Go to"), this);
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

QWidget* PlayerRemoteDialog::BuildPresentation() {
  auto* group = new QGroupBox(tr("Display and audio"), this);
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

QWidget* PlayerRemoteDialog::BuildUserCodes() {
  auto* group = new QGroupBox(tr("User code"), this);
  auto* rows = new QVBoxLayout(group);

  auto* row = new QHBoxLayout();
  QPushButton* const standard =
      AddCommandButton(group, row, kStandardUserCodeButtonName, tr("Standard"),
                       player::PlayerCommand::kQueryStandardUserCode,
                       tr("Read the disc's standard user code."));
  QPushButton* const pioneer = AddCommandButton(
      group, row, kPioneerUserCodeButtonName, tr("Pioneer"),
      player::PlayerCommand::kQueryPioneerUserCode,
      tr("Read the disc's Pioneer user code. This one moves the player: it "
         "searches to the lead-in to read it, so the disc will not be where "
         "you left it afterwards, and it takes up to ten seconds."));
  row->addStretch(1);
  rows->addLayout(row);

  // Fifteen lines holds the whole of a 200-byte Pioneer user code — thirteen
  // dump lines and its heading — without scrolling, which is the reply this
  // box exists for.
  user_code_ = MakeByteView(group, kUserCodeViewName, 15);
  user_code_->setPlaceholderText(
      tr("The disc's user code, as the player reports it, byte for byte."));
  rows->addWidget(user_code_);

  // These two are the only command buttons whose answer is worth showing, so
  // they say that they are waiting for one. Everything else is a movement, and
  // the panel shows the result of a movement.
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

  return group;
}

QWidget* PlayerRemoteDialog::BuildManualCommand() {
  auto* group = new QGroupBox(tr("Manual command"), this);
  auto* rows = new QVBoxLayout(group);

  auto* explanation = new QLabel(
      tr("Send a command straight to the player and see exactly what it "
         "answers. This is how to find out what a player this build does not "
         "recognise can do — and therefore what is needed to add it."),
      group);
  explanation->setWordWrap(true);
  rows->addWidget(explanation);

  auto* row = new QHBoxLayout();

  manual_ = new QLineEdit(group);
  manual_->setObjectName(QLatin1String(kManualEditName));
  manual_->setPlaceholderText(tr("for example: ?P"));

  // The terminator is added on the way out, so the field holds one character
  // fewer than a player will accept. Refusing the twentieth character here is
  // kinder than building a command and having it rejected for a reason the
  // user cannot see.
  manual_->setMaxLength(static_cast<int>(player::kMaximumCommandLength) - 1);
  row->addWidget(manual_, 1);

  manual_send_ = new QPushButton(tr("Send"), group);
  manual_send_->setObjectName(QLatin1String(kManualSendButtonName));
  row->addWidget(manual_send_);

  rows->addLayout(row);

  manual_reply_ = MakeByteView(group, kManualReplyViewName, 6);
  manual_reply_->setPlaceholderText(
      tr("Whatever the player answers, shown exactly as it arrived."));
  rows->addWidget(manual_reply_);

  connect(manual_send_, &QPushButton::clicked, this,
          &PlayerRemoteDialog::OnManualSend);
  connect(manual_, &QLineEdit::returnPressed, this,
          &PlayerRemoteDialog::OnManualSend);

  return group;
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
  }

  // Which addresses are offered depends on what this player can search by as
  // well as on the disc, so both are worked out again here.
  ApplyAddressModes();
}

void PlayerRemoteDialog::SetStatus(const ddd::player::PlayerStatus& status) {
  const player::DiscType previous = status_.disc_type;
  status_ = status;

  headline_->setText(PlayerStatusBarText(connection_, status_));

  // Only when the disc has actually changed. Rebuilding four times a second
  // would throw away the mode the user had chosen, several times before they
  // reached the entry field.
  if (status_.disc_type != previous) {
    ApplyAddressModes();
  }
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
