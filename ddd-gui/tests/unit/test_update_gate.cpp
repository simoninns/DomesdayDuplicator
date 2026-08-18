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
  manifest.compatibility.minimum_register_map_version =
      kRegisterMapVersionMinimum;
  manifest.compatibility.epcs_layout_version = kEpcsBootBlockLayoutVersion;
  return manifest;
}

// A gateware payload, for the cases that need a bundle carrying one.
UpdateComponent GatewareComponent() {
  UpdateComponent gateware;
  gateware.file = "gateware-app.rpd";
  gateware.length = 2048;
  gateware.identity = "0123abcd";
  gateware.interface_version = kRegisterMapVersionMaximum;
  return gateware;
}

UpdateGateInput MakeInput() {
  UpdateGateInput input;
  input.application_version = "1.4.0";
  input.device_attached = true;
  input.device.protocol_version = 1;
  input.device.gateware_present = true;

  input.device.register_map_version = kRegisterMapVersionMaximum;
  input.device.image_role = kImageRoleApplication;
  return input;
}

// A file carrying only the bring-up payloads chosen in the update window. It
// is a real file with a real use and this is not the window for it, so the
// refusal names the one that is rather than calling the file empty.
TEST(UpdateGate, RefusesABringUpOnlyFileAndSaysWhereItBelongs) {
  UpdateManifest manifest = MakeManifest();
  manifest.firmware.reset();

  UpdateComponent provisioning;
  provisioning.file = "gateware-provisioning.svf";
  provisioning.length = 18400000;
  provisioning.identity = "0123abcd";
  provisioning.interface_version = 2;
  manifest.provisioning = provisioning;

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_FALSE(result.allowed());
  ASSERT_FALSE(result.reasons.empty());
  EXPECT_NE(result.reasons.front().find("Bring up"), std::string::npos)
      << result.reasons.front();
}

