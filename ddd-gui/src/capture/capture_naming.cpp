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

const char* DiscTypeChoiceName(DiscTypeChoice choice) {
  switch (choice) {
    case DiscTypeChoice::kCav:
      return "CAV";
    case DiscTypeChoice::kClv:
      return "CLV";
    case DiscTypeChoice::kUnset:
      break;
  }
  return "";
}

const char* VideoStandardChoiceName(VideoStandardChoice choice) {
  switch (choice) {
    case VideoStandardChoice::kNtsc:
      return "NTSC";
    case VideoStandardChoice::kPal:
      return "PAL";
    case VideoStandardChoice::kUnset:
      break;
  }
  return "";
}

const char* AudioTypeChoiceName(AudioTypeChoice choice) {
  switch (choice) {
    case AudioTypeChoice::kDefault:
      return "Default";
    case AudioTypeChoice::kAnalogue:
      return "Analogue";
    case AudioTypeChoice::kAc3:
      return "AC3";
    case AudioTypeChoice::kDts:
      return "DTS";
    case AudioTypeChoice::kUnset:
      break;
  }
  return "";
}

const char* DiscTypeChoiceToken(DiscTypeChoice choice) {
  return DiscTypeChoiceName(choice);
}

const char* VideoStandardChoiceToken(VideoStandardChoice choice) {
  return VideoStandardChoiceName(choice);
}

const char* AudioTypeChoiceToken(AudioTypeChoice choice) {
  switch (choice) {
    case AudioTypeChoice::kAnalogue:
      return "ANA";
    case AudioTypeChoice::kAc3:
      return "AC3";
    case AudioTypeChoice::kDts:
      return "DTS";
    case AudioTypeChoice::kDefault:
    case AudioTypeChoice::kUnset:
      break;
  }

  // Default contributes nothing to a name while still being a real answer in
  // the sidecar. The old application's rule, and a sound one: "_Default" in a
  // file name says less than the four characters cost.
  return "";
}

std::string BuildCaptureStem(const CaptureNamingFields& fields,
                             const std::string& typed_name, bool test_mode,
                             std::time_t when) {
  if (test_mode) {
    return DefaultCaptureStem(true, when);
  }

  // A typed name wins outright, and carries no timestamp. That is what the
  // Capture panel's Name field has always meant, and this is the one place the
  // meaning is stated.
  const std::string typed = SanitiseCaptureStem(typed_name);
  if (!typed.empty()) {
    return typed;
  }

  const std::string title =
      fields.title_used ? SanitiseCaptureStem(fields.title) : std::string();
  std::string name = title.empty() ? std::string(kCaptureNamePrefix) : title;

  // kCaptureNamePrefix carries its own trailing underscore, so that the
  // no-fields case produces exactly the name it produced before any of this
  // existed. Everything below appends its own separator, so a title has to be
  // given one.
  if (!title.empty()) {
    name += "_";
  }

  const auto append = [&name](const char* token) {
    if (token != nullptr && token[0] != '\0') {
      name += token;
      name += "_";
    }
  };

  if (fields.metadata_in_name) {
    if (fields.disc_type_used) {
      append(DiscTypeChoiceToken(fields.disc_type));
    }
    if (fields.video_standard_used) {
      append(VideoStandardChoiceToken(fields.video_standard));
    }
    if (fields.audio_used) {
      append(AudioTypeChoiceToken(fields.audio));
    }
  }

  // The side joins the name whether or not the rest of the details do, and that
  // asymmetry is deliberate rather than inherited. The two files somebody makes
  // in a row are the two sides of one disc, and telling them apart afterwards
  // is the whole problem a capture name exists to solve.
  if (fields.side_used) {
    name += "side" + std::to_string(fields.side) + "_";
  }

  if (fields.metadata_in_name) {
    if (fields.notes_used) {
      const std::string notes = SanitiseCaptureStem(fields.notes);
      if (!notes.empty()) {
        name += notes + "_";
      }
    }
    if (fields.mint_marks_used) {
      const std::string mint = SanitiseCaptureStem(fields.mint_marks);
      if (!mint.empty()) {
        name += mint + "_";
      }
    }
  }

  return name + FormatCaptureTimestamp(when);
}

