/************************************************************************

    jtag_cli.h

    ddd-jtag: playing a programming file through the cable from a shell
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace ddd::capture {

// The whole of `ddd-jtag`, apart from main().
//
// It exists for two jobs, and they are worth naming separately because only
// one of them involves a board.
//
// **Checking a file.** `--dry-run` parses a programming file and plays it
// into a cable that goes nowhere, reporting what it would have clocked out.
// That is a complete check of everything this application contributes — the
// parser, the TAP state machine, the vector stream — and it needs no
// hardware, so it is the one part of this path that may be run
// automatically.
//
// **Programming a board.** Without `--dry-run` the same file goes through
// the DE0-Nano's on-board USB-Blaster and lands in the FPGA's configuration
// flash. That writes flash, so it is a deliberate manual act and nothing in
// CI ever runs it (AGENTS.md §4). It is also the bench harness the
// provisioning work is proved with, which is why it reports how long the run
// took: the duration of a full provisioning pass is a number this project
// needs and cannot get any other way (TESTING.md, B-V1).
//
// In a function rather than in main(), for the reason main() should hold
// nothing worth testing: the exit codes and the messages are the whole
// interface, and both can be checked without a built binary, a shell or a
// cable.

// What ddd-jtag returns to the shell. Distinct codes, because a script
// driving a bench procedure wants to know whether it has a bad file or a
// missing cable.
enum JtagCliExit {
  kJtagCliSuccess = 0,

  // The command line itself was wrong. Usage was printed.
  kJtagCliUsage = 2,

  // The programming file could not be read or could not be understood.
  // Nothing was clocked out.
  kJtagCliFile = 3,

  // No cable, or one this application does not drive.
  kJtagCliNoCable = 4,

  // The file was played and did not finish: the device disagreed with it, or
  // the cable stopped answering, or the run was stopped.
  kJtagCliFailed = 5,
};

// Parsed arguments, so that the parsing can be tested apart from the
// programming.
struct JtagCliOptions {
  std::string svf_path;

  // --dry-run: play the file into nothing, to find out whether it is one
  // this application can play at all.
  bool dry_run = false;

  bool show_help = false;

  // Set when parsing failed; already written for a human.
  std::string problem;
};

JtagCliOptions ParseJtagCliOptions(const std::vector<std::string>& args);

// What ddd-jtag prints when asked how to use it.
std::string JtagCliUsage();

// Run it. `out` takes the progress and the result; `error` takes the
// problems.
int RunJtagCli(const std::vector<std::string>& args, std::ostream& out,
               std::ostream& error);

}  // namespace ddd::capture
