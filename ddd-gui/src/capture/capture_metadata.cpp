/************************************************************************

    capture_metadata.cpp

    The YAML sidecar written beside every capture
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "capture_metadata.h"

#include <fstream>

#include "capture_format.h"
#include "yaml_writer.h"

namespace ddd::capture {
namespace {

// The document's own version, written into it.
//
// One number, incremented when a field changes meaning — not when one is added.
// A reader that understands version 1 must keep working against a file with
// more fields in it than it knows about, because that is the ordinary way this
// document will grow and a reader that broke on it would make every addition a
// breaking change.
constexpr int kMetadataSchemaVersion = 1;

// A timestamp as ISO 8601, in local time with the offset on the end.
//
// Local time because everything else about a capture is — the file name carries
// the local time it was taken, and a sidecar disagreeing with the file beside
// it is a puzzle for somebody at two in the morning. The offset is what keeps
// that unambiguous: "2026-08-17T14:30:00+01:00" is a moment, where the file
// name's timestamp alone is only a reading on a clock.
std::string FormatTimestamp(std::time_t when) {
  if (when == 0) {
    return {};
  }

  std::tm parts{};
#ifdef _WIN32
  localtime_s(&parts, &when);
#else
  localtime_r(&when, &parts);
#endif

  const auto two = [](int value) {
    std::string digits = std::to_string(value);
    if (digits.size() < 2) {
      digits.insert(digits.begin(), '0');
    }
    return digits;
  };

  std::string text = std::to_string(parts.tm_year + 1900) + "-" +
                     two(parts.tm_mon + 1) + "-" + two(parts.tm_mday) + "T" +
                     two(parts.tm_hour) + ":" + two(parts.tm_min) + ":" +
                     two(parts.tm_sec);

// The zone offset, where the platform reports one. Windows' struct tm has no
// tm_gmtoff, and rather than reconstruct it from the timezone globals — which
// are not thread-safe and get daylight saving wrong at the boundaries — the
// timestamp is simply written without an offset there. A local time with no
// offset is what the old application wrote everywhere, so this is no worse on
// the one platform it applies to and better on the other two.
#ifndef _WIN32
  const long offset = parts.tm_gmtoff;
  const long absolute = offset < 0 ? -offset : offset;
  text += offset < 0 ? "-" : "+";
  text += two(static_cast<int>(absolute / 3600));
  text += ":";
  text += two(static_cast<int>((absolute % 3600) / 60));
#endif

  return text;
}

void WriteFact(YamlWriter& yaml, const char* key, const ScannedFact& fact) {
  if (!fact.known()) {
    return;
  }

  // The value and where it came from, as a pair under the field's own name. A
  // flat "disc_side: 2" with a separate "disc_side_source: reported" beside it
  // would read the same to a program and would come apart the first time
  // somebody edited one and not the other.
  yaml.BeginMapping(key);
  yaml.String("value", fact.value);
  yaml.StringIfPresent("source", fact.source);
  yaml.EndMapping();
}

void WriteNaming(YamlWriter& yaml, const CaptureNamingFields& naming) {
  yaml.BeginMapping("naming");

  if (naming.title_used) {
    yaml.StringIfPresent("title", naming.title);
  }
  if (naming.disc_type_used) {
    yaml.StringIfPresent("disc_type", DiscTypeChoiceName(naming.disc_type));
  }
  if (naming.video_standard_used) {
    yaml.StringIfPresent("video_standard",
                         VideoStandardChoiceName(naming.video_standard));
  }
  if (naming.audio_used) {
    yaml.StringIfPresent("audio", AudioTypeChoiceName(naming.audio));
  }
  if (naming.side_used) {
    yaml.Integer("side", naming.side);
  }
  if (naming.notes_used) {
    yaml.StringIfPresent("notes", naming.notes);
  }
  if (naming.mint_marks_used) {
    yaml.StringIfPresent("mint_marks", naming.mint_marks);
  }
  yaml.StringIfPresent("metadata_notes", naming.metadata_notes);

  yaml.EndMapping();
}

void WriteDevice(YamlWriter& yaml, const DeviceBuild& device) {
  yaml.BeginMapping("device");

  yaml.StringIfPresent("firmware_version", device.firmware_version);
  yaml.StringIfPresent("gateware_version", device.gateware_version);

  if (device.gateware_register_map != 0) {
    yaml.Unsigned("gateware_register_map", device.gateware_register_map);
  }

  yaml.EndMapping();
}

void WritePlayer(YamlWriter& yaml, const PlayerIdentity& player) {
  yaml.BeginMapping("player");

  yaml.StringIfPresent("model_name", player.model_name);
  yaml.StringIfPresent("model_id_code", player.model_id_code);
  yaml.StringIfPresent("model_code", player.model_code);
  yaml.StringIfPresent("firmware_version", player.firmware_version);
  yaml.StringIfPresent("port", player.port);
  if (player.baud_rate != 0) {
    yaml.Unsigned("baud_rate", player.baud_rate);
  }
  if (!player.empty()) {
    // Only alongside a player that was actually there. On its own, a bare
    // "recognised_model: false" would read as a statement about a player rather
    // than about there not being one.
    yaml.Boolean("recognised_model", player.recognised_model);
  }

  yaml.EndMapping();
}

void WriteDisc(YamlWriter& yaml, const DiscScan& disc) {
  yaml.BeginMapping("disc");

  if (!disc.examined) {
    // Written as an empty mapping rather than omitted. The section existing and
    // being empty says the question was asked; the section being absent
    // altogether would leave a reader unable to tell this document from one
    // written before the field existed.
    yaml.EndMapping();
    return;
  }

  yaml.Boolean("examined", true);

  WriteFact(yaml, "disc_present", disc.disc_present);
  WriteFact(yaml, "tray", disc.tray);
  WriteFact(yaml, "disc_type", disc.disc_type);
  WriteFact(yaml, "addressing", disc.addressing);
  WriteFact(yaml, "disc_size", disc.disc_size);
  WriteFact(yaml, "disc_side", disc.disc_side);
  WriteFact(yaml, "video_standard", disc.video_standard);
  WriteFact(yaml, "programme_start", disc.programme_start);
  WriteFact(yaml, "programme_end", disc.programme_end);
  WriteFact(yaml, "programme_duration", disc.programme_duration);
  WriteFact(yaml, "lead_in_reachable", disc.lead_in_reachable);
  WriteFact(yaml, "chapters", disc.chapters);

  yaml.StringIfPresent("disc_status_reply", disc.disc_status_reply);

  if (!disc.standard_user_code_outcome.empty()) {
    yaml.BeginMapping("standard_user_code");
    yaml.String("outcome", disc.standard_user_code_outcome);
    yaml.StringIfPresent("text", disc.standard_user_code);
    yaml.EndMapping();
  }

  if (!disc.pioneer_user_code_outcome.empty()) {
    yaml.BeginMapping("pioneer_user_code");
    yaml.String("outcome", disc.pioneer_user_code_outcome);
    yaml.StringIfPresent("text", disc.pioneer_user_code);
    yaml.EndMapping();
  }

  yaml.EndMapping();
}

}  // namespace

std::filesystem::path CaptureMetadataPath(
    const std::filesystem::path& capture_path) {
  const std::string text = capture_path.string();
  const std::string suffix = MatchedCaptureFileSuffix(text);

  // A path that carries neither capture suffix — which nothing in this
  // application produces, but a caller could hand one over — has the sidecar
  // suffix appended rather than replacing whatever extension it had. Appending
  // cannot destroy the association between the two files; replacing an unknown
  // extension could.
  if (suffix.empty()) {
    return std::filesystem::path(text + kCaptureMetadataSuffix);
  }

  return std::filesystem::path(text.substr(0, text.size() - suffix.size()) +
                               kCaptureMetadataSuffix);
}

std::string BuildCaptureMetadataYaml(const CaptureMetadata& metadata) {
  YamlWriter yaml;

  yaml.Comment("Domesday Duplicator capture metadata.");
  yaml.Comment("");
  yaml.Comment("This file describes the capture of the same name beside it.");
  yaml.Comment("A field that was never established is absent rather than");
  yaml.Comment("blank, so everything written here was actually known.");
  yaml.BlankLine();

  yaml.Integer("schema_version", kMetadataSchemaVersion);
  yaml.String("application_version", metadata.application_version);
  yaml.BlankLine();

  yaml.BeginMapping("capture");
  yaml.StringIfPresent("file", metadata.capture_file_name);
  yaml.StringIfPresent("format", metadata.format);
  yaml.Boolean("test_mode", metadata.test_mode);
  yaml.Unsigned("sample_rate_hz", metadata.sample_rate_hz);
  yaml.Integer("decimation_factor", metadata.decimation_factor);
  yaml.StringIfPresent("front_end_gain", metadata.front_end_gain);
  yaml.StringIfPresent("started", FormatTimestamp(metadata.started));
  yaml.StringIfPresent("finished", FormatTimestamp(metadata.finished));
  yaml.Number("duration_seconds", metadata.outcome.duration_seconds, 3);
  yaml.Unsigned("samples", metadata.outcome.samples);
  yaml.Unsigned("bytes", metadata.outcome.bytes);
  yaml.Boolean("completed", metadata.outcome.completed);
  yaml.StringIfPresent("detail", metadata.outcome.detail);
  yaml.StringIfPresent("sequence_check", metadata.outcome.sequence_check);
  yaml.Unsigned("device_overflow_events",
                metadata.outcome.device_overflow_events);
  yaml.Unsigned("device_dropped_words", metadata.outcome.device_dropped_words);
  if (metadata.outcome.test_pattern_checked) {
    yaml.Boolean("test_pattern_passed", metadata.outcome.test_pattern_passed);
  }
  yaml.EndMapping();
  yaml.BlankLine();

  if (metadata.signal.known) {
    yaml.Comment("Measured over this file's own samples and no others.");
    yaml.BeginMapping("signal");
    yaml.Unsigned("minimum_value", metadata.signal.minimum_value);
    yaml.Unsigned("maximum_value", metadata.signal.maximum_value);
    yaml.Number("rms", metadata.signal.rms, 2);
    yaml.Unsigned("clipped_low_samples", metadata.signal.clipped_low_samples);
    yaml.Unsigned("clipped_high_samples", metadata.signal.clipped_high_samples);
    yaml.EndMapping();
    yaml.BlankLine();
  }

  WriteNaming(yaml, metadata.naming);
  yaml.BlankLine();

  WriteDevice(yaml, metadata.device);
  yaml.BlankLine();

  WritePlayer(yaml, metadata.player);
  yaml.BlankLine();

  WriteDisc(yaml, metadata.disc);

  return yaml.str();
}

bool WriteCaptureMetadataFile(const std::filesystem::path& path,
                              const CaptureMetadata& metadata,
                              std::string& error) {
  const std::string document = BuildCaptureMetadataYaml(metadata);

  // Binary mode, so that the newlines this writer put in are the newlines that
  // reach the file. A text-mode stream on Windows would translate them to CRLF,
  // which YAML accepts but which makes the file differ between platforms for no
  // reason anybody chose.
  std::ofstream file(path, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!file.is_open()) {
    error = "The metadata file could not be created at " + path.string();
    return false;
  }

  file.write(document.data(), static_cast<std::streamsize>(document.size()));
  file.close();

  if (file.fail()) {
    error = "The metadata file could not be written to " + path.string();
    return false;
  }

  return true;
}

}  // namespace ddd::capture
