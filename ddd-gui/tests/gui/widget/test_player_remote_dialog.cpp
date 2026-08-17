/************************************************************************

    test_player_remote_dialog.cpp

    T1 tests for the player's remote control
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QTabWidget>
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "fake_serial_port.h"
#include "player_controller.h"
#include "player_controls.h"
#include "player_registry.h"
#include "player_remote_dialog.h"
#include "player_text.h"
#include "serial_port_scanner.h"

namespace ddd::gui {
namespace {

using namespace std::chrono_literals;

constexpr const char* kPortPath = "/dev/ttyFAKE0";
constexpr const char* kLdV4300DReply = "P1515A1";

template <typename Predicate>
bool PumpUntil(Predicate predicate, std::chrono::milliseconds limit = 5000ms) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) {
      return true;
    }
    QApplication::processEvents();
    std::this_thread::sleep_for(1ms);
  }
  return predicate();
}

// A connection to a model, built by hand. What lets the capability gating be
// tested against players nobody has on a bench — including ones with holes in
// their command set that no registered definition has.
PlayerConnection ConnectionTo(const player::PlayerDefinition& definition,
                              std::string_view firmware = "A1") {
  PlayerConnection connection;
  connection.state = PlayerConnectionState::kConnected;
  connection.port_path = QLatin1String(kPortPath);
  connection.baud_rate = 9600;
  connection.model_name = QString::fromUtf8(
      definition.name.data(), static_cast<qsizetype>(definition.name.size()));
  connection.recognised_model = true;
  connection.controls = player::ControlsFor(definition, firmware);
  return connection;
}

const player::PlayerDefinition& LdV4300D() {
  const player::PlayerDefinition* const found =
      player::FindPlayerByIdCode("15");
  EXPECT_NE(found, nullptr);
  return found != nullptr ? *found : player::GenericPlayer();
}

class PlayerRemoteDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const ::testing::TestInfo* const info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    QCoreApplication::setOrganizationName(QStringLiteral("Domesday86Test"));
    QCoreApplication::setApplicationName(
        QStringLiteral("ddd-gui-player-remote-%1")
            .arg(QLatin1String(info->name())));
    QSettings().clear();

    SerialPortCandidate candidate;
    candidate.path = QLatin1String(kPortPath);
    candidate.usb_adapter = true;
    ports_.push_back(candidate);
    port_.set_only_path(kPortPath);
  }

  void TearDown() override {
    dialog_.reset();
    controller_.reset();
    QSettings().clear();
  }

  // No controller at all: the dialog builds, lays out and drives nothing, and
  // its state is set directly. Every gating test uses this, because the
  // gating is a function of the connection rather than of a real player.
  void BuildBare() { dialog_ = std::make_unique<PlayerRemoteDialog>(nullptr); }

  // A real controller over the test's fake port, with nothing started. The
  // Connection-tab tests drive the link themselves, because what they are about
  // is what the tab says in each state it can reach.
  void BuildWithController() {
    PlayerBackend backend;
    backend.make_port = [this] {
      return std::make_unique<player::BorrowedSerialPort>(&port_);
    };
    backend.list_ports = [this] { return ports_; };
    backend.clock = port_.clock();

    controller_ = std::make_unique<PlayerController>(std::move(backend));
    dialog_ = std::make_unique<PlayerRemoteDialog>(controller_.get());
  }

  // The same, with a player already on the port and the link up. Used where the
  // point is that a button puts the right bytes on the wire.
  bool BuildConnected() {
    port_.AddPioneerPlayer(9600, kLdV4300DReply);
    BuildWithController();

    controller_->Start();
    controller_->SetEnabled(true);

    return PumpUntil([this] { return controller_->connected(); });
  }

  bool Wrote(const std::string& bytes) {
    return PumpUntil([this, &bytes] {
      const std::vector<std::string> writes = port_.writes();
      return std::find(writes.begin(), writes.end(), bytes) != writes.end();
    });
  }

  template <typename T>
  T* Find(const char* name) const {
    return dialog_->findChild<T*>(QLatin1String(name));
  }

  QString TextOf(const char* name) const { return Find<QLabel>(name)->text(); }

  player::FakeSerialPort port_;
  std::vector<SerialPortCandidate> ports_;
  std::unique_ptr<PlayerController> controller_;
  std::unique_ptr<PlayerRemoteDialog> dialog_;
};

TEST_F(PlayerRemoteDialogTest, ItBuildsAndDrivesNothingWithNoControllerAtAll) {
  BuildBare();

  // Every control is present and inert, which is the property that keeps the
  // interface testable on a machine with no player and no serial adapter.
  for (const char* name : {PlayerRemoteDialog::kPlayButtonName,
                           PlayerRemoteDialog::kPauseButtonName,
                           PlayerRemoteDialog::kStopButtonName,
                           PlayerRemoteDialog::kSearchButtonName,
                           PlayerRemoteDialog::kManualSendButtonName}) {
    auto* button = Find<QPushButton>(name);
    ASSERT_NE(button, nullptr) << name;
    EXPECT_FALSE(button->isEnabled()) << name;
  }

  EXPECT_FALSE(
      Find<QLineEdit>(PlayerRemoteDialog::kManualEditName)->isEnabled());
}

TEST_F(PlayerRemoteDialogTest, ItDoesNotBlockTheRestOfTheApplication) {
  // The point of this window rather than a detail of it: setting up a capture
  // is driving the player while watching the spectrum, and a modal would make
  // that impossible.
  BuildBare();
  EXPECT_EQ(dialog_->windowModality(), Qt::NonModal);
}

TEST_F(PlayerRemoteDialogTest, EveryButtonSendsWhatTheDefinitionMapsItTo) {
  // Against a real controller over a scripted port, so what is asserted is the
  // bytes on the wire rather than a signal this test could have got wrong in
  // the same way the code did.
  ASSERT_TRUE(BuildConnected());

  const struct {
    const char* button;
    const char* bytes;
  } cases[] = {
      {PlayerRemoteDialog::kPlayButtonName, "PL\r"},
      {PlayerRemoteDialog::kPauseButtonName, "PA\r"},
      {PlayerRemoteDialog::kStillButtonName, "ST\r"},
      {PlayerRemoteDialog::kStopButtonName, "RJ\r"},
      {PlayerRemoteDialog::kTrayOpenButtonName, "OP\r"},
      {PlayerRemoteDialog::kTrayCloseButtonName, "CO\r"},
      {PlayerRemoteDialog::kStepForwardButtonName, "SF\r"},
      {PlayerRemoteDialog::kStepReverseButtonName, "SR\r"},
      {PlayerRemoteDialog::kScanForwardButtonName, "NF\r"},
      {PlayerRemoteDialog::kScanReverseButtonName, "NR\r"},
      {PlayerRemoteDialog::kMultiSpeedForwardButtonName, "MF\r"},
      {PlayerRemoteDialog::kMultiSpeedReverseButtonName, "MB\r"},
      {PlayerRemoteDialog::kPlayIgnoringStopCodesButtonName, "PL64RBMF\r"},
      {PlayerRemoteDialog::kDisplayOnButtonName, "1DS\r"},
      {PlayerRemoteDialog::kDisplayOffButtonName, "0DS\r"},
      {PlayerRemoteDialog::kKeyLockOnButtonName, "1KL\r"},
      {PlayerRemoteDialog::kKeyLockOffButtonName, "0KL\r"},
      {PlayerRemoteDialog::kStandardUserCodeButtonName, "$Y\r"},
      {PlayerRemoteDialog::kPioneerUserCodeButtonName, "?U\r"},
  };

  for (const auto& one : cases) {
    auto* button = Find<QPushButton>(one.button);
    ASSERT_NE(button, nullptr) << one.button;
    ASSERT_TRUE(button->isEnabled()) << one.button;
    button->click();
    EXPECT_TRUE(Wrote(one.bytes)) << one.button;
  }
}

TEST_F(PlayerRemoteDialogTest, TheSpeedAndAudioSelectorsCarryTheirParameter) {
  ASSERT_TRUE(BuildConnected());

  auto* speed = Find<QComboBox>(PlayerRemoteDialog::kSpeedComboName);
  ASSERT_NE(speed, nullptr);
  const int quadruple =
      speed->findData(static_cast<int>(player::PlaybackSpeed::kQuadruple));
  ASSERT_GE(quadruple, 0);
  speed->setCurrentIndex(quadruple);
  emit speed->activated(quadruple);

  // "<n>SP", 7 being ×4 in the shared Pioneer table.
  EXPECT_TRUE(Wrote("7SP\r"));

  auto* audio = Find<QComboBox>(PlayerRemoteDialog::kAudioComboName);
  ASSERT_NE(audio, nullptr);
  const int digital =
      audio->findData(static_cast<int>(player::AudioMode::kDigitalStereo));
  ASSERT_GE(digital, 0);
  audio->setCurrentIndex(digital);
  emit audio->activated(digital);

  EXPECT_TRUE(Wrote("7AD\r"));
}

TEST_F(PlayerRemoteDialogTest, ASeekSendsTheAddressThatWasTypedIn) {
  ASSERT_TRUE(BuildConnected());

  auto* mode = Find<QComboBox>(PlayerRemoteDialog::kAddressModeComboName);
  auto* address = Find<QLineEdit>(PlayerRemoteDialog::kAddressEditName);
  ASSERT_NE(mode, nullptr);
  ASSERT_NE(address, nullptr);

  const int frame =
      mode->findData(static_cast<int>(player::PlayerCommand::kSeekFrame));
  ASSERT_GE(frame, 0);
  mode->setCurrentIndex(frame);
  address->setText(QStringLiteral("1234"));
  Find<QPushButton>(PlayerRemoteDialog::kSearchButtonName)->click();

  EXPECT_TRUE(Wrote("FR1234SE\r"));

  const int chapter =
      mode->findData(static_cast<int>(player::PlayerCommand::kSeekChapter));
  ASSERT_GE(chapter, 0);
  mode->setCurrentIndex(chapter);
  emit mode->activated(chapter);
  address->setText(QStringLiteral("7"));
  Find<QPushButton>(PlayerRemoteDialog::kSearchButtonName)->click();

  EXPECT_TRUE(Wrote("CH7SE\r"));
}

TEST_F(PlayerRemoteDialogTest, AnEntryThatIsNotAnAddressIsNotSentAtAll) {
  // Refused here rather than sent and refused by the player: a seek to a
  // number invented out of "1:99" would move the disc somewhere nobody asked
  // for.
  ASSERT_TRUE(BuildConnected());

  player::PlayerStatus clv;
  clv.valid = true;
  clv.disc_type = player::DiscType::kClv;
  dialog_->SetStatus(clv);

  auto* address = Find<QLineEdit>(PlayerRemoteDialog::kAddressEditName);
  address->setText(QStringLiteral("1:99"));

  const size_t writes_before = port_.writes().size();
  Find<QPushButton>(PlayerRemoteDialog::kSearchButtonName)->click();

  auto* note = Find<QLabel>(PlayerRemoteDialog::kSearchNoteLabelName);
  ASSERT_NE(note, nullptr);
  EXPECT_TRUE(note->isVisibleTo(dialog_.get()));
  EXPECT_FALSE(note->text().isEmpty());

  PumpUntil([] { return false; }, 100ms);
  EXPECT_EQ(port_.writes().size(), writes_before);
}

TEST_F(PlayerRemoteDialogTest, TheAddressingFollowsTheDisc) {
  BuildBare();
  dialog_->SetConnection(ConnectionTo(LdV4300D()));

  auto* mode = Find<QComboBox>(PlayerRemoteDialog::kAddressModeComboName);
  ASSERT_NE(mode, nullptr);

  const auto offers = [mode](player::PlayerCommand command) {
    return mode->findData(static_cast<int>(command)) >= 0;
  };

  // With no disc identified yet, both are offered: declining to guess is
  // better than guessing.
  EXPECT_TRUE(offers(player::PlayerCommand::kSeekFrame));
  EXPECT_TRUE(offers(player::PlayerCommand::kSeekTimeCode));

  player::PlayerStatus clv;
  clv.valid = true;
  clv.disc_type = player::DiscType::kClv;
  dialog_->SetStatus(clv);

  // A frame number means nothing on a CLV disc, so it is not offered.
  EXPECT_FALSE(offers(player::PlayerCommand::kSeekFrame));
  EXPECT_TRUE(offers(player::PlayerCommand::kSeekTimeCode));
  EXPECT_TRUE(offers(player::PlayerCommand::kSeekChapter));

  player::PlayerStatus cav;
  cav.valid = true;
  cav.disc_type = player::DiscType::kCav;
  dialog_->SetStatus(cav);

  EXPECT_TRUE(offers(player::PlayerCommand::kSeekFrame));
  EXPECT_FALSE(offers(player::PlayerCommand::kSeekTimeCode));
}

TEST_F(PlayerRemoteDialogTest, AControlTheModelLacksIsDisabledAndSaysWhy) {
  // The old dialog offered every button to every player, so a control the
  // player did not have was present, enabled, and silently did nothing.
  player::PlayerDefinition without_scan = LdV4300D();
  without_scan.capabilities.scan = false;

  BuildBare();
  dialog_->SetConnection(ConnectionTo(without_scan));

  auto* scan = Find<QPushButton>(PlayerRemoteDialog::kScanForwardButtonName);
  ASSERT_NE(scan, nullptr);
  EXPECT_FALSE(scan->isEnabled());
  EXPECT_TRUE(scan->toolTip().contains(QStringLiteral("does not offer")));

  // And the ones it does have are offered, with their own explanation.
  auto* play = Find<QPushButton>(PlayerRemoteDialog::kPlayButtonName);
  EXPECT_TRUE(play->isEnabled());
  EXPECT_FALSE(play->toolTip().contains(QStringLiteral("does not offer")));
}

TEST_F(PlayerRemoteDialogTest, AModeWithNoWireParameterIsNotInTheList) {
  player::PlayerDefinition analogue_only = LdV4300D();
  analogue_only.capabilities.digital_audio = false;

  BuildBare();
  dialog_->SetConnection(ConnectionTo(analogue_only));

  auto* audio = Find<QComboBox>(PlayerRemoteDialog::kAudioComboName);
  ASSERT_NE(audio, nullptr);
  EXPECT_LT(
      audio->findData(static_cast<int>(player::AudioMode::kDigitalStereo)), 0);
  EXPECT_GE(audio->findData(static_cast<int>(player::AudioMode::kAnalogStereo)),
            0);
}

TEST_F(PlayerRemoteDialogTest, LosingThePlayerGreysTheWholeThingOut) {
  // The window stays open rather than vanishing: a remote that disappeared when
  // a cable was jogged would be worse than one that says what happened.
  BuildBare();
  dialog_->SetConnection(ConnectionTo(LdV4300D()));
  ASSERT_TRUE(
      Find<QPushButton>(PlayerRemoteDialog::kPlayButtonName)->isEnabled());

  PlayerConnection lost;
  lost.state = PlayerConnectionState::kDisconnected;
  lost.problem = PlayerConnectionProblem::kLinkLost;
  dialog_->SetConnection(lost);

  EXPECT_FALSE(
      Find<QPushButton>(PlayerRemoteDialog::kPlayButtonName)->isEnabled());
  EXPECT_FALSE(
      Find<QPushButton>(PlayerRemoteDialog::kSearchButtonName)->isEnabled());
  EXPECT_FALSE(
      Find<QLineEdit>(PlayerRemoteDialog::kManualEditName)->isEnabled());

  auto* headline = Find<QLabel>(PlayerRemoteDialog::kHeadlineLabelName);
  ASSERT_NE(headline, nullptr);
  EXPECT_FALSE(headline->text().isEmpty());
}

TEST_F(PlayerRemoteDialogTest, AManualCommandIsSentAsTypedAndShownAsAnswered) {
  ASSERT_TRUE(BuildConnected());
  port_.AddResponse(9600, "?P\r", "P04\r");

  auto* manual = Find<QLineEdit>(PlayerRemoteDialog::kManualEditName);
  auto* reply = Find<QPlainTextEdit>(PlayerRemoteDialog::kManualReplyViewName);
  ASSERT_NE(manual, nullptr);
  ASSERT_NE(reply, nullptr);

  manual->setText(QStringLiteral("?P"));
  Find<QPushButton>(PlayerRemoteDialog::kManualSendButtonName)->click();

  ASSERT_TRUE(PumpUntil([reply] {
    return reply->toPlainText().contains(QStringLiteral("P04"));
  }));
  EXPECT_TRUE(reply->toPlainText().contains(QStringLiteral("?P")));
}

TEST_F(PlayerRemoteDialogTest, AManualErrorReplyIsShownRatherThanSwallowed) {
  // The reading the bench turned up: a parked LD-V4300D answers the Pioneer
  // user-code query with "E04". A manual command exists precisely to find out
  // what a player really says, so the bytes are shown either way.
  ASSERT_TRUE(BuildConnected());
  port_.AddResponse(9600, "?U\r", "E04\r");

  Find<QLineEdit>(PlayerRemoteDialog::kManualEditName)
      ->setText(QStringLiteral("?U"));
  Find<QPushButton>(PlayerRemoteDialog::kManualSendButtonName)->click();

  auto* reply = Find<QPlainTextEdit>(PlayerRemoteDialog::kManualReplyViewName);
  ASSERT_TRUE(PumpUntil([reply] {
    return reply->toPlainText().contains(QStringLiteral("E04"));
  }));
  EXPECT_TRUE(reply->toPlainText().contains(QStringLiteral("refused")));
}

TEST_F(PlayerRemoteDialogTest, AUserCodeReadLandsInItsOwnBox) {
  ASSERT_TRUE(BuildConnected());
  port_.AddResponse(9600, "$Y\r", "ENCODED123\r");

  Find<QPushButton>(PlayerRemoteDialog::kStandardUserCodeButtonName)->click();

  auto* code = Find<QPlainTextEdit>(PlayerRemoteDialog::kUserCodeViewName);
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(PumpUntil([code] {
    return code->toPlainText().contains(QStringLiteral("ENCODED123"));
  }));

  // A user code containing an 'E' is not a refusal, which is the reason text
  // replies skip the acknowledgement convention in the first place.
  EXPECT_FALSE(code->toPlainText().contains(QStringLiteral("refused")));

  // And the manual box is left alone: a reply lands in the box that asked.
  EXPECT_TRUE(Find<QPlainTextEdit>(PlayerRemoteDialog::kManualReplyViewName)
                  ->toPlainText()
                  .isEmpty());
}

TEST_F(PlayerRemoteDialogTest, TheWholePioneerUserCodeIsDumpedForReading) {
  // The reply this box exists for, taken byte for byte off the project's own
  // bench: two identical 60-byte records, sixty characters the player could not
  // read, a twenty-byte tail, and a terminator.
  const std::string record =
      "#59-014    *MCA / CASPER THX LTBX         !2 %0510803@@@@@@@";
  ASSERT_EQ(record.size(), 60U);

  const std::string user_code =
      record + record + std::string(60, '`') + std::string(20, '0') + "\r";
  ASSERT_EQ(user_code.size(), 201U);

  ASSERT_TRUE(BuildConnected());
  port_.AddResponse(9600, "?U\r", user_code);

  Find<QPushButton>(PlayerRemoteDialog::kPioneerUserCodeButtonName)->click();

  auto* code = Find<QPlainTextEdit>(PlayerRemoteDialog::kUserCodeViewName);
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(PumpUntil([code] {
    return code->toPlainText().contains(QStringLiteral("CASPER"));
  }));

  const QString shown = code->toPlainText();

  // All 200 characters reach the box. Nothing is trimmed and nothing is folded
  // away: the records end in fill characters, and a reply that had been through
  // whitespace trimming or a UTF-8 decode would arrive here shorter.
  EXPECT_TRUE(shown.contains(QStringLiteral("200 characters")));

  // Split at the boundaries the format defines rather than dumped as one blob.
  EXPECT_TRUE(shown.contains(QStringLiteral("Disc Control Data")));
  EXPECT_TRUE(shown.contains(QStringLiteral("Key Data")));
  EXPECT_TRUE(shown.contains(QStringLiteral("Control Data")));

  // The dump itself, with each region numbered from its place in the whole —
  // the Key Data starts at 120 and says so.
  EXPECT_TRUE(shown.contains(QStringLiteral("0000  ")));
  EXPECT_TRUE(shown.contains(QStringLiteral("0120  ")));
  EXPECT_TRUE(shown.contains(QStringLiteral("0180  ")));
  EXPECT_TRUE(shown.contains(QStringLiteral("|#59-014    *MCA |")));

  // And the point of the whole exercise: sixty backticks are not this
  // application failing to decode something, and they are not scattered through
  // the reply. They are exactly the Key Data — the customer's own identifying
  // information — and the player saying it could not read it.
  EXPECT_TRUE(shown.contains(QStringLiteral("none of it could be read")));
}

TEST_F(PlayerRemoteDialogTest, AShortLegibleReplyIsLeftAsWords) {
  // The other half of the one rule: "P04" is already readable, and burying it
  // under a hex dump would make the common case worse to serve the rare one.
  ASSERT_TRUE(BuildConnected());
  port_.AddResponse(9600, "?P\r", "P04\r");

  Find<QLineEdit>(PlayerRemoteDialog::kManualEditName)
      ->setText(QStringLiteral("?P"));
  Find<QPushButton>(PlayerRemoteDialog::kManualSendButtonName)->click();

  auto* reply = Find<QPlainTextEdit>(PlayerRemoteDialog::kManualReplyViewName);
  ASSERT_TRUE(PumpUntil([reply] {
    return reply->toPlainText().contains(QStringLiteral("P04"));
  }));

  EXPECT_FALSE(reply->toPlainText().contains(QStringLiteral("bytes.")));
  EXPECT_FALSE(reply->toPlainText().contains(QStringLiteral("0000  ")));
}

// --- The pages ------------------------------------------------------------

// The four things this window holds are wanted at very different rates, and
// stacking them in one column cost every user a window half as tall again as it
// needs to be. The order is how often each is wanted.
TEST_F(PlayerRemoteDialogTest, TheFourPagesAreOfferedInOrderOfHowOftenTheyAre) {
  BuildBare();

  auto* const tabs = Find<QTabWidget>(PlayerRemoteDialog::kTabsName);
  ASSERT_NE(tabs, nullptr) << "the remote is not tabbed";
  ASSERT_EQ(tabs->count(), 4);

  EXPECT_TRUE(tabs->tabText(0).contains(QStringLiteral("Control")));
  EXPECT_TRUE(tabs->tabText(1).contains(QStringLiteral("Connection")));
  EXPECT_TRUE(tabs->tabText(2).contains(QStringLiteral("Disc codes")));
  EXPECT_TRUE(tabs->tabText(3).contains(QStringLiteral("Manual")));
}

TEST_F(PlayerRemoteDialogTest, ThePageToOpenOnCanBeChosen) {
  // What the main window uses to land somebody on the tab that answers the
  // question they have: the transport when there is a player, and the reason
  // when there is not.
  BuildBare();

  auto* const tabs = Find<QTabWidget>(PlayerRemoteDialog::kTabsName);
  ASSERT_NE(tabs, nullptr);

  dialog_->ShowTab(PlayerRemoteDialog::Tab::kConnection);
  EXPECT_EQ(tabs->currentIndex(),
            static_cast<int>(PlayerRemoteDialog::Tab::kConnection));

  dialog_->ShowTab(PlayerRemoteDialog::Tab::kControl);
  EXPECT_EQ(tabs->currentIndex(),
            static_cast<int>(PlayerRemoteDialog::Tab::kControl));
}

// The headline sits above the tab bar rather than on a page, because what is
// connected and what it is doing is the one thing every page wants in view.
TEST_F(PlayerRemoteDialogTest, TheHeadlineIsOutsideThePages) {
  BuildBare();

  auto* const headline = Find<QLabel>(PlayerRemoteDialog::kHeadlineLabelName);
  auto* const tabs = Find<QTabWidget>(PlayerRemoteDialog::kTabsName);
  ASSERT_NE(headline, nullptr);
  ASSERT_NE(tabs, nullptr);

  EXPECT_FALSE(tabs->isAncestorOf(headline))
      << "the headline is on a page, so three pages out of four cannot see it";
}

// --- The Connection tab ----------------------------------------------------
//
// This was the Player dock. Its assertions moved here whole, because what they
// are about — whether there is a player, how it was reached, and what it is
// doing — did not change when the place it is shown did.

TEST_F(PlayerRemoteDialogTest, TheConnectionTabSaysWhatItIsDoingWithNoLayer) {
  // The property every window in this application has: it lays out exactly as
  // it does in the real application and drives nothing, so the interface stays
  // testable on a machine with no player and no serial adapter.
  BuildBare();

  EXPECT_EQ(TextOf(PlayerRemoteDialog::kSummaryLabelName),
            PlayerConnectionSummary(PlayerConnection{}));

  // Present but inert, rather than absent — the tab has the same shape in
  // every build of the window.
  auto* const enabled = Find<QCheckBox>(PlayerRemoteDialog::kEnabledCheckName);
  ASSERT_NE(enabled, nullptr);
  EXPECT_FALSE(enabled->isEnabled());

  auto* const search =
      Find<QPushButton>(PlayerRemoteDialog::kSearchNowButtonName);
  ASSERT_NE(search, nullptr);
  EXPECT_FALSE(search->isEnabled());
}

TEST_F(PlayerRemoteDialogTest, EveryReadingIsBlankUntilThereIsAPlayer) {
  // A stale position left on screen beside "no player" reads as a live one.
  BuildWithController();

  EXPECT_EQ(TextOf(PlayerRemoteDialog::kStateLabelName), QStringLiteral("—"));
  EXPECT_EQ(TextOf(PlayerRemoteDialog::kAddressLabelName), QStringLiteral("—"));

  // How the player was reached is a row that only exists once one has been:
  // isHidden() rather than isVisible(), for the reason given on the
  // "use this model" button below.
  EXPECT_TRUE(Find<QLabel>(PlayerRemoteDialog::kSourceLabelName)->isHidden());
}

TEST_F(PlayerRemoteDialogTest, TheCheckboxIsThePlayerControlSetting) {
  BuildWithController();
  controller_->Start();

  auto* const enabled = Find<QCheckBox>(PlayerRemoteDialog::kEnabledCheckName);
  ASSERT_NE(enabled, nullptr);
  EXPECT_FALSE(enabled->isChecked());

  enabled->setChecked(true);
  EXPECT_TRUE(controller_->settings().enabled);

  // And a change made anywhere else — the Tools menu, the settings dialog — is
  // reflected here, rather than leaving the box and the application
  // disagreeing.
  controller_->SetEnabled(false);
  EXPECT_FALSE(enabled->isChecked());
}

TEST_F(PlayerRemoteDialogTest, AConnectedPlayerIsNamedWithHowItWasReached) {
  port_.AddStatusResponses(9600, "P04", "10011", "0012345");
  ASSERT_TRUE(BuildConnected());
  ASSERT_TRUE(PumpUntil([this] { return controller_->status().valid; }));

  EXPECT_EQ(TextOf(PlayerRemoteDialog::kSummaryLabelName),
            QStringLiteral("Pioneer LD-V4300D"));
  EXPECT_TRUE(TextOf(PlayerRemoteDialog::kSourceLabelName)
                  .contains(QLatin1String(kPortPath)));
  EXPECT_TRUE(TextOf(PlayerRemoteDialog::kSourceLabelName)
                  .contains(QStringLiteral("9600")));

  EXPECT_EQ(TextOf(PlayerRemoteDialog::kStateLabelName),
            QStringLiteral("Playing"));
  EXPECT_EQ(TextOf(PlayerRemoteDialog::kDiscLabelName), QStringLiteral("CAV"));
  EXPECT_EQ(TextOf(PlayerRemoteDialog::kAddressLabelName),
            QStringLiteral("Frame 12345"));

  // The row for a reading this model cannot produce stays hidden rather than
  // showing a blank forever.
  EXPECT_FALSE(
      Find<QLabel>(PlayerRemoteDialog::kPositionLabelName)->isVisible());
}

TEST_F(PlayerRemoteDialogTest, AnUnverifiedDefinitionIsSaidSoOnScreen) {
  // The interface's half of players/README.md's promise.
  ASSERT_TRUE(BuildConnected());

  auto* const note = Find<QLabel>(PlayerRemoteDialog::kVerificationLabelName);
  ASSERT_NE(note, nullptr);
  EXPECT_FALSE(note->text().isEmpty());
}

TEST_F(PlayerRemoteDialogTest, TheWrongModelOffersToBeAccepted) {
  port_.AddPioneerPlayer(9600, "P1506A9");  // an LD-V8000
  BuildWithController();

  PlayerSettings settings = controller_->settings();
  settings.enabled = true;
  settings.model_id_code = QStringLiteral("15");  // LD-V4300D
  controller_->SetSettings(settings);

  ASSERT_TRUE(PumpUntil([this] {
    return controller_->connection().state ==
           PlayerConnectionState::kModelMismatch;
  }));

  auto* const use_model =
      Find<QPushButton>(PlayerRemoteDialog::kUseModelButtonName);
  ASSERT_NE(use_model, nullptr);

  // isHidden() rather than isVisible() or isVisibleTo(): the window is never
  // shown in a widget test, and this button is on a tab page that is not the
  // current one — so both of those are false however right the code is. What
  // is under test is the button's own hidden flag, which is what the code sets.
  EXPECT_FALSE(use_model->isHidden());

  use_model->click();

  ASSERT_TRUE(PumpUntil([this] {
    return controller_->connection().state == PlayerConnectionState::kConnected;
  }));
  EXPECT_TRUE(use_model->isHidden());
}

TEST_F(PlayerRemoteDialogTest, SearchingNowIsOfferedOnlyWhenNothingIsThere) {
  BuildWithController();
  controller_->Start();
  controller_->SetEnabled(true);

  auto* const search =
      Find<QPushButton>(PlayerRemoteDialog::kSearchNowButtonName);
  ASSERT_NE(search, nullptr);

  ASSERT_TRUE(PumpUntil([this] {
    return controller_->connection().state ==
           PlayerConnectionState::kDisconnected;
  }));
  EXPECT_TRUE(search->isEnabled());

  // Nothing to search for once there is a player.
  port_.AddPioneerPlayer(9600, kLdV4300DReply);
  search->click();

  ASSERT_TRUE(PumpUntil([this] { return controller_->connected(); }));
  EXPECT_FALSE(search->isEnabled());
}

TEST_F(PlayerRemoteDialogTest, LosingThePlayerBlanksTheReadings) {
  port_.AddStatusResponses(9600, "P04", "10011", "0012345");
  ASSERT_TRUE(BuildConnected());

  ASSERT_TRUE(PumpUntil([this] { return controller_->status().valid; }));
  ASSERT_EQ(TextOf(PlayerRemoteDialog::kStateLabelName),
            QStringLiteral("Playing"));

  port_.set_link_broken(true);

  ASSERT_TRUE(PumpUntil([this] { return !controller_->connected(); }));
  EXPECT_EQ(TextOf(PlayerRemoteDialog::kStateLabelName), QStringLiteral("—"));
  EXPECT_EQ(TextOf(PlayerRemoteDialog::kAddressLabelName), QStringLiteral("—"));
}

}  // namespace
}  // namespace ddd::gui
