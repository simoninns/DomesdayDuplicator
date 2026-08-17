/************************************************************************

    disc_profile.cpp

    What an examination of the disc found, and how it found each part
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "disc_profile.h"

#include <cmath>

namespace ddd::player {
namespace {

// A seven-digit time code as whole seconds. "1234500" is 1:23:45, frame 00.
//
// The frames are dropped rather than rounded, because what this feeds is a
// duration shown to a user and an estimate of a file size, and neither is
// improved by a thirtieth of a second.
int64_t TimeCodeSeconds(int32_t time_code) {
  const int64_t value = time_code;
  const int64_t hours = value / 1'000'000;
  const int64_t minutes = (value / 10'000) % 100;
  const int64_t seconds = (value / 100) % 100;
  return (hours * 3600) + (minutes * 60) + seconds;
}

}  // namespace

std::optional<double> FrameRate(VideoStandard standard) {
  switch (standard) {
    case VideoStandard::kNtsc:
      // Colour NTSC, so 30000/1001 rather than a round 30. The difference is a
      // tenth of a per cent, which over a full CAV side is about two seconds —
      // small, but free to get right.
      return 30000.0 / 1001.0;
    case VideoStandard::kPal:
      return 25.0;
    case VideoStandard::kUnknown:
      return std::nullopt;
  }
  return std::nullopt;
}

std::optional<std::chrono::seconds> AddressSpanDuration(
    int32_t start, int32_t end, DiscType type, VideoStandard standard) {
  if (type == DiscType::kUnknown || end <= start) {
    return std::nullopt;
  }

  if (type == DiscType::kClv) {
    // The addresses are already times, so no frame rate is needed and none of
    // this depends on the video standard.
    const int64_t seconds = TimeCodeSeconds(end) - TimeCodeSeconds(start);
    if (seconds <= 0) {
      return std::nullopt;
    }
    return std::chrono::seconds(seconds);
  }

  const std::optional<double> rate = FrameRate(standard);
  if (!rate.has_value() || *rate <= 0.0) {
    return std::nullopt;
  }

  const double frames = static_cast<double>(end - start) + 1.0;
  return std::chrono::seconds(
      static_cast<int64_t>(std::llround(frames / *rate)));
}

std::optional<std::chrono::seconds> ProgrammeDuration(const DiscProfile& disc) {
  if (!disc.disc_type.known() || !disc.programme_end.known()) {
    return std::nullopt;
  }

  // A disc whose start was never measured is taken to begin at its beginning,
  // which for both addressing schemes is the smallest address there is. That is
  // an assumption, and it is a safe one: the alternative is refusing to state a
  // duration for a disc whose length is perfectly well known.
  const int32_t start =
      disc.programme_start.known() ? disc.programme_start.value : 0;

  return AddressSpanDuration(start, disc.programme_end.value,
                             disc.disc_type.value, disc.video_standard.value);
}

}  // namespace ddd::player