std::string AppendDurationToStem(const std::string& stem, double seconds) {
  // Negative or non-finite is not a duration. Rather than refuse, this records
  // nothing: the alternative is a file that fails to be renamed at the end of a
  // capture, which is the worst possible moment to introduce a failure.
  if (!(seconds > 0.0)) {
    return stem;
  }

  const auto whole = static_cast<uint64_t>(seconds);
  const uint64_t hours = whole / 3600;
  const uint64_t minutes = (whole / 60) % 60;
  const uint64_t remainder = whole % 60;

  return stem + "_" + TwoDigits(static_cast<int>(hours)) + "H" +
         TwoDigits(static_cast<int>(minutes)) + "M" +
         TwoDigits(static_cast<int>(remainder)) + "S";
}

std::filesystem::path BuildCapturePath(const std::filesystem::path& directory,
                                       const std::string& stem, bool test_mode,
                                       std::time_t when,
                                       CaptureOutputFormat format) {
  // Test mode ignores the given name entirely rather than sanitising it — see
  // the header. Everything else falls back to the default only when nothing
  // usable survived.
  std::string name =
      test_mode ? DefaultCaptureStem(true, when) : SanitiseCaptureStem(stem);
  if (name.empty()) {
    name = DefaultCaptureStem(test_mode, when);
  }

  return directory / AddCaptureFileSuffix(name, format);
}

std::filesystem::path MakeUniqueCapturePath(
    const std::filesystem::path& preferred) {
  std::error_code error;
  if (!std::filesystem::exists(preferred, error)) {
    return preferred;
  }

  // The suffix is compound, so parent_path()/stem() would leave ".ddd" behind
  // and produce "name.ddd (1).flac". Taking the whole suffix off the string is
  // the only way to insert the number where a reader expects it.
  const std::string full = preferred.string();
  const std::string suffix = MatchedCaptureFileSuffix(full);
  const std::string base = StripCaptureFileSuffix(full);

  // " (1)", " (2)", … — the convention every desktop uses for a name already
  // taken, so it needs no explaining and reads as a copy rather than as part
  // of the name. The first collision is (1) and not (2): the number counts the
  // copies, not the files, which is what somebody comparing "Casper side 1"
  // with "Casper side 1 (1)" expects it to mean.
  std::filesystem::path candidate = preferred;
  for (int copy = 1; copy < kMaximumNameAttempts; ++copy) {
    candidate = std::filesystem::path(base + " (" + std::to_string(copy) + ")" +
                                      suffix);
    if (!std::filesystem::exists(candidate, error)) {
      return candidate;
    }
  }

  return candidate;
}

CaptureDestination ResolveCaptureDestination(
    const std::filesystem::path& directory, const std::string& stem,
    bool test_mode, std::time_t when, CaptureOutputFormat format) {
  const std::filesystem::path wanted =
      BuildCapturePath(directory, stem, test_mode, when, format);

  CaptureDestination destination;
  destination.path = MakeUniqueCapturePath(wanted);
  destination.as_requested = destination.path == wanted;

  // Taken off the resolved path rather than off the name that was asked for,
  // so that the stem an interface shows is the stem the file really carries —
  // including the "_2" this may have just added.
  const std::string suffix = CaptureFileSuffix(format);
  const std::string full = destination.path.filename().string();
  destination.stem =
      full.size() >= suffix.size() && full.compare(full.size() - suffix.size(),
                                                   suffix.size(), suffix) == 0
          ? full.substr(0, full.size() - suffix.size())
          : full;

  return destination;
}

}  // namespace ddd::capture
