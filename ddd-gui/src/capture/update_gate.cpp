/************************************************************************

    update_gate.cpp

    Whether this bundle may be installed on this device by this build
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_gate.h"

#include <optional>

#include "wire_protocol.h"

namespace ddd::capture {

bool ProtocolVersionIsSupported(int version) {
  return version >= kProtocolVersionMinimum &&
         version <= kProtocolVersionMaximum;
}

bool RegisterMapVersionIsSupported(int version) {
  return version >= kRegisterMapVersionMinimum &&
         version <= kRegisterMapVersionMaximum;
}

UpdateGateResult CheckUpdateGate(const UpdateManifest& manifest,
                                 const UpdateGateInput& input) {
  UpdateGateResult result;

  const auto refuse = [&result](UpdateGateVerdict verdict, std::string reason) {
    // The application-too-old verdict outranks the others, because it is the
    // one with a next step: everything else says "this bundle is not for
    // this device", and that one says "update the application first".
    if (result.verdict != UpdateGateVerdict::kApplicationTooOld) {
      result.verdict = verdict;
    }
    result.reasons.push_back(std::move(reason));
  };

  // The schema version first, and refused rather than read charitably. A
  // manifest from the future may mean something different by a field of the
  // same name, and reading the fields it recognises and ignoring the rest is
  // how a device gets flashed with something nobody described.
  if (manifest.manifest_version != kUpdateManifestVersion) {
    refuse(UpdateGateVerdict::kApplicationTooOld,
           "This update file was made for a newer version of the "
           "application. Update the application first.");
    return result;
  }

  // A rollback bundle is refused here and accepted by CheckRollbackGate, and
  // the refusal comes before everything else because it is about what the
  // file is rather than about what it contains: a rollback carries firmware
  // and gateware exactly as an update does, so every check below it would
  // pass or fail on the wrong question.
  if (manifest.purpose == UpdatePurpose::kRollback) {
    refuse(UpdateGateVerdict::kIncompatible,
           "This file takes a device back to the original Duplicator firmware "
           "rather than updating it. Use Tools \u25b8 Firmware \u25b8 Legacy "
           "\u25b8 Roll back to legacy firmware\u2026");
    return result;
  }

  if (!manifest.firmware.has_value() && !manifest.gateware.has_value()) {
    // A set carrying only the provisioning gateware is a real file with a real
    // purpose, and it is not this one: those vectors are played through a JTAG
    // cable by the bring-up wizard, not sent to a running device. Named rather
    // than dismissed as empty, so that somebody who has chosen the right file
    // in the wrong window is told which window.
    refuse(UpdateGateVerdict::kIncompatible,
           manifest.provisioning.has_value()
               ? "This file carries only the provisioning gateware, which is "
                 "programmed through a JTAG cable rather than installed from "
                 "here. Use Tools ▸ Firmware ▸ Legacy ▸ Bring up a new or "
                 "legacy board…"
               : "This update file contains nothing to install.");
    return result;
  }

  if (!input.device_attached) {
    refuse(UpdateGateVerdict::kIncompatible,
           "No Domesday Duplicator is attached.");
    return result;
  }

  // Legacy firmware has no update agent, and that is a fact about the device
  // rather than about the file: nothing this application can send it would be
  // received, whatever the bundle carries. Refused here so that the refusal
  // is machine-checked in the one place every install passes through, and so
  // that no part of the update path ever opens a device it cannot drive.
  if (input.device_personality == DevicePersonality::kLegacy) {
    refuse(UpdateGateVerdict::kIncompatible,
           "This device is running the original Duplicator firmware, which "
           "has no way to install an update. Its firmware and gateware have "
           "to be programmed directly before updates can be installed from "
           "here.");
    return result;
  }

  // A device with no working firmware can only be repaired by a bundle that
  // carries firmware. Refused here rather than discovered part way through
  // the recovery, because "nothing to install" is a fact about the file and
  // the device that is known before the first byte moves.
  if (input.device_personality == DevicePersonality::kRecovery &&
      !manifest.firmware.has_value()) {
    refuse(UpdateGateVerdict::kIncompatible,
           "This device has no working firmware, and this update file does "
           "not contain any. Choose an update that includes firmware.");
    return result;
  }

  // The minimum application version. Enforced rather than advisory: this is
  // the check that stops a user driving the device past what the application
  // driving it understands.
  if (!manifest.compatibility.minimum_application_version.empty()) {
    const std::optional<int> comparison = CompareDottedVersions(
        input.application_version,
        manifest.compatibility.minimum_application_version);

    if (!comparison.has_value()) {
      // One side is not a dotted version — a commit hash, "unknown", or a
      // development build. Neither ordering is available, so neither is
      // asserted. Saying "your application is new enough" on the strength of
      // a comparison that could not be made is exactly the silent failure
      // this whole mechanism exists to avoid.
      result.reasons.push_back(
          "This build of the application is not a numbered release, so its "
          "age could not be checked against this update.");
    } else if (*comparison < 0) {
      refuse(UpdateGateVerdict::kApplicationTooOld,
             "This update needs application version " +
                 manifest.compatibility.minimum_application_version +
                 " or newer. Update the application first.");
    }
  }

  // What each payload will make the device speak once it is installed. A
  // firmware whose protocol this build does not know is a device this build
  // could not talk to afterwards — including to tell the user what had
  // happened.
  if (manifest.firmware.has_value()) {
    const int64_t version = manifest.firmware->interface_version;
    if (version > kProtocolVersionMaximum) {
      refuse(UpdateGateVerdict::kApplicationTooOld,
             "The firmware in this update speaks a newer protocol than this "
             "application understands. Update the application first.");
    } else if (version < kProtocolVersionMinimum) {
      refuse(UpdateGateVerdict::kIncompatible,
             "The firmware in this update is older than this application can "
             "work with.");
    }
  }

  // Whether this device can take a gateware update at all.
  //
  // The route to the EPCS runs through the gateware's own flash bridge, so a
  // device below map version 2 has no route: the writes would go nowhere and
  // the readback would be whatever the flash happened to hold. Checked here
  // rather than discovered at the first chunk, because it is a fact about
  // this device and this file that is known before anything moves.
  //
  // Skipped for a device in recovery, which has no firmware running and
  // therefore nothing to read a register with. It is asked again when the
  // device comes back, by the firmware that has just been written to it.
  if (manifest.gateware.has_value() &&
      input.device_personality == DevicePersonality::kApplication) {
    if (!input.device.gateware_present) {
      refuse(UpdateGateVerdict::kIncompatible,
             "This device's FPGA is not answering, so its gateware cannot be "
             "updated from here. Reconnect the device, and if it still does "
             "not answer, the bench procedure will program it.");
    } else if (!input.device.GatewareCanBeUpdated()) {
      refuse(UpdateGateVerdict::kIncompatible,
             "This device's gateware predates the update mechanism and cannot "
             "replace itself. It has to be programmed once with the bench "
             "procedure before gateware updates can be installed from here.");
    }
  }

  if (manifest.gateware.has_value()) {
    const int64_t version = manifest.gateware->interface_version;
    if (version > kRegisterMapVersionMaximum) {
      refuse(UpdateGateVerdict::kApplicationTooOld,
             "The gateware in this update uses a newer register map than this "
             "application understands. Update the application first.");
    } else if (version < kRegisterMapVersionMinimum) {
      refuse(UpdateGateVerdict::kIncompatible,
             "The gateware in this update is older than this application can "
             "work with.");
    }
  }

  // A firmware-only bundle installed onto older gateware: the firmware
  // states the oldest register map it can drive, and the device says which
  // one it has. Checked only when the gateware actually answered — a device
  // whose FPGA is unconfigured is an ordinary state, and refusing to repair
  // its firmware because of it would be refusing help to the device that
  // most needs it.
  if (manifest.compatibility.minimum_register_map_version > 0 &&
      input.device.gateware_present &&
      input.device.register_map_version <
          manifest.compatibility.minimum_register_map_version) {
    refuse(UpdateGateVerdict::kIncompatible,
           "The firmware in this update needs newer gateware than this device "
           "has. Install an update that carries both.");
  }

  return result;
}

UpdateGateResult CheckRollbackGate(const UpdateManifest& manifest,
                                   const UpdateGateInput& input) {
  UpdateGateResult result;

  const auto refuse = [&result](UpdateGateVerdict verdict, std::string reason) {
    if (result.verdict != UpdateGateVerdict::kApplicationTooOld) {
      result.verdict = verdict;
    }
    result.reasons.push_back(std::move(reason));
  };

  if (manifest.manifest_version != kUpdateManifestVersion) {
    refuse(UpdateGateVerdict::kApplicationTooOld,
           "This file was made for a newer version of the application. Update "
           "the application first.");
    return result;
  }

  // The purpose is what makes the age comparisons below unnecessary, so it is
  // required rather than assumed. An ordinary update bundle offered here is
  // named, not dismissed: somebody has chosen a real file in the wrong window.
  if (manifest.purpose != UpdatePurpose::kRollback) {
    refuse(UpdateGateVerdict::kIncompatible,
           "This is an ordinary update file rather than a legacy rollback. Use "
           "Tools \u25b8 Firmware \u25b8 Update firmware\u2026");
    return result;
  }

  // Both halves, or neither. A rollback that installed the legacy firmware
  // over modern gateware would leave the one pairing this whole plan is
  // ordered to avoid — legacy firmware driving CTL_07 into gateware driving
  // the same net — so a bundle that cannot complete the job must not start
  // it.
  if (!manifest.firmware.has_value() ||
      !manifest.factory_gateware.has_value()) {
    refuse(UpdateGateVerdict::kIncompatible,
           "A rollback file has to carry both the legacy firmware and the "
           "legacy gateware, and this one does not.");
    return result;
  }

  if (!input.device_attached) {
    refuse(UpdateGateVerdict::kIncompatible,
           "No Domesday Duplicator is attached.");
    return result;
  }

  // Already there. Said plainly rather than refused as an error, because a
  // user who has arrived here with a legacy device has got what they came for.
  if (input.device_personality == DevicePersonality::kLegacy) {
    refuse(UpdateGateVerdict::kIncompatible,
           "This device is already running the original Duplicator firmware.");
    return result;
  }

  // The device does its own writing, so it has to be running firmware that
  // can. A device in recovery has none, and one that is mid-bring-up is not a
  // device anybody means to roll back.
  if (input.device_personality != DevicePersonality::kApplication) {
    refuse(UpdateGateVerdict::kIncompatible,
           "This device is not running its own firmware, so it cannot write "
           "anything to itself. Finish bringing it up first.");
    return result;
  }

  // And the flash bridge, because the legacy gateware goes to the EPCS
  // through it. This is the one refusal a user is most likely to meet: it is
  // what a device whose FPGA is unconfigured looks like.
  if (!input.device.gateware_present) {
    refuse(UpdateGateVerdict::kIncompatible,
           "This device's FPGA is not answering, so its gateware cannot be "
           "replaced from here. Reconnect the device and try again.");
  } else if (!input.device.GatewareCanBeUpdated()) {
    refuse(UpdateGateVerdict::kIncompatible,
           "This device's gateware predates the flash bridge, so it cannot "
           "replace itself. There is nothing to roll back from.");
  }

  // Deliberately not checked: minimum_application_version, and the firmware's
  // and gateware's interface versions. Every one of them would refuse a valid
  // rollback for being old, which is the property that defines the file. What
  // stands in their place is the purpose above, the pair of payloads, and the
  // device state — all of which are checked, and none of which a rollback
  // bundle can be malformed enough to pass.
  return result;
}

}  // namespace ddd::capture
