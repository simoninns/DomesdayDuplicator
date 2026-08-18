/************************************************************************

    capture_settings.cpp

    The settings a capture runs with, and where they are kept
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_settings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>

#include "sample_format.h"

namespace ddd::gui {
namespace {

constexpr const char* kPreferredDeviceKey = "capture/preferred_device";
constexpr const char* kQueueSizeKey = "capture/queue_size_bytes";
constexpr const char* kSmallTransfersKey = "capture/small_transfers";
constexpr const char* kTransferQueueKey = "capture/transfer_queue_bytes";
constexpr const char* kFrontEndGainKey = "hardware/front_end_gain_switches";
constexpr const char* kCaptureDirectoryKey = "capture/directory";
constexpr const char* kCaptureNameKey = "capture/name";
constexpr const char* kOutputFormatKey = "capture/output_format";
constexpr const char* kDecimationFactorKey = "capture/decimation_factor";
constexpr const char* kCompressionLevelKey = "capture/compression_level";
constexpr const char* kDurationLimitKey = "capture/duration_limit_seconds";
constexpr const char* kLowSpaceKey = "capture/low_space_warning_minutes";

// The naming fields. Their own group, so that a settings file stays readable
// and so that clearing them is one group to remove by hand.
constexpr const char* kNamingTitleUsedKey = "naming/title_used";
constexpr const char* kNamingTitleKey = "naming/title";
constexpr const char* kNamingDiscTypeUsedKey = "naming/disc_type_used";
constexpr const char* kNamingDiscTypeKey = "naming/disc_type";
constexpr const char* kNamingStandardUsedKey = "naming/video_standard_used";
constexpr const char* kNamingStandardKey = "naming/video_standard";
constexpr const char* kNamingAudioUsedKey = "naming/audio_used";
constexpr const char* kNamingAudioKey = "naming/audio";
constexpr const char* kNamingSideUsedKey = "naming/side_used";
constexpr const char* kNamingSideKey = "naming/side";
constexpr const char* kNamingNotesUsedKey = "naming/notes_used";
constexpr const char* kNamingNotesKey = "naming/notes";
constexpr const char* kNamingMintUsedKey = "naming/mint_marks_used";
constexpr const char* kNamingMintKey = "naming/mint_marks";
constexpr const char* kNamingMetadataNotesKey = "naming/metadata_notes";
constexpr const char* kNamingMetadataInNameKey = "naming/metadata_in_name";
constexpr const char* kNamingAppendDurationKey = "naming/append_duration";
constexpr const char* kNamingPerSideNotesKey = "naming/per_side_notes";
constexpr const char* kNamingPerSideMintKey = "naming/per_side_mint_marks";

// The largest side number the naming fields will hold. Not a limit anybody
// meets — a disc has two sides and a set has a few dozen — but a stored value
// has to be bounded somewhere, and a spin box that offers a hundred is
// generous without being absurd.
constexpr int kMaximumDiscSide = 100;

// The choice enumerations are stored as words rather than as their
// enumerators' numbers, on the same reasoning as the output format: a settings
// file stays readable, and inserting a choice later cannot renumber what an
// existing file meant.
QString DiscTypeChoiceKey(capture::DiscTypeChoice choice) {
  switch (choice) {
    case capture::DiscTypeChoice::kCav:
      return QStringLiteral("cav");
    case capture::DiscTypeChoice::kClv:
      return QStringLiteral("clv");
    case capture::DiscTypeChoice::kUnset:
      break;
  }
  return QString();
}

capture::DiscTypeChoice DiscTypeChoiceFromKey(const QString& text) {
  if (text == QLatin1String("cav")) {
    return capture::DiscTypeChoice::kCav;
  }
  if (text == QLatin1String("clv")) {
    return capture::DiscTypeChoice::kClv;
  }
  return capture::DiscTypeChoice::kUnset;
}

QString VideoStandardChoiceKey(capture::VideoStandardChoice choice) {
  switch (choice) {
    case capture::VideoStandardChoice::kNtsc:
      return QStringLiteral("ntsc");
    case capture::VideoStandardChoice::kPal:
      return QStringLiteral("pal");
    case capture::VideoStandardChoice::kUnset:
      break;
  }
  return QString();
}

capture::VideoStandardChoice VideoStandardChoiceFromKey(const QString& text) {
  if (text == QLatin1String("ntsc")) {
    return capture::VideoStandardChoice::kNtsc;
  }
  if (text == QLatin1String("pal")) {
    return capture::VideoStandardChoice::kPal;
  }
  return capture::VideoStandardChoice::kUnset;
}

QString AudioTypeChoiceKey(capture::AudioTypeChoice choice) {
  switch (choice) {
    case capture::AudioTypeChoice::kDefault:
      return QStringLiteral("default");
    case capture::AudioTypeChoice::kAnalogue:
      return QStringLiteral("analogue");
    case capture::AudioTypeChoice::kAc3:
      return QStringLiteral("ac3");
    case capture::AudioTypeChoice::kDts:
      return QStringLiteral("dts");
    case capture::AudioTypeChoice::kUnset:
      break;
  }
  return QString();
}

capture::AudioTypeChoice AudioTypeChoiceFromKey(const QString& text) {
  if (text == QLatin1String("default")) {
    return capture::AudioTypeChoice::kDefault;
  }
  if (text == QLatin1String("analogue")) {
    return capture::AudioTypeChoice::kAnalogue;
  }
  if (text == QLatin1String("ac3")) {
    return capture::AudioTypeChoice::kAc3;
  }
  if (text == QLatin1String("dts")) {
    return capture::AudioTypeChoice::kDts;
  }
  return capture::AudioTypeChoice::kUnset;
}

capture::CaptureNamingFields LoadNamingFields(const QSettings& settings) {
  capture::CaptureNamingFields fields;

  const auto flag = [&settings](const char* key) {
    return settings.value(QLatin1String(key), false).toBool();
  };
  const auto text = [&settings](const char* key) {
    return settings.value(QLatin1String(key)).toString().toStdString();
  };

  fields.title_used = flag(kNamingTitleUsedKey);
  fields.title = text(kNamingTitleKey);

  fields.disc_type_used = flag(kNamingDiscTypeUsedKey);
  fields.disc_type = DiscTypeChoiceFromKey(
      settings.value(QLatin1String(kNamingDiscTypeKey)).toString());

  fields.video_standard_used = flag(kNamingStandardUsedKey);
  fields.video_standard = VideoStandardChoiceFromKey(
      settings.value(QLatin1String(kNamingStandardKey)).toString());

  fields.audio_used = flag(kNamingAudioUsedKey);
  fields.audio = AudioTypeChoiceFromKey(
      settings.value(QLatin1String(kNamingAudioKey)).toString());

  fields.side_used = flag(kNamingSideUsedKey);
  fields.side =
      std::clamp(settings.value(QLatin1String(kNamingSideKey), 1).toInt(), 1,
                 kMaximumDiscSide);

  fields.notes_used = flag(kNamingNotesUsedKey);
  fields.notes = text(kNamingNotesKey);

  fields.mint_marks_used = flag(kNamingMintUsedKey);
  fields.mint_marks = text(kNamingMintKey);

  fields.metadata_notes = text(kNamingMetadataNotesKey);
  fields.metadata_in_name = flag(kNamingMetadataInNameKey);
  fields.append_duration = flag(kNamingAppendDurationKey);
  fields.per_side_notes = flag(kNamingPerSideNotesKey);
  fields.per_side_mint_marks = flag(kNamingPerSideMintKey);

  return fields;
}

void SaveNamingFields(QSettings& store,
                      const capture::CaptureNamingFields& fields) {
  const auto set = [&store](const char* key, const QVariant& value) {
    store.setValue(QLatin1String(key), value);
  };

  set(kNamingTitleUsedKey, fields.title_used);
  set(kNamingTitleKey, QString::fromStdString(fields.title));
  set(kNamingDiscTypeUsedKey, fields.disc_type_used);
  set(kNamingDiscTypeKey, DiscTypeChoiceKey(fields.disc_type));
  set(kNamingStandardUsedKey, fields.video_standard_used);
  set(kNamingStandardKey, VideoStandardChoiceKey(fields.video_standard));
  set(kNamingAudioUsedKey, fields.audio_used);
  set(kNamingAudioKey, AudioTypeChoiceKey(fields.audio));
  set(kNamingSideUsedKey, fields.side_used);
  set(kNamingSideKey, fields.side);
  set(kNamingNotesUsedKey, fields.notes_used);
  set(kNamingNotesKey, QString::fromStdString(fields.notes));
  set(kNamingMintUsedKey, fields.mint_marks_used);
  set(kNamingMintKey, QString::fromStdString(fields.mint_marks));
  set(kNamingMetadataNotesKey, QString::fromStdString(fields.metadata_notes));
  set(kNamingMetadataInNameKey, fields.metadata_in_name);
  set(kNamingAppendDurationKey, fields.append_duration);
  set(kNamingPerSideNotesKey, fields.per_side_notes);
  set(kNamingPerSideMintKey, fields.per_side_mint_marks);
}

// The transfer queue is bounded on both sides. Below one slot there is never
// more than a single transfer outstanding, which defeats the point of it; above
// the usbfs default there is nothing to gain and a submission failure to lose.
constexpr size_t kMinimumTransferQueueBytes = size_t{2} << 20;
constexpr size_t kMaximumTransferQueueBytes = size_t{12} << 20;

// The output format is stored as a word rather than as the enumerator's number,
// so that a settings file stays readable and adding a format later cannot
// renumber what an existing one meant.
constexpr const char* kFlacFormatName = "flac";
constexpr const char* kSigned16BitFormatName = "s16";

QString OutputFormatName(capture::CaptureOutputFormat format) {
  return format == capture::CaptureOutputFormat::kSigned16Bit
             ? QLatin1String(kSigned16BitFormatName)
             : QLatin1String(kFlacFormatName);
}

// Anything that is not the uncompressed format's name reads as FLAC — a
// settings file naming a format this build does not have should produce a
// working capture in the default format rather than a refusal.
capture::CaptureOutputFormat OutputFormatFromName(const QString& name) {
  return name == QLatin1String(kSigned16BitFormatName)
             ? capture::CaptureOutputFormat::kSigned16Bit
             : capture::CaptureOutputFormat::kFlac;
}

}  // namespace

capture::UsbSourceOptions CaptureSettings::UsbOptions() const {
  capture::UsbSourceOptions options;
  options.small_transfers = small_transfers;
  options.transfer_queue_bytes = transfer_queue_bytes;
  return options;
}

analysis::FrontEndGain CaptureSettings::DeclaredGain() const {
  return analysis::FrontEndGain::FromSwitchPattern(front_end_gain_switches);
}

double CaptureSettings::EstimatedBytesPerSecond() const {
  // Uncompressed is not an estimate at all: it is exactly the wire rate, and
  // decimation divides it exactly. FLAC is the estimate — see free_space.h,
  // where the working figure of half the wire rate is deliberately conservative
  // because real RF compresses better than that.
  const double undecimated =
      output_format == capture::CaptureOutputFormat::kSigned16Bit
          ? static_cast<double>(capture::kWireBytesPerSecond)
          : capture::kEstimatedCaptureBytesPerSecond;

  return undecimated / static_cast<double>(decimation_factor);
}

QString DefaultCaptureDirectory() {
  const QString movies =
      QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
  if (!movies.isEmpty()) {
    return movies;
  }

  // Movies is not defined on every platform Qt supports. Home is, and a capture
  // landing in the home folder is at least somewhere the user can find it —
  // unlike the working directory, which for an application started from a
  // desktop shortcut may be anywhere at all.
  const QString home =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  return home.isEmpty() ? QDir::currentPath() : home;
}

QString CaptureSettings::ResolvedCaptureDirectory() const {
  return capture_directory.isEmpty() ? DefaultCaptureDirectory()
                                     : capture_directory;
}

std::string CaptureSettings::CaptureStem(std::time_t when) const {
  return capture::BuildCaptureStem(naming, capture_name.toStdString(),
                                   test_mode, when);
}

CaptureSettings LoadCaptureSettings() {
  const QSettings settings;
  CaptureSettings loaded;

  loaded.preferred_device_path =
      settings.value(QLatin1String(kPreferredDeviceKey)).toString();

  loaded.queue_size_bytes = std::clamp(
      static_cast<size_t>(
          settings
              .value(QLatin1String(kQueueSizeKey),
                     static_cast<qulonglong>(loaded.queue_size_bytes))
              .toULongLong()),
      capture::DiskBufferRing::kMinimumQueueSizeBytes,
      capture::DiskBufferRing::kMaximumQueueSizeBytes);

  loaded.small_transfers =
      settings.value(QLatin1String(kSmallTransfersKey), loaded.small_transfers)
          .toBool();

  loaded.transfer_queue_bytes = std::clamp(
      static_cast<size_t>(
          settings
              .value(QLatin1String(kTransferQueueKey),
                     static_cast<qulonglong>(loaded.transfer_queue_bytes))
              .toULongLong()),
      kMinimumTransferQueueBytes, kMaximumTransferQueueBytes);

  // Not clamped like the others, because there is no nearest sensible value to
  // clamp a switch pattern to: FromSwitchPattern reads anything outside 1..15
  // as undeclared, which is the only safe reading of a setting that would
  // otherwise put a wrong voltage on the screen.
  loaded.front_end_gain_switches = static_cast<uint8_t>(
      settings.value(QLatin1String(kFrontEndGainKey), 0).toUInt() & 0xFFU);

  // Stored as typed and resolved when it is used, so that a settings file
  // written on one machine and read on another does not pin captures to a home
  // directory that is not there.
  loaded.capture_directory =
      settings.value(QLatin1String(kCaptureDirectoryKey)).toString();
  loaded.capture_name =
      settings.value(QLatin1String(kCaptureNameKey)).toString();

  loaded.naming = LoadNamingFields(settings);

  loaded.output_format =
      OutputFormatFromName(settings
                               .value(QLatin1String(kOutputFormatKey),
                                      OutputFormatName(loaded.output_format))
                               .toString());

  // Read as "all of them" unless it is a factor this build writes. There is no
  // nearest sensible value to clamp to — a 3 would be a rate nothing downstream
  // expects — and every sample is the reading that cannot produce a wrong file.
  const int stored_decimation =
      settings
          .value(QLatin1String(kDecimationFactorKey), loaded.decimation_factor)
          .toInt();
  loaded.decimation_factor =
      capture::IsSupportedDecimationFactor(stored_decimation)
          ? stored_decimation
          : capture::kUndecimatedFactor;

  loaded.compression_level = std::clamp(
      settings
          .value(QLatin1String(kCompressionLevelKey), loaded.compression_level)
          .toInt(),
      0, 8);

  loaded.duration_limit_seconds =
      std::clamp(settings.value(QLatin1String(kDurationLimitKey), 0).toInt(), 0,
                 CaptureSettings::kMaximumDurationLimitSeconds);

  loaded.low_space_warning_minutes = std::clamp(
      settings
          .value(QLatin1String(kLowSpaceKey), loaded.low_space_warning_minutes)
          .toInt(),
      0, CaptureSettings::kMaximumLowSpaceWarningMinutes);

  return loaded;
}

void SaveCaptureSettings(const CaptureSettings& settings) {
  QSettings store;
  store.setValue(QLatin1String(kPreferredDeviceKey),
                 settings.preferred_device_path);
  store.setValue(QLatin1String(kQueueSizeKey),
                 static_cast<qulonglong>(settings.queue_size_bytes));
  store.setValue(QLatin1String(kSmallTransfersKey), settings.small_transfers);
  store.setValue(QLatin1String(kTransferQueueKey),
                 static_cast<qulonglong>(settings.transfer_queue_bytes));
  store.setValue(QLatin1String(kFrontEndGainKey),
                 static_cast<uint>(settings.front_end_gain_switches));
  store.setValue(QLatin1String(kCaptureDirectoryKey),
                 settings.capture_directory);
  store.setValue(QLatin1String(kCaptureNameKey), settings.capture_name);
  SaveNamingFields(store, settings.naming);
  store.setValue(QLatin1String(kOutputFormatKey),
                 OutputFormatName(settings.output_format));
  store.setValue(QLatin1String(kDecimationFactorKey),
                 settings.decimation_factor);
  store.setValue(QLatin1String(kCompressionLevelKey),
                 settings.compression_level);
  store.setValue(QLatin1String(kDurationLimitKey),
                 settings.duration_limit_seconds);
  store.setValue(QLatin1String(kLowSpaceKey),
                 settings.low_space_warning_minutes);
}

}  // namespace ddd::gui
