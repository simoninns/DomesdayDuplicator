/************************************************************************

    capture_naming.cpp

    What a capture file is called, and where it is put
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_naming.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

#include "capture_format.h"

namespace ddd::capture {
namespace {

// The characters Windows will not accept in a filename, plus the separators
// that would turn a name into a path. The separators are the security-relevant
// half: without them a name of "../../etc/passwd" chooses where the capture is
// written, and a text field is not somewhere that decision should be made.
constexpr std::string_view kForbiddenCharacters = "<>:\"/\\|?*";

// Reserved on Windows with or without an extension, so "CON.ddd.flac" fails to
// open just as "CON" does.
constexpr std::array<std::string_view, 22> kReservedNames = {
    "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
    "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
    "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

std::string ToUpper(const std::string& text) {
  std::string result = text;
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::toupper(character));
                 });
  return result;
}

bool IsReservedName(const std::string& text) {
  const std::string upper = ToUpper(text);
  return std::find(kReservedNames.begin(), kReservedNames.end(), upper) !=
         kReservedNames.end();
}

std::string TwoDigits(int value) {
  std::string digits = std::to_string(value);
  if (digits.size() < 2) {
    digits.insert(digits.begin(), '0');
  }
  return digits;
}

}  // namespace

std::string FormatCaptureTimestamp(std::time_t when) {
  std::tm parts{};
#ifdef _WIN32
  localtime_s(&parts, &when);
#else
  localtime_r(&when, &parts);
#endif

  // Built by hand rather than with strftime, because strftime's output depends
  // on the C locale and a capture's name should not: a machine set to a
  // different locale must produce a name that sorts alongside every other one.
  return std::to_string(parts.tm_year + 1900) + "-" +
         TwoDigits(parts.tm_mon + 1) + "-" + TwoDigits(parts.tm_mday) + "_" +
         TwoDigits(parts.tm_hour) + "-" + TwoDigits(parts.tm_min) + "-" +
         TwoDigits(parts.tm_sec);
}

std::string DefaultCaptureStem(bool test_mode, std::time_t when) {
  const std::string prefix =
      test_mode ? kTestCaptureNamePrefix : kCaptureNamePrefix;
  return prefix + FormatCaptureTimestamp(when);
}

std::string SanitiseCaptureStem(const std::string& text) {
  std::string result;
  result.reserve(text.size());

  for (const char character : text) {
    const auto value = static_cast<unsigned char>(character);
    if (value < 0x20 || value == 0x7F) {
      continue;
    }
    if (kForbiddenCharacters.find(character) != std::string_view::npos) {
      continue;
    }
    result.push_back(character);
  }

  // Leading and trailing whitespace is invisible in a text field and confusing
  // in a directory listing; a trailing dot is silently dropped by Windows,
  // which would make the name on disk differ from the name that was typed.
  const auto not_space = [](unsigned char character) {
    return std::isspace(character) == 0;
  };
  result.erase(result.begin(),
               std::find_if(result.begin(), result.end(), not_space));
  while (!result.empty() &&
         (std::isspace(static_cast<unsigned char>(result.back())) != 0 ||
          result.back() == '.')) {
    result.pop_back();
  }

  if (IsReservedName(result)) {
    return {};
  }

  return result;
}

std::filesystem::path BuildCapturePath(const std::filesystem::path& directory,
                                       const std::string& stem, bool test_mode,
                                       std::time_t when) {
  // Test mode ignores the given name entirely rather than sanitising it — see
  // the header. Everything else falls back to the default only when nothing
  // usable survived.
  std::string name =
      test_mode ? DefaultCaptureStem(true, when) : SanitiseCaptureStem(stem);
  if (name.empty()) {
    name = DefaultCaptureStem(test_mode, when);
  }

  return directory / AddCaptureFileSuffix(name);
}

std::filesystem::path MakeUniqueCapturePath(
    const std::filesystem::path& preferred) {
  std::error_code error;
  if (!std::filesystem::exists(preferred, error)) {
    return preferred;
  }

  // The suffix is compound, so parent_path()/stem() would leave ".ddd" behind
  // and produce "name.ddd_2.flac". Taking the whole suffix off the string is
  // the only way to insert the number where a reader expects it.
  const std::string full = preferred.string();
  const std::string suffix = kCaptureFileSuffix;
  const std::string base =
      (full.size() >= suffix.size() &&
       full.compare(full.size() - suffix.size(), suffix.size(), suffix) == 0)
          ? full.substr(0, full.size() - suffix.size())
          : full;

  std::filesystem::path candidate = preferred;
  for (int attempt = 2; attempt <= kMaximumNameAttempts; ++attempt) {
    candidate = std::filesystem::path(base + "_" + std::to_string(attempt) +
                                      std::string(kCaptureFileSuffix));
    if (!std::filesystem::exists(candidate, error)) {
      return candidate;
    }
  }

  return candidate;
}

}  // namespace ddd::capture
