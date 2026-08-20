/************************************************************************

    log_format.cpp

    Numbers as a log line says them
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "log_format.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace ddd::capture {
namespace {

// Two digits, for the minutes and seconds of a longer duration. Without it
// "1 h 12 m 4 s" reads as an unrelated set of numbers rather than as a clock.
std::string TwoDigits(int value) {
  std::string text = std::to_string(value);
  if (text.size() < 2) {
    text.insert(text.begin(), '0');
  }
  return text;
}

}  // namespace

std::string FormatDecimal(double value, int decimals) {
  if (!std::isfinite(value)) {
    value = 0.0;
  }
  if (decimals < 0) {
    decimals = 0;
  }

  std::array<char, 64> buffer{};
  const int written =
      std::snprintf(buffer.data(), buffer.size(), "%.*f", decimals, value);
  if (written <= 0) {
    return "0";
  }

  // snprintf writes the decimal separator the C locale asks for, and a machine
  // set to a European locale asks for a comma. A log file has one spelling of a
  // number, so the separator is put back by hand rather than by trusting
  // whoever last called setlocale — this library is Qt-free and runs inside a
  // command-line tool as well as under an application that resets it.
  std::string text;
  text.reserve(static_cast<size_t>(written));
  for (int index = 0; index < written; ++index) {
    const char character = buffer[static_cast<size_t>(index)];
    text += character == ',' ? '.' : character;
  }
  return text;
}

std::string FormatBytes(uint64_t bytes) {
  constexpr uint64_t kKibi = uint64_t{1} << 10;
  constexpr uint64_t kMebi = uint64_t{1} << 20;
  constexpr uint64_t kGibi = uint64_t{1} << 30;

  const auto value = static_cast<double>(bytes);

  if (bytes >= kGibi) {
    return FormatDecimal(value / static_cast<double>(kGibi), 2) + " GiB";
  }
  if (bytes >= kMebi) {
    return FormatDecimal(value / static_cast<double>(kMebi), 1) + " MiB";
  }
  if (bytes >= kKibi) {
    return FormatDecimal(value / static_cast<double>(kKibi), 1) + " KiB";
  }
  return std::to_string(bytes) + " B";
}

std::string FormatDuration(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0.0) {
    seconds = 0.0;
  }

  if (seconds < 1.0) {
    return FormatDecimal(seconds * 1000.0, 0) + " ms";
  }
  if (seconds < 60.0) {
    return FormatDecimal(seconds, 2) + " s";
  }

  const auto whole = static_cast<uint64_t>(seconds);
  const int display_seconds = static_cast<int>(whole % 60);
  const auto minutes = whole / 60;

  if (minutes < 60) {
    return std::to_string(minutes) + " m " + TwoDigits(display_seconds) + " s";
  }

  return std::to_string(minutes / 60) + " h " +
         TwoDigits(static_cast<int>(minutes % 60)) + " m " +
         TwoDigits(display_seconds) + " s";
}

std::string FormatSampleDuration(uint64_t samples, uint32_t sample_rate_hz) {
  if (sample_rate_hz == 0) {
    return FormatDuration(0.0);
  }
  return FormatDuration(static_cast<double>(samples) /
                        static_cast<double>(sample_rate_hz));
}

}  // namespace ddd::capture
