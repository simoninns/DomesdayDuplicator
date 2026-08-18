/************************************************************************

    player_capabilities.h

    What a model can be asked to do
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>

namespace ddd::player {

// Whether a model can report where its optical assembly physically is.
//
// A tri-state rather than a flag because the one player known to offer it — the
// LD-V8000 — offers it from one firmware revision onwards, so the answer is not
// a property of the model alone. A definition declares the gate and the
// firmware version decides; see PlayerDefinition::physical_position_firmware.
enum class PhysicalPositionSupport : uint8_t {
  kUnsupported,
  kFirmwareGated,
  kSupported,
};

// What the interface may offer for a connected model.
//
// The point of having this at all is a remote whose buttons are honest. The old
// application offered every control to every player, so a control the player
// did not have was present, enabled, and silently did nothing — which is
// indistinguishable from a broken cable.
//
// Every flag defaults to true because the Pioneer Level III set is the baseline
// and a model that lacks something says so; a model that has to enumerate what
// it *has* is a model whose definition is wrong by omission the moment a flag
// is added here.
//
// The registry checks each flag against the command table, so a capability that
// is claimed with nothing to send for it fails the build rather than producing
// a dead button.
struct PlayerCapabilities {
  bool tray_control = true;
  bool stop_codes_disabled_play = true;
  bool step = true;
  bool scan = true;
  bool multi_speed = true;
  bool speed_selection = true;
  bool frame_search = true;
  bool time_code_search = true;
  bool chapter_search = true;
  bool on_screen_display = true;
  bool audio_selection = true;

  // Digital audio tracks, as distinct from the analogue pair. Checked against
  // the audio parameter table rather than against a command, since it is the
  // same command with a different parameter.
  bool digital_audio = true;

  bool key_lock = true;
  bool standard_user_code = true;
  bool pioneer_user_code = true;

  // Can this model be asked which television standard the disc carries?
  //
  // On by default because it is documented in the shared Level III manual and
  // confirmed on this project's own player. A model that turns out not to have
  // it turns this off, and the examination reports the standard as not
  // established rather than getting it wrong — which is the only outcome that
  // would actually harm a capture.
  bool tv_system = true;

  PhysicalPositionSupport physical_position =
      PhysicalPositionSupport::kUnsupported;
};

}  // namespace ddd::player
