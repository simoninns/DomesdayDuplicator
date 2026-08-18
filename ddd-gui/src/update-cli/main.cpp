/************************************************************************

    main.cpp

    ddd-update: installing a bundle from a shell
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <iostream>
#include <string>
#include <vector>

#include "update_cli.h"

// A main() and nothing else. Everything worth testing is in
// ddd::capture::RunUpdateCli, which links no Qt — the same engine code the
// application's update dialog drives, so a bug found here is a bug in what
// the application runs rather than in something that resembles it.
int main(int argc, char* argv[]) {
  const std::vector<std::string> args(argv + 1, argv + argc);

  return ddd::capture::RunUpdateCli(args, std::cout, std::cerr);
}
