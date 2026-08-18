/************************************************************************

    json_value.h

    A small strict JSON reader and writer for the update manifest
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ddd::capture {

// JSON, for one caller: the update bundle's manifest.
//
// Why this exists rather than a dependency. The engine is Qt-free by rule, so
// QJsonDocument is not available to it, and adding a JSON library to reach one
// object of four fields would put a package on the dependency list of the
// Flatpak, the MSI and the DMG for the sake of about two hundred lines. The
// manifest is also the *only* untrusted input this code will ever see, arriving
// from a download before its signature has been checked, which argues for a
// parser small enough to read end to end rather than a general one nobody in
// this project has read at all.
//
// It is strict where a general parser would be lenient, and every strictness is
// a decision about the manifest rather than about JSON:
//
//   * duplicate keys in an object are rejected. A manifest carrying two
//     "sha256" fields has no single meaning, and choosing one of them is how a
//     parser disagrees with the tool that signed it;
//   * nesting is capped, so a hostile file cannot exhaust the stack before the
//     signature that would have rejected it has been checked;
//   * trailing content after the top-level value is rejected;
//   * comments, trailing commas, single quotes and NaN are not accepted. They
//     are not JSON, and a manifest is machine-written.
//
// Numbers are kept as the text they were written as. The manifest's numbers are
// byte counts and schema versions — integers that must survive a round trip
// exactly — and storing them as double and printing them back is how a length
// acquires a ".0" or loses a digit. AsInteger() parses that text strictly when
// a caller asks for a number, and nothing here ever converts through floating
// point.

class JsonValue {
 public:
  enum class Type : uint8_t {
    kNull,
    kBoolean,
    kNumber,
    kString,
    kArray,
    kObject,
  };

  // Members in the order they were parsed or added. Order is not semantically
  // meaningful in JSON, but preserving it means a manifest read and written
  // again is the same file, which is what makes a round-trip test able to
  // compare bytes rather than fields.
  using Member = std::pair<std::string, JsonValue>;

  JsonValue() = default;

  static JsonValue Null();
  static JsonValue Boolean(bool value);

  // The number as it will be written. Integers go in as integers; there is no
  // floating-point constructor because the manifest has no floating-point
  // field, and adding one would mean deciding how many digits to print.
  static JsonValue Number(int64_t value);

  // A number exactly as it was written in the source, which is how the parser
  // builds one. `lexeme` must already be valid JSON number syntax — this does
  // not check, because the parser has just done so character by character.
  // Prefer Number() everywhere else.
  static JsonValue NumberFromText(std::string lexeme);

  static JsonValue String(std::string value);
  static JsonValue Array(std::vector<JsonValue> elements);
  static JsonValue Object(std::vector<Member> members);

  Type type() const { return type_; }

  bool IsNull() const { return type_ == Type::kNull; }
  bool IsBoolean() const { return type_ == Type::kBoolean; }
  bool IsNumber() const { return type_ == Type::kNumber; }
  bool IsString() const { return type_ == Type::kString; }
  bool IsArray() const { return type_ == Type::kArray; }
  bool IsObject() const { return type_ == Type::kObject; }

  // The value if it is of the type asked for, and nothing otherwise. Every
  // accessor answers "not that type" and "not present" the same way, because
  // the caller's response to both is the same: this manifest cannot be used.
  std::optional<bool> AsBoolean() const;
  std::optional<std::string_view> AsString() const;

  // The number as an integer, or nothing if this is not a number or is a
  // number with a fraction, an exponent or more digits than an int64_t holds.
  //
  // Refusing 1e3 and 1000.0 is deliberate. They are the same quantity as 1000
  // to a mathematician and to most JSON libraries, but a manifest field is a
  // byte count written by a build script, and a payload length that arrived in
  // scientific notation is a sign that something other than that script wrote
  // the file.
  std::optional<int64_t> AsInteger() const;

  // A number's source text, empty for any other type. Serialising writes this
  // back unchanged, so a document read and written again keeps the digits it
  // arrived with.
  std::string_view NumberText() const;

  const std::vector<JsonValue>* AsArray() const;
  const std::vector<Member>* AsObject() const;

  // A member of this object by name, or nullptr if this is not an object or
  // has no such member.
  const JsonValue* Find(std::string_view name) const;

 private:
  Type type_ = Type::kNull;
  bool boolean_ = false;

  // The number's source text for a number, the decoded characters for a string.
  std::string text_;

  std::vector<JsonValue> elements_;
  std::vector<Member> members_;
};

// Where a parse gave up, and why.
struct JsonParseError {
  // Plain-language description, suitable for a log line. Never the input's own
  // text: a manifest that failed to parse is not trusted enough to quote.
  std::string message;

  // Byte offset into the input at which the parser stopped.
  size_t offset = 0;
};

// Parse a complete JSON document. Returns nothing, and fills `error` when it is
// not null, for anything that is not one well-formed value.
std::optional<JsonValue> ParseJson(std::string_view text,
                                   JsonParseError* error);

// Write a value back out, indented two spaces per level and with a trailing
// newline — the same layout tools/make-update-bundle.sh writes, so a manifest
// produced by the script and one produced here read the same and diff cleanly
// against each other.
//
// Nothing in the application writes a manifest into a real bundle: signing
// happens at build time and the engine only ever verifies. The writer exists so
// that the reader can be tested against something other than itself, and so a
// test can build a manifest, sign it and read it back the way the release
// pipeline does.
std::string SerialiseJson(const JsonValue& value);

}  // namespace ddd::capture
