/************************************************************************

    test_json_value.cpp

    The manifest parser's strictness, stated as tests
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>

#include "json_value.h"

namespace ddd::capture {
namespace {

bool Rejects(std::string_view text) {
  return !ParseJson(text, nullptr).has_value();
}

// Parse, failing the test if it does not. Returning a value rather than an
// optional keeps every case below free of the has_value() dance, and of the
// static-analysis complaint that follows one (see tests/support for the same
// device in the bundle tests).
JsonValue Parsed(std::string_view text) {
  JsonParseError error;
  const std::optional<JsonValue> value = ParseJson(text, &error);
  EXPECT_TRUE(value.has_value()) << error.message;
  return value.value_or(JsonValue());
}

TEST(JsonValue, ReadsAnObjectOfEveryType) {
  const JsonValue document = Parsed(R"({
    "name": "firmware.img",
    "length": 116432,
    "signed": true,
    "notes": null,
    "tags": ["a", "b"]
  })");

  ASSERT_NE(document.Find("name"), nullptr);
  EXPECT_EQ(document.Find("name")->AsString(), "firmware.img");
  EXPECT_EQ(document.Find("length")->AsInteger(), 116432);
  EXPECT_EQ(document.Find("signed")->AsBoolean(), true);
  EXPECT_TRUE(document.Find("notes")->IsNull());

  const std::vector<JsonValue>* tags = document.Find("tags")->AsArray();
  ASSERT_NE(tags, nullptr);
  ASSERT_EQ(tags->size(), 2u);
  EXPECT_EQ((*tags)[1].AsString(), "b");
}

TEST(JsonValue, MissingMembersAndWrongTypesAnswerTheSameWay) {
  const JsonValue document = Parsed(R"({"length": "116432"})");

  EXPECT_EQ(document.Find("absent"), nullptr);
  EXPECT_FALSE(document.Find("length")->AsInteger().has_value());
}

// Numbers keep the text they arrived as, so a byte count cannot acquire a
// ".0" or lose a digit by passing through a double.
TEST(JsonValue, NumbersSurviveARoundTripExactly) {
  const JsonValue document = Parsed(R"({"length": 9007199254740993})");
  EXPECT_EQ(document.Find("length")->AsInteger(), 9007199254740993);
  EXPECT_EQ(SerialiseJson(document), "{\n  \"length\": 9007199254740993\n}\n");
}

// A length written as 1e3 or 1000.0 is the same quantity to a mathematician
// and a sign that something other than the build script wrote the manifest.
TEST(JsonValue, IntegerAccessorRefusesFractionsAndExponents) {
  const JsonValue document = Parsed(R"({"a": 1e3, "b": 1000.0, "c": -5})");
  EXPECT_FALSE(document.Find("a")->AsInteger().has_value());
  EXPECT_FALSE(document.Find("b")->AsInteger().has_value());
  EXPECT_EQ(document.Find("c")->AsInteger(), -5);
}

TEST(JsonValue, DecodesEscapesIncludingSurrogatePairs) {
  const JsonValue document = Parsed(R"({"text": "a\"b\\c\/dé😀"})");
  EXPECT_EQ(document.Find("text")->AsString(), "a\"b\\c/dé😀");
}

TEST(JsonValue, RoundTripsThroughTheSerialiser) {
  constexpr std::string_view kText = R"({
  "manifest_version": 1,
  "components": {
    "firmware": {
      "file": "firmware.img",
      "length": 42
    }
  },
  "empty_object": {},
  "empty_array": [],
  "list": [
    1,
    2
  ]
}
)";

  const JsonValue document = Parsed(kText);
  EXPECT_EQ(SerialiseJson(document), kText);
}

TEST(JsonValue, WritesTheEscapesItCanRead) {
  const JsonValue document = JsonValue::Object({
      {"text", JsonValue::String("line\nquote\"slash\\tab\ttiny\x01")},
  });
  const JsonValue reparsed = Parsed(SerialiseJson(document));
  EXPECT_EQ(reparsed.Find("text")->AsString(),
            "line\nquote\"slash\\tab\ttiny\x01");
}

// The strictness is the security property, so each rejection is named.
TEST(JsonValue, RefusesDuplicateMemberNames) {
  EXPECT_TRUE(Rejects(R"({"sha256": "aa", "sha256": "bb"})"));
}

TEST(JsonValue, RefusesTrailingContent) {
  EXPECT_TRUE(Rejects(R"({"a": 1} {"b": 2})"));
  EXPECT_TRUE(Rejects(R"({"a": 1} garbage)"));
}

TEST(JsonValue, RefusesTheExtensionsThatAreNotJson) {
  EXPECT_TRUE(Rejects("{\"a\": 1,}"));
  EXPECT_TRUE(Rejects("[1, 2,]"));
  EXPECT_TRUE(Rejects("{'a': 1}"));
  EXPECT_TRUE(Rejects("{\"a\": 1} // a comment"));
  EXPECT_TRUE(Rejects("{\"a\": NaN}"));
  EXPECT_TRUE(Rejects("{\"a\": 007}"));
  EXPECT_TRUE(Rejects("{\"a\": +1}"));
  EXPECT_TRUE(Rejects("{\"a\": .5}"));
}

TEST(JsonValue, RefusesMalformedStrings) {
  EXPECT_TRUE(Rejects("{\"a\": \"unterminated}"));
  EXPECT_TRUE(Rejects("{\"a\": \"raw\nnewline\"}"));
  EXPECT_TRUE(Rejects(R"({"a": "\q"})"));
  EXPECT_TRUE(Rejects(R"({"a": "\u00"})"));
  EXPECT_TRUE(Rejects(R"({"a": "\uD83D"})"));
  EXPECT_TRUE(Rejects(R"({"a": "\uDE00"})"));
}

// A file that has not been authenticated yet must not be able to run the stack
// out before the signature that would have rejected it has been checked.
TEST(JsonValue, RefusesDocumentsNestedBeyondTheLimit) {
  std::string deep;
  for (int index = 0; index < 200; ++index) {
    deep += "[";
  }
  deep += "1";
  for (int index = 0; index < 200; ++index) {
    deep += "]";
  }
  EXPECT_TRUE(Rejects(deep));

  // Well inside the limit still parses, so the guard is a cap and not a ban on
  // structure.
  std::string shallow;
  for (int index = 0; index < 8; ++index) {
    shallow += "[";
  }
  shallow += "1";
  for (int index = 0; index < 8; ++index) {
    shallow += "]";
  }
  EXPECT_FALSE(Rejects(shallow));
}

TEST(JsonValue, ReportsWhereItGaveUp) {
  JsonParseError error;
  EXPECT_FALSE(ParseJson(R"({"a": 1, "b": })", &error).has_value());
  EXPECT_FALSE(error.message.empty());
  EXPECT_GT(error.offset, 0u);
}

}  // namespace
}  // namespace ddd::capture
