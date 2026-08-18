/************************************************************************

    update_cli.cpp

    ddd-update: installing a bundle from a shell
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "update_cli.h"

#include <fstream>
#include <ostream>
#include <vector>

#include "device_programmer.h"
#include "device_recovery.h"
#include "device_updater.h"
#include "logger.h"
#include "update_bundle.h"
#include "update_gate.h"
#include "update_key.h"
#include "update_orchestrator.h"
#include "usb_device.h"
#include "usb_device_info.h"
#include "version.h"

namespace ddd::capture {
namespace {

// Read a whole file. Returns false for anything that is not a readable
// regular file, which is as much detail as a caller needs: the next thing it
// says is "that file could not be read".
bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return false;
  }

  const std::streamoff size = file.tellg();
  if (size < 0) {
    return false;
  }
  file.seekg(0, std::ios::beg);

  out.resize(static_cast<size_t>(size));
  if (size > 0 && !file.read(reinterpret_cast<char*>(out.data()),
                             static_cast<std::streamsize>(size))) {
    return false;
  }
  return true;
}

// A logger that writes to a stream, so the command-line tool says what the
// engine says rather than swallowing it.
class StreamLogger : public ILogger {
 public:
  explicit StreamLogger(std::ostream& out) : out_(out) {}

  void Log(LogLevel level, std::string_view message) override {
    // Debug is dropped: it is the transport's running commentary, and a tool
    // whose output is read by a person should not open with it.
    if (level == LogLevel::kDebug) {
      return;
    }
    out_ << message << "\n";
  }

 private:
  std::ostream& out_;
};

}  // namespace

std::string UpdateCliUsage() {
  return "ddd-update — install a Domesday Duplicator update bundle\n"
         "\n"
         "Usage:\n"
         "  ddd-update [options] <bundle.dddfw>\n"
         "\n"
         "Options:\n"
         "  --device <path>       Update the device at this path, when several "
         "are attached\n"
         "  --dev-update-key      Accept a bundle signed with the development "
         "key, whose\n"
         "                        secret half is public. Proves the file is "
         "well "
         "formed and\n"
         "                        nothing about where it came from\n"
         "  --dry-run             Verify the bundle and check it against the "
         "device, then\n"
         "                        stop without sending anything\n"
         "  --help                Show this text\n"
         "\n"
         "A device with no working firmware — one that has never been "
         "programmed, or one\n"
         "whose update was interrupted — enumerates as the FX3 boot ROM and "
         "is programmed\n"
         "by this same command, from this same bundle. There is nothing "
         "different to run.\n"
         "\n"
         "Exit codes: 0 success, 2 usage, 3 bundle, 4 no device, 5 update "
         "failed.\n";
}

UpdateCliOptions ParseUpdateCliOptions(const std::vector<std::string>& args) {
  UpdateCliOptions options;

  for (size_t index = 0; index < args.size(); ++index) {
    const std::string& argument = args[index];

    if (argument == "--help" || argument == "-h") {
      options.show_help = true;
      return options;
    }

    if (argument == "--dev-update-key") {
      options.accept_development_key = true;
      continue;
    }

    if (argument == "--dry-run") {
      options.dry_run = true;
      continue;
    }

    if (argument == "--device") {
      if (index + 1 >= args.size()) {
        options.problem = "--device needs a device path after it.";
        return options;
      }
      options.device_path = args[++index];
      continue;
    }

    if (!argument.empty() && argument.front() == '-') {
      options.problem = "Unknown option: " + argument;
      return options;
    }

    if (!options.bundle_path.empty()) {
      options.problem = "Only one bundle can be installed at a time.";
      return options;
    }
    options.bundle_path = argument;
  }

  if (options.bundle_path.empty()) {
    options.problem = "No bundle given.";
  }

  return options;
}

int RunUpdateCli(const std::vector<std::string>& args, std::ostream& out,
                 std::ostream& error) {
  const UpdateCliOptions options = ParseUpdateCliOptions(args);

  if (options.show_help) {
    out << UpdateCliUsage();
    return kUpdateCliSuccess;
  }

  if (!options.problem.empty()) {
    error << options.problem << "\n\n" << UpdateCliUsage();
    return kUpdateCliUsage;
  }

  std::vector<uint8_t> archive;
  if (!ReadWholeFile(options.bundle_path, archive)) {
    error << "Could not read " << options.bundle_path << "\n";
    return kUpdateCliBundle;
  }

  UpdateKeyPolicy policy = DefaultUpdateKeyPolicy();
  if (options.accept_development_key) {
    policy.accept_development_key = true;
  }

  std::string bundle_error;
  const std::optional<UpdateBundle> bundle =
      OpenUpdateBundleForPolicy(archive, policy, &bundle_error);
  if (!bundle.has_value()) {
    error << bundle_error << "\n";
    return kUpdateCliBundle;
  }

  out << "Bundle " << bundle->manifest.version << " ("
      << bundle->manifest.commit << "), signed by the "
      << (bundle->manifest.channel == UpdateChannel::kRelease ? "release"
                                                              : "development")
      << " key\n";

  // A development signature proves the file is well formed and proves
  // nothing at all about where it came from. Said every time, at the top,
  // rather than left for the reader to infer from the channel name.
  if (bundle->manifest.channel == UpdateChannel::kDevelopment) {
    out << "  This is a development bundle. Its signing key's secret half is "
           "public,\n"
           "  so the signature proves the file's format and not its origin.\n";
  }

  if (!bundle->manifest.release_notes.empty()) {
    out << "  " << bundle->manifest.release_notes << "\n";
  }

  StreamLogger logger(out);

  std::unique_ptr<IUsbDevice> usb = MakeUsbDevice(&logger);
  if (usb == nullptr) {
    error << "The USB backend could not be started.\n";
    return kUpdateCliNoDevice;
  }

  std::vector<DeviceInfo> devices;
  if (!usb->Enumerate(devices)) {
    error << "The attached devices could not be listed.\n";
    return kUpdateCliNoDevice;
  }

  // Any personality, because a device with no working firmware is one of the
  // two this tool exists to program — the other being a device that has one
  // and wants a newer.
  const DeviceInfo* const selected =
      SelectDevice(devices, options.device_path, DeviceSelection::kAny);
  if (selected == nullptr) {
    error << "No Domesday Duplicator is attached.\n";
    return kUpdateCliNoDevice;
  }

  const bool recovery = selected->personality == DevicePersonality::kRecovery;

  // Said here rather than left to the gate, because the gate is consulted
  // after the device has been opened and its identity read — and a device
  // running the legacy firmware answers neither. Nothing is asked of it.
  if (selected->personality == DevicePersonality::kLegacy) {
    error << "The device at " << selected->path
          << " is running the original Duplicator firmware, which predates "
             "the update mechanism.\nIt has no way to install an update: its "
             "firmware and gateware have to be programmed directly.\n";
    return kUpdateCliNoDevice;
  }

  if (selected->personality == DevicePersonality::kFlashProgrammer) {
    error << "The device at " << selected->path
          << " is running the Cypress flash programmer, left behind by an "
             "fx3-programmer session.\nPower cycle it and try again.\n";
    return kUpdateCliNoDevice;
  }

  out << "Device at " << selected->path << ": "
      << (recovery ? "recovery mode — no working firmware"
                   : selected->product_string)
      << "\n";

  // A device in recovery has no identity to read: there is nothing running on
  // it to answer. The gate is told which it is and makes the checks that
  // still apply.
  DeviceIdentity identity;
  std::unique_ptr<IDeviceUpdater> updater;

  if (!recovery) {
    updater = MakeDeviceUpdater(*usb, selected->path, &logger);
    if (updater == nullptr) {
      error << "The device could not be opened.\n";
      return kUpdateCliNoDevice;
    }

    const std::optional<DeviceIdentity> read = updater->ReadIdentity();
    if (!read.has_value()) {
      error << "The device did not answer.\n";
      return kUpdateCliNoDevice;
    }
    identity = *read;
  }

  UpdateGateInput gate_input;
  gate_input.application_version = std::string(Version());
  gate_input.device_attached = true;
  gate_input.device = identity;
  gate_input.device_personality = selected->personality;

  const UpdateGateResult gate = CheckUpdateGate(bundle->manifest, gate_input);
  for (const std::string& reason : gate.reasons) {
    // Reported on the error stream even when the gate allows the install,
    // because the reasons that survive an allowed verdict are the ones a
    // script's operator ought to see — "this build is not a numbered release,
    // so its age could not be checked" is a caveat, not a passing remark.
    error << reason << "\n";
  }
  if (!gate.allowed()) {
    return kUpdateCliBundle;
  }

  if (options.dry_run) {
    out << "The bundle verifies and can be "
        << (recovery ? "programmed onto this device"
                     : "installed on this device")
        << ". Nothing was sent.\n";
    return kUpdateCliSuccess;
  }

  if (recovery) {
    out << "This device has no working firmware. Programming it from this "
           "bundle.\n";
  }
  out << "Installing. Leave the device plugged in and powered.\n";

  // One line per stage rather than one per poll: this output is read in a
  // terminal and in a CI log, and neither is improved by a thousand
  // percentages.
  UpdateStage last_stage = UpdateStage::kFailed;
  const UpdateProgressCallback report = [&out,
                                         &last_stage](const UpdateProgress& s) {
    if (s.stage == last_stage) {
      return;
    }
    last_stage = s.stage;
    out << UpdateStageName(s.stage) << ": " << s.message << "\n";
  };

  UpdateOutcome outcome;

  if (recovery) {
    // The recovery path and the ordinary path end in the same place: the
    // installer's prelude wakes the device with this bundle's own firmware
    // and then runs the very same orchestrator over it.
    DeviceAccess access;
    const std::string recovery_path = selected->path;
    access.open_programmer = [usb = usb.get(), recovery_path, &logger] {
      return MakeDeviceProgrammer(*usb, recovery_path, &logger);
    };
    access.open_updater = [usb = usb.get(),
                           &logger](const std::string& device_path) {
      return MakeDeviceUpdater(*usb, device_path, &logger);
    };

    RecoveryInstaller installer(std::move(access), &logger);
    installer.SetProgressCallback(report);
    outcome = installer.Run(*bundle);
  } else {
    UpdateOrchestrator orchestrator(*updater, &logger);
    orchestrator.SetProgressCallback(report);
    outcome = orchestrator.Run(*bundle);
  }

  if (!outcome.succeeded) {
    error << outcome.problem << "\n";
    return kUpdateCliFailed;
  }

  out << "Update complete. The device now reports "
      << outcome.identity.product_string;
  if (!outcome.identity.gateware_commit.empty()) {
    out << " with gateware " << outcome.identity.gateware_commit;
  }
  out << ".\n";

  return kUpdateCliSuccess;
}

}  // namespace ddd::capture
