/************************************************************************

    update_cli.h

    ddd-update: installing a bundle from a shell
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace ddd::capture {

// The whole of `ddd-update`, apart from main().
//
// The updater engine is Qt-free, so this drives the *identical* code path the
// application's update dialog does: verify the bundle, run the compatibility
// gate, program the device, reset it, re-read its identity, and exit non-zero
// on any failure. Two things follow from it being the same code path rather
// than a parallel implementation. It is the scriptable half of the bench
// procedures in TESTING.md, and — more importantly — a bug found here is a
// bug in the code the application runs, not in a test harness that resembles
// it.
//
// `dev-bundle.sh && ddd-update` is the whole edit-to-running-device loop.
//
// In a function rather than in main(), for the reason main() should hold
// nothing worth testing: the exit code and the messages are the entire
// interface, and both can then be checked without a built binary, a shell or
// a device.

// What ddd-update returns to the shell. Distinct codes, because a script
// driving a bench procedure wants to know whether it has a bad file or a bad
// device.
enum UpdateCliExit {
  kUpdateCliSuccess = 0,

  // The command line itself was wrong. Usage was printed.
  kUpdateCliUsage = 2,

  // The bundle could not be read, verified, or is not for this device or
  // this build. Nothing was sent to any device.
  kUpdateCliBundle = 3,

  // No device to update.
  kUpdateCliNoDevice = 4,

  // The update was attempted and did not complete.
  kUpdateCliFailed = 5,
};

// Parsed arguments, so that the parsing can be tested apart from the
// updating.
struct UpdateCliOptions {
  std::string bundle_path;

  // --device <path>, to pick between several attached devices.
  std::string device_path;

  // --dev-update-key: accept a bundle signed with the committed development
  // key, whose secret half is public. Required per invocation, never
  // remembered, and the interface says so every time it is used.
  bool accept_development_key = false;

  // --dry-run: verify the bundle and run the compatibility gate, then stop
  // before a single byte is sent. This is what a script checks a bundle with,
  // and it is the only mode that touches no device state at all.
  bool dry_run = false;

  bool show_help = false;

  // Set when parsing failed; already written for a human.
  std::string problem;
};

UpdateCliOptions ParseUpdateCliOptions(const std::vector<std::string>& args);

// What ddd-update prints when asked how to use it.
std::string UpdateCliUsage();

// Run it. `out` takes the progress and the result; `error` takes the
// problems.
int RunUpdateCli(const std::vector<std::string>& args, std::ostream& out,
                 std::ostream& error);

}  // namespace ddd::capture
