/************************************************************************

    test_update_gate.cpp

    T1 unit test for the install-time compatibility gate
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include "update_gate.h"
#include "wire_protocol.h"

namespace ddd::capture {
namespace {

UpdateManifest MakeManifest() {
  UpdateManifest manifest;
  manifest.manifest_version = kUpdateManifestVersion;
  manifest.channel = UpdateChannel::kRelease;
  manifest.version = "1.4.0";
  manifest.commit = "0123abcd";

  UpdateComponent firmware;
  firmware.file = "firmware.img";
  firmware.length = 1024;
  firmware.identity = "0123abcd";
  firmware.interface_version = 1;
  manifest.firmware = firmware;

  manifest.compatibility.minimum_application_version = "1.0.0";
  manifest.compatibility.minimum_register_map_version = 1;
  manifest.compatibility.epcs_layout_version = 1;
  return manifest;
}

UpdateGateInput MakeInput() {
  UpdateGateInput input;
  input.application_version = "1.4.0";
  input.device_attached = true;
  input.device.protocol_version = 1;
  input.device.gateware_present = true;
  input.device.register_map_version = 1;
  return input;
}

TEST(UpdateGate, AllowsAnOrdinaryUpdate) {
  const UpdateGateResult result = CheckUpdateGate(MakeManifest(), MakeInput());

  EXPECT_TRUE(result.allowed());
  EXPECT_TRUE(result.reasons.empty());
}

TEST(UpdateGate, RefusesWhenNoDeviceIsAttached) {
  UpdateGateInput input = MakeInput();
  input.device_attached = false;

  const UpdateGateResult result = CheckUpdateGate(MakeManifest(), input);

  EXPECT_FALSE(result.allowed());
  EXPECT_EQ(result.verdict, UpdateGateVerdict::kIncompatible);
  ASSERT_EQ(result.reasons.size(), 1u);
}

// The rule the whole gate exists for: a user cannot use this application to
// flash the device past this application's own understanding. The verdict is
// distinct from plain incompatibility because it is the one with a next
// step — update the application — and the interface routes on that.
TEST(UpdateGate, RefusesABundleThatNeedsANewerApplication) {
  UpdateManifest manifest = MakeManifest();
  manifest.compatibility.minimum_application_version = "9.0.0";

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_FALSE(result.allowed());
  EXPECT_EQ(result.verdict, UpdateGateVerdict::kApplicationTooOld);
  ASSERT_FALSE(result.reasons.empty());
  EXPECT_NE(result.reasons.front().find("Update the application first"),
            std::string::npos);
}

TEST(UpdateGate, RefusesAManifestSchemaItDoesNotKnow) {
  UpdateManifest manifest = MakeManifest();
  manifest.manifest_version = kUpdateManifestVersion + 1;

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_EQ(result.verdict, UpdateGateVerdict::kApplicationTooOld);
}

// A firmware whose protocol this build does not know is a device this build
// could not talk to afterwards — including to say what had happened.
TEST(UpdateGate, RefusesFirmwareSpeakingANewerProtocol) {
  UpdateManifest manifest = MakeManifest();
  UpdateComponent firmware = manifest.firmware.value_or(UpdateComponent{});
  firmware.interface_version = kProtocolVersionMaximum + 1;
  manifest.firmware = firmware;

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_EQ(result.verdict, UpdateGateVerdict::kApplicationTooOld);
}

TEST(UpdateGate, RefusesGatewareUsingANewerRegisterMap) {
  UpdateManifest manifest = MakeManifest();

  UpdateComponent gateware;
  gateware.file = "gateware-app.rpd";
  gateware.length = 2048;
  gateware.interface_version = kRegisterMapVersionMaximum + 1;
  manifest.gateware = gateware;

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_EQ(result.verdict, UpdateGateVerdict::kApplicationTooOld);
}

// Rollback is a feature, and a downgrade passes through the same gate as an
// upgrade rather than through a second, more forgiving one.
TEST(UpdateGate, AllowsADowngradeThatIsStillWithinRange) {
  UpdateManifest manifest = MakeManifest();
  manifest.version = "1.1.0";
  manifest.compatibility.minimum_application_version = "1.0.0";

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_TRUE(result.allowed());
}

TEST(UpdateGate, RefusesADowngradeBelowWhatThisBuildSupports) {
  UpdateManifest manifest = MakeManifest();
  UpdateComponent firmware = manifest.firmware.value_or(UpdateComponent{});
  firmware.interface_version = kProtocolVersionMinimum - 1;
  manifest.firmware = firmware;

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_FALSE(result.allowed());
  EXPECT_EQ(result.verdict, UpdateGateVerdict::kIncompatible);
}

// A developer build cannot say how old it is, so it says that rather than
// asserting an ordering that was never computed. Saying "new enough" on the
// strength of a comparison that could not be made is exactly the silent
// failure the mechanism exists to prevent.
TEST(UpdateGate, SaysSoWhenTheApplicationVersionCannotBeOrdered) {
  UpdateGateInput input = MakeInput();
  input.application_version = "0123abcd-dirty";

  const UpdateGateResult result = CheckUpdateGate(MakeManifest(), input);

  EXPECT_TRUE(result.allowed());
  ASSERT_EQ(result.reasons.size(), 1u);
  EXPECT_NE(result.reasons.front().find("not a numbered release"),
            std::string::npos);
}

TEST(UpdateGate, RefusesABundleWithNothingInIt) {
  UpdateManifest manifest = MakeManifest();
  manifest.firmware.reset();

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_EQ(result.verdict, UpdateGateVerdict::kIncompatible);
}

// A firmware-only bundle installed onto gateware older than the firmware can
// drive.
TEST(UpdateGate, RefusesFirmwareThatNeedsNewerGatewareThanTheDeviceHas) {
  UpdateManifest manifest = MakeManifest();
  manifest.compatibility.minimum_register_map_version = 2;

  UpdateGateInput input = MakeInput();
  input.device.register_map_version = 1;

  const UpdateGateResult result = CheckUpdateGate(manifest, input);

  EXPECT_FALSE(result.allowed());
  EXPECT_EQ(result.verdict, UpdateGateVerdict::kIncompatible);
}

// A device whose FPGA never answered is an ordinary state, and refusing to
// repair its firmware because of it would be refusing help to the device
// that most needs it.
TEST(UpdateGate, DoesNotUseTheGatewareCheckWhenNoGatewareAnswered) {
  UpdateManifest manifest = MakeManifest();
  manifest.compatibility.minimum_register_map_version = 2;

  UpdateGateInput input = MakeInput();
  input.device.gateware_present = false;
  input.device.register_map_version = 0;

  const UpdateGateResult result = CheckUpdateGate(manifest, input);

  EXPECT_TRUE(result.allowed());
}

TEST(UpdateGate, SupportedRangesAreRangesAndNotSingleValues) {
  EXPECT_TRUE(ProtocolVersionIsSupported(kProtocolVersionMinimum));
  EXPECT_TRUE(ProtocolVersionIsSupported(kProtocolVersionMaximum));
  EXPECT_FALSE(ProtocolVersionIsSupported(kProtocolVersionMaximum + 1));

  // Firmware predating the field reports zero, which is not a version and
  // must not be treated as one.
  EXPECT_FALSE(ProtocolVersionIsSupported(kProtocolVersionUnknown));

  EXPECT_TRUE(RegisterMapVersionIsSupported(kRegisterMapVersionMinimum));
  EXPECT_TRUE(RegisterMapVersionIsSupported(kRegisterMapVersionMaximum));
  EXPECT_FALSE(RegisterMapVersionIsSupported(kRegisterMapVersionMaximum + 1));
}

}  // namespace
}  // namespace ddd::capture
