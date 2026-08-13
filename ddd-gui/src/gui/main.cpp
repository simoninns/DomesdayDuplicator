/************************************************************************

    main.cpp

    Capture application entry point
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <QApplication>
#include <QCommandLineParser>
#include <QString>

#include "application_logger.h"
#include "logger.h"
#include "main_window.h"
#include "theme_controller.h"
#include "version.h"

int main(int argc, char* argv[]) {
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  QApplication app(argc, argv);

  // Set before anything constructs a QSettings: the identity decides which file
  // settings are read from and written to, and a QSettings built earlier would
  // quietly use a different one. The application name differs from the older
  // capture application's, so the two do not share a settings file.
  QApplication::setOrganizationName(QStringLiteral("Domesday86"));
  QApplication::setOrganizationDomain(QStringLiteral("domesday86.com"));
  QApplication::setApplicationName(QStringLiteral("ddd-gui"));

  const auto version = ddd::capture::Version();
  QApplication::setApplicationVersion(QString::fromUtf8(
      version.data(), static_cast<qsizetype>(version.size())));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Capture application for the Domesday Duplicator"));
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption debug_option(
      {QStringLiteral("d"), QStringLiteral("debug")},
      QStringLiteral("Log debug-level diagnostics and show the Log panel."));
  parser.addOption(debug_option);
  parser.process(app);

  ddd::gui::ApplicationLogger logger;
  if (parser.isSet(debug_option)) {
    logger.SetMinimumLevel(ddd::capture::LogLevel::kDebug);
  }

  ddd::gui::ThemeController theme_controller(&app);
  theme_controller.Initialize();

  ddd::gui::MainWindow window(&theme_controller, &logger);
  if (parser.isSet(debug_option)) {
    window.ShowLogPanel();
  }
  window.show();

  logger.Info("Capture application started.");

  return QApplication::exec();
}
