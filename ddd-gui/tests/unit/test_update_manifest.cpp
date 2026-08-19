/************************************************************************

    test_update_manifest.cpp

    Reading, writing and refusing update manifests
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "update_fixtures.h"
#include "update_manifest.h"

namespace ddd::capture {
namespace {

// Replace the first occurrence of `from` with `to`, so a test can say what it
// broke rather than restating a whole manifest to change one field.
std::string With(std::string_view from, std::string_view to) {
  std::string text(test::kManifestJson);
  const size_t position = text.find(from);
  EXPECT_NE(position, std::string::npos) << from;
  if (position != std::string::npos) {
    text.replace(position, from.size(), to);
  }
  return text;
}

std::vector<std::string> ErrorsFrom(std::string_view text) {
  std::vector<std::string> errors;
  EXPECT_FALSE(ParseUpdateManifest(text, &errors).has_value());
  return errors;
}

TEST(UpdateManifest, ReadsTheFixture) {
  std::vector<std::string> errors;
  const std::optional<UpdateManifest> parsed =
      ParseUpdateManifest(test::kManifestJson, &errors);
  for (const std::string& message : errors) {
    ADD_FAILURE() << message;
  }
  ASSERT_TRUE(parsed.has_value());

  const UpdateManifest manifest = test::Checked(parsed);
  EXPECT_EQ(manifest.manifest_version, kUpdateManifestVersion);
  EXPECT_EQ(manifest.channel, UpdateChannel::kDevelopment);
  EXPECT_EQ(manifest.version, "1.4.0");
  EXPECT_EQ(manifest.commit, "0123abcd");
  EXPECT_EQ(manifest.created, "2026-08-14T09:15:00Z");
  EXPECT_FALSE(manifest.release_notes.empty());

  ASSERT_TRUE(manifest.firmware.has_value());
  const UpdateComponent firmware = test::Checked(manifest.firmware);
  EXPECT_EQ(firmware.file, "firmware.img");
  EXPECT_EQ(firmware.length, test::kFirmwarePayload.size());
  EXPECT_EQ(ToHex(firmware.sha256),
            "c8422f817b69fa687531b26f4f190b2c4a97fe7b6791850ff76dc10a2d52b2d9");
  EXPECT_EQ(firmware.identity, "0123abcd");
  EXPECT_EQ(firmware.interface_version, 1);

  ASSERT_TRUE(manifest.gateware.has_value());
  const UpdateComponent gateware = test::Checked(manifest.gateware);
  EXPECT_EQ(gateware.file, "gateware-app.rpd");
  EXPECT_EQ(gateware.interface_version, 2);

  EXPECT_EQ(manifest.compatibility.minimum_application_version, "1.4.0");
  EXPECT_EQ(manifest.compatibility.minimum_register_map_version, 2);
  EXPECT_EQ(manifest.compatibility.epcs_layout_version, 1);
}

// The writer exists so that the reader can be checked against something, and
// the manifest that comes back out has to be the manifest that went in — byte
// for byte, because the bytes are what a signature covers.
TEST(UpdateManifest, RoundTripsThroughTheWriter) {
  const std::optional<UpdateManifest> parsed =
      ParseUpdateManifest(test::kManifestJson, nullptr);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(SerialiseUpdateManifest(test::Checked(parsed)),
            test::kManifestJson);
}

// A firmware-only bundle is a complete bundle: it is what the development loop
// produces and what the early phases of the update work shipped.
TEST(UpdateManifest, AcceptsABundleWithOnlyOneComponent) {
  UpdateManifest manifest =
      test::Checked(ParseUpdateManifest(test::kManifestJson, nullptr));
  ASSERT_TRUE(manifest.firmware.has_value());
  manifest.gateware.reset();

  std::vector<std::string> errors;
  const std::optional<UpdateManifest> reparsed =
      ParseUpdateManifest(SerialiseUpdateManifest(manifest), &errors);
  for (const std::string& message : errors) {
    ADD_FAILURE() << message;
  }
  ASSERT_TRUE(reparsed.has_value());
  EXPECT_TRUE(test::Checked(reparsed).firmware.has_value());
  EXPECT_FALSE(test::Checked(reparsed).gateware.has_value());
}

TEST(UpdateManifest, RefusesABundleWithNothingToInstall) {
  UpdateManifest manifest =
      test::Checked(ParseUpdateManifest(test::kManifestJson, nullptr));
  manifest.firmware.reset();
  manifest.gateware.reset();

  EXPECT_FALSE(ErrorsFrom(SerialiseUpdateManifest(manifest)).empty());
}

// A schema this build does not know may mean something different by a field of
// the same name, so nothing is read out of it at all.
TEST(UpdateManifest, RefusesAnUnknownSchemaVersion) {
  const std::vector<std::string> errors =
      ErrorsFrom(With(R"("manifest_version": 1)", R"("manifest_version": 2)"));
  ASSERT_EQ(errors.size(), 1u)
      << "an unknown schema should stop the parse, not produce a list";
  EXPECT_NE(errors.front().find("schema version"), std::string::npos);
}

TEST(UpdateManifest, RefusesAnUnknownChannel) {
  EXPECT_FALSE(
      ErrorsFrom(With(R"("channel": "development")", R"("channel": "beta")"))
          .empty());
}

TEST(UpdateManifest, RefusesAMalformedDigest) {
  EXPECT_FALSE(
      ErrorsFrom(
          With("c8422f817b69fa687531b26f4f190b2c4a97fe7b6791850ff76dc10a2d52b2"
               "d9",
               "c8422f81"))
          .empty());
}

TEST(UpdateManifest, RefusesANonNumericVersion) {
  EXPECT_FALSE(
      ErrorsFrom(With(R"("version": "1.4.0")", R"("version": "0123abcd")"))
          .empty());
}

TEST(UpdateManifest, RefusesAMissingField) {
  EXPECT_FALSE(ErrorsFrom(With(R"(      "identity": "0123abcd",
      "interface_version": 1)",
                               R"(      "interface_version": 1)"))
                   .empty());
}

// Every problem is reported, not only the first: the reader of these messages
// is usually whoever is building the release pipeline, and one round trip per
// mistake is a poor way to spend an afternoon.
TEST(UpdateManifest, ReportsEveryProblemItFinds) {
  std::string text =
      With(R"("version": "1.4.0")", R"("version": "not-a-version")");
  const size_t position = text.find(R"("commit": "0123abcd",)");
  ASSERT_NE(position, std::string::npos);
  text.replace(position, std::string_view(R"("commit": "0123abcd",)").size(),
               R"("commit": "",)");

  std::vector<std::string> errors;
  EXPECT_FALSE(ParseUpdateManifest(text, &errors).has_value());
  EXPECT_GE(errors.size(), 2u);
}

// --- the bring-up components -------------------------------------------

// The bring-up wizard's input: one release bundle carrying all four payloads.
// The update window installs the two it knows and ignores the rest; the wizard
// needs every one of them.
TEST(UpdateManifest, ReadsABundleCarryingEveryComponent) {
  std::vector<std::string> errors;
  const std::optional<UpdateManifest> parsed =
      ParseUpdateManifest(test::kBringUpManifestJson, &errors);
  for (const std::string& message : errors) {
    ADD_FAILURE() << message;
  }
  ASSERT_TRUE(parsed.has_value());

  const UpdateManifest manifest = test::Checked(parsed);
  ASSERT_TRUE(manifest.provisioning.has_value());

  const UpdateComponent provisioning = test::Checked(manifest.provisioning);
  EXPECT_EQ(provisioning.file, "gateware-provisioning.svf");
  EXPECT_EQ(provisioning.length, test::kProvisioningPayload.size());

  // And the other three beside it, because one file serves both consumers:
  // the vectors and the factory image are what a bring-up needs on top of the
  // firmware and application gateware an ordinary update installs.
  EXPECT_TRUE(manifest.firmware.has_value());
  EXPECT_TRUE(manifest.gateware.has_value());
  EXPECT_TRUE(manifest.factory_gateware.has_value());
}

TEST(UpdateManifest, WritesAProvisioningComponentBackUnderItsOwnName) {
  const std::optional<UpdateManifest> parsed =
      ParseUpdateManifest(test::kBringUpManifestJson, nullptr);
  ASSERT_TRUE(parsed.has_value());

  const std::string written = SerialiseUpdateManifest(test::Checked(parsed));
  EXPECT_NE(written.find(std::string(kProvisioningComponentName)),
            std::string::npos)
      << written;

  // And reads back as the same thing, which is what makes the name a format
  // rather than a spelling one half of the code happens to use.
  const std::optional<UpdateManifest> again =
      ParseUpdateManifest(written, nullptr);
  ASSERT_TRUE(again.has_value());
  EXPECT_TRUE(test::Checked(again).provisioning.has_value());
}

// A manifest carrying nothing but the vectors is legal. Whether it may be
// *installed* is the gate's business, and the gate says no — vectors are
// played through a cable rather than sent to a running device.
TEST(UpdateManifest, AcceptsAManifestCarryingOnlyTheBringUpPayloads) {
  std::string text(test::kBringUpManifestJson);
  const size_t start = text.find(R"(    "firmware": {)");
  const size_t end = text.find(R"(    "gateware-provisioning-svf")");
  ASSERT_NE(start, std::string::npos);
  ASSERT_NE(end, std::string::npos);
  text.erase(start, end - start);

  std::vector<std::string> errors;
  const std::optional<UpdateManifest> parsed =
      ParseUpdateManifest(text, &errors);
  for (const std::string& message : errors) {
    ADD_FAILURE() << message;
  }
  ASSERT_TRUE(parsed.has_value());
  EXPECT_TRUE(test::Checked(parsed).provisioning.has_value());
}

// The property the extension rests on: a payload nobody described is refused
// rather than skipped. A reader that installed the two components it knew and
// ignored a third would report a complete install of a partial bundle.
TEST(UpdateManifest, RefusesAComponentKindItDoesNotKnow) {
  const std::vector<std::string> errors =
      ErrorsFrom(With(R"("gateware": {)", R"("bootloader": {)"));
  ASSERT_FALSE(errors.empty());

  bool named = false;
  for (const std::string& message : errors) {
    named = named || message.find("bootloader") != std::string::npos;
  }
  EXPECT_TRUE(named) << "the refusal did not name the component it refused";
}

TEST(UpdateManifest, RefusesTextThatIsNotJsonAtAll) {
  EXPECT_FALSE(ErrorsFrom("").empty());
  EXPECT_FALSE(ErrorsFrom("not json").empty());
  EXPECT_FALSE(ErrorsFrom("[1, 2, 3]").empty());
}

}  // namespace
}  // namespace ddd::capture
