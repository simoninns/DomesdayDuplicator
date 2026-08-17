/************************************************************************

    update_manifest.cpp

    What a release bundle says about itself
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_manifest.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <system_error>
#include <utility>

#include "json_value.h"

namespace ddd::capture {
namespace {

void Report(std::vector<std::string>* errors, std::string message) {
  if (errors != nullptr) {
    errors->push_back(std::move(message));
  }
}

// A required string member. Empty strings are refused as well as missing ones:
// every string in this schema names something, and a bundle that names its
// firmware payload "" is not a bundle with a small mistake in it.
bool ReadString(const JsonValue& parent, std::string_view name,
                std::string_view where, std::string* out,
                std::vector<std::string>* errors) {
  const JsonValue* value = parent.Find(name);
  if (value == nullptr) {
    Report(errors,
           std::string(where) + ": missing \"" + std::string(name) + "\"");
    return false;
  }

  const std::optional<std::string_view> text = value->AsString();
  if (!text) {
    Report(errors, std::string(where) + ": \"" + std::string(name) +
                       "\" is not a string");
    return false;
  }
  if (text->empty()) {
    Report(errors,
           std::string(where) + ": \"" + std::string(name) + "\" is empty");
    return false;
  }

  *out = std::string(*text);
  return true;
}

// A required non-negative integer member.
bool ReadInteger(const JsonValue& parent, std::string_view name,
                 std::string_view where, int64_t* out,
                 std::vector<std::string>* errors) {
  const JsonValue* value = parent.Find(name);
  if (value == nullptr) {
    Report(errors,
           std::string(where) + ": missing \"" + std::string(name) + "\"");
    return false;
  }

  const std::optional<int64_t> number = value->AsInteger();
  if (!number) {
    Report(errors, std::string(where) + ": \"" + std::string(name) +
                       "\" is not a whole number");
    return false;
  }
  if (*number < 0) {
    Report(errors,
           std::string(where) + ": \"" + std::string(name) + "\" is negative");
    return false;
  }

  *out = *number;
  return true;
}

// Read one component, if the manifest declares it.
//
// Absent and malformed have to be told apart, and an optional return cannot do
// it: a firmware-only bundle is complete, while a bundle whose firmware entry
// is missing its digest is not a bundle at all. So absence is success with
// nothing written, and only a component that is present and wrong is a failure.
bool ReadComponent(const JsonValue& components, std::string_view name,
                   std::optional<UpdateComponent>* out,
                   std::vector<std::string>* errors) {
  const JsonValue* value = components.Find(name);
  if (value == nullptr) {
    return true;
  }
  if (!value->IsObject()) {
    Report(errors, "components." + std::string(name) + " is not an object");
    return false;
  }

  const std::string where = "components." + std::string(name);
  UpdateComponent component;
  bool complete = true;

  complete &= ReadString(*value, "file", where, &component.file, errors);

  int64_t length = 0;
  if (ReadInteger(*value, "length", where, &length, errors)) {
    component.length = static_cast<uint64_t>(length);
  } else {
    complete = false;
  }

  std::string digest_text;
  if (ReadString(*value, "sha256", where, &digest_text, errors)) {
    const std::optional<Sha256Digest> digest = ParseHexDigest(digest_text);
    if (digest) {
      component.sha256 = *digest;
    } else {
      Report(errors, where + ": \"sha256\" is not 64 hex characters");
      complete = false;
    }
  } else {
    complete = false;
  }

  complete &=
      ReadString(*value, "identity", where, &component.identity, errors);
  complete &= ReadInteger(*value, "interface_version", where,
                          &component.interface_version, errors);

  if (!complete) {
    return false;
  }
  *out = std::move(component);
  return true;
}

std::vector<JsonValue::Member> ComponentMembers(
    const UpdateComponent& component) {
  return {
      {"file", JsonValue::String(component.file)},
      {"length", JsonValue::Number(static_cast<int64_t>(component.length))},
      {"sha256", JsonValue::String(ToHex(component.sha256))},
      {"identity", JsonValue::String(component.identity)},
      {"interface_version", JsonValue::Number(component.interface_version)},
  };
}

// Split a dotted version into its numbers, or nothing if it is not one.
std::optional<std::vector<int64_t>> SplitDottedVersion(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }

  std::vector<int64_t> parts;
  size_t start = 0;
  while (true) {
    const size_t dot = text.find('.', start);
    const std::string_view part = dot == std::string_view::npos
                                      ? text.substr(start)
                                      : text.substr(start, dot - start);
    if (part.empty()) {
      return std::nullopt;
    }

    int64_t number = 0;
    const std::from_chars_result result =
        std::from_chars(part.data(), part.data() + part.size(), number);
    if (result.ec != std::errc() || result.ptr != part.data() + part.size() ||
        number < 0) {
      return std::nullopt;
    }
    parts.push_back(number);

    if (dot == std::string_view::npos) {
      return parts;
    }
    start = dot + 1;
  }
}

}  // namespace

std::optional<UpdateManifest> ParseUpdateManifest(
    std::string_view text, std::vector<std::string>* errors) {
  JsonParseError parse_error;
  const std::optional<JsonValue> document = ParseJson(text, &parse_error);
  if (!document) {
    Report(errors, "the manifest is not valid JSON: " + parse_error.message +
                       " at byte " + std::to_string(parse_error.offset));
    return std::nullopt;
  }
  if (!document->IsObject()) {
    Report(errors, "the manifest is not a JSON object");
    return std::nullopt;
  }

  // The schema version is checked before anything else, and a mismatch stops
  // the parse rather than adding to a list. Every message that followed would
  // be this build's opinion about a schema it has just admitted it does not
  // know.
  UpdateManifest manifest;
  if (!ReadInteger(*document, "manifest_version", "manifest",
                   &manifest.manifest_version, errors)) {
    return std::nullopt;
  }
  if (manifest.manifest_version != kUpdateManifestVersion) {
    Report(errors, "the manifest declares schema version " +
                       std::to_string(manifest.manifest_version) +
                       ", and this build understands version " +
                       std::to_string(kUpdateManifestVersion));
    return std::nullopt;
  }

  bool complete = true;

  std::string channel;
  if (ReadString(*document, "channel", "manifest", &channel, errors)) {
    if (channel == "release") {
      manifest.channel = UpdateChannel::kRelease;
    } else if (channel == "development") {
      manifest.channel = UpdateChannel::kDevelopment;
    } else {
      // Not defaulted to development, tempting though that is. An unknown
      // channel is an unknown promise about what the signature means.
      Report(errors,
             "manifest: \"channel\" is neither \"release\" nor "
             "\"development\"");
      complete = false;
    }
  } else {
    complete = false;
  }

  complete &=
      ReadString(*document, "version", "manifest", &manifest.version, errors);
  complete &=
      ReadString(*document, "commit", "manifest", &manifest.commit, errors);
  complete &=
      ReadString(*document, "created", "manifest", &manifest.created, errors);
  complete &= ReadString(*document, "release_notes", "manifest",
                         &manifest.release_notes, errors);

  if (!SplitDottedVersion(manifest.version)) {
    Report(errors, "manifest: \"version\" is not a dotted numeric version");
    complete = false;
  }

  const JsonValue* components = document->Find("components");
  if (components == nullptr || !components->IsObject()) {
    Report(errors, "manifest: missing or malformed \"components\"");
    complete = false;
  } else {
    complete &= ReadComponent(*components, kFirmwareComponentName,
                              &manifest.firmware, errors);
    complete &= ReadComponent(*components, kGatewareComponentName,
                              &manifest.gateware, errors);
    complete &= ReadComponent(*components, kProvisioningComponentName,
                              &manifest.provisioning, errors);

    // A component kind this build does not know is refused rather than
    // skipped. Every member of this object names a payload that something has
    // to write to somebody's hardware, and a reader that quietly ignored one
    // would install a partial bundle while reporting a complete one.
    for (const JsonValue::Member& member : *components->AsObject()) {
      if (member.first != kFirmwareComponentName &&
          member.first != kGatewareComponentName &&
          member.first != kProvisioningComponentName) {
        Report(errors, "manifest: \"components\" declares \"" + member.first +
                           "\", which this build does not know how to install");
        complete = false;
      }
    }

    // A bundle with nothing to install is not a bundle, and the case is worth
    // naming separately from a malformed component: it is what a release
    // pipeline produces when a build step failed quietly and nobody noticed
    // the artefact had come out empty.
    if (components->Find(kFirmwareComponentName) == nullptr &&
        components->Find(kGatewareComponentName) == nullptr &&
        components->Find(kProvisioningComponentName) == nullptr) {
      Report(errors, "manifest: \"components\" declares nothing to install");
    }
    if (!manifest.firmware && !manifest.gateware && !manifest.provisioning) {
      complete = false;
    }
  }

  const JsonValue* compatibility = document->Find("compatibility");
  if (compatibility == nullptr || !compatibility->IsObject()) {
    Report(errors, "manifest: missing or malformed \"compatibility\"");
    complete = false;
  } else {
    complete &= ReadString(
        *compatibility, "minimum_application_version", "compatibility",
        &manifest.compatibility.minimum_application_version, errors);
    complete &= ReadInteger(
        *compatibility, "minimum_register_map_version", "compatibility",
        &manifest.compatibility.minimum_register_map_version, errors);
    complete &=
        ReadInteger(*compatibility, "epcs_layout_version", "compatibility",
                    &manifest.compatibility.epcs_layout_version, errors);

    if (!manifest.compatibility.minimum_application_version.empty() &&
        !SplitDottedVersion(
            manifest.compatibility.minimum_application_version)) {
      Report(errors,
             "compatibility: \"minimum_application_version\" is not a dotted "
             "numeric version");
      complete = false;
    }
  }

  if (!complete) {
    return std::nullopt;
  }
  return manifest;
}

std::string SerialiseUpdateManifest(const UpdateManifest& manifest) {
  std::vector<JsonValue::Member> components;
  if (manifest.firmware) {
    components.emplace_back(
        "firmware", JsonValue::Object(ComponentMembers(*manifest.firmware)));
  }
  if (manifest.gateware) {
    components.emplace_back(
        "gateware", JsonValue::Object(ComponentMembers(*manifest.gateware)));
  }
  if (manifest.provisioning) {
    components.emplace_back(
        std::string(kProvisioningComponentName),
        JsonValue::Object(ComponentMembers(*manifest.provisioning)));
  }

  std::vector<JsonValue::Member> compatibility = {
      {"minimum_application_version",
       JsonValue::String(manifest.compatibility.minimum_application_version)},
      {"minimum_register_map_version",
       JsonValue::Number(manifest.compatibility.minimum_register_map_version)},
      {"epcs_layout_version",
       JsonValue::Number(manifest.compatibility.epcs_layout_version)},
  };

  const JsonValue document = JsonValue::Object({
      {"manifest_version", JsonValue::Number(manifest.manifest_version)},
      {"channel", JsonValue::String(manifest.channel == UpdateChannel::kRelease
                                        ? "release"
                                        : "development")},
      {"version", JsonValue::String(manifest.version)},
      {"commit", JsonValue::String(manifest.commit)},
      {"created", JsonValue::String(manifest.created)},
      {"release_notes", JsonValue::String(manifest.release_notes)},
      {"components", JsonValue::Object(std::move(components))},
      {"compatibility", JsonValue::Object(std::move(compatibility))},
  });

  return SerialiseJson(document);
}

std::optional<int> CompareDottedVersions(std::string_view left,
                                         std::string_view right) {
  const std::optional<std::vector<int64_t>> left_parts =
      SplitDottedVersion(left);
  const std::optional<std::vector<int64_t>> right_parts =
      SplitDottedVersion(right);
  if (!left_parts || !right_parts) {
    return std::nullopt;
  }

  const size_t count = std::max(left_parts->size(), right_parts->size());
  for (size_t index = 0; index < count; ++index) {
    const int64_t left_part =
        index < left_parts->size() ? (*left_parts)[index] : 0;
    const int64_t right_part =
        index < right_parts->size() ? (*right_parts)[index] : 0;
    if (left_part != right_part) {
      return left_part < right_part ? -1 : 1;
    }
  }
  return 0;
}

}  // namespace ddd::capture
