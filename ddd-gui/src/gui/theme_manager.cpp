/************************************************************************

    theme_manager.cpp

    Theme mode parsing, persistence and colour-scheme resolution
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "theme_manager.h"

namespace ddd::gui {

ThemeManager::Mode ThemeManager::ModeFromString(const QString& text, bool* ok) {
  const QString normalized = text.trimmed().toLower();

  if (ok != nullptr) {
    *ok = true;
  }

  if (normalized == QStringLiteral("auto")) {
    return Mode::kAuto;
  }
  if (normalized == QStringLiteral("light")) {
    return Mode::kLight;
  }
  if (normalized == QStringLiteral("dark")) {
    return Mode::kDark;
  }

  if (ok != nullptr) {
    *ok = false;
  }
  return Mode::kAuto;
}

QString ThemeManager::ModeToString(Mode mode) {
  switch (mode) {
    case Mode::kAuto:
      return QStringLiteral("auto");
    case Mode::kLight:
      return QStringLiteral("light");
    case Mode::kDark:
      return QStringLiteral("dark");
  }

  return QStringLiteral("auto");
}

QString ThemeManager::ColorSchemeToString(Qt::ColorScheme scheme) {
  switch (scheme) {
    case Qt::ColorScheme::Dark:
      return QStringLiteral("dark");
    case Qt::ColorScheme::Light:
      return QStringLiteral("light");
    case Qt::ColorScheme::Unknown:
      return QStringLiteral("unknown");
  }

  return QStringLiteral("unknown");
}

bool ThemeManager::ShouldTrackSystemChanges(Mode mode) {
  return mode == Mode::kAuto;
}

ThemeManager::Resolution ThemeManager::ResolveScheme(
    Mode mode, Qt::ColorScheme system_scheme, bool palette_is_dark) {
  if (mode == Mode::kLight) {
    return Resolution{mode, Qt::ColorScheme::Light, false, false,
                      QStringLiteral("forced light mode")};
  }

  if (mode == Mode::kDark) {
    return Resolution{mode, Qt::ColorScheme::Dark, true, false,
                      QStringLiteral("forced dark mode")};
  }

  if (system_scheme == Qt::ColorScheme::Dark) {
    return Resolution{mode, system_scheme, true, false,
                      QStringLiteral("auto (style hints)")};
  }

  if (system_scheme == Qt::ColorScheme::Light) {
    return Resolution{mode, system_scheme, false, false,
                      QStringLiteral("auto (style hints)")};
  }

  return Resolution{
      mode, palette_is_dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light,
      palette_is_dark, true, QStringLiteral("auto (palette fallback)")};
}

bool ThemeManager::IsPaletteDark(const QPalette& palette) {
  const QColor window_color = palette.color(QPalette::Window);
  const QColor text_color = palette.color(QPalette::WindowText);
  return window_color.lightness() < text_color.lightness();
}

ThemeManager::Mode ThemeManager::LoadMode(const QSettings& settings) {
  const QString stored = settings.value(QLatin1String(kSettingsKey)).toString();
  return ModeFromString(stored);
}

void ThemeManager::SaveMode(QSettings* settings, Mode mode) {
  settings->setValue(QLatin1String(kSettingsKey), ModeToString(mode));
}

}  // namespace ddd::gui