// The provisioning component beside firmware changes nothing: the ordinary
// path installs the firmware and leaves the vectors alone, which is what a
// build that predates the component would do as well.
TEST(UpdateGate, IgnoresAProvisioningComponentBesideFirmware) {
  UpdateManifest manifest = MakeManifest();

  UpdateComponent provisioning;
  provisioning.file = "gateware-provisioning.svf";
  provisioning.length = 18400000;
  provisioning.identity = "0123abcd";
  provisioning.interface_version = 2;
  manifest.provisioning = provisioning;

  EXPECT_TRUE(CheckUpdateGate(manifest, MakeInput()).allowed());
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

// Installing an older release is a legitimate thing to want, and a downgrade
// passes through the same gate as an upgrade rather than through a second,
// more forgiving one.
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

// --- A device with no firmware ---------------------------------------------

// It has no identity to report, so every check that compares against one has
// nothing to compare and correctly stays quiet. What must not happen is a
// refusal built out of the absence.
TEST(UpdateGate, ADeviceInRecoveryPassesTheChecksThatNeedAnIdentity) {
  const UpdateManifest manifest = MakeManifest();

  UpdateGateInput input = MakeInput();
  input.device_personality = DevicePersonality::kRecovery;
  input.device = DeviceIdentity{};

  const UpdateGateResult result = CheckUpdateGate(manifest, input);

  EXPECT_TRUE(result.allowed());
}

// The one check that only applies there: nothing on the device can write
// gateware, because there is nothing running on it at all.
TEST(UpdateGate, ADeviceInRecoveryRefusesABundleWithNoFirmware) {
  UpdateManifest manifest = MakeManifest();
  manifest.firmware.reset();
  manifest.gateware = GatewareComponent();

  UpdateGateInput input = MakeInput();
  input.device_personality = DevicePersonality::kRecovery;
  input.device = DeviceIdentity{};

  const UpdateGateResult result = CheckUpdateGate(manifest, input);

  EXPECT_FALSE(result.allowed());
  EXPECT_EQ(result.verdict, UpdateGateVerdict::kIncompatible);
  ASSERT_FALSE(result.reasons.empty());
  EXPECT_NE(result.reasons.front().find("does not contain any"),
            std::string::npos)
      << result.reasons.front();
}

// --- A device running the legacy firmware ----------------------------------

// The one refusal here that no bundle can satisfy. That firmware predates the
// update protocol, so there is nothing on the device to receive an update —
// and the gate is where that has to be said, because it is the one place
// every install passes through before anything opens the device.
TEST(UpdateGate, ALegacyDeviceIsRefusedWhateverTheBundleCarries) {
  UpdateManifest manifest = MakeManifest();
  manifest.gateware = GatewareComponent();

  UpdateGateInput input = MakeInput();
  input.device_personality = DevicePersonality::kLegacy;
  input.device = DeviceIdentity{};

  const UpdateGateResult result = CheckUpdateGate(manifest, input);

  EXPECT_FALSE(result.allowed());
  EXPECT_EQ(result.verdict, UpdateGateVerdict::kIncompatible);
  ASSERT_FALSE(result.reasons.empty());
  EXPECT_NE(result.reasons.front().find("original Duplicator firmware"),
            std::string::npos)
      << result.reasons.front();
}

// And the refusal is about the device, not about anything missing from the
// file: the bundle that repairs a device with no firmware at all is refused
// here too.
TEST(UpdateGate, ALegacyDeviceIsRefusedAFirmwareOnlyBundle) {
  UpdateManifest manifest = MakeManifest();

  UpdateGateInput input = MakeInput();
  input.device_personality = DevicePersonality::kLegacy;

  EXPECT_FALSE(CheckUpdateGate(manifest, input).allowed());
}

// The same bundle on a working device is fine: a gateware-only update needs
// no firmware, and this refusal is about the device rather than the file.
TEST(UpdateGate, AGatewareOnlyBundleIsStillFineOnAWorkingDevice) {
  UpdateManifest manifest = MakeManifest();
  manifest.firmware.reset();
  manifest.gateware = GatewareComponent();

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_TRUE(result.allowed());
}

// --- Whether this device can take a gateware update at all -----------------

// The route to the configuration flash runs through the gateware's own flash
// bridge, so an FPGA that is not answering has no route at all. That is a fact
// about the device, known before a byte moves, and the refusal names the one
// thing that fixes it.
TEST(UpdateGate, RefusesGatewareForADeviceWhoseFpgaDidNotAnswer) {
  UpdateManifest manifest = MakeManifest();
  manifest.gateware = GatewareComponent();

  UpdateGateInput input = MakeInput();
  input.device.gateware_present = false;
  input.device.register_map_version = 0;

  const UpdateGateResult result = CheckUpdateGate(manifest, input);

  EXPECT_FALSE(result.allowed());
  EXPECT_EQ(result.verdict, UpdateGateVerdict::kIncompatible);
}

// The same device takes a firmware-only bundle without complaint. The check
// is about what the bundle asks the device to do, not about the device: an
// unconfigured FPGA is an ordinary state, and refusing to repair the firmware
// over it would refuse help to the device that most needs it.
TEST(UpdateGate, AllowsFirmwareForADeviceWhoseFpgaDidNotAnswer) {
  UpdateManifest manifest = MakeManifest();
  manifest.compatibility.minimum_register_map_version = 0;

  UpdateGateInput input = MakeInput();
  input.device.gateware_present = false;
  input.device.register_map_version = 0;

  const UpdateGateResult result = CheckUpdateGate(manifest, input);

  EXPECT_TRUE(result.allowed());
}

// --- The EPCS boot block layout --------------------------------------------

// The factory image's boot logic is frozen at provisioning time, so a bundle
// laid out for a boot block this build cannot describe is refused before a
// byte moves: a boot block the resident logic cannot parse is the one thing a
// later update cannot repair.
TEST(UpdateGate, RefusesAGatewareLaidOutForANewerBootBlock) {
  UpdateManifest manifest = MakeManifest();
  manifest.gateware = GatewareComponent();
  manifest.compatibility.epcs_layout_version = kEpcsBootBlockLayoutVersion + 1;

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_FALSE(result.allowed());
  EXPECT_EQ(result.verdict, UpdateGateVerdict::kApplicationTooOld);
}

// Zero is a bundle that declares nothing rather than one claiming layout
// zero, and a bundle that declares nothing is not refused for it.
TEST(UpdateGate, DoesNotGateAGatewareThatDeclaresNoBootBlockLayout) {
  UpdateManifest manifest = MakeManifest();
  manifest.gateware = GatewareComponent();
  manifest.compatibility.epcs_layout_version = 0;

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_TRUE(result.allowed());
}

// A firmware-only bundle is not laid out for anything, so the layout it
// declares is not the gate's business.
TEST(UpdateGate, DoesNotGateAFirmwareOnlyBundleOnTheBootBlockLayout) {
  UpdateManifest manifest = MakeManifest();
  manifest.compatibility.epcs_layout_version = kEpcsBootBlockLayoutVersion + 1;

  const UpdateGateResult result = CheckUpdateGate(manifest, MakeInput());

  EXPECT_TRUE(result.allowed());
}

// A unit running its factory gateware is exactly the unit a gateware update
// repairs: the factory image carries the bridge for that reason, and
// refusing it would refuse help to the device that most needs it.
TEST(UpdateGate, AllowsGatewareForADeviceRunningItsRecoveryGateware) {
  UpdateManifest manifest = MakeManifest();
  manifest.gateware = GatewareComponent();

  UpdateGateInput input = MakeInput();
  input.device.image_role = kImageRoleFactory;
  ASSERT_TRUE(input.device.GatewareIsRecovery());

  const UpdateGateResult result = CheckUpdateGate(manifest, input);

  EXPECT_TRUE(result.allowed());
}

// A device with no firmware has nothing to read a register with, so the
// gateware check has nothing to check and correctly stays quiet. It is asked
// again when the device comes back, by the firmware just written to it.
TEST(UpdateGate, DoesNotAskADeviceInRecoveryAboutItsGateware) {
  UpdateManifest manifest = MakeManifest();
  manifest.gateware = GatewareComponent();

  UpdateGateInput input = MakeInput();
  input.device_personality = DevicePersonality::kRecovery;
  input.device = DeviceIdentity{};

  const UpdateGateResult result = CheckUpdateGate(manifest, input);

  EXPECT_TRUE(result.allowed());
}

TEST(UpdateGate, SupportedRangesAreRangesAndNotSingleValues) {
  EXPECT_TRUE(ProtocolVersionIsSupported(kProtocolVersionMinimum));
  EXPECT_TRUE(ProtocolVersionIsSupported(kProtocolVersionMaximum));
  EXPECT_FALSE(ProtocolVersionIsSupported(kProtocolVersionMaximum + 1));

  // A device that reports nothing at all in the field reports zero, which is
  // not a version and must not be treated as one.
  EXPECT_FALSE(ProtocolVersionIsSupported(0));

  EXPECT_TRUE(RegisterMapVersionIsSupported(kRegisterMapVersionMinimum));
  EXPECT_TRUE(RegisterMapVersionIsSupported(kRegisterMapVersionMaximum));
  EXPECT_FALSE(RegisterMapVersionIsSupported(kRegisterMapVersionMaximum + 1));
  EXPECT_FALSE(RegisterMapVersionIsSupported(kRegisterMapVersionMinimum - 1));
}

}  // namespace
}  // namespace ddd::capture
