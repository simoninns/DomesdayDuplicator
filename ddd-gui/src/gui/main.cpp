/************************************************************************

    main.cpp

    Capture application entry point
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <QApplication>
#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QString>
#include <QTextStream>
#include <memory>
#include <string>
#include <utility>

#include "about_text.h"
#include "analysis_cli.h"
#include "application_logger.h"
#include "auto_capture_controller.h"
#include "capture_controller.h"
#include "logger.h"
#include "main_window.h"
#include "player_controller.h"
#include "theme_controller.h"
#include "usb_device.h"
#include "version.h"

int main(int argc, char* argv[]) {
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  // Qt's own Wayland plugin, not this application. Every popup — a menu, a
  // submenu, a tooltip, a combo box — is a surface of its own, and when one
  // goes away the compositor's text-input protocol sends a leave event for a
  // surface the plugin has already forgotten, which it complains about:
  //
  //   qt.qpa.wayland.textinput: … Got leave event for surface 0x0 …
  //
  // Nothing is wrong and nothing can be done about it from here, so opening a
  // menu would print a line of somebody else's diagnostics into a terminal a
  // user is watching for this application's own messages. Silenced by category
  // rather than by matching the text, so nothing else is hidden with it.
  //
  // Rules from QT_LOGGING_RULES are applied after these and win, so anybody
  // debugging the plugin can still switch the category back on.
  QLoggingCategory::setFilterRules(
      QStringLiteral("qt.qpa.wayland.textinput=false"));

  QApplication app(argc, argv);

  // Set before anything constructs a QSettings: the identity decides which file
  // settings are read from and written to, and a QSettings built earlier would
  // quietly use a different one. The application name differs from the older
  // capture application's, so the two do not share a settings file.
  QApplication::setOrganizationName(QStringLiteral("Domesday86"));
  QApplication::setOrganizationDomain(QStringLiteral("domesday86.com"));
  QApplication::setWindowIcon(ddd::gui::ApplicationIcon());
  QApplication::setApplicationName(QStringLiteral("ddd-gui"));

  // The commit this build was made from. The packaging workflows run --version
  // and reject an artefact that reports "unknown", which is what a build with
  // no commit to name says.
  const std::string commit(ddd::capture::Commit());
  QApplication::setApplicationVersion(QString::fromStdString(commit));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Capture application for the Domesday Duplicator"));
  parser.addHelpOption();
  parser.addVersionOption();

  const QCommandLineOption debug_option(
      {QStringLiteral("d"), QStringLiteral("debug")},
      QStringLiteral("Log debug-level diagnostics and show the Log panel."));
  parser.addOption(debug_option);

  const QCommandLineOption analyse_option(
      QStringLiteral("analyse-test-data"),
      QStringLiteral("Check a test-mode capture for sequence breaks and exit. "
                     "Exits 0 for an intact ramp, 1 for a break, 2 for a file "
                     "that could not be analysed."),
      QStringLiteral("file"));
  parser.addOption(analyse_option);

  // The same opt-in ddd-update carries, and worth the same words. A release
  // build pins the release key and accepts nothing else; this widens it for
  // one run, and the update page banners every development-signed bundle it
  // then opens. Without it a developer's own bundle cannot be installed from
  // a release-configured build at all, which is the intended default and a
  // poor experience for the one person it should not obstruct.
  const QCommandLineOption development_key_option(
      QStringLiteral("dev-update-key"),
      QStringLiteral("Accept an update file signed with the development key, "
                     "whose secret half is public. It proves the file is well "
                     "formed and nothing about where it came from."));
  parser.addOption(development_key_option);

  parser.process(app);

  // Before the logger, the theme and the window, so that nothing about this
  // path depends on a display being available. On Windows the executable is
  // built without a console, so the output goes wherever a caller redirects it
  // — the exit code is what a script reads, and that works either way.
  if (parser.isSet(analyse_option)) {
    QTextStream out(stdout);
    QTextStream error(stderr);
    return ddd::gui::RunTestDataAnalysis(parser.value(analyse_option), out,
                                         error);
  }

  ddd::gui::ApplicationLogger logger;
  if (parser.isSet(debug_option)) {
    logger.SetMinimumLevel(ddd::capture::LogLevel::kDebug);
  }

  ddd::gui::ThemeController theme_controller(&app);
  theme_controller.Initialize();

  // A null backend is survivable rather than fatal: the window opens, says why
  // there is no device, and everything that does not need one still works.
  const std::unique_ptr<ddd::capture::IUsbDevice> usb_device =
      ddd::capture::MakeUsbDevice(&logger);

  ddd::gui::CaptureController capture_controller(usb_device.get(), &logger);

  // Nothing is opened, enumerated or written to until the settings say player
  // control is on — see PlayerSettings::enabled, which is off until a user
  // turns it on.
  ddd::gui::PlayerController player_controller({}, &logger);

  ddd::gui::AutoCaptureController auto_capture_controller(
      &player_controller, &capture_controller, &logger);

  ddd::gui::MainWindow window(&theme_controller, &logger, &capture_controller,
                              &player_controller, &auto_capture_controller);
  if (parser.isSet(debug_option)) {
    window.ShowLogPanel();
  }
  if (parser.isSet(development_key_option)) {
    ddd::capture::UpdateKeyPolicy policy =
        ddd::capture::DefaultUpdateKeyPolicy();
    policy.accept_development_key = true;
    window.SetUpdateKeyPolicy(std::move(policy));
    logger.Info(
        "Accepting development-signed update files for this run "
        "(--dev-update-key).");
  }
  window.show();

  logger.Info("Capture application started.");

  // Started after the window is up so that the first device report lands on a
  // window that already has panels to receive it.
  capture_controller.Start();
  player_controller.Start();

  return QApplication::exec();
}
