/************************************************************************

    qt_message_filter.cpp

    Keeping the platform plugin's own noise out of the console
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "qt_message_filter.h"

#include <QTextStream>
#include <QtGlobal>
#include <QtLogging>
#include <cstdio>

namespace ddd::gui {
namespace {

// The handler that was in place before ours, so that installing this filter
// takes nothing away. Null means there was none and Qt's own default was in
// use, which is the ordinary case.
QtMessageHandler g_previous_handler = nullptr;

void FilteringHandler(QtMsgType type, const QMessageLogContext& context,
                      const QString& message) {
  if (IsSilencedPluginMessage(message)) {
    return;
  }

  if (g_previous_handler != nullptr) {
    g_previous_handler(type, context, message);
    return;
  }

  // Qt's default handler cannot be called by name, so what it would have done
  // is done here: the message is formatted by the same rules, QT_MESSAGE_
  // PATTERN included, and written where the default writes it on a desktop.
  //
  // A fatal message is not aborted on. Qt does that itself after the handler
  // returns, whichever handler that is.
  QTextStream stream(stderr);
  stream << qFormatLogMessage(type, context, message) << Qt::endl;
}

}  // namespace

bool IsSilencedPluginMessage(const QString& message) {
  return message ==
         QLatin1StringView(
             "This plugin supports grabbing the mouse only for popup windows");
}

void InstallQtMessageFilter() {
  // Guarded against a second call, which would otherwise install this handler
  // as its own predecessor and recurse on the first message through it.
  static bool installed = false;
  if (installed) {
    return;
  }
  installed = true;

  g_previous_handler = qInstallMessageHandler(FilteringHandler);
}

}  // namespace ddd::gui
