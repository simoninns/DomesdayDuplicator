/************************************************************************

    analysis_cli.cpp

    --analyse-test-data, so the integrity gate can be scripted
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "analysis_cli.h"

#include <QTextStream>
#include <filesystem>

#include "test_data_analysis.h"

namespace ddd::gui {

int RunTestDataAnalysis(const QString& file_path, QTextStream& out,
                        QTextStream& error) {
  out << "Analysing " << file_path << "\n";
  out.flush();

  const capture::TestDataAnalysis analysis =
      capture::AnalyseTestData(std::filesystem::path(file_path.toStdString()));

  const QString message = QString::fromStdString(analysis.message);

  // A verdict on the capture goes to stdout; "I could not read this" goes to
  // stderr. The distinction matters to the thing reading the output: a script
  // capturing stdout is collecting results, and a message about its own
  // arguments does not belong in that collection.
  if (analysis.outcome == capture::TestDataAnalysis::Outcome::kUnreadable) {
    error << "Error: " << message << "\n";
    error.flush();
  } else {
    out << message << "\n";
    out.flush();
  }

  return analysis.ExitCode();
}

}  // namespace ddd::gui
