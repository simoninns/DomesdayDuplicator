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

  if (!manifest.firmware.has_value() && !manifest.gateware.has_value()) {
    // A file carrying only the bring-up gateware is a real file with a real
    // use, and it is not this one: those vectors are played through a JTAG
    // cable by the bring-up wizard, not sent to a running device. Named rather
    // than dismissed as empty, so that somebody who has chosen the right file
    // in the wrong window is told which window.
    refuse(UpdateGateVerdict::kIncompatible,
           manifest.provisioning.has_value()
               ? "This file carries only the bring-up gateware, which is "
                 "programmed through a JTAG cable rather than installed from "
                 "here. Use Tools ▸ Firmware ▸ Bring up a new or legacy "
                 "board…"
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

  // The manifest's minimum_application_version is deliberately not read here.
  //
  // It was compared against the application's own dotted release version,
  // which no longer exists: the application stamps the commit it was built
  // from, the same as the firmware and the gateware, and a commit orders
  // nothing. Against a non-dotted version the comparison could not be made,
  // and rather than being made charitably it pushed a line into the reasons
  // shown to the user — which, now that no build has a dotted version, would
  // have appeared on every install for every user for ever.
  //
  // Nothing is lost that was being enforced. The floor has always been 0.0.0
  // (tools/release/compatibility.env), and what stops a user driving a device
  // past what the application understands is machine-to-machine and checked
  // just below: the firmware's protocol version and the gateware's register
  // map version, both against the ranges this build was compiled knowing.
  // That is also the more accurate signal — it describes what this
  // application cannot do rather than how old it is — and it is derived from
  // the sources at release time rather than being a decision to remember.
  //
  // The field itself stays in the manifest and stays published: it is
  // required by the schema, so a bundle without it would fail to parse in
  // every application already in the field.

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
  // The route to the EPCS runs through the gateware's own flash bridge, so an
  // FPGA that is not answering has no route: the writes would go nowhere and
  // the readback would be whatever the flash happened to hold. Checked here
  // rather than discovered at the first chunk, because it is a fact about
  // this device and this file that is known before anything moves.
  //
  // Skipped for a device in recovery, which has no firmware running and
  // therefore nothing to read a register with. It is asked again when the
  // device comes back, by the firmware that has just been written to it.
  if (manifest.gateware.has_value() &&
      input.device_personality == DevicePersonality::kApplication &&
      !input.device.gateware_present) {
    refuse(UpdateGateVerdict::kIncompatible,
           "This device's FPGA is not answering, so its gateware cannot be "
           "updated from here. Reconnect the device, and if it still does "
           "not answer, the bench procedure will program it.");
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

  // The EPCS boot block layout the gateware payload assumes.
  //
  // Nothing on the device reports this, so it is checked against what this
  // build knows rather than against the unit: a bundle laid out for a boot
  // block nobody here has described is refused before a byte moves. The
  // resident factory image's boot logic was frozen at provisioning time and
  // cannot be taught a new layout from the field, so a boot block it cannot
  // parse leaves the board with nothing to fall back to — the one outcome no
  // later update can repair.
  //
  // Zero means the bundle declares nothing, which is a bundle from before the
  // field existed rather than a claim about layout zero. Anything else above
  // what this build knows is a bundle from ahead of it, and there is nothing
  // below the first layout for it to be behind.
  if (manifest.gateware.has_value() &&
      manifest.compatibility.epcs_layout_version >
          kEpcsBootBlockLayoutVersion) {
    refuse(UpdateGateVerdict::kApplicationTooOld,
           "The gateware in this update uses a newer flash layout than this "
           "application understands. Update the application first.");
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

const char* UpdateGateVerdictName(UpdateGateVerdict verdict) {
  switch (verdict) {
    case UpdateGateVerdict::kAllowed:
      return "allowed";
    case UpdateGateVerdict::kApplicationTooOld:
      return "refused: this application is too old";
    case UpdateGateVerdict::kIncompatible:
      return "refused: incompatible";
  }
  return "unknown";
}

}  // namespace ddd::capture
