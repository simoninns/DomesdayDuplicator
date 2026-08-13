/************************************************************************

    theme_controller.h

    Applies and persists the application theme at runtime
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QApplication>
#include <QMetaObject>
#include <QObject>

#include "theme_manager.h"

namespace ddd::gui {

// Runtime glue around ThemeManager: loads the persisted mode, applies the
// resolved palette and stylesheet to the whole application, follows desktop
// colour-scheme changes while in auto mode, and persists mode changes made
// from the View menu.
//
// Widgets that paint their own content — every signal panel, once they exist —
// cannot pick a colour from the palette alone, so they subscribe to
// ThemeChanged and re-read the tokens in theme_color_tokens.h.
//
// Thread-safety: NOT thread-safe. Create and use on the GUI thread only; it
// mutates the QApplication palette.
class ThemeController : public QObject {
  Q_OBJECT

 public:
  // Does not take ownership of `app`; the controller must not outlive it.
  explicit ThemeController(QApplication* app, QObject* parent = nullptr);

  // Loads the persisted mode from QSettings and applies it.
  void Initialize();

  ThemeManager::Mode mode() const { return mode_; }

  // Whether the currently applied theme is dark.
  bool is_dark() const { return is_dark_; }

  // Applies and persists a new theme mode.
  void SetMode(ThemeManager::Mode mode);

 signals:
  // Emitted after the application palette and stylesheet are restyled.
  void ThemeChanged(bool is_dark);

 private:
  void Apply();
  void UpdateSystemTracking();
  void ApplyPaletteAndStyleSheet(bool is_dark);

  QApplication* app_;
  ThemeManager::Mode mode_ = ThemeManager::Mode::kAuto;
  bool is_dark_ = false;
  QMetaObject::Connection tracking_connection_;
};

}  // namespace ddd::gui
