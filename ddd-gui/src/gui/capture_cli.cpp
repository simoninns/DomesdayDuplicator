/************************************************************************

    capture_cli.cpp

    The capture options the command line accepts, and what they mean
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_cli.h"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QLatin1String>
#include <QStringList>

namespace ddd::gui {
namespace {

// The switch names, in one place: the parser is given them here, and
// WantsCoreApplication() below has to recognise two of them without a parser.
constexpr const char* kStartCaptureName = "start-capture";
constexpr const char* kStopCaptureName = "stop-capture";
constexpr const char* kHeadlessName = "headless";
constexpr const char* kCaptureDirectoryName = "capture-directory";
constexpr const char* kCaptureNameName = "capture-name";
constexpr const char* kSampleRateName = "sample-rate";
constexpr const char* kDurationLimitName = "duration-limit";
constexpr const char* kOutputFormatName = "output-format";

// The format words, spelled as the settings file spells them, so that a script
// and a settings file name the same format the same way. The reading of them
// here is strict where the settings loader's is forgiving: a settings file
// naming a format this build does not have should still produce a working
// capture, but a command line naming one is a typo, and a script that silently
// wrote FLAC when it asked for raw samples would be found out much later.
constexpr const char* kFlacFormatWord = "flac";
constexpr const char* kSigned16BitFormatWord = "s16";

// The rates this build can capture at, derived from the decimation factors
// rather than written out beside them: a factor added to capture_format.h
// appears on the command line without anything having to be kept in step.
constexpr int kSupportedDecimationFactors[] = {capture::kUndecimatedFactor,
                                               capture::kTapeDecimationFactor};

constexpr uint32_t kHzPerMsps = 1'000'000;

int MegasamplesPerSecondFor(int decimation_factor) {
  return static_cast<int>(capture::SampleRateHzFor(decimation_factor) /
                          kHzPerMsps);
}

std::optional<int> DecimationFactorForRate(int megasamples_per_second) {
  for (const int factor : kSupportedDecimationFactors) {
    if (MegasamplesPerSecondFor(factor) == megasamples_per_second) {
      return factor;
    }
  }
  return std::nullopt;
}

QString SupportedRateWords() {
  QStringList rates;
  for (const int factor : kSupportedDecimationFactors) {
    rates.append(QString::number(MegasamplesPerSecondFor(factor)));
  }
  return rates.join(QStringLiteral(" or "));
}

// Whether a raw argument is this option, in either of the spellings Qt's parser
// accepts for a long name.
bool IsOptionToken(const QString& token, const char* name) {
  const QString long_form = QLatin1String("--") + QLatin1String(name);
  const QString short_form = QLatin1String("-") + QLatin1String(name);
  return token == long_form || token == short_form;
}

}  // namespace

bool CaptureCliOptions::HasAttributeOverrides() const {
  return capture_directory.has_value() || capture_name.has_value() ||
         decimation_factor.has_value() || duration_limit_seconds.has_value() ||
         output_format.has_value();
}

CaptureCliOptionSet AddCaptureCliOptions(QCommandLineParser& parser) {
  CaptureCliOptionSet set{
      QCommandLineOption(
          QLatin1String(kStartCaptureName),
          QStringLiteral("Start capturing as soon as a device is found. The "
                         "window still opens unless --headless is given.")),
      QCommandLineOption(
          QLatin1String(kStopCaptureName),
          QStringLiteral("Stop the capture a running instance is taking, wait "
                         "for its file to be finished, and exit.")),
      QCommandLineOption(
          QLatin1String(kHeadlessName),
          QStringLiteral("Run with no window. Requires --start-capture, and "
                         "runs until the duration limit, --stop-capture, or an "
                         "interrupt.")),
      QCommandLineOption(
          QLatin1String(kCaptureDirectoryName),
          QStringLiteral("Write the capture here instead of the configured "
                         "folder. Created if it does not exist."),
          QStringLiteral("directory")),
      QCommandLineOption(
          QLatin1String(kCaptureNameName),
          QStringLiteral("Name the capture this, without a suffix, instead of "
                         "the configured or generated name."),
          QStringLiteral("name")),
      QCommandLineOption(
          QLatin1String(kSampleRateName),
          QStringLiteral("Capture at this rate in Msps: %1. Decimation is done "
                         "by the device.")
              .arg(SupportedRateWords()),
          QStringLiteral("msps")),
      QCommandLineOption(
          QLatin1String(kDurationLimitName),
          QStringLiteral("Stop automatically after this many seconds. Omit it "
                         "to run until stopped."),
          QStringLiteral("seconds")),
      QCommandLineOption(QLatin1String(kOutputFormatName),
                         QStringLiteral("Write the capture as %1 or %2.")
                             .arg(QLatin1String(kFlacFormatWord),
                                  QLatin1String(kSigned16BitFormatWord)),
                         QStringLiteral("format")),
  };

  parser.addOption(set.start_capture);
  parser.addOption(set.stop_capture);
  parser.addOption(set.headless);
  parser.addOption(set.capture_directory);
  parser.addOption(set.capture_name);
  parser.addOption(set.sample_rate);
  parser.addOption(set.duration_limit);
  parser.addOption(set.output_format);

  return set;
}

CaptureCliParseResult ParseCaptureCliOptions(const QCommandLineParser& parser,
                                             const CaptureCliOptionSet& set) {
  CaptureCliParseResult result;
  CaptureCliOptions& options = result.options;

  options.start_capture = parser.isSet(set.start_capture);
  options.stop_capture = parser.isSet(set.stop_capture);
  options.headless = parser.isSet(set.headless);

  if (parser.isSet(set.capture_directory)) {
    const QString directory = parser.value(set.capture_directory).trimmed();
    if (directory.isEmpty()) {
      result.error = QStringLiteral(
          "--capture-directory needs a folder. Leave it out to use the "
          "configured one.");
      return result;
    }

    // Existing and not a folder is the only case worth refusing. A folder that
    // is not there yet is made when the capture is opened, exactly as it is for
    // a capture started from the window, so a script that names a folder per
    // disc works without creating it first.
    const QFileInfo info(directory);
    if (info.exists() && !info.isDir()) {
      result.error = QStringLiteral("--capture-directory '%1' is not a folder.")
                         .arg(directory);
      return result;
    }
    options.capture_directory = directory;
  }

  if (parser.isSet(set.capture_name)) {
    const QString name = parser.value(set.capture_name).trimmed();
    if (name.isEmpty()) {
      result.error = QStringLiteral(
          "--capture-name needs a name. Leave it out for the generated one.");
      return result;
    }

    // A name, not a path. The window's name field would accept a separator and
    // fail much later when the file was opened; a script gets told now, which
    // is the whole point of checking a command line rather than a text box.
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))) {
      result.error =
          QStringLiteral(
              "--capture-name '%1' contains a path separator. Name the folder "
              "with --capture-directory.")
              .arg(name);
      return result;
    }
    options.capture_name = name;
  }

  if (parser.isSet(set.sample_rate)) {
    const QString text = parser.value(set.sample_rate).trimmed();
    bool numeric = false;
    const int rate = text.toInt(&numeric);
    const std::optional<int> factor =
        numeric ? DecimationFactorForRate(rate) : std::nullopt;
    if (!factor.has_value()) {
      result.error =
          QStringLiteral("Unknown --sample-rate '%1'. Use %2, in Msps.")
              .arg(text, SupportedRateWords());
      return result;
    }
    options.decimation_factor = factor;
  }

  if (parser.isSet(set.duration_limit)) {
    const QString text = parser.value(set.duration_limit).trimmed();
    bool numeric = false;
    const int seconds = text.toInt(&numeric);

    // Zero means "no limit" in the settings, and is refused here rather than
    // accepted as one: a script that computed a limit of zero has a bug, and
    // silently capturing until something else stopped it would hide it.
    if (!numeric || seconds < 1 ||
        seconds > CaptureSettings::kMaximumDurationLimitSeconds) {
      result.error =
          QStringLiteral(
              "Unknown --duration-limit '%1'. Use 1 to %2 seconds (%3 hours), "
              "or leave it out to capture until stopped.")
              .arg(text)
              .arg(CaptureSettings::kMaximumDurationLimitSeconds)
              .arg(CaptureSettings::kMaximumDurationLimitMinutes / 60);
      return result;
    }
    options.duration_limit_seconds = seconds;
  }

  if (parser.isSet(set.output_format)) {
    const QString word = parser.value(set.output_format).trimmed().toLower();
    if (word == QLatin1String(kFlacFormatWord)) {
      options.output_format = capture::CaptureOutputFormat::kFlac;
    } else if (word == QLatin1String(kSigned16BitFormatWord)) {
      options.output_format = capture::CaptureOutputFormat::kSigned16Bit;
    } else {
      result.error =
          QStringLiteral("Unknown --output-format '%1'. Use %2 or %3.")
              .arg(parser.value(set.output_format),
                   QLatin1String(kFlacFormatWord),
                   QLatin1String(kSigned16BitFormatWord));
      return result;
    }
  }

  // --stop-capture is a message to a process that is already running and has
  // already been told what to capture. Anything else on the line is an
  // instruction with nowhere to go, so it is refused rather than dropped.
  if (options.stop_capture && (options.start_capture || options.headless ||
                               options.HasAttributeOverrides())) {
    result.error = QStringLiteral(
        "--stop-capture stops a capture that is already running, so it cannot "
        "be given with the options that set one up.");
    return result;
  }

  if (options.headless && !options.start_capture) {
    result.error = QStringLiteral(
        "--headless needs --start-capture. Without a window and without a "
        "capture there would be nothing for the application to do.");
    return result;
  }

  return result;
}

void ApplyCliOverrides(CaptureSettings& settings,
                       const CaptureCliOptions& options) {
  if (options.capture_directory.has_value()) {
    settings.capture_directory = *options.capture_directory;
  }
  if (options.capture_name.has_value()) {
    settings.capture_name = *options.capture_name;
  }
  if (options.decimation_factor.has_value()) {
    settings.decimation_factor = *options.decimation_factor;
  }
  if (options.duration_limit_seconds.has_value()) {
    settings.duration_limit_seconds = *options.duration_limit_seconds;
  }
  if (options.output_format.has_value()) {
    settings.output_format = *options.output_format;
  }
}

bool WantsCoreApplication(int argc, char* argv[]) {
  for (int index = 1; index < argc; ++index) {
    const QString token = QString::fromLocal8Bit(argv[index]);

    // Everything after a bare -- is an argument rather than an option, and this
    // application has none. Stopping here anyway costs nothing and keeps the
    // scan honest about what an option is.
    if (token == QLatin1String("--")) {
      break;
    }

    if (IsOptionToken(token, kHeadlessName) ||
        IsOptionToken(token, kStopCaptureName)) {
      return true;
    }
  }
  return false;
}

}  // namespace ddd::gui
