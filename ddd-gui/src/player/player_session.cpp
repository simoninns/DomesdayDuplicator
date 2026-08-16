/************************************************************************

    player_session.cpp

    Finding a player, identifying it, and talking to it
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "player_session.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "command_encoder.h"
#include "player_registry.h"

namespace ddd::player {
namespace {

Reply Failure(ReplyStatus status) {
  Reply reply;
  reply.status = status;
  return reply;
}

// Why the command could not be built, in terms of what it means to the caller.
ReplyStatus StatusForEncodeFailure(EncodeStatus status) {
  switch (status) {
    case EncodeStatus::kCommandUnsupported:
    case EncodeStatus::kParameterUnsupported:
      return ReplyStatus::kUnsupported;
    case EncodeStatus::kArgumentRequired:
    case EncodeStatus::kUnexpectedArgument:
    case EncodeStatus::kArgumentOutOfRange:
    case EncodeStatus::kCommandTooLong:
      return ReplyStatus::kInvalidArgument;
    case EncodeStatus::kOk:
      break;
  }
  return ReplyStatus::kInvalidArgument;
}

}  // namespace

PlayerSession::PlayerSession(ISerialPort* port, Clock clock)
    : port_(port), clock_(std::move(clock)) {
  if (!clock_) {
    clock_ = [] { return std::chrono::steady_clock::now(); };
  }
}

ProbeResult PlayerSession::Probe(const std::string& path,
                                 std::optional<uint32_t> baud_rate) {
  Disconnect();

  bool any_port_opened = false;
  bool unusable_seen = false;
  std::string unexpected_reply;
  uint32_t unexpected_rate = 0;

  for (const ProbeSpec* spec : RegisteredProbes()) {
    std::vector<uint32_t> rates;
    if (baud_rate.has_value()) {
      rates.push_back(*baud_rate);
    } else {
      rates.assign(spec->baud_rates.begin(), spec->baud_rates.end());
    }

    const int attempts =
        baud_rate.has_value() ? spec->fixed_attempts : spec->search_attempts;
    const std::chrono::milliseconds timeout =
        baud_rate.has_value() ? spec->fixed_timeout : spec->search_timeout;

    std::string request(spec->request);
    request += kCommandTerminator;

    for (const uint32_t rate : rates) {
      if (!port_->Open(path, SerialSettings{rate})) {
        continue;
      }
      any_port_opened = true;

      for (int attempt = 0; attempt < attempts; ++attempt) {
        port_->DiscardBuffers();

        if (!port_->Write(request)) {
          break;
        }

        std::string raw;
        if (!ReadReply(raw, timeout, 1)) {
          break;
        }

        const std::string text = StripTerminator(raw);
        if (text.empty()) {
          continue;
        }

        PlayerIdentity identity;
        if (Identify(*spec, text, identity)) {
          identity_ = identity;
          port_path_ = path;
          baud_rate_ = rate;
          connected_ = true;

          ProbeResult result;
          result.status = ProbeResult::Status::kConnected;
          result.identity = identity_;
          result.baud_rate = rate;
          return result;
        }

        // Garbage is the usual answer from a player listening at a different
        // rate, so the first one is remembered and the search carries on. It
        // is only reported if nothing anywhere identifies itself, where it is
        // the difference between "no player" and "something that is not a
        // player".
        if (!unusable_seen) {
          unusable_seen = true;
          unexpected_reply = text;
          unexpected_rate = rate;
        }
      }

      port_->Close();
    }
  }

  ProbeResult result;

  if (unusable_seen) {
    result.status = ProbeResult::Status::kUnusableAnswer;
    result.unexpected_reply = unexpected_reply;
    result.baud_rate = unexpected_rate;
    return result;
  }

  result.status = any_port_opened ? ProbeResult::Status::kNoAnswer
                                  : ProbeResult::Status::kPortUnavailable;
  return result;
}

void PlayerSession::Disconnect() {
  if (port_ != nullptr && port_->IsOpen()) {
    port_->Close();
  }

  connected_ = false;
  identity_ = PlayerIdentity{};
  port_path_.clear();
  baud_rate_ = 0;
}

bool PlayerSession::SupportsPhysicalPosition() const {
  if (!connected_ || !identity_.valid()) {
    return false;
  }
  return ddd::player::SupportsPhysicalPosition(*identity_.definition,
                                               identity_.firmware_version);
}

Reply PlayerSession::Execute(PlayerCommand command,
                             std::optional<int32_t> argument) {
  if (!connected_) {
    return Failure(ReplyStatus::kNotConnected);
  }

  const PlayerDefinition& definition = *identity_.definition;
  const EncodedCommand encoded = EncodeCommand(definition, command, argument);
  if (!encoded.ok()) {
    return Failure(StatusForEncodeFailure(encoded.status));
  }

  const CommandSpec& spec = Spec(definition, command);
  return Send(encoded.bytes, spec.response, spec.timeout, 1);
}

Reply PlayerSession::SetAudio(AudioMode mode) {
  if (!connected_) {
    return Failure(ReplyStatus::kNotConnected);
  }

  const PlayerDefinition& definition = *identity_.definition;
  const EncodedCommand encoded = EncodeAudio(definition, mode);
  if (!encoded.ok()) {
    return Failure(StatusForEncodeFailure(encoded.status));
  }

  const CommandSpec& spec = Spec(definition, PlayerCommand::kSetAudio);
  return Send(encoded.bytes, spec.response, spec.timeout, 1);
}

Reply PlayerSession::SetSpeed(PlaybackSpeed speed) {
  if (!connected_) {
    return Failure(ReplyStatus::kNotConnected);
  }

  const PlayerDefinition& definition = *identity_.definition;
  const EncodedCommand encoded = EncodeSpeed(definition, speed);
  if (!encoded.ok()) {
    return Failure(StatusForEncodeFailure(encoded.status));
  }

  const CommandSpec& spec = Spec(definition, PlayerCommand::kSetSpeed);
  return Send(encoded.bytes, spec.response, spec.timeout, 1);
}

Reply PlayerSession::SendRaw(std::string_view command, ResponseKind response,
                             TimeoutClass timeout, int expected_replies) {
  if (!connected_) {
    return Failure(ReplyStatus::kNotConnected);
  }

  std::string bytes(command);
  if (bytes.empty() || bytes.back() != kCommandTerminator) {
    bytes += kCommandTerminator;
  }

  if (bytes.size() > kMaximumCommandLength) {
    return Failure(ReplyStatus::kInvalidArgument);
  }

  return Send(bytes, response, timeout, expected_replies);
}

Reply PlayerSession::Send(std::string_view bytes, ResponseKind response,
                          TimeoutClass timeout, int expected_replies) {
  // Every reply from here carries the bytes that provoked it, including the
  // failures: "the link died sending FR100SE" is a more useful thing to have
  // been told than "the link died".
  const auto with_bytes = [bytes](Reply reply) {
    reply.sent = std::string(bytes);
    return reply;
  };

  port_->DiscardBuffers();

  if (!port_->Write(bytes)) {
    Disconnect();
    return with_bytes(Failure(ReplyStatus::kLinkFailed));
  }

  std::string raw;
  if (!ReadReply(raw, CommandTimeout(timeout), expected_replies)) {
    Disconnect();
    return with_bytes(Failure(ReplyStatus::kLinkFailed));
  }

  return with_bytes(response == ResponseKind::kAcknowledgement
                        ? ParseAcknowledgement(raw)
                        : ParseText(raw));
}

bool PlayerSession::ReadReply(std::string& into,
                              std::chrono::milliseconds timeout,
                              int expected_replies) {
  into.clear();

  const std::chrono::steady_clock::time_point deadline = clock_() + timeout;
  const size_t wanted = static_cast<size_t>(std::max(expected_replies, 1));

  while (clock_() < deadline) {
    const std::chrono::milliseconds remaining =
        std::max(std::chrono::duration_cast<std::chrono::milliseconds>(
                     deadline - clock_()),
                 std::chrono::milliseconds{1});

    if (!port_->Read(into, remaining)) {
      return false;
    }

    if (CountTerminators(into) >= wanted) {
      return true;
    }
  }

  // Out of time with an incomplete reply. Thrown away rather than returned:
  // half a reply parsed is a wrong answer, and the old application discarded it
  // for the same reason.
  into.clear();
  return true;
}

bool PlayerSession::Identify(const ProbeSpec& spec, std::string_view text,
                             PlayerIdentity& identity) {
  if (!text.starts_with(spec.reply_prefix)) {
    return false;
  }

  if (text.size() < spec.minimum_reply_length()) {
    return false;
  }

  identity.id_code =
      std::string(text.substr(spec.id_code_offset, spec.id_code_length));

  // The firmware revision is taken when it is there and left empty when it is
  // not. A player that answers with the prefix and an ID and stops is still
  // identified; only the firmware-gated capabilities are then unavailable,
  // which is the correct answer for a player that would not say.
  const size_t firmware_end = spec.firmware_offset + spec.firmware_length;
  if (text.size() >= firmware_end) {
    identity.firmware_version =
        std::string(text.substr(spec.firmware_offset, spec.firmware_length));
  }

  identity.model_code =
      std::string(text.substr(0, std::min(text.size(), firmware_end)));

  const PlayerDefinition* found = FindPlayerByIdCode(identity.id_code);
  identity.recognised = found != nullptr;
  identity.definition = found != nullptr ? found : &GenericPlayer();
  return true;
}

}  // namespace ddd::player
