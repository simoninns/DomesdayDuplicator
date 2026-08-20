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
#include <optional>
#include <string>
#include <utility>

#include "about_text.h"
#include "analysis_cli.h"
#include "application_logger.h"
#include "auto_capture_controller.h"
#include "capture_controller.h"
#include "console_attach.h"
#include "log_options.h"
#include "logger.h"
#include "main_window.h"
#include "platform_description.h"
#include "player_controller.h"
#include "qt_message_filter.h"
#include "spdlog_logger.h"
#include "theme_controller.h"
#include "usb_device.h"
#include "version.h"

int main(int argc, char* argv[]) {
  // First, before anything can write to a stream. On Windows this application
  // is a GUI-subsystem executable and has no console of its own, so --version,
  // --help and the console half of the log would all go nowhere; borrowing the
  // console it was started from is what makes them visible. It does nothing on
  // Linux or macOS.
  ddd::gui::AttachParentConsole();

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

  // The same plugin, and the same reasoning, for a message that carries no
  // category to switch off. See qt_message_filter.h.
  ddd::gui::InstallQtMessageFilter();

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

  // The three logging switches are spelled as the project's other tools spell
  // them, so that a level or a destination named on one means the same thing on
  // the next.
  const QCommandLineOption log_level_option(
      {QStringLiteral("l"), QStringLiteral("log-level")},
      QStringLiteral("Log records at this level and above: trace, debug, info, "
                     "warn, error, critical or off. Default: info."),
      QStringLiteral("level"), QStringLiteral("info"));
  parser.addOption(log_level_option);

  const QCommandLineOption log_file_option(
      {QStringLiteral("f"), QStringLiteral("log-file")},
      QStringLiteral("Write the log to this file as well as to the console. "
                     "The file is replaced at every start."),
      QStringLiteral("file"));
  parser.addOption(log_file_option);

  const QCommandLineOption log_out_option(
      QStringLiteral("log-out"),
      QStringLiteral("Where the log goes besides the Log panel: none, console, "
                     "file or both. Naming a --log-file implies file. "
                     "Default: none."),
      QStringLiteral("destination"), QStringLiteral("none"));
  parser.addOption(log_out_option);

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
  // path depends on a display being available. On Windows the output goes to
  // whatever console AttachParentConsole() found above, or wherever a caller
  // redirected it — the exit code is what a script reads, and that works even
  // when there was no console to find.
  if (parser.isSet(analyse_option)) {
    QTextStream out(stdout);
    QTextStream error(stderr);
    return ddd::gui::RunTestDataAnalysis(parser.value(analyse_option), out,
                                         error);
  }

  QTextStream error_stream(stderr);

  ddd::capture::LogConfig log_config;

  const QString level_name = parser.value(log_level_option);
  const std::optional<ddd::capture::LogLevel> level =
      ddd::capture::ParseLogLevel(level_name.toStdString());
  if (!level.has_value()) {
    error_stream << QStringLiteral(
                        "Unknown --log-level '%1'. Use trace, debug, info, "
                        "warn, error, critical or off.\n")
                        .arg(level_name);
    return 1;
  }
  log_config.level = *level;

  // --debug is older than --log-level and does two things: it lowers the level
  // and it opens the Log panel. Only the first half is a level, so an explicit
  // --log-level wins over it and the two combine into "open the panel, at a
  // level of my choosing".
  if (parser.isSet(debug_option) && !parser.isSet(log_level_option)) {
    log_config.level = ddd::capture::LogLevel::kDebug;
  }

  const QString destination_name = parser.value(log_out_option);
  const std::optional<ddd::capture::LogDestination> destination =
      ddd::capture::ParseLogDestination(destination_name.toStdString());
  if (!destination.has_value()) {
    error_stream << QStringLiteral(
                        "Unknown --log-out '%1'. Use none, console, file or "
                        "both.\n")
                        .arg(destination_name);
    return 1;
  }
  log_config.destination = *destination;
  log_config.file = parser.value(log_file_option).toStdString();

  // The default destination is none: the Log panel is the log, and an
  // application started from a desktop has no console to write to anyway.
  // Naming a file is an explicit request for one, so it turns the default into
  // "the file" rather than being ignored — which is what a default of none
  // would otherwise mean, and would be a switch that silently did nothing.
  //
  // An explicit --log-out always wins, including an explicit "none" beside a
  // --log-file: that combination is warned about below rather than second
  // guessed.
  if (!parser.isSet(log_out_option) && !log_config.file.empty()) {
    log_config.destination = ddd::capture::LogDestination::kFile;
  }

  // The console and the file. The GUI logger below mirrors every record it
  // shows into this one, so the Log panel and the console carry the same log
  // rather than two that have to be reconciled.
  ddd::capture::SpdlogLogger log_destinations(log_config);

  ddd::gui::ApplicationLogger logger(&log_destinations);
  logger.SetMinimumLevel(log_config.level);

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

  // Its own line, and said every run. These two are what a fault report is
  // read against: which build this was, and what it was running on. Both are
  // wanted whether or not anything goes wrong, so neither is behind a level.
  logger.Info("Application commit " + commit + ".");
  logger.Info("Platform: " + ddd::gui::PlatformDescription().toStdString() +
              ".");

  // Said after the window exists, so that it reaches the Log panel as well as
  // the console. Anything logged before that point has nowhere in the GUI to
  // land, which is why the setup below reports itself here rather than where it
  // happened.
  if (log_destinations.writes_to_file()) {
    logger.Info("Writing the log to " + log_config.file + ".");
  }
  for (const std::string& warning : log_destinations.warnings()) {
    logger.Warning(warning);
  }

  // Both halves of asking for something that cannot be done, and both only
  // when the destination was named explicitly: the defaults are chosen to be
  // silent, and a warning about a switch nobody used would be noise.
  if (parser.isSet(log_out_option) && log_config.file.empty() &&
      (log_config.destination == ddd::capture::LogDestination::kFile ||
       log_config.destination == ddd::capture::LogDestination::kBoth)) {
    logger.Warning(
        std::string("--log-out ") +
        ddd::capture::LogDestinationName(log_config.destination) +
        " was asked for without --log-file, so nothing is being written to a "
        "file.");
  }

  if (!log_config.file.empty() && !log_destinations.writes_to_file() &&
      log_destinations.warnings().empty()) {
    // A log file was named and the destination does not include one, which is
    // a switch quietly doing nothing. Not said when the file simply could not
    // be opened — that has its own message, above, and saying both would name
    // one problem twice.
    logger.Warning("--log-file " + log_config.file +
                   " is not being written: " + "--log-out " +
                   ddd::capture::LogDestinationName(log_config.destination) +
                   " does not include a file.");
  }

  // Started after the window is up so that the first device report lands on a
  // window that already has panels to receive it.
  capture_controller.Start();
  player_controller.Start();

  return QApplication::exec();
}
