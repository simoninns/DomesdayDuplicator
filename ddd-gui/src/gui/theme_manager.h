/************************************************************************

    theme_manager.h

    Theme mode parsing, persistence and colour-scheme resolution
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QPalette>
#include <QSettings>
#include <QString>
#include <Qt>

namespace ddd::gui {

// Pure theme-mode logic: string parsing, QSettings persistence, and resolution
// of a requested mode against the system colour scheme. Split from
// ThemeController so the decisions can be tested without an application object
// or a display.
//
// Thread-safety: all members are static and stateless, so they are safe to call
// from any thread. QSettings arguments follow QSettings' own rules — an
// instance belongs to the thread that created it.
class ThemeManager {
 public:
  enum class Mode {
    kAuto,
    kLight,
    kDark,
  };

  // Outcome of resolving a mode against the system scheme.
  struct Resolution {
    Mode mode;
    Qt::ColorScheme scheme;
    bool is_dark;
    bool used_palette_fallback;
    QString source;
  };

  // QSettings key under which the theme mode is persisted.
  static constexpr const char* kSettingsKey = "view/theme_mode";

  // Parses a mode string ("auto", "light", "dark"; case-insensitive,
  // surrounding whitespace ignored). Unrecognised or empty input yields kAuto;
  // when `ok` is non-null it reports whether the input was valid.
  static Mode ModeFromString(const QString& text, bool* ok = nullptr);

  // Returns the canonical lower-case name for a mode.
  static QString ModeToString(Mode mode);

  // Returns "light", "dark" or "unknown", for logging.
  static QString ColorSchemeToString(Qt::ColorScheme scheme);

  // Auto mode follows the desktop, so only it needs live change tracking.
  static bool ShouldTrackSystemChanges(Mode mode);

  // Resolves a mode using the system-reported scheme and, when that scheme is
  // unknown, a palette-luminance fallback. A pure function of its inputs.
  //
  // The fallback matters on platforms and styles where Qt cannot report a
  // scheme at all: without it, auto mode on such a desktop would silently mean
  // light, which is the wrong answer half the time.
  static Resolution ResolveScheme(Mode mode, Qt::ColorScheme system_scheme,
                                  bool palette_is_dark);

  // Heuristic: a palette is dark when its window colour is darker than its
  // window text colour.
  static bool IsPaletteDark(const QPalette& palette);

  // Reads the persisted mode; missing or invalid values yield kAuto.
  static Mode LoadMode(const QSettings& settings);

  // Persists the mode under kSettingsKey.
  static void SaveMode(QSettings* settings, Mode mode);
};

}  // namespace ddd::gui
