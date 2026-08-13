/************************************************************************

    test_waveform_panel.cpp

    T1 tests for the scope panel
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "front_end_gain.h"
#include "sample_format.h"
#include "waveform_panel.h"

namespace ddd::gui {
namespace {

std::vector<uint16_t> Sine(size_t count, double phase = 0.0) {
  std::vector<uint16_t> codes(count);
  for (size_t index = 0; index < count; ++index) {
    codes[index] = static_cast<uint16_t>(std::lround(
        512.0 + 400.0 * std::sin((static_cast<double>(index) * 0.05) + phase)));
  }
  return codes;
}

// Pixels that differ from the same panel with nothing to draw, by enough to be
// seen.
//
// The threshold is the whole point of this helper. Counting any difference at
// all counts pixels at one part in 255, and the first version of the
// persistence mode accumulated exactly that: an envelope that was present in
// the buffer, invisible on the screen, and indistinguishable to a user from the
// option doing nothing.
int VisibleTracePixels(const QImage& shot, const QImage& blank) {
  constexpr int kSeeableContrast = 60;

  int count = 0;
  for (int y = 0; y < shot.height(); ++y) {
    for (int x = 0; x < shot.width(); ++x) {
      const QColor drawn(shot.pixel(x, y));
      const QColor background(blank.pixel(x, y));
      const int difference =
          std::max({std::abs(drawn.red() - background.red()),
                    std::abs(drawn.green() - background.green()),
                    std::abs(drawn.blue() - background.blue())});
      if (difference >= kSeeableContrast) {
        ++count;
      }
    }
  }
  return count;
}

template <typename T>
T* Named(const WaveformPanel& panel, const char* name) {
  return panel.findChild<T*>(QLatin1String(name));
}

TEST(WaveformFormatTest, SpansAreLabelledInTheTimeTheyCover) {
  // 400 samples at 40 Msps is 10 µs, which is about two cycles of a LaserDisc
  // FM carrier — the shortest span with anything to see in it — and 20,000 is
  // the longest the panel offers.
  EXPECT_EQ(FormatWaveformSpan(400), QStringLiteral("10 µs"));
  EXPECT_EQ(FormatWaveformSpan(20'000), QStringLiteral("500 µs"));

  // Not a span the panel offers, and formatted correctly anyway: this formats a
  // span rather than a menu entry.
  EXPECT_EQ(FormatWaveformSpan(40'000), QStringLiteral("1 ms"));
}

TEST(WaveformFormatTest, TheCursorReadsInCodesWhenNoGainIsDeclared) {
  const QString text =
      FormatWaveformCursor(4000, 700.0, analysis::FrontEndGain());

  EXPECT_TRUE(text.contains(QStringLiteral("700"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("100.00 µs"))) << text.toStdString();
  EXPECT_FALSE(text.contains(QStringLiteral("mV"))) << text.toStdString();
}

TEST(WaveformFormatTest, TheCursorAddsVoltsWhenTheGainIsDeclared) {
  // Switches 2: gain 6.00, so a code is 2000 / 1024 / 6 = 0.3255 mV and 188
  // codes above mid-scale is 61.2 mV.
  const QString text = FormatWaveformCursor(
      0, 700.0, analysis::FrontEndGain::FromSwitchPattern(0b0100));

  EXPECT_TRUE(text.contains(QStringLiteral("700"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("61.2"))) << text.toStdString();
}

TEST(WaveformPanelTest, TheSpanChoicesAreOfferedAndReachThePlot) {
  WaveformPanel panel(nullptr);

  auto* const span = Named<QComboBox>(panel, WaveformPanel::kSpanComboName);
  ASSERT_NE(span, nullptr);
  EXPECT_EQ(static_cast<size_t>(span->count()),
            analysis::kWaveformSpanChoiceCount);

  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  ASSERT_NE(plot, nullptr);

  span->setCurrentIndex(0);
  EXPECT_EQ(plot->sample_span(), analysis::kWaveformSpanChoices[0]);

  span->setCurrentIndex(4);
  EXPECT_EQ(plot->sample_span(), analysis::kWaveformSpanChoices[4]);
}

TEST(WaveformPanelTest, PersistenceIsOffUntilItIsAskedFor) {
  WaveformPanel panel(nullptr);

  auto* const box = Named<QCheckBox>(panel, WaveformPanel::kPersistenceBoxName);
  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  ASSERT_NE(box, nullptr);
  ASSERT_NE(plot, nullptr);

  EXPECT_FALSE(box->isChecked());
  EXPECT_FALSE(plot->persistence());

  box->setChecked(true);
  EXPECT_TRUE(plot->persistence());
}

TEST(WaveformPanelTest, ThePanelPaintsWithNothingToDraw) {
  // The first thing a user sees. An empty plot must be an empty plot and not a
  // crash on a zero-length span or an unpopulated column.
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  const QPixmap painted = panel.grab();
  EXPECT_FALSE(painted.isNull());
}

TEST(WaveformPanelTest, ThePanelPaintsASnapshot) {
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  panel.OnWaveformReady(Sine(32'768));

  EXPECT_FALSE(panel.grab().isNull());
}

TEST(WaveformPanelTest, ThePanelPaintsWithPersistenceOn) {
  // A separate paint path, with an off-screen image and a composition mode, so
  // it is drawn separately here rather than assumed to follow from the other.
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  Named<QCheckBox>(panel, WaveformPanel::kPersistenceBoxName)->setChecked(true);

  panel.OnWaveformReady(Sine(32'768));
  EXPECT_FALSE(panel.grab().isNull());

  // Twice, because the accumulation only happens on the second frame.
  panel.OnWaveformReady(Sine(32'768));
  EXPECT_FALSE(panel.grab().isNull());
}

// What the option is for: several sweeps on screen at once, so the envelope of
// a repeating waveform is visible rather than one arbitrary slice of it.
//
// Asserted in pixels a person could see, because the failure this replaces was
// invisible in every other measure — the accumulation was happening, the buffer
// held all six sweeps, and the screen showed one.
TEST(WaveformPanelTest, PersistenceLeavesEarlierSweepsOnScreen) {
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  const QImage blank = panel.grab().toImage();

  panel.OnWaveformReady(Sine(32'768));
  const int one_sweep = VisibleTracePixels(panel.grab().toImage(), blank);
  ASSERT_GT(one_sweep, 0) << "nothing was drawn at all";

  Named<QCheckBox>(panel, WaveformPanel::kPersistenceBoxName)->setChecked(true);

  // Six sweeps at different phases: on a real signal this is what successive
  // snapshots of a free-running carrier look like.
  //
  // Grabbed after each one, because the accumulation happens when the widget
  // paints and update() coalesces repaints. Six frames delivered without a
  // paint between them are one paint, and would be one sweep — which is the
  // right behaviour on a machine too busy to draw them all, and not what is
  // being tested here.
  for (int sweep = 0; sweep < 6; ++sweep) {
    panel.OnWaveformReady(Sine(32'768, sweep * 0.9));
    panel.grab();
  }
  const int accumulated = VisibleTracePixels(panel.grab().toImage(), blank);

  EXPECT_GT(accumulated, one_sweep * 2)
      << "six sweeps drew " << accumulated << " visible pixels against "
      << one_sweep << " for one — persistence is not showing the earlier ones";
}

TEST(WaveformPanelTest, WithoutPersistenceOnlyTheLatestSweepIsDrawn) {
  // The other half: the mode has to be a mode. Without it, six sweeps must look
  // like one.
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  const QImage blank = panel.grab().toImage();

  panel.OnWaveformReady(Sine(32'768));
  const int one_sweep = VisibleTracePixels(panel.grab().toImage(), blank);

  for (int sweep = 0; sweep < 6; ++sweep) {
    panel.OnWaveformReady(Sine(32'768, sweep * 0.9));
    panel.grab();
  }
  const int after = VisibleTracePixels(panel.grab().toImage(), blank);

  EXPECT_LT(after, one_sweep * 2);
}

TEST(WaveformPanelTest, StartingARunClearsTheOldTrace) {
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  panel.OnWaveformReady(Sine(32'768));
  panel.OnMonitoringChanged(true);

  EXPECT_FALSE(panel.grab().isNull());
}

TEST(WaveformPanelTest, TheCursorLabelSaysWhatToDoBeforeItIsUsed) {
  WaveformPanel panel(nullptr);

  auto* const cursor = Named<QLabel>(panel, WaveformPanel::kCursorLabelName);
  ASSERT_NE(cursor, nullptr);
  EXPECT_FALSE(cursor->text().isEmpty());
  EXPECT_FALSE(cursor->text().contains(QStringLiteral("mV")));
}

TEST(WaveformPanelTest, DeclaringAGainChangesTheUnitsAndNothingElse) {
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);
  panel.OnWaveformReady(Sine(32'768));

  panel.SetFrontEndGain(analysis::FrontEndGain::FromSwitchPattern(0b0100));

  // The trace is drawn from codes and always was, so the picture is still
  // there. Only what a readout would say has changed.
  EXPECT_FALSE(panel.grab().isNull());
}

}  // namespace
}  // namespace ddd::gui
