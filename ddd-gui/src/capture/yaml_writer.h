/************************************************************************

    yaml_writer.h

    A small YAML emitter for the capture metadata sidecar
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ddd::capture {

// YAML, for one caller: the sidecar written beside every capture.
//
// Why this exists rather than a dependency, on the same reasoning as
// json_value.h. The engine is Qt-free by rule, so nothing Qt offers is
// available to it, and adding yaml-cpp to the dependency list of the Flatpak,
// the MSI and the DMG to emit a document of about sixty scalars would be a
// package for the sake of two hundred lines. Nothing here ever *reads* YAML,
// which is where the difficulty in that format actually lives: this writes a
// document whose shape is fixed at compile time and whose only variable is what
// the strings contain.
//
// It emits a deliberately small subset — nested block mappings and scalars, and
// nothing else. No sequences, no anchors, no flow style, no multi-document
// streams. That subset is what the sidecar needs, and a writer that cannot
// express a construct is a writer that cannot get that construct wrong.
//
// **Every string is emitted double-quoted, always**, even where YAML would
// accept it bare. That is the decision that makes this safe to hand arbitrary
// user text: a disc title of "no" is the boolean false in YAML 1.1, "1:30" is a
// sexagesimal integer, a leading "*" is an alias and a leading "%" is a
// directive. Quoting unconditionally means none of those cases has to be
// detected, so none of them can be missed — and the cost is a document that
// looks slightly more machine-written than a hand-tuned one would.
class YamlWriter {
 public:
  // A `# ...` line at the current indent. Comments are for the human who opens
  // the file in a text editor years later; no reader is expected to parse them.
  void Comment(std::string_view text);

  void BlankLine();

  // Open a nested mapping under `key`, and close the most recent one.
  //
  // A mapping is written even when nothing is put in it — see the sidecar,
  // where an empty section says "this was asked about and there was nothing"
  // rather than leaving a reader to wonder whether the writer knew about the
  // section at all. Such a mapping is emitted as `key: {}` rather than as a
  // bare `key:`, which YAML reads as null: a reader indexing into the section
  // finds nothing in it either way, and only one of the two lets it index at
  // all.
  //
  // The key line is therefore written when the mapping's first entry is, not
  // when the mapping is opened — by which time it is known which of the two
  // forms this is.
  void BeginMapping(std::string_view key);
  void EndMapping();

  void String(std::string_view key, std::string_view value);

  // As String, but writes nothing at all when the value is empty.
  //
  // The rule the whole sidecar is built on: a field that was never established
  // is absent, not blank. A file asserting an empty disc title is a file
  // asserting something, and the thing it asserts is not true.
  void StringIfPresent(std::string_view key, std::string_view value);

  void Integer(std::string_view key, int64_t value);
  void Unsigned(std::string_view key, uint64_t value);

  // A number with a fixed number of decimals, so that a duration in a file
  // never arrives in exponent form and never depends on the C locale — a
  // machine set to a comma decimal separator must not write a document whose
  // numbers a reader elsewhere parses as something else.
  void Number(std::string_view key, double value, int decimals);

  void Boolean(std::string_view key, bool value);

  // The document as written so far. Ends with a newline whenever it is not
  // empty.
  const std::string& str() const { return text_; }

  // The quoting rule above, exposed for the tests that pin it. Returns the
  // scalar including its quotes.
  static std::string QuoteScalar(std::string_view value);

 private:
  // Emit the key lines of every mapping opened but not yet written out, so that
  // the entry about to be written has its parents above it. Called by
  // everything that puts a line in the document.
  void FlushOpenMappings();

  void WriteIndent(int depth);
  void WriteKey(std::string_view key);

  std::string text_;

  // The mappings currently open, outermost first, each with whether its key
  // line has been emitted yet. A mapping still holding an unemitted key when it
  // is closed is one nothing was put in.
  struct OpenMapping {
    std::string key;
    bool written = false;
  };
  std::vector<OpenMapping> open_;
};

}  // namespace ddd::capture
