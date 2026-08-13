/************************************************************************

    test_theme_manager.cpp

    T1 tests for theme mode parsing and colour-scheme resolution
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QPalette>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

#include "theme_manager.h"

namespace ddd::gui {
namespace {

using Mode = ThemeManager::Mode;

TEST(ThemeManagerTest, ParsesCanonicalModeNames) {
  EXPECT_EQ(ThemeManager::ModeFromString(QStringLiteral("auto")), Mode::kAuto);
  EXPECT_EQ(ThemeManager::ModeFromString(QStringLiteral("light")),
            Mode::kLight);
  EXPECT_EQ(ThemeManager::ModeFromString(QStringLiteral("dark")), Mode::kDark);
}

TEST(ThemeManagerTest, ParsingIgnoresCaseAndSurroundingWhitespace) {
  EXPECT_EQ(ThemeManager::ModeFromString(QStringLiteral("  DaRk \t")),
            Mode::kDark);
  EXPECT_EQ(ThemeManager::ModeFromString(QStringLiteral("LIGHT")),
            Mode::kLight);
  EXPECT_EQ(ThemeManager::ModeFromString(QStringLiteral(" Auto ")),
            Mode::kAuto);
}

TEST(ThemeManagerTest, UnrecognisedInputFallsBackToAutoAndReportsIt) {
  bool ok = true;
  EXPECT_EQ(ThemeManager::ModeFromString(QStringLiteral("chartreuse"), &ok),
            Mode::kAuto);
  EXPECT_FALSE(ok);

  ok = true;
  EXPECT_EQ(ThemeManager::ModeFromString(QString(), &ok), Mode::kAuto);
  EXPECT_FALSE(ok);

  ok = false;
  EXPECT_EQ(ThemeManager::ModeFromString(QStringLiteral("dark"), &ok),
            Mode::kDark);
  EXPECT_TRUE(ok);
}

TEST(ThemeManagerTest, ModeNamesRoundTrip) {
  for (const Mode mode : {Mode::kAuto, Mode::kLight, Mode::kDark}) {
    EXPECT_EQ(ThemeManager::ModeFromString(ThemeManager::ModeToString(mode)),
              mode);
  }
}

TEST(ThemeManagerTest, NamesEveryColorScheme) {
  EXPECT_EQ(ThemeManager::ColorSchemeToString(Qt::ColorScheme::Light),
            QStringLiteral("light"));
  EXPECT_EQ(ThemeManager::ColorSchemeToString(Qt::ColorScheme::Dark),
            QStringLiteral("dark"));
  EXPECT_EQ(ThemeManager::ColorSchemeToString(Qt::ColorScheme::Unknown),
            QStringLiteral("unknown"));
}

TEST(ThemeManagerTest, OnlyAutoModeTracksTheDesktop) {
  EXPECT_TRUE(ThemeManager::ShouldTrackSystemChanges(Mode::kAuto));
  EXPECT_FALSE(ThemeManager::ShouldTrackSystemChanges(Mode::kLight));
  EXPECT_FALSE(ThemeManager::ShouldTrackSystemChanges(Mode::kDark));
}

// The resolution matrix: every mode against every scheme the platform can
// report, plus both answers from the palette fallback.

TEST(ThemeManagerTest, ForcedModesIgnoreTheSystemScheme) {
  for (const Qt::ColorScheme scheme :
       {Qt::ColorScheme::Light, Qt::ColorScheme::Dark,
        Qt::ColorScheme::Unknown}) {
    for (const bool palette_is_dark : {false, true}) {
      const ThemeManager::Resolution light =
          ThemeManager::ResolveScheme(Mode::kLight, scheme, palette_is_dark);
      EXPECT_FALSE(light.is_dark);
      EXPECT_EQ(light.scheme, Qt::ColorScheme::Light);
      EXPECT_FALSE(light.used_palette_fallback);

      const ThemeManager::Resolution dark =
          ThemeManager::ResolveScheme(Mode::kDark, scheme, palette_is_dark);
      EXPECT_TRUE(dark.is_dark);
      EXPECT_EQ(dark.scheme, Qt::ColorScheme::Dark);
      EXPECT_FALSE(dark.used_palette_fallback);
    }
  }
}

TEST(ThemeManagerTest, AutoModeFollowsAReportedScheme) {
  for (const bool palette_is_dark : {false, true}) {
    const ThemeManager::Resolution dark = ThemeManager::ResolveScheme(
        Mode::kAuto, Qt::ColorScheme::Dark, palette_is_dark);
    EXPECT_TRUE(dark.is_dark);
    EXPECT_FALSE(dark.used_palette_fallback);

    const ThemeManager::Resolution light = ThemeManager::ResolveScheme(
        Mode::kAuto, Qt::ColorScheme::Light, palette_is_dark);
    EXPECT_FALSE(light.is_dark);
    EXPECT_FALSE(light.used_palette_fallback);
  }
}

TEST(ThemeManagerTest, AutoModeUsesThePaletteWhenTheSchemeIsUnknown) {
  const ThemeManager::Resolution dark = ThemeManager::ResolveScheme(
      Mode::kAuto, Qt::ColorScheme::Unknown, /*palette_is_dark=*/true);
  EXPECT_TRUE(dark.is_dark);
  EXPECT_EQ(dark.scheme, Qt::ColorScheme::Dark);
  EXPECT_TRUE(dark.used_palette_fallback);

  const ThemeManager::Resolution light = ThemeManager::ResolveScheme(
      Mode::kAuto, Qt::ColorScheme::Unknown, /*palette_is_dark=*/false);
  EXPECT_FALSE(light.is_dark);
  EXPECT_EQ(light.scheme, Qt::ColorScheme::Light);
  EXPECT_TRUE(light.used_palette_fallback);
}

