/************************************************************************

    test_yaml_writer.cpp

    T1 tests for the sidecar's YAML emitter
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>

#include "yaml_writer.h"

namespace ddd::capture {
namespace {

TEST(YamlWriterTest, AScalarIsAlwaysQuoted) {
  // The decision the whole writer rests on. Every one of these is legal YAML
  // unquoted and none of them means the string it looks like: "no" is a boolean
  // in YAML 1.1, "1:30" is a sexagesimal integer, a leading "*" is an alias and
  // a leading "%" is a directive. Quoting unconditionally means none of those
  // cases has to be detected, so none of them can be missed.
  EXPECT_EQ(YamlWriter::QuoteScalar("no"), "\"no\"");
  EXPECT_EQ(YamlWriter::QuoteScalar("1:30"), "\"1:30\"");
  EXPECT_EQ(YamlWriter::QuoteScalar("*anchor"), "\"*anchor\"");
  EXPECT_EQ(YamlWriter::QuoteScalar("%YAML"), "\"%YAML\"");
  EXPECT_EQ(YamlWriter::QuoteScalar(""), "\"\"");
}

TEST(YamlWriterTest, QuotesAndBackslashesSurviveARoundTrip) {
  EXPECT_EQ(YamlWriter::QuoteScalar("say \"hello\""), "\"say \\\"hello\\\"\"");
  EXPECT_EQ(YamlWriter::QuoteScalar("C:\\discs"), "\"C:\\\\discs\"");
}

TEST(YamlWriterTest, ControlCharactersAreEscapedRatherThanDropped) {
  // The Pioneer user code is why. A code carries NUL where a field was never
  // encoded, and those characters are the evidence being recorded — dropping
  // them would turn "sixty characters carry nothing" into "sixty characters are
  // missing from this file", which is a different finding.
  EXPECT_EQ(YamlWriter::QuoteScalar(std::string("a\0b", 3)), "\"a\\x00b\"");
  EXPECT_EQ(YamlWriter::QuoteScalar("a\x60`b"), "\"a\x60`b\"");
  EXPECT_EQ(YamlWriter::QuoteScalar("line\nbreak"), "\"line\\nbreak\"");
  EXPECT_EQ(YamlWriter::QuoteScalar("tab\there"), "\"tab\\there\"");
  EXPECT_EQ(YamlWriter::QuoteScalar("\x7F"), "\"\\x7f\"");
}

TEST(YamlWriterTest, UnicodeIsPassedThroughRatherThanEscaped) {
  // A disc title in any language but English arrives as UTF-8, and escaping it
  // a byte at a time would turn a legible title into a row of hex.
  EXPECT_EQ(YamlWriter::QuoteScalar("Kärlek"), "\"Kärlek\"");
}

TEST(YamlWriterTest, MappingsNestByTwoSpaces) {
  YamlWriter yaml;
  yaml.BeginMapping("disc");
  yaml.String("type", "CAV");
  yaml.BeginMapping("side");
  yaml.Integer("value", 2);
  yaml.String("source", "reported");
  yaml.EndMapping();
  yaml.EndMapping();

  EXPECT_EQ(yaml.str(),
            "\"disc\":\n"
            "  \"type\": \"CAV\"\n"
            "  \"side\":\n"
            "    \"value\": 2\n"
            "    \"source\": \"reported\"\n");
}

TEST(YamlWriterTest, AMappingNothingWasPutInIsAnEmptyMapping) {
  // `disc:` alone reads as null, and a reader indexing into it would find not
  // an empty section but nothing to index at all. The distinction matters
  // because an empty section is a real answer here: it says the question was
  // asked and there was no examination.
  YamlWriter yaml;
  yaml.BeginMapping("disc");
  yaml.EndMapping();

  EXPECT_EQ(yaml.str(), "\"disc\": {}\n");
}

TEST(YamlWriterTest, AnEmptyChildStillGetsItsParents) {
  YamlWriter yaml;
  yaml.BeginMapping("outer");
  yaml.BeginMapping("inner");
  yaml.EndMapping();
  yaml.EndMapping();

  EXPECT_EQ(yaml.str(),
            "\"outer\":\n"
            "  \"inner\": {}\n");
}

TEST(YamlWriterTest, AnAbsentStringIsNotWrittenAtAll) {
  // The rule the whole sidecar is built on: a field that was never established
  // is absent, not blank. A blank one would be the file asserting something.
  YamlWriter yaml;
  yaml.StringIfPresent("title", "");
  yaml.StringIfPresent("notes", "worth keeping");

  EXPECT_EQ(yaml.str(), "\"notes\": \"worth keeping\"\n");
}

TEST(YamlWriterTest, NumbersAreWrittenWithAFixedNumberOfDecimals) {
  YamlWriter yaml;
  yaml.Number("duration_seconds", 2472.5, 3);
  yaml.Number("rms", 0.0, 2);
  yaml.Unsigned("samples", 40'000'000);
  yaml.Integer("offset", -3);
  yaml.Boolean("test_mode", false);

  EXPECT_EQ(yaml.str(),
            "\"duration_seconds\": 2472.500\n"
            "\"rms\": 0.00\n"
            "\"samples\": 40000000\n"
            "\"offset\": -3\n"
            "\"test_mode\": false\n");
}

TEST(YamlWriterTest, ANumberThatIsNotOneIsRecordedAsZero) {
  // `.nan` is legal YAML and means the writer produced arithmetic nobody
  // checked. An archival file should not carry that.
  YamlWriter yaml;
  yaml.Number("rms", 0.0 / 0.0, 2);
  EXPECT_EQ(yaml.str(), "\"rms\": 0.00\n");
}

TEST(YamlWriterTest, CommentsSitAtTheCurrentIndent) {
  YamlWriter yaml;
  yaml.Comment("the whole document");
  yaml.Comment("");
  yaml.BeginMapping("capture");
  yaml.Comment("about this section");
  yaml.String("format", "FLAC");
  yaml.EndMapping();

  EXPECT_EQ(yaml.str(),
            "# the whole document\n"
            "#\n"
            "\"capture\":\n"
            "  # about this section\n"
            "  \"format\": \"FLAC\"\n");
}

TEST(YamlWriterTest, ABlankLineDoesNotBringAnEmptySectionIntoExistence) {
  YamlWriter yaml;
  yaml.BeginMapping("disc");
  yaml.BlankLine();
  yaml.EndMapping();

  EXPECT_EQ(yaml.str(), "\n\"disc\": {}\n");
}

}  // namespace
}  // namespace ddd::capture
