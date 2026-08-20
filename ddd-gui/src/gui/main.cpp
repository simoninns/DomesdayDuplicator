/************************************************************************

    main.cpp

    Capture application entry point
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
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
#include "capture_cli.h"
#include "capture_control_server.h"
#include "capture_controller.h"
#include "capture_settings.h"
#include "capture_stop_client.h"
#include "console_attach.h"
#include "headless_capture_runner.h"
#include "log_options.h"
#include "logger.h"
#include "main_window.h"
#include "platform_description.h"
#include "player_controller.h"
#include "qt_message_filter.h"
#include "signal_watcher.h"
#include "spdlog_logger.h"
#include "theme_controller.h"
#include "usb_device.h"
#include "version.h"

namespace {

// A capture with no window around it: the whole of --headless.
//
// Everything it wires is tested elsewhere — the lifecycle in
// HeadlessCaptureRunner, the socket in CaptureControlServer, the interrupt in
// SignalWatcher — so what is left here is the wiring, and it is kept out of
// main() so that the three modes read as three modes rather than as one long
// function with branches in it.
int RunHeadlessCapture(QCoreApplication& app, ddd::gui::ApplicationLogger& log,
                       const ddd::gui::CaptureCliOptions& options,
                       QTextStream& out, QTextStream& error) {
  // A null backend is fatal here where the window survives it: a window with no
  // device can still show settings and open a capture for reading, and a
  // headless run has nothing left to do at all.
  const std::unique_ptr<ddd::capture::IUsbDevice> usb_device =
      ddd::capture::MakeUsbDevice(&log);

  ddd::gui::CaptureController capture_controller(usb_device.get(), &log);

  ddd::gui::CaptureSettings settings = capture_controller.settings();
  ddd::gui::ApplyCliOverrides(settings, options);
  capture_controller.ApplySessionSettings(settings);

  // Before anything is started, and a hard failure rather than a warning. Two
  // processes streaming from one device is not something either can do, and a
  // headless capture that could not be reached over the socket would be a
  // process a script had no way to stop — on Windows, where there is no
  // interrupt to fall back on, no way at all.
  ddd::gui::CaptureControlServer control_server(&capture_controller, &log);
  QString listen_error;
  if (!control_server.Listen(&listen_error)) {
    error << listen_error << "\n";
    error.flush();
    return ddd::gui::kExitInstanceRunning;
  }

  ddd::gui::HeadlessCaptureRunner runner(&capture_controller, out, error);

  int exit_code = ddd::gui::kExitSuccess;
  QObject::connect(&runner, &ddd::gui::HeadlessCaptureRunner::Finished, &app,
                   [&exit_code, &app](int code) {
                     exit_code = code;
                     app.quit();
                   });

  // Ctrl+C and kill on Unix, a console control event on Windows. Null if the
  // platform would not take the handler, which is not a reason to refuse to
  // capture: --stop-capture works whatever this returns, which is why the
  // socket above is not optional and this is.
  ddd::gui::SignalWatcher* const watcher =
      ddd::gui::SignalWatcher::Install(&app);
  if (watcher != nullptr) {
    QObject::connect(watcher, &ddd::gui::SignalWatcher::Interrupted, &runner,
                     &ddd::gui::HeadlessCaptureRunner::RequestStop);
  }

  // The runner first, so that it is connected before the controller's first
  // device report goes out — that report is the one that starts the capture.
  runner.Begin();
  capture_controller.Start();

  QCoreApplication::exec();

  // Stack destruction from here, exactly as the windowed path unwinds: the
  // controller's destructor stops the monitor, the analysis worker and the
  // pipeline in the order they have to be stopped in.
  return exit_code;
}

}  // namespace

int main(int argc, char* argv[]) {
  // First, before anything can write to a stream. On Windows this application
  // is a GUI-subsystem executable and has no console of its own, so --version,
  // --help and the console half of the log would all go nowhere; borrowing the
  // console it was started from is what makes them visible. It does nothing on
  // Linux or macOS.
  ddd::gui::AttachParentConsole();

  // Which application object to build, decided from the raw arguments because
  // QCommandLineParser needs the application it is about to decide on. A
  // headless capture and a --stop-capture client both want a QCoreApplication:
  // a QApplication would demand a platform plugin neither has any use for,
  // which on a machine with no display is the difference between running and
  // refusing to start.
  const bool core_only = ddd::gui::WantsCoreApplication(argc, argv);

  std::unique_ptr<QCoreApplication> app;

  // The same object as app, when there is a window. Held separately because the
  // theme controller takes a QApplication and the branch below is the only
  // place that knows which of the two was built.
  QApplication* gui_app = nullptr;

  if (core_only) {
    app = std::make_unique<QCoreApplication>(argc, argv);
  } else {
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
    // user is watching for this application's own messages. Silenced by
    // category rather than by matching the text, so nothing else is hidden with
    // it.
    //
    // Rules from QT_LOGGING_RULES are applied after these and win, so anybody
    // debugging the plugin can still switch the category back on.
    QLoggingCategory::setFilterRules(
        QStringLiteral("qt.qpa.wayland.textinput=false"));

    // The same plugin, and the same reasoning, for a message that carries no
    // category to switch off. See qt_message_filter.h.
    ddd::gui::InstallQtMessageFilter();

    auto gui = std::make_unique<QApplication>(argc, argv);
    gui_app = gui.get();
    app = std::move(gui);
  }

  // Set before anything constructs a QSettings: the identity decides which file
  // settings are read from and written to, and a QSettings built earlier would
  // quietly use a different one. The application name differs from the older
  // capture application's, so the two do not share a settings file.
  //
  // Unconditional, and not only for the window: a headless capture reads the
  // same settings file, and one that read a different one would capture with
  // settings nobody had ever chosen.
  QCoreApplication::setOrganizationName(QStringLiteral("Domesday86"));
  QCoreApplication::setOrganizationDomain(QStringLiteral("domesday86.com"));
  QCoreApplication::setApplicationName(QStringLiteral("ddd-gui"));

  if (!core_only) {
    QApplication::setWindowIcon(ddd::gui::ApplicationIcon());
  }

  // The commit this build was made from. The packaging workflows run --version
  // and reject an artefact that reports "unknown", which is what a build with
  // no commit to name says.
  const std::string commit(ddd::capture::Commit());
  QCoreApplication::setApplicationVersion(QString::fromStdString(commit));

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

  const ddd::gui::CaptureCliOptionSet capture_options =
      ddd::gui::AddCaptureCliOptions(parser);

  parser.process(*app);

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

  QTextStream out_stream(stdout);
  QTextStream error_stream(stderr);

  const ddd::gui::CaptureCliParseResult capture_cli =
      ddd::gui::ParseCaptureCliOptions(parser, capture_options);
  if (!capture_cli.ok()) {
    error_stream << capture_cli.error << "\n";
    error_stream.flush();
    return ddd::gui::kExitBadArguments;
  }

  // The client mode, and it is only a client: it opens no device, reads no
  // settings and writes no log. Everything it does is one line down a socket to
  // the application that is actually capturing.
  if (capture_cli.options.stop_capture) {
    return ddd::gui::RunStopCapture(out_stream, error_stream);
  }

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

  // With no window there is no Log panel, so nothing about the log can be said
  // after one exists — it is said here, where a redirected console is the only
  // place it can land.
  if (capture_cli.options.headless) {
    logger.Info("Application commit " + commit + ".");
    logger.Info("Platform: " + ddd::gui::PlatformDescription().toStdString() +
                ".");
    for (const std::string& warning : log_destinations.warnings()) {
      logger.Warning(warning);
    }
    return RunHeadlessCapture(*app, logger, capture_cli.options, out_stream,
                              error_stream);
  }

  ddd::gui::ThemeController theme_controller(gui_app);
  theme_controller.Initialize();

  // A null backend is survivable rather than fatal: the window opens, says why
  // there is no device, and everything that does not need one still works.
  const std::unique_ptr<ddd::capture::IUsbDevice> usb_device =
      ddd::capture::MakeUsbDevice(&logger);

  ddd::gui::CaptureController capture_controller(usb_device.get(), &logger);

  // Before the window is built. CapturePanel reads the controller's settings in
  // its constructor, so applying them here is what makes the panel open already
  // showing what the command line asked for — and applied rather than set,
  // because what a command line names belongs to this run and is not the user's
  // new saved answer.
  if (capture_cli.options.HasAttributeOverrides()) {
    ddd::gui::CaptureSettings settings = capture_controller.settings();
    ddd::gui::ApplyCliOverrides(settings, capture_cli.options);
    capture_controller.ApplySessionSettings(settings);
  }

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

  // The window has one of these too, so that a capture started by hand and a
  // capture started from a script are stopped the same way. Failure is fatal
  // only when the command line asked for a capture: an ordinary second window
  // is a reasonable thing to want — to read a capture, or to look at the
  // settings — and refusing to open it because the first one holds the socket
  // would be worse than losing --stop-capture for that instance.
  ddd::gui::CaptureControlServer control_server(&capture_controller, &logger);
  QString listen_error;
  if (!control_server.Listen(&listen_error)) {
    if (capture_cli.options.start_capture) {
      error_stream << listen_error << "\n";
      error_stream.flush();
      return ddd::gui::kExitInstanceRunning;
    }
    logger.Warning(listen_error.toStdString() +
                   " This window will not answer --stop-capture.");
  }

  // Started after the window is up so that the first device report lands on a
  // window that already has panels to receive it.
  capture_controller.Start();
  player_controller.Start();

  // After the controller is started, so that a device already attached is
  // captured from at once rather than at the next poll.
  if (capture_cli.options.start_capture) {
    ddd::gui::StartCaptureWhenDeviceAppears(&capture_controller);
  }

  return QCoreApplication::exec();
}
