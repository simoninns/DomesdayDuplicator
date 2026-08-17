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
  kNominalLimit,
  kZeroReference,

  // Where the sweep was started from. Distinct from the mid-scale reference it
  // usually sits on top of, because "this is the centre of the range" and "this
  // is the edge every sweep is aligned to" are different claims and a reader
  // has to be able to tell which line is which.
  kTriggerMarker,

  // Where the board's anti-aliasing filter turns over. Its own token rather
  // than the gridline colour, because it is not a gridline: everything above
  // it on a spectrum is the filter's skirt rather than the signal, and that is
  // worth a line a reader can tell apart from the ones that only say where the
  // numbers are.
  kFilterCorner,

  // A pass or fail verdict. Not a plot, but here for the same reason
  // everything else is: a palette has no notion of "this capture is good", and
  // the literal green that reads well on a light window is unreadable on a dark
  // one. This is the one message in the application somebody reads from across
  // a bench, so it is worth getting right in both themes.
  kVerdictPass,
  kVerdictFail,

  // The two buttons that are doing something, so their state is visible from
  // across a room and not only by reading their labels.
  //
  // Deliberately muted. A saturated green and red would be the brightest things
  // in a window whose whole job is to display a signal, and the eye would go to
  // the controls instead of to the waveform. These are dark enough to sit
  // behind white text and desaturated enough to read as "on" rather than as an
  // alarm — the red says "recording", which is a normal state, not a fault.
  kMonitoringActive,
  kCapturingActive,

  // A control worth noticing before pressing something else — the Naming button
  // when a capture is about to be written with nothing but a timestamp for a
  // name.
  //
  // Muted for the same reason as the two above, and amber rather than red for
  // the reason kNominalLimit is: an unnamed capture is a perfectly legitimate
  // thing to take, and nothing here is wrong. This says "you may have meant to
  // fill this in", and a colour that said "fault" would be a lie about a
  // recording that is going to be fine.
  kAttention,
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

// Whether the resolved theme is the dark one.
//
// Read from the palette rather than asked of the theme controller, so that a
// custom-painted widget needs nothing but itself to pick its colours — and so
// that it follows a theme change through the palette-change event it already
// gets, with no signal to connect and no controller to be handed.
inline bool IsDarkPalette(const QPalette& palette) {
  return palette.color(QPalette::Window).lightness() < 128;
}

// Black or white, whichever can be read on the given background.
//
// Computed rather than chosen, so that changing one of the tokens above cannot
// quietly produce a control with unreadable text. The weights are the standard
// perceived-brightness ones: the eye is far more sensitive to green than to
// blue, and a plain average of the channels picks wrong on exactly the
// mid-tones these tokens use.
inline QColor ReadableTextOn(const QColor& background) {
  const qreal brightness = (0.299 * background.red()) +
                           (0.587 * background.green()) +
                           (0.114 * background.blue());
  return brightness < 140.0 ? QColor(255, 255, 255) : QColor(0, 0, 0);
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
    case PlotColorToken::kNominalLimit:
      // Amber rather than red. Passing it is worth seeing and is not a fault:
      // the fault is clipping, which has the red.
      return dark_theme ? QColor(230, 180, 90) : QColor(170, 110, 0);
    case PlotColorToken::kZeroReference:
      return dark_theme ? QColor(150, 150, 150) : QColor(110, 110, 110);
    case PlotColorToken::kTriggerMarker:
      return dark_theme ? QColor(120, 190, 140) : QColor(30, 120, 60);
    case PlotColorToken::kFilterCorner:
      // Violet: the one hue not already spoken for on the spectrum, where the
      // trace is green, the peak hold amber and the waterfall runs through
      // most of the rest.
      return dark_theme ? QColor(165, 150, 215) : QColor(105, 85, 165);
    case PlotColorToken::kVerdictPass:
      return dark_theme ? QColor(120, 220, 130) : QColor(0, 120, 40);
    case PlotColorToken::kVerdictFail:
      return dark_theme ? QColor(255, 120, 120) : QColor(180, 0, 0);
    case PlotColorToken::kMonitoringActive:
      return dark_theme ? QColor(72, 110, 80) : QColor(94, 140, 100);
    case PlotColorToken::kCapturingActive:
      return dark_theme ? QColor(128, 66, 64) : QColor(160, 86, 82);
    case PlotColorToken::kAttention:
      return dark_theme ? QColor(124, 98, 52) : QColor(168, 134, 70);
  }

  return dark_theme ? QColor(255, 255, 255) : QColor(0, 0, 0);
}

// Magnitude to colour for the spectrogram, with proportion 0 at the bottom of
// the displayed range and 1 at the top.
//
// A ramp rather than a single hue with varying brightness. The interesting
// features here are a carrier an order of magnitude above the noise and the
// occasional interferer between them, and the eye reads a hue change far more
// readily than it reads a brightness change — a purely dark-to-light ramp turns
// the whole display into an indistinct grey wash at exactly the levels worth
// looking at.
//
// Both themes run from the window's own background at the bottom, so an empty
// spectrogram looks like an empty panel rather than a block of colour, and
// both end somewhere emphatic.
inline QColor SpectrogramColor(qreal proportion, bool dark_theme) {
  struct Stop {
    qreal at;
    QColor colour;
  };

  const Stop dark_stops[] = {{0.0, QColor(18, 18, 28)},
                             {0.35, QColor(45, 40, 130)},
                             {0.60, QColor(170, 45, 110)},
                             {0.82, QColor(240, 145, 45)},
                             {1.0, QColor(255, 245, 190)}};

  const Stop light_stops[] = {{0.0, QColor(252, 252, 255)},
                              {0.35, QColor(150, 190, 232)},
                              {0.60, QColor(45, 95, 185)},
                              {0.82, QColor(125, 40, 145)},
                              {1.0, QColor(140, 0, 25)}};

  const Stop* stops = dark_theme ? dark_stops : light_stops;
  constexpr int kStopCount = 5;

  const qreal level = qBound<qreal>(0.0, proportion, 1.0);
  for (int index = 1; index < kStopCount; ++index) {
    if (level > stops[index].at) {
      continue;
    }
    const qreal span = stops[index].at - stops[index - 1].at;
    const qreal within =
        span > 0.0 ? (level - stops[index - 1].at) / span : 0.0;
    return Blend(stops[index - 1].colour, stops[index].colour, within);
  }

  return stops[kStopCount - 1].colour;
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
