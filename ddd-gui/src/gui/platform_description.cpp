/************************************************************************

    platform_description.cpp

    What this build is running on, for the log
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "platform_description.h"

#include <QStringList>
#include <QSysInfo>
#include <QtGlobal>

namespace ddd::gui {

QString DescribePlatform(const QString& product, const QString& kernel_type,
                         const QString& kernel, const QString& architecture,
                         const QString& running_qt, const QString& built_qt) {
  QStringList parts;

  // The product name first, because it is the one a user would give if asked
  // what they are running. Everything after it is what a developer needs and a
  // user would not think to say.
  if (!product.isEmpty()) {
    parts << product;
  }

  // The kernel, which is the version that actually decides how USB behaves.
  // A distribution's own version number says nothing about that on Linux, and
  // on macOS the Darwin version is the one a kernel bug is filed against.
  if (!kernel_type.isEmpty() || !kernel.isEmpty()) {
    QString kernel_part = QStringLiteral("kernel");
    if (!kernel_type.isEmpty()) {
      kernel_part += QStringLiteral(" ") + kernel_type;
    }
    if (!kernel.isEmpty()) {
      kernel_part += QStringLiteral(" ") + kernel;
    }
    parts << kernel_part;
  }

  if (!architecture.isEmpty()) {
    parts << architecture;
  }

  if (!running_qt.isEmpty()) {
    // Both versions only when they differ, which is the only time the second
    // one says anything: a bundle that ships one Qt and loads another is a
    // fault that reads as an application bug until this line contradicts it.
    parts << (built_qt.isEmpty() || built_qt == running_qt
                  ? QStringLiteral("Qt %1").arg(running_qt)
                  : QStringLiteral("Qt %1 (built against %2)")
                        .arg(running_qt, built_qt));
  }

  if (parts.isEmpty()) {
    // Every source came back empty, which no real platform does. Said rather
    // than logged as an empty line, because a line that says nothing looks
    // like a bug in the logging rather than in what it was asking.
    return QStringLiteral("not known");
  }

  return parts.join(QStringLiteral(", "));
}

QString PlatformDescription() {
  return DescribePlatform(
      QSysInfo::prettyProductName(), QSysInfo::kernelType(),
      QSysInfo::kernelVersion(), QSysInfo::currentCpuArchitecture(),
      QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR));
}

}  // namespace ddd::gui
