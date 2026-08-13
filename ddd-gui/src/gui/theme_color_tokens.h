/************************************************************************

    theme_color_tokens.h

    Shared colour tokens for theme-aware custom painting
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QColor>
#include <QPalette>
#include <QtGlobal>

namespace ddd::gui::theme_tokens {

// Custom-painted widgets cannot take their trace and grid colours from the
// palette — a palette has no notion of "the waveform" — so the signal displays
// share these tokens instead of each picking its own literals. Every token has
// a light and a dark value, chosen to stay legible against the corresponding
// window background.
enum class PlotColorToken {
  kSignalTrace,
  kSignalTraceMuted,
  kSpectrumTrace,
  kSpectrumPeakHold,
  kAmplitudeTrace,
  kAmplitudeEnvelope,
  kClipMarker,
  kZeroReference,
};

// Linear RGBA blend between two colours; ratio is clamped to [0, 1].
inline QColor Blend(const QColor& from, const QColor& to, qreal ratio) {
  const auto clamped = static_cast<float>(qBound<qreal>(0.0, ratio, 1.0));
  return QColor::fromRgbF(
      from.redF() + (to.redF() - from.redF()) * clamped,
      from.greenF() + (to.greenF() - from.greenF()) * clamped,
      from.blueF() + (to.blueF() - from.blueF()) * clamped,
      from.alphaF() + (to.alphaF() - from.alphaF()) * clamped);
}

// De-emphasised text colour, taken from the palette's disabled group.
inline QColor MutedText(const QPalette& palette) {
  return palette.color(QPalette::Disabled, QPalette::WindowText);
}

// Neutral line colour between window background and text; emphasis in [0, 1]
// moves the result towards the text colour.
inline QColor NeutralLine(const QPalette& palette, qreal emphasis) {
  return Blend(palette.color(QPalette::Window),
               palette.color(QPalette::WindowText), emphasis);
}

// Gridline colour for scale anchors. Derived from the palette rather than
// tokenised, so a grid never fights the window it is drawn on.
inline QColor GridLine(const QPalette& palette) {
  return NeutralLine(palette, 0.25);
}

// Trace and marker colours for the resolved theme.
inline QColor PlotColor(PlotColorToken token, bool dark_theme) {
  switch (token) {
    case PlotColorToken::kSignalTrace:
      return dark_theme ? QColor(100, 200, 255) : QColor(0, 100, 200);
    case PlotColorToken::kSignalTraceMuted:
      return dark_theme ? QColor(70, 130, 170) : QColor(120, 165, 205);
    case PlotColorToken::kSpectrumTrace:
      return dark_theme ? QColor(140, 230, 140) : QColor(0, 130, 40);
    case PlotColorToken::kSpectrumPeakHold:
      return dark_theme ? QColor(255, 220, 120) : QColor(190, 130, 0);
    case PlotColorToken::kAmplitudeTrace:
      return dark_theme ? QColor(255, 255, 100) : QColor(200, 180, 0);
    case PlotColorToken::kAmplitudeEnvelope:
      return dark_theme ? QColor(160, 160, 90) : QColor(220, 205, 120);
    case PlotColorToken::kClipMarker:
      return dark_theme ? QColor(255, 110, 110) : QColor(190, 0, 0);
    case PlotColorToken::kZeroReference:
      return dark_theme ? QColor(150, 150, 150) : QColor(110, 110, 110);
  }

  return dark_theme ? QColor(255, 255, 255) : QColor(0, 0, 0);
}

// Text colour for a device or capture status readout. Used for the
// sequence-protection state and the error taxonomy, where "working", "working
// but degraded" and "broken" have to be distinguishable at a glance.
inline QColor StatusColor(bool is_error, bool is_warning, bool dark_theme) {
  if (is_error) {
    return dark_theme ? QColor(255, 120, 120) : QColor(180, 0, 0);
  }
  if (is_warning) {
    return dark_theme ? QColor(255, 200, 100) : QColor(160, 100, 0);
  }
  return dark_theme ? QColor(140, 220, 140) : QColor(0, 120, 40);
}

}  // namespace ddd::gui::theme_tokens
