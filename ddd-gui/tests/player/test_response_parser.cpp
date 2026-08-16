/************************************************************************

    test_response_parser.cpp

    T1 tests for reading what a player sends back
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "player_state.h"
#include "players/pioneer_ld_v4300d.h"
#include "response_parser.h"

namespace ddd::player {
namespace {

const StateDecode& States() { return pioneer::kLdV4300D.state_decode; }
const DiscStatusDecode& Disc() { return pioneer::kLdV4300D.disc_status; }

TEST(ResponseParserTest, AnAcknowledgementIsAnythingWithoutAnE) {
  // Deliberately as lenient as the old application. Several players answer
  // commands with something other than the documented "R", and requiring it
  // would refuse commands that worked.
  EXPECT_EQ(ParseAcknowledgement("R\r").status, ReplyStatus::kOk);
  EXPECT_EQ(ParseAcknowledgement("P\r").status, ReplyStatus::kOk);
}

TEST(ResponseParserTest, AnErrorReplyIsARefusalAndCarriesItsCode) {
  const Reply reply = ParseAcknowledgement("E04\r");
  EXPECT_EQ(reply.status, ReplyStatus::kRefused);
  EXPECT_EQ(reply.error_code, "E04");
}

TEST(ResponseParserTest, ARefusalWithNoLegibleCodeStillRefuses) {
  // A bare 'E' is a refusal without a code. Reporting "E" as the code would put
  // a meaningless string in front of a user.
  const Reply reply = ParseAcknowledgement("E\r");
  EXPECT_EQ(reply.status, ReplyStatus::kRefused);
  EXPECT_TRUE(reply.error_code.empty());
}

TEST(ResponseParserTest, SilenceIsNotARefusal) {
  // The distinction the old application did not make: it returned false for
  // both, and a refusal means the disc or the player state is wrong while
  // silence means the link is.
  EXPECT_EQ(ParseAcknowledgement("").status, ReplyStatus::kNoAnswer);
  EXPECT_EQ(ParseAcknowledgement("\r").status, ReplyStatus::kNoAnswer);
}

TEST(ResponseParserTest, ATextReplyIsNotPutThroughTheErrorConvention) {
  // A user code is arbitrary bytes and may perfectly well contain an 'E'.
  // Reading it as a refusal would make some discs impossible to identify.
  const Reply reply = ParseText("ENCODED123\r");
  EXPECT_EQ(reply.status, ReplyStatus::kOk);
  EXPECT_EQ(reply.text, "ENCODED123");
}

TEST(ResponseParserTest, ATextReplyKeepsEveryByteButTheTerminator) {
  // The Pioneer user code is a fixed-width record whose fields are space-padded
  // to their width, so the whitespace trimming that is right for a reply about
  // to be read as a number would be deleting payload here. Everything that does
  // parse a reply strips for itself, so nothing downstream needs this to.
  EXPECT_EQ(ParseText("#59-014    *MCA  \r").text, "#59-014    *MCA  ");
  EXPECT_EQ(ParseText("  leading kept\r").text, "  leading kept");

  // The terminator does come off, in either spelling.
  EXPECT_EQ(ParseText("P04\r\n").text, "P04");

  // And a reply that is nothing but a terminator is still no answer.
  EXPECT_EQ(ParseText("\r").status, ReplyStatus::kNoAnswer);
}

TEST(ResponseParserTest, TheUnreadableCharacterIsTheOneThePlayerDocuments) {
  // Named rather than spelled out at each use, because it is the difference
  // between "this application cannot decode the reply" and "the player could
  // not read the disc" — and on the bench an LD-V4300D sent sixty of them in
  // the middle of one 200-byte user code.
  EXPECT_EQ(kUnreadableCharacter, '`');
  EXPECT_EQ(static_cast<int>(kUnreadableCharacter), 0x60);
}

TEST(ResponseParserTest, AnErrorCodeIsTellableFromAUserCodeThatContainsAnE) {
  // The reading that made this necessary: an LD-V4300D on the bench answers the
  // Pioneer user-code query with "E04" while parked. Taken as text — which is
  // right for a user code — that would be shown to somebody as their disc's
  // user code, so the whole reply being 'E' and digits is worth recognising.
  EXPECT_TRUE(IsErrorCode("E04"));
  EXPECT_TRUE(IsErrorCode("E1"));

  // Deliberately much stricter than the acknowledgement convention, because the
  // reason text replies avoid that convention is exactly that a user code may
  // legitimately contain an 'E'.
  EXPECT_FALSE(IsErrorCode("ENCODED123"));
  EXPECT_FALSE(IsErrorCode("E"));
  EXPECT_FALSE(IsErrorCode(""));
  EXPECT_FALSE(IsErrorCode("04"));
  EXPECT_FALSE(IsErrorCode("E04X"));
  EXPECT_FALSE(IsErrorCode("XE04"));
}

TEST(ResponseParserTest, AFrameAddressIsRead) {
  const DiscAddress address = ParseAddress("12345\r", AddressMode::kFrame);
  EXPECT_TRUE(address.valid);
  EXPECT_EQ(address.value, 12345);
  EXPECT_FALSE(address.in_lead_in);
  EXPECT_FALSE(address.in_lead_out);
}

TEST(ResponseParserTest, TheLeadInAndLeadOutMarkersAreKept) {
  // Not decoration: capturing from the lead-in depends on knowing the player
  // has reached it, and the end of a side is recognised by the lead-out.
  const DiscAddress lead_in = ParseAddress("<00100\r", AddressMode::kFrame);
  EXPECT_TRUE(lead_in.valid);
  EXPECT_TRUE(lead_in.in_lead_in);
  EXPECT_EQ(lead_in.value, 100);

  const DiscAddress lead_out = ParseAddress(">54000\r", AddressMode::kFrame);
  EXPECT_TRUE(lead_out.valid);
  EXPECT_TRUE(lead_out.in_lead_out);
  EXPECT_EQ(lead_out.value, 54000);
}

TEST(ResponseParserTest, ATimeCodeIsReadInTimeCodeMode) {
  const DiscAddress address = ParseAddress("1234500\r", AddressMode::kTimeCode);
  EXPECT_TRUE(address.valid);
  EXPECT_EQ(address.value, 1234500);
}

TEST(ResponseParserTest, ZeroPaddingIsPaddingRatherThanWidth) {
  // Exactly what an LD-V4300D on the bench answers: seven zero-padded digits,
  // whatever the disc is. Counting the padding as significant would make every
  // reading from that player too wide for frame mode and refuse it.
  const DiscAddress padded = ParseAddress("0002103\r", AddressMode::kFrame);
  EXPECT_TRUE(padded.valid);
  EXPECT_EQ(padded.value, 2103);

  // And a genuinely zero address is still an address rather than nothing.
  const DiscAddress zero = ParseAddress("0000000\r", AddressMode::kFrame);
  EXPECT_TRUE(zero.valid);
  EXPECT_EQ(zero.value, 0);
}

TEST(ResponseParserTest, ATimeCodeReadAsAFrameIsRefusedRatherThanTruncated) {
  // The old application took the first five digits of whatever arrived, so a
  // CLV time code read in frame mode became a plausible-looking frame number
  // that was wrong by orders of magnitude. Here the width mismatch is the
  // evidence that the mode is wrong.
  const DiscAddress address = ParseAddress("1234500\r", AddressMode::kFrame);
  EXPECT_FALSE(address.valid);
  EXPECT_EQ(address.value, -1);
}

TEST(ResponseParserTest, AnAddressThatIsNotANumberIsNotAnAddress) {
  EXPECT_FALSE(ParseAddress("\r", AddressMode::kFrame).valid);
  EXPECT_FALSE(ParseAddress("", AddressMode::kFrame).valid);
  EXPECT_FALSE(ParseAddress("E04\r", AddressMode::kFrame).valid);
  EXPECT_FALSE(ParseAddress("<\r", AddressMode::kFrame).valid);
}

TEST(ResponseParserTest, EveryDocumentedActiveModeIsRecognised) {
  EXPECT_EQ(ParsePlayerState("P00\r", States()), PlayerState::kDoorOpen);
  EXPECT_EQ(ParsePlayerState("P01\r", States()), PlayerState::kParked);
  EXPECT_EQ(ParsePlayerState("P02\r", States()), PlayerState::kSettingUp);
  EXPECT_EQ(ParsePlayerState("P03\r", States()), PlayerState::kUnloading);
  EXPECT_EQ(ParsePlayerState("P04\r", States()), PlayerState::kPlaying);
  EXPECT_EQ(ParsePlayerState("P05\r", States()), PlayerState::kStillFrame);
  EXPECT_EQ(ParsePlayerState("P06\r", States()), PlayerState::kPaused);
  EXPECT_EQ(ParsePlayerState("P07\r", States()), PlayerState::kSearching);
  EXPECT_EQ(ParsePlayerState("P08\r", States()), PlayerState::kScanning);
  EXPECT_EQ(ParsePlayerState("P09\r", States()), PlayerState::kMultiSpeed);
}

TEST(ResponseParserTest, TheTwoUndocumentedRepliesAreStillRecognised) {
  // Both were found by watching real players: P42 during ordinary playback on
  // some models, PA5 at the end of a disc. Dropping them would make a playing
  // player look like one in an unknown state.
  EXPECT_EQ(ParsePlayerState("P42\r", States()), PlayerState::kPlaying);
  EXPECT_EQ(ParsePlayerState("PA5\r", States()), PlayerState::kPlaying);
}

TEST(ResponseParserTest, AnUnrecognisedModeIsUnknownRatherThanGuessed) {
  // The states differ in whether the disc is turning, so guessing would have
  // the automatic capture wait for a disc that is stationary.
  EXPECT_EQ(ParsePlayerState("P99\r", States()), PlayerState::kUnknown);
  EXPECT_EQ(ParsePlayerState("D01\r", States()), PlayerState::kUnknown);
  EXPECT_EQ(ParsePlayerState("\r", States()), PlayerState::kUnknown);
}

TEST(ResponseParserTest, WhetherTheDiscIsTurningFollowsFromTheState) {
  EXPECT_TRUE(IsSpinning(PlayerState::kPlaying));
  EXPECT_TRUE(IsSpinning(PlayerState::kStillFrame));
  EXPECT_TRUE(IsSpinning(PlayerState::kSearching));
  EXPECT_FALSE(IsSpinning(PlayerState::kParked));
  EXPECT_FALSE(IsSpinning(PlayerState::kDoorOpen));
  EXPECT_FALSE(IsSpinning(PlayerState::kUnknown));

  // Advancing is the narrower question the automatic capture's stall detector
  // asks: a player holding a still frame is spinning and going nowhere.
  EXPECT_TRUE(IsAdvancing(PlayerState::kPlaying));
  EXPECT_FALSE(IsAdvancing(PlayerState::kStillFrame));
  EXPECT_FALSE(IsAdvancing(PlayerState::kSearching));
}

TEST(ResponseParserTest, TheTrayFollowsFromTheState) {
  EXPECT_EQ(TrayStateFor(PlayerState::kDoorOpen), TrayState::kOpen);
  EXPECT_EQ(TrayStateFor(PlayerState::kPlaying), TrayState::kClosed);
  EXPECT_EQ(TrayStateFor(PlayerState::kParked), TrayState::kClosed);
  EXPECT_EQ(TrayStateFor(PlayerState::kUnknown), TrayState::kUnknown);
}

TEST(ResponseParserTest, TheDiscTypeIsReadFromTheStatusReply) {
  // "11011" is the shape an LD-V4300D really answers with — five digits, not
  // the letter-and-digit form it would be easy to assume.
  EXPECT_EQ(ParseDiscType("10011\r", Disc()), DiscType::kCav);
  EXPECT_EQ(ParseDiscType("11011\r", Disc()), DiscType::kClv);
}

TEST(ResponseParserTest, AnUnreadableDiscStatusIsUnknown) {
  EXPECT_EQ(ParseDiscType("1\r", Disc()), DiscType::kUnknown);
  EXPECT_EQ(ParseDiscType("\r", Disc()), DiscType::kUnknown);
  EXPECT_EQ(ParseDiscType("19011\r", Disc()), DiscType::kUnknown);
}

TEST(ResponseParserTest, TheWholeProgrammeStatusIsReadAndNotJustTheType) {
  // The manual's own worked example: "1 0 0 0 1" is a loaded 12-inch CAV disc,
  // side 1, with chapters. Every character is documented, so every character is
  // decoded — this reply is the disc's own programme status, and reading only
  // one digit of it was throwing away the side, the size and the chapters.
  const DiscStatus status = ParseDiscStatus("10001\r", Disc());

  ASSERT_TRUE(status.valid);
  EXPECT_EQ(status.loaded, std::optional<bool>(true));
  EXPECT_EQ(status.type, DiscType::kCav);
  EXPECT_EQ(status.size, DiscSize::k30cm);
  EXPECT_EQ(status.side, std::optional<int>(1));
  EXPECT_EQ(status.chapters, std::optional<bool>(true));
}

TEST(ResponseParserTest, TheSecondSideOfAnEightInchDiscReadsAsBoth) {
  const DiscStatus status = ParseDiscStatus("11110\r", Disc());

  ASSERT_TRUE(status.valid);
  EXPECT_EQ(status.type, DiscType::kClv);
  EXPECT_EQ(status.size, DiscSize::k20cm);
  EXPECT_EQ(status.side, std::optional<int>(2));
  EXPECT_EQ(status.chapters, std::optional<bool>(false));
}

TEST(ResponseParserTest, ThisProjectsOwnCasperReadingDecodesAsItsSecondSide) {
  // "11011", taken off the bench from an MCA Casper disc. The Pioneer user code
  // on that same side carries "!2", which this project had already guessed was
  // a side number — two independent readings agreeing is what turned that guess
  // into a fact.
  const DiscStatus status = ParseDiscStatus("11011\r", Disc());

  ASSERT_TRUE(status.valid);
  EXPECT_EQ(status.type, DiscType::kClv);
  EXPECT_EQ(status.size, DiscSize::k30cm);
  EXPECT_EQ(status.side, std::optional<int>(2));
  EXPECT_EQ(status.chapters, std::optional<bool>(true));
}

TEST(ResponseParserTest, AFieldThePlayerCouldNotDetermineIsAbsentAndNotFalse) {
  // Both manuals document 'X' for the last three fields, and the LD-V8000's
  // gives "0XXXX" as the reply from a player with nothing loaded. Read as
  // digits those X's would say "12-inch, side 1, no chapters" about a disc
  // that is not there.
  const DiscStatus status = ParseDiscStatus("0XXXX\r", Disc());

  ASSERT_TRUE(status.valid);
  EXPECT_EQ(status.loaded, std::optional<bool>(false));
  EXPECT_EQ(status.size, DiscSize::kUnknown);
  EXPECT_FALSE(status.side.has_value());
  EXPECT_FALSE(status.chapters.has_value());
}

TEST(ResponseParserTest, ADiscStatusOfTheWrongShapeIsNotReadAtAll) {
  // Short of the fields the decode names, so nothing in it is trustworthy —
  // including the characters that do happen to be there.
  EXPECT_FALSE(ParseDiscStatus("100\r", Disc()).valid);
  EXPECT_FALSE(ParseDiscStatus("\r", Disc()).valid);
}

TEST(ResponseParserTest, AModelThatReportsNoSuchFieldIsNotAskedToInventOne) {
  DiscStatusDecode decode;
  decode.disc_side = ReplyField{};

  const DiscStatus status = ParseDiscStatus("11011\r", decode);

  ASSERT_TRUE(status.valid);
  EXPECT_FALSE(status.side.has_value());
  EXPECT_EQ(status.type, DiscType::kClv);
}

TEST(ResponseParserTest, TheTvSystemReplyIsTheOneThingThatCarriesTheStandard) {
  // "220", read off this project's own LD-V4300D with a PAL CAV disc playing:
  // PAL out, PAL disc, no external sync. The NTSC value and the layout are the
  // LD-V4400 manual's (§38); the PAL value is the bench's, because that manual
  // describes an NTSC-only player and has no row for it.
  const TvSystem pal = ParseTvSystem("220\r", pioneer::kLdV4300D.tv_system);

  ASSERT_TRUE(pal.valid);
  EXPECT_EQ(pal.disc, VideoStandard::kPal);
  EXPECT_EQ(pal.output, VideoStandard::kPal);
  EXPECT_FALSE(pal.sync_connected());
}

TEST(ResponseParserTest, TheManualsOwnTwoExamplesReadAsItSaysTheyDo) {
  // "110" and "111" — the LD-V4400 manual's worked examples, both NTSC discs,
  // differing only in whether an external sync generator is connected.
  const TvSystem loose = ParseTvSystem("110\r", pioneer::kLdV4300D.tv_system);
  ASSERT_TRUE(loose.valid);
  EXPECT_EQ(loose.disc, VideoStandard::kNtsc);
  EXPECT_FALSE(loose.sync_connected());

  const TvSystem synced = ParseTvSystem("111\r", pioneer::kLdV4300D.tv_system);
  ASSERT_TRUE(synced.valid);
  EXPECT_EQ(synced.disc, VideoStandard::kNtsc);
  EXPECT_TRUE(synced.sync_connected());
  EXPECT_EQ(synced.external_sync, VideoStandard::kNtsc);
}

TEST(ResponseParserTest, TheDiscsStandardIsReadSeparatelyFromTheOutputs) {
  // A player converting a PAL disc to an NTSC output. The two fields exist
  // because they can disagree, and it is the disc's that a capture is of.
  const TvSystem converting =
      ParseTvSystem("120\r", pioneer::kLdV4300D.tv_system);

  ASSERT_TRUE(converting.valid);
  EXPECT_EQ(converting.output, VideoStandard::kNtsc);
  EXPECT_EQ(converting.disc, VideoStandard::kPal);
}

TEST(ResponseParserTest, AnUnknownTvSystemDigitIsNotGuessedAt) {
  const TvSystem nothing = ParseTvSystem("000\r", pioneer::kLdV4300D.tv_system);
  ASSERT_TRUE(nothing.valid);
  EXPECT_EQ(nothing.disc, VideoStandard::kUnknown);

  // Short of the three characters the layout names, so nothing in it is read.
  EXPECT_FALSE(ParseTvSystem("22\r", pioneer::kLdV4300D.tv_system).valid);
  EXPECT_FALSE(ParseTvSystem("\r", pioneer::kLdV4300D.tv_system).valid);
}

TEST(ResponseParserTest, AddressingFollowsFromTheDiscType) {
  EXPECT_EQ(AddressModeFor(DiscType::kCav), AddressMode::kFrame);
  EXPECT_EQ(AddressModeFor(DiscType::kClv), AddressMode::kTimeCode);
}

TEST(ResponseParserTest, ThePhysicalPositionIsByteSwappedAndScaled) {
  // The player reports the slider position in units of 10 micrometres, as
  // hexadecimal, from a little-endian processor. "1234" is therefore 0x3412
  // counts — 13330 — which is 133.30 mm.
  //
  // The sentinel is negative because no position is: a reply that parsed to
  // nothing fails the comparison rather than passing it silently.
  constexpr float kNoPosition = -1.0F;

  EXPECT_NEAR(ParsePhysicalPositionMillimetres("1234\r").value_or(kNoPosition),
              133.30F, 0.001F);

  // Zero is a real position — the assembly parked at the inner edge.
  EXPECT_NEAR(ParsePhysicalPositionMillimetres("0000\r").value_or(kNoPosition),
              0.0F, 0.001F);
}

TEST(ResponseParserTest, AnUnreadablePositionIsNoPositionAtAll) {
  // Rather than zero, which is a real position and would show the optical
  // assembly parked at the inner edge of a disc it is halfway across.
  EXPECT_FALSE(ParsePhysicalPositionMillimetres("12\r").has_value());
  EXPECT_FALSE(ParsePhysicalPositionMillimetres("\r").has_value());
  EXPECT_FALSE(ParsePhysicalPositionMillimetres("ZZZZ\r").has_value());
}

TEST(ResponseParserTest, TerminatorsAreCountedAndStripped) {
  EXPECT_EQ(CountTerminators("P04\r"), 1U);
  EXPECT_EQ(CountTerminators("P04\rP04\r"), 2U);
  EXPECT_EQ(CountTerminators("P04"), 0U);

  EXPECT_EQ(StripTerminator("P04\r"), "P04");
  EXPECT_EQ(StripTerminator("P04"), "P04");

  // A byte left over from a previous exchange would shift every offset the
  // decoders read at, so leading whitespace goes too.
  EXPECT_EQ(StripTerminator("\rP04\r"), "P04");
  EXPECT_EQ(StripTerminator(" P04 \r"), "P04");
}

}  // namespace
}  // namespace ddd::player
