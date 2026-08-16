/************************************************************************

    response_parser.cpp

    What came back from the player, turned into something typed
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "response_parser.h"

#include <algorithm>
#include <cctype>

#include "player_command.h"

namespace ddd::player {
namespace {

bool IsDigit(char character) {
  return std::isdigit(static_cast<unsigned char>(character)) != 0;
}

bool IsHexDigit(char character) {
  return std::isxdigit(static_cast<unsigned char>(character)) != 0;
}

bool IsSpace(char character) {
  return std::isspace(static_cast<unsigned char>(character)) != 0;
}

// The leading run of decimal digits.
std::string_view LeadingDigits(std::string_view text) {
  size_t length = 0;
  while (length < text.size() && IsDigit(text[length])) {
    ++length;
  }
  return text.substr(0, length);
}

// How many digits an address may have in this mode. Five is a CAV frame number
// and seven a CLV time code, which is the Pioneer address format.
size_t AddressDigits(AddressMode mode) {
  return mode == AddressMode::kTimeCode ? 7 : 5;
}

// The player's error code, if the refusal carried a legible one.
std::string ExtractErrorCode(std::string_view text) {
  const size_t start = text.find('E');
  if (start == std::string_view::npos) {
    return {};
  }

  size_t length = 1;
  while (start + length < text.size() && IsDigit(text[start + length])) {
    ++length;
  }

  // A bare 'E' with no digits after it is not a code, and reporting one would
  // put a meaningless "E" in front of a user.
  if (length == 1) {
    return {};
  }

  return std::string(text.substr(start, length));
}

}  // namespace

size_t CountTerminators(std::string_view raw) {
  return static_cast<size_t>(
      std::count(raw.begin(), raw.end(), kCommandTerminator));
}

std::string StripTerminator(std::string_view raw) {
  // Leading whitespace as well as the terminator, because a stray byte left in
  // the receive buffer would otherwise shift every offset the decoders read at.
  // The session discards the buffers before each exchange, so this is a second
  // line of defence rather than the first.
  size_t start = 0;
  while (start < raw.size() && IsSpace(raw[start])) {
    ++start;
  }

  size_t end = raw.size();
  while (end > start && (raw[end - 1] == kCommandTerminator ||
                         raw[end - 1] == '\n' || raw[end - 1] == ' ')) {
    --end;
  }

  return std::string(raw.substr(start, end - start));
}

Reply ParseAcknowledgement(std::string_view raw) {
  Reply reply;
  reply.text = StripTerminator(raw);

  if (reply.text.empty()) {
    reply.status = ReplyStatus::kNoAnswer;
    return reply;
  }

  if (reply.text.find('E') != std::string::npos) {
    reply.status = ReplyStatus::kRefused;
    reply.error_code = ExtractErrorCode(reply.text);
    return reply;
  }

  reply.status = ReplyStatus::kOk;
  return reply;
}

Reply ParseText(std::string_view raw) {
  // The terminator and nothing else — see the header. A user code is a
  // fixed-width record with space-padded fields, and trimming it would be
  // deleting data the caller asked for.
  size_t end = raw.size();
  while (end > 0 &&
         (raw[end - 1] == kCommandTerminator || raw[end - 1] == '\n')) {
    --end;
  }

  Reply reply;
  reply.text = std::string(raw.substr(0, end));
  reply.status = reply.text.empty() ? ReplyStatus::kNoAnswer : ReplyStatus::kOk;
  return reply;
}

bool IsErrorCode(std::string_view text) {
  // 'E' and at least one digit, and nothing else at all.
  if (text.size() < 2 || text.front() != 'E') {
    return false;
  }

  return std::all_of(text.begin() + 1, text.end(), IsDigit);
}

DiscAddress ParseAddress(std::string_view raw, AddressMode mode) {
  DiscAddress address;

  std::string text = StripTerminator(raw);
  std::string_view remaining(text);

  if (!remaining.empty() && remaining.front() == '<') {
    address.in_lead_in = true;
    remaining.remove_prefix(1);
  } else if (!remaining.empty() && remaining.front() == '>') {
    address.in_lead_out = true;
    remaining.remove_prefix(1);
  }

  std::string_view digits = LeadingDigits(remaining);

  if (digits.empty()) {
    return address;
  }

  // Leading zeros are padding, not width.
  //
  // Found on the bench: an LD-V4300D answers the address query with seven
  // zero-padded digits — "0002103" — whatever the disc is. Counting those as
  // significant would make every reading from that player too wide for frame
  // mode and refuse it, which is the opposite of what the width check is for:
  // it exists to catch a time code being read as a frame number, and a time
  // code that means anything has significant digits above the frame range.
  const size_t first_significant = digits.find_first_not_of('0');
  digits = first_significant == std::string_view::npos
               ? digits.substr(digits.size() - 1)
               : digits.substr(first_significant);

  if (digits.size() > AddressDigits(mode)) {
    return address;
  }

  int32_t value = 0;
  for (const char digit : digits) {
    value = (value * 10) + (digit - '0');
  }

  address.valid = true;
  address.value = value;
  return address;
}

PlayerState ParsePlayerState(std::string_view raw, const StateDecode& decode) {
  const std::string text = StripTerminator(raw);

  if (text.empty() || !std::string_view(text).starts_with(decode.prefix)) {
    return PlayerState::kUnknown;
  }

  for (const StateMapping& mapping : decode.mappings) {
    if (std::string_view(text).starts_with(mapping.code)) {
      return mapping.state;
    }
  }

  // A reply in the right shape carrying a code this model's table does not
  // have. Unknown rather than guessed: the states differ in whether the disc is
  // turning, and inventing one would have the automatic capture wait for a disc
  // that is stationary.
  return PlayerState::kUnknown;
}

DiscType ParseDiscType(std::string_view raw, const DiscStatusDecode& decode) {
  const std::string text = StripTerminator(raw);

  if (text.size() <= decode.disc_type_index) {
    return DiscType::kUnknown;
  }

  const char digit = text[decode.disc_type_index];

  if (digit == decode.cav_digit) {
    return DiscType::kCav;
  }
  if (digit == decode.clv_digit) {
    return DiscType::kClv;
  }

  return DiscType::kUnknown;
}

std::optional<float> ParsePhysicalPositionMillimetres(std::string_view raw) {
  const std::string text = StripTerminator(raw);

  // Four hexadecimal digits are one 16-bit count; anything shorter is not a
  // position.
  if (text.size() < 4) {
    return std::nullopt;
  }

  uint32_t value = 0;
  for (size_t index = 0; index < 4; ++index) {
    const char digit = text[index];
    if (!IsHexDigit(digit)) {
      return std::nullopt;
    }

    uint32_t nibble = 0;
    if (IsDigit(digit)) {
      nibble = static_cast<uint32_t>(digit - '0');
    } else {
      nibble = static_cast<uint32_t>(
          std::tolower(static_cast<unsigned char>(digit)) - 'a' + 10);
    }
    value = (value << 4) | nibble;
  }

  // The reporting processor is little-endian, so the two bytes arrive the other
  // way round.
  const uint32_t swapped = ((value & 0xFF00U) >> 8) | ((value & 0x00FFU) << 8);

  // The count is in units of 10 micrometres, which is a hundredth of a
  // millimetre.
  return static_cast<float>(swapped) / 100.0F;
}

}  // namespace ddd::player
