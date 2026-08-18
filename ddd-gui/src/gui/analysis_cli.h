/************************************************************************

    analysis_cli.h

    --analyse-test-data, so the integrity gate can be scripted
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>

class QTextStream;

namespace ddd::gui {

// The headless half of step 4 of the capture-integrity procedure
// (TESTING.md §5). Reads a test-mode capture, checks the gateware's ramp, and
// returns a process exit code: 0 for an intact ramp, 1 for a break, 2 for a
// file that could not be analysed at all.
//
// In a file of its own rather than in main(), for the reason main() should hold
// nothing worth testing: the exit code is the whole interface here — a script
// driving the T5 gate reads nothing else — and an exit code produced inside
// main() can only be checked by running the executable, which is a test that
// needs a built binary, a shell and a platform. As a function it is an ordinary
// unit test.
//
// The read loop, the ramp check and the wording of the verdict are all in
// ddd::capture::AnalyseTestData, shared with the dialog. What is here is the
// printing and the code.
int RunTestDataAnalysis(const QString& file_path, QTextStream& out,
                        QTextStream& error);

}  // namespace ddd::gui