TEST(ThemeManagerTest, ResolutionCarriesTheRequestedModeAndItsSource) {
  const ThemeManager::Resolution resolution = ThemeManager::ResolveScheme(
      Mode::kAuto, Qt::ColorScheme::Unknown, /*palette_is_dark=*/true);
  EXPECT_EQ(resolution.mode, Mode::kAuto);
  EXPECT_FALSE(resolution.source.isEmpty());
}

TEST(ThemeManagerTest, DetectsADarkPaletteByLightness) {
  QPalette dark;
  dark.setColor(QPalette::Window, QColor(30, 30, 30));
  dark.setColor(QPalette::WindowText, QColor(240, 240, 240));
  EXPECT_TRUE(ThemeManager::IsPaletteDark(dark));

  QPalette light;
  light.setColor(QPalette::Window, QColor(245, 245, 245));
  light.setColor(QPalette::WindowText, QColor(20, 20, 20));
  EXPECT_FALSE(ThemeManager::IsPaletteDark(light));
}

TEST(ThemeManagerTest, PersistsAndReloadsTheMode) {
  // A temporary directory rather than the default location: this must not touch
  // the developer's own configuration, and in a packaging sandbox HOME does not
  // exist, where QSettings discards writes silently.
  QTemporaryDir settings_dir;
  ASSERT_TRUE(settings_dir.isValid());
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                     settings_dir.path());

  QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("Domesday86"),
                     QStringLiteral("ddd-gui-theme-test"));
  settings.clear();

  // Nothing stored yet: auto, so a first run follows the desktop.
  EXPECT_EQ(ThemeManager::LoadMode(settings), Mode::kAuto);

  ThemeManager::SaveMode(&settings, Mode::kDark);
  EXPECT_EQ(ThemeManager::LoadMode(settings), Mode::kDark);

  ThemeManager::SaveMode(&settings, Mode::kLight);
  EXPECT_EQ(ThemeManager::LoadMode(settings), Mode::kLight);

  // A value written by hand, or left by an older version, must not stop the
  // application starting.
  settings.setValue(QLatin1String(ThemeManager::kSettingsKey),
                    QStringLiteral("puce"));
  EXPECT_EQ(ThemeManager::LoadMode(settings), Mode::kAuto);

  settings.clear();
}

}  // namespace
}  // namespace ddd::gui
