/************************************************************************

    yaml_writer.cpp

    A small YAML emitter for the capture metadata sidecar
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "yaml_writer.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace ddd::capture {
namespace {

// Two spaces per level, which is what every YAML document anyone will compare
// this against uses. Tabs are not indentation in YAML at all — the
// specification forbids them there — so this is not a style preference.
constexpr const char* kIndent = "  ";

char HexDigit(unsigned value) {
  return static_cast<char>(value < 10 ? '0' + value : 'a' + (value - 10));
}

}  // namespace

std::string YamlWriter::QuoteScalar(std::string_view value) {
  std::string quoted;
  quoted.reserve(value.size() + 2);
  quoted.push_back('"');

  for (const char character : value) {
    const auto byte = static_cast<unsigned char>(character);

    switch (character) {
      case '"':
        quoted += "\\\"";
        continue;
      case '\\':
        quoted += "\\\\";
        continue;
      case '\n':
        quoted += "\\n";
        continue;
      case '\r':
        quoted += "\\r";
        continue;
      case '\t':
        quoted += "\\t";
        continue;
      default:
        break;
    }

    // Control characters as \xNN. The player's user-code replies are the reason
    // this is here rather than being assumed impossible: a Pioneer user code
    // carries NUL for a field that was never encoded, and those characters are
    // the evidence being recorded — see user_code.h, where telling an unencoded
    // field from an unreadable one is the whole job. Written as an escape so
    // the document stays a text file that survives being opened, edited and
    // saved by an ordinary editor.
    //
    // Bytes at 0x80 and above are passed through untouched. They are UTF-8
    // continuation and lead bytes — a disc title in any language but English
    // arrives as those — and escaping them one byte at a time would turn a
    // legible title into a row of hex.
    if (byte < 0x20 || byte == 0x7F) {
      quoted += "\\x";
      quoted.push_back(HexDigit(byte >> 4U));
      quoted.push_back(HexDigit(byte & 0x0FU));
      continue;
    }

    quoted.push_back(character);
  }

  quoted.push_back('"');
  return quoted;
}

void YamlWriter::WriteIndent(int depth) {
  for (int level = 0; level < depth; ++level) {
    text_ += kIndent;
  }
}

void YamlWriter::FlushOpenMappings() {
  for (size_t level = 0; level < open_.size(); ++level) {
    if (open_[level].written) {
      continue;
    }
    WriteIndent(static_cast<int>(level));
    WriteKey(open_[level].key);
    text_ += '\n';
    open_[level].written = true;
  }
}

void YamlWriter::WriteKey(std::string_view key) {
  // Keys are quoted on the same terms as values and for the same reason. Every
  // key this writer is given is a compile-time constant in the sidecar, so the
  // quoting is never actually needed — which is exactly why it is done here
  // rather than left to whoever adds the next section.
  text_ += QuoteScalar(key);
  text_ += ':';
}

void YamlWriter::Comment(std::string_view text) {
  FlushOpenMappings();
  WriteIndent(static_cast<int>(open_.size()));

  // A bare "#" for an empty comment rather than "# ", so that a paragraph of
  // comment lines does not leave trailing whitespace on the blank ones between
  // them — which every editor and every diff would then show as damage.
  if (text.empty()) {
    text_ += "#\n";
    return;
  }

  text_ += "# ";
  text_.append(text);
  text_ += '\n';
}

void YamlWriter::BlankLine() {
  // Deliberately does not flush. A blank line before a section that turns out
  // to be empty should not be what causes that section to exist.
  text_ += '\n';
}

void YamlWriter::BeginMapping(std::string_view key) {
  open_.push_back(OpenMapping{std::string(key), false});
}

void YamlWriter::EndMapping() {
  if (open_.empty()) {
    return;
  }

  const OpenMapping mapping = open_.back();
  open_.pop_back();

  if (mapping.written) {
    return;
  }

  // Nothing was put in it. The parents above still have to appear, since this
  // line sits under them.
  FlushOpenMappings();
  WriteIndent(static_cast<int>(open_.size()));
  WriteKey(mapping.key);
  text_ += " {}\n";
}

void YamlWriter::String(std::string_view key, std::string_view value) {
  FlushOpenMappings();
  WriteIndent(static_cast<int>(open_.size()));
  WriteKey(key);
  text_ += ' ';
  text_ += QuoteScalar(value);
  text_ += '\n';
}

void YamlWriter::StringIfPresent(std::string_view key, std::string_view value) {
  if (value.empty()) {
    return;
  }
  String(key, value);
}

void YamlWriter::Integer(std::string_view key, int64_t value) {
  FlushOpenMappings();
  WriteIndent(static_cast<int>(open_.size()));
  WriteKey(key);
  text_ += ' ';
  text_ += std::to_string(value);
  text_ += '\n';
}

void YamlWriter::Unsigned(std::string_view key, uint64_t value) {
  FlushOpenMappings();
  WriteIndent(static_cast<int>(open_.size()));
  WriteKey(key);
  text_ += ' ';
  text_ += std::to_string(value);
  text_ += '\n';
}

void YamlWriter::Number(std::string_view key, double value, int decimals) {
  FlushOpenMappings();
  WriteIndent(static_cast<int>(open_.size()));
  WriteKey(key);
  text_ += ' ';

  // A value that is not a number has no YAML spelling worth writing: `.nan` is
  // legal and means the writer has produced arithmetic nobody checked, so it is
  // recorded as zero rather than propagated into an archival file.
  if (!std::isfinite(value)) {
    value = 0.0;
  }

  std::array<char, 64> buffer{};
  const int written =
      std::snprintf(buffer.data(), buffer.size(), "%.*f", decimals, value);
  if (written <= 0) {
    text_ += "0\n";
    return;
  }

  // snprintf writes the decimal separator the C locale asks for, and a machine
  // set to a European locale asks for a comma. YAML has one spelling of a
  // number and it is not that one, so the separator is put back by hand rather
  // than by trusting whoever called setlocale — this library is Qt-free and
  // runs in a command-line tool as well as under an application that resets it.
  for (int index = 0; index < written; ++index) {
    text_ += buffer[static_cast<size_t>(index)] == ','
                 ? '.'
                 : buffer[static_cast<size_t>(index)];
  }
  text_ += '\n';
}

void YamlWriter::Boolean(std::string_view key, bool value) {
  FlushOpenMappings();
  WriteIndent(static_cast<int>(open_.size()));
  WriteKey(key);
  text_ += value ? " true\n" : " false\n";
}

}  // namespace ddd::capture
