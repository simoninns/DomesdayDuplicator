/************************************************************************

    json_value.cpp

    A small strict JSON reader and writer for the update manifest
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "json_value.h"

#include <charconv>
#include <cstddef>
#include <system_error>

namespace ddd::capture {
namespace {

// How deep a document may nest before the parser refuses to go further.
//
// The manifest nests three deep — document, components, one component — so this
// is an order of magnitude of headroom and still small enough that the
// recursive descent below cannot run the stack out. The alternative to a limit
// is unbounded recursion driven by a file whose signature has not been checked
// yet, which is a denial of service with no upside.
constexpr int kMaximumDepth = 32;

// The characters JSON calls whitespace, and no others. A tab and a newline are
// whitespace; a vertical tab is not, whatever isspace() decides in the current
// locale.
bool IsWhitespace(char character) {
  return character == ' ' || character == '\t' || character == '\n' ||
         character == '\r';
}

bool IsDigit(char character) { return character >= '0' && character <= '9'; }

// Recursive-descent parser over a fixed buffer. Every failure records where it
// happened and stops; nothing recovers and continues, because a manifest that
// is wrong anywhere is unusable everywhere.
class Parser {
 public:
  Parser(std::string_view text, JsonParseError* error)
      : text_(text), error_(error) {}

  std::optional<JsonValue> ParseDocument() {
    SkipWhitespace();
    std::optional<JsonValue> value = ParseValue(0);
    if (!value) {
      return std::nullopt;
    }

    SkipWhitespace();
    if (position_ != text_.size()) {
      return Fail("trailing content after the top-level value");
    }
    return value;
  }

 private:
  std::optional<JsonValue> Fail(std::string message) {
    if (error_ != nullptr && error_->message.empty()) {
      error_->message = std::move(message);
      error_->offset = position_;
    }
    return std::nullopt;
  }

  bool AtEnd() const { return position_ >= text_.size(); }
  char Peek() const { return text_[position_]; }

  void SkipWhitespace() {
    while (!AtEnd() && IsWhitespace(Peek())) {
      ++position_;
    }
  }

  // Consume a bare literal — true, false or null — checking every character
  // rather than only the first.
  bool ConsumeLiteral(std::string_view literal) {
    if (text_.compare(position_, literal.size(), literal) != 0) {
      return false;
    }
    position_ += literal.size();
    return true;
  }

  std::optional<JsonValue> ParseValue(int depth) {
    if (depth > kMaximumDepth) {
      return Fail("nested too deeply");
    }
    if (AtEnd()) {
      return Fail("expected a value");
    }

    switch (Peek()) {
      case '{':
        return ParseObject(depth);
      case '[':
        return ParseArray(depth);
      case '"': {
        std::optional<std::string> decoded = ParseString();
        if (!decoded) {
          return std::nullopt;
        }
        return JsonValue::String(std::move(*decoded));
      }
      case 't':
        if (!ConsumeLiteral("true")) {
          return Fail("expected a value");
        }
        return JsonValue::Boolean(true);
      case 'f':
        if (!ConsumeLiteral("false")) {
          return Fail("expected a value");
        }
        return JsonValue::Boolean(false);
      case 'n':
        if (!ConsumeLiteral("null")) {
          return Fail("expected a value");
        }
        return JsonValue::Null();
      default:
        return ParseNumber();
    }
  }

  std::optional<JsonValue> ParseObject(int depth) {
    ++position_;  // the '{'
    std::vector<JsonValue::Member> members;

    SkipWhitespace();
    if (!AtEnd() && Peek() == '}') {
      ++position_;
      return JsonValue::Object(std::move(members));
    }

    while (true) {
      SkipWhitespace();
      if (AtEnd() || Peek() != '"') {
        return Fail("expected a member name");
      }

      const size_t name_offset = position_;
      std::optional<std::string> name = ParseString();
      if (!name) {
        return std::nullopt;
      }

      // A repeated name has no single meaning, and a parser that silently
      // keeps the first or the last is a parser that can read a manifest
      // differently from the tool that signed it.
      for (const JsonValue::Member& member : members) {
        if (member.first == *name) {
          position_ = name_offset;
          return Fail("duplicate member name");
        }
      }

      SkipWhitespace();
      if (AtEnd() || Peek() != ':') {
        return Fail("expected ':' after a member name");
      }
      ++position_;

      SkipWhitespace();
      std::optional<JsonValue> value = ParseValue(depth + 1);
      if (!value) {
        return std::nullopt;
      }
      members.emplace_back(std::move(*name), std::move(*value));

      SkipWhitespace();
      if (AtEnd()) {
        return Fail("unterminated object");
      }
      if (Peek() == ',') {
        ++position_;
        continue;
      }
      if (Peek() == '}') {
        ++position_;
        return JsonValue::Object(std::move(members));
      }
      return Fail("expected ',' or '}'");
    }
  }

  std::optional<JsonValue> ParseArray(int depth) {
    ++position_;  // the '['
    std::vector<JsonValue> elements;

    SkipWhitespace();
    if (!AtEnd() && Peek() == ']') {
      ++position_;
      return JsonValue::Array(std::move(elements));
    }

    while (true) {
      SkipWhitespace();
      std::optional<JsonValue> value = ParseValue(depth + 1);
      if (!value) {
        return std::nullopt;
      }
      elements.push_back(std::move(*value));

      SkipWhitespace();
      if (AtEnd()) {
        return Fail("unterminated array");
      }
      if (Peek() == ',') {
        ++position_;
        continue;
      }
      if (Peek() == ']') {
        ++position_;
        return JsonValue::Array(std::move(elements));
      }
      return Fail("expected ',' or ']'");
    }
  }

  // Decode a string, resolving the escapes JSON defines and rejecting
  // everything else. Surrogate pairs are joined; a lone surrogate is an error
  // rather than a replacement character, because these strings are filenames
  // and hex digests, and a silently substituted character there would surface
  // later as a mismatch reported against the wrong thing.
  std::optional<std::string> ParseString() {
    ++position_;  // the opening quote
    std::string decoded;

    while (true) {
      if (AtEnd()) {
        Fail("unterminated string");
        return std::nullopt;
      }

      const char character = text_[position_];
      if (character == '"') {
        ++position_;
        return decoded;
      }

      // Control characters must be escaped in JSON. Accepting a raw one would
      // let a manifest carry a newline inside a filename.
      if (static_cast<unsigned char>(character) < 0x20) {
        Fail("unescaped control character in a string");
        return std::nullopt;
      }

      if (character != '\\') {
        decoded.push_back(character);
        ++position_;
        continue;
      }

      ++position_;
      if (AtEnd()) {
        Fail("unterminated escape");
        return std::nullopt;
      }

      const char escape = text_[position_++];
      switch (escape) {
        case '"':
          decoded.push_back('"');
          break;
        case '\\':
          decoded.push_back('\\');
          break;
        case '/':
          decoded.push_back('/');
          break;
        case 'b':
          decoded.push_back('\b');
          break;
        case 'f':
          decoded.push_back('\f');
          break;
        case 'n':
          decoded.push_back('\n');
          break;
        case 'r':
          decoded.push_back('\r');
          break;
        case 't':
          decoded.push_back('\t');
          break;
        case 'u':
          if (!ParseUnicodeEscape(&decoded)) {
            return std::nullopt;
          }
          break;
        default:
          Fail("unknown escape");
          return std::nullopt;
      }
    }
  }

  // \uXXXX, with a following \uXXXX when the first is a high surrogate.
  bool ParseUnicodeEscape(std::string* decoded) {
    const std::optional<uint32_t> first = ParseHexQuad();
    if (!first) {
      return false;
    }

    uint32_t code_point = *first;
    if (code_point >= 0xD800 && code_point <= 0xDBFF) {
      if (position_ + 1 >= text_.size() || text_[position_] != '\\' ||
          text_[position_ + 1] != 'u') {
        Fail("high surrogate without a low surrogate");
        return false;
      }
      position_ += 2;

      const std::optional<uint32_t> second = ParseHexQuad();
      if (!second) {
        return false;
      }
      if (*second < 0xDC00 || *second > 0xDFFF) {
        Fail("high surrogate followed by something other than a low surrogate");
        return false;
      }
      code_point = 0x10000 + ((code_point - 0xD800) << 10) + (*second - 0xDC00);
    } else if (code_point >= 0xDC00 && code_point <= 0xDFFF) {
      Fail("low surrogate without a high surrogate");
      return false;
    }

    AppendUtf8(code_point, decoded);
    return true;
  }

  std::optional<uint32_t> ParseHexQuad() {
    if (position_ + 4 > text_.size()) {
      Fail("truncated \\u escape");
      return std::nullopt;
    }

    uint32_t value = 0;
    for (size_t index = 0; index < 4; ++index) {
      const char character = text_[position_ + index];
      uint32_t nibble = 0;
      if (character >= '0' && character <= '9') {
        nibble = static_cast<uint32_t>(character - '0');
      } else if (character >= 'a' && character <= 'f') {
        nibble = static_cast<uint32_t>(character - 'a' + 10);
      } else if (character >= 'A' && character <= 'F') {
        nibble = static_cast<uint32_t>(character - 'A' + 10);
      } else {
        Fail("\\u escape is not four hex digits");
        return std::nullopt;
      }
      value = (value << 4) | nibble;
    }
    position_ += 4;
    return value;
  }

  static void AppendUtf8(uint32_t code_point, std::string* out) {
    if (code_point < 0x80) {
      out->push_back(static_cast<char>(code_point));
    } else if (code_point < 0x800) {
      out->push_back(static_cast<char>(0xC0 | (code_point >> 6)));
      out->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else if (code_point < 0x10000) {
      out->push_back(static_cast<char>(0xE0 | (code_point >> 12)));
      out->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else {
      out->push_back(static_cast<char>(0xF0 | (code_point >> 18)));
      out->push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
  }

  // The grammar is checked here character by character and the text is kept as
  // written; nothing is converted until a caller asks for an integer.
  std::optional<JsonValue> ParseNumber() {
    const size_t start = position_;

    if (!AtEnd() && Peek() == '-') {
      ++position_;
    }

    if (AtEnd() || !IsDigit(Peek())) {
      return Fail("expected a value");
    }

    // A leading zero may not be followed by another digit: 007 is not JSON.
    if (Peek() == '0') {
      ++position_;
    } else {
      while (!AtEnd() && IsDigit(Peek())) {
        ++position_;
      }
    }

    if (!AtEnd() && Peek() == '.') {
      ++position_;
      if (AtEnd() || !IsDigit(Peek())) {
        return Fail("a decimal point must be followed by a digit");
      }
      while (!AtEnd() && IsDigit(Peek())) {
        ++position_;
      }
    }

    if (!AtEnd() && (Peek() == 'e' || Peek() == 'E')) {
      ++position_;
      if (!AtEnd() && (Peek() == '+' || Peek() == '-')) {
        ++position_;
      }
      if (AtEnd() || !IsDigit(Peek())) {
        return Fail("an exponent must have at least one digit");
      }
      while (!AtEnd() && IsDigit(Peek())) {
        ++position_;
      }
    }

    return JsonValue::NumberFromText(
        std::string(text_.substr(start, position_ - start)));
  }

  std::string_view text_;
  JsonParseError* error_ = nullptr;
  size_t position_ = 0;
};

// Escape a string for output. Only the characters JSON requires are escaped:
// the manifest's text is UTF-8 and stays UTF-8, so a release note in any
// language is written as itself rather than as a run of \u escapes.
void AppendEscaped(std::string_view text, std::string* out) {
  static constexpr char kDigits[] = "0123456789abcdef";

  out->push_back('"');
  for (char character : text) {
    switch (character) {
      case '"':
        out->append("\\\"");
        break;
      case '\\':
        out->append("\\\\");
        break;
      case '\b':
        out->append("\\b");
        break;
      case '\f':
        out->append("\\f");
        break;
      case '\n':
        out->append("\\n");
        break;
      case '\r':
        out->append("\\r");
        break;
      case '\t':
        out->append("\\t");
        break;
      default:
        if (static_cast<unsigned char>(character) < 0x20) {
          const auto byte = static_cast<unsigned char>(character);
          out->append("\\u00");
          out->push_back(kDigits[byte >> 4]);
          out->push_back(kDigits[byte & 0x0F]);
        } else {
          out->push_back(character);
        }
        break;
    }
  }
  out->push_back('"');
}

void AppendIndent(int depth, std::string* out) {
  out->append(static_cast<size_t>(depth) * 2, ' ');
}

void AppendValue(const JsonValue& value, int depth, std::string* out);

void AppendObject(const JsonValue& value, int depth, std::string* out) {
  const std::vector<JsonValue::Member>* members = value.AsObject();
  if (members->empty()) {
    out->append("{}");
    return;
  }

  out->append("{\n");
  for (size_t index = 0; index < members->size(); ++index) {
    AppendIndent(depth + 1, out);
    AppendEscaped((*members)[index].first, out);
    out->append(": ");
    AppendValue((*members)[index].second, depth + 1, out);
    if (index + 1 < members->size()) {
      out->push_back(',');
    }
    out->push_back('\n');
  }
  AppendIndent(depth, out);
  out->push_back('}');
}

void AppendArray(const JsonValue& value, int depth, std::string* out) {
  const std::vector<JsonValue>* elements = value.AsArray();
  if (elements->empty()) {
    out->append("[]");
    return;
  }

  out->append("[\n");
  for (size_t index = 0; index < elements->size(); ++index) {
    AppendIndent(depth + 1, out);
    AppendValue((*elements)[index], depth + 1, out);
    if (index + 1 < elements->size()) {
      out->push_back(',');
    }
    out->push_back('\n');
  }
  AppendIndent(depth, out);
  out->push_back(']');
}

void AppendValue(const JsonValue& value, int depth, std::string* out) {
  switch (value.type()) {
    case JsonValue::Type::kNull:
      out->append("null");
      break;
    case JsonValue::Type::kBoolean:
      // The accessors return an optional so that a caller asking the wrong
      // type gets nothing rather than a default. Here the type has just been
      // switched on, so value_or's fallback is unreachable — it is written
      // anyway because a switch is a weaker guarantee to a reader, and to a
      // static analyser, than a value that cannot be absent.
      out->append(value.AsBoolean().value_or(false) ? "true" : "false");
      break;
    case JsonValue::Type::kNumber:
      out->append(value.NumberText());
      break;
    case JsonValue::Type::kString:
      AppendEscaped(value.AsString().value_or(std::string_view()), out);
      break;
    case JsonValue::Type::kArray:
      AppendArray(value, depth, out);
      break;
    case JsonValue::Type::kObject:
      AppendObject(value, depth, out);
      break;
  }
}

}  // namespace

JsonValue JsonValue::Null() { return {}; }

JsonValue JsonValue::Boolean(bool value) {
  JsonValue result;
  result.type_ = Type::kBoolean;
  result.boolean_ = value;
  return result;
}

JsonValue JsonValue::Number(int64_t value) {
  return NumberFromText(std::to_string(value));
}

JsonValue JsonValue::NumberFromText(std::string lexeme) {
  JsonValue result;
  result.type_ = Type::kNumber;
  result.text_ = std::move(lexeme);
  return result;
}

JsonValue JsonValue::String(std::string value) {
  JsonValue result;
  result.type_ = Type::kString;
  result.text_ = std::move(value);
  return result;
}

JsonValue JsonValue::Array(std::vector<JsonValue> elements) {
  JsonValue result;
  result.type_ = Type::kArray;
  result.elements_ = std::move(elements);
  return result;
}

JsonValue JsonValue::Object(std::vector<Member> members) {
  JsonValue result;
  result.type_ = Type::kObject;
  result.members_ = std::move(members);
  return result;
}

std::optional<bool> JsonValue::AsBoolean() const {
  if (type_ != Type::kBoolean) {
    return std::nullopt;
  }
  return boolean_;
}

std::optional<std::string_view> JsonValue::AsString() const {
  if (type_ != Type::kString) {
    return std::nullopt;
  }
  return std::string_view(text_);
}

std::optional<int64_t> JsonValue::AsInteger() const {
  if (type_ != Type::kNumber) {
    return std::nullopt;
  }

  int64_t value = 0;
  const char* const begin = text_.data();
  const char* const end = begin + text_.size();
  const std::from_chars_result result = std::from_chars(begin, end, value);

  // Anything left over means a fraction or an exponent, both of which are
  // valid JSON and neither of which is a byte count.
  if (result.ec != std::errc() || result.ptr != end) {
    return std::nullopt;
  }
  return value;
}

std::string_view JsonValue::NumberText() const {
  if (type_ != Type::kNumber) {
    return {};
  }
  return text_;
}

const std::vector<JsonValue>* JsonValue::AsArray() const {
  if (type_ != Type::kArray) {
    return nullptr;
  }
  return &elements_;
}

const std::vector<JsonValue::Member>* JsonValue::AsObject() const {
  if (type_ != Type::kObject) {
    return nullptr;
  }
  return &members_;
}

const JsonValue* JsonValue::Find(std::string_view name) const {
  if (type_ != Type::kObject) {
    return nullptr;
  }
  for (const Member& member : members_) {
    if (member.first == name) {
      return &member.second;
    }
  }
  return nullptr;
}

std::optional<JsonValue> ParseJson(std::string_view text,
                                   JsonParseError* error) {
  if (error != nullptr) {
    *error = JsonParseError{};
  }
  return Parser(text, error).ParseDocument();
}

std::string SerialiseJson(const JsonValue& value) {
  std::string out;
  AppendValue(value, 0, &out);
  out.push_back('\n');
  return out;
}

}  // namespace ddd::capture
