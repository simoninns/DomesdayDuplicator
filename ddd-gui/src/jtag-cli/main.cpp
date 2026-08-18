/************************************************************************

    main.cpp

    ddd-jtag: playing a programming file through the cable from a shell
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <iostream>
#include <string>
#include <vector>

#include "jtag_cli.h"

// A main() and nothing else. Everything worth testing is in
// ddd::capture::RunJtagCli, which links no Qt — the same engine code the
// application's provisioning flow will drive, so a bug found here is a bug in
// what the application runs rather than in something that resembles it.
int main(int argc, char* argv[]) {
  const std::vector<std::string> args(argv + 1, argv + argc);

  return ddd::capture::RunJtagCli(args, std::cout, std::cerr);
}
