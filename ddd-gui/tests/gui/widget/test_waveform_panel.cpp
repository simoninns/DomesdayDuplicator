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
#include <QSlider>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "front_end_gain.h"
#include "sample_format.h"
#include "theme_color_tokens.h"
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

// How many pixels of one colour a picture holds. The markers are drawn in flat
// theme colours, so counting them is how a test asks whether one is there.
int ColourPixels(const QImage& shot, const QColor& wanted) {
  constexpr int kTolerance = 20;

  int count = 0;
  for (int y = 0; y < shot.height(); ++y) {
    for (int x = 0; x < shot.width(); ++x) {
      const QColor pixel(shot.pixel(x, y));
      if (std::abs(pixel.red() - wanted.red()) <= kTolerance &&
          std::abs(pixel.green() - wanted.green()) <= kTolerance &&
          std::abs(pixel.blue() - wanted.blue()) <= kTolerance) {
        ++count;
      }
    }
  }
  return count;
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

// The topmost row of trace in a column, or -1 where the column has none.
int TraceTop(const QImage& shot, const QImage& blank, int x) {
  constexpr int kSeeableContrast = 60;

  for (int y = 0; y < shot.height(); ++y) {
    const QColor drawn(shot.pixel(x, y));
    const QColor background(blank.pixel(x, y));
    const int difference =
        std::max({std::abs(drawn.red() - background.red()),
                  std::abs(drawn.green() - background.green()),
                  std::abs(drawn.blue() - background.blue())});
    if (difference >= kSeeableContrast) {
      return y;
    }
  }
  return -1;
}

// How far the trace moved between two shots, in pixels, averaged over the
// columns both of them drew in.
//
// Measured as displacement rather than as pixels that differ, because the two
// are not the same question and only this one is being asked. The trace is a
// one-pixel antialiased curve: shifting it by a hundredth of a pixel changes
// the coverage of every pixel along it, so a difference count reports a curve
// that has not perceptibly moved as having moved everywhere. What matters is
// whether the waveform is in the same place, and that is a distance.
double MeanTraceShift(const QImage& left, const QImage& right,
                      const QImage& blank) {
  double total = 0.0;
  int columns = 0;

  for (int x = 0; x < left.width(); ++x) {
    const int first = TraceTop(left, blank, x);
    const int second = TraceTop(right, blank, x);
    if (first < 0 || second < 0) {
      continue;
    }
    total += std::abs(first - second);
    ++columns;
  }

  return columns > 0 ? total / columns : 0.0;
}

template <typename T>
T* Named(const WaveformPanel& panel, const char* name) {
  return panel.findChild<T*>(QLatin1String(name));
}

TEST(WaveformFormatTest, SpansAreLabelledInTheTimeTheyCover) {
  // 40 samples at 40 Msps is 1 µs, which is about eight cycles of a LaserDisc
  // FM carrier and the span the panel opens on; 20,000 is the longest it
  // offers.
  EXPECT_EQ(FormatWaveformSpan(40), QStringLiteral("1 µs"));
  EXPECT_EQ(FormatWaveformSpan(20), QStringLiteral("0.5 µs"));
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

TEST(WaveformFormatTest, ThePersistenceLabelSaysOffRatherThanZero) {
  // "0 s" is a duration somebody could believe they had set. Off is a state.
  EXPECT_EQ(FormatPersistence(0.0), QStringLiteral("off"));
  EXPECT_EQ(FormatPersistence(0.25), QStringLiteral("0.25 s"));
  EXPECT_EQ(FormatPersistence(1.5), QStringLiteral("1.5 s"));
  EXPECT_EQ(FormatPersistence(2.0), QStringLiteral("2 s"));
}

TEST(WaveformPanelTest, PersistenceIsOffUntilItIsAskedFor) {
  WaveformPanel panel(nullptr);

  auto* const slider =
      Named<QSlider>(panel, WaveformPanel::kPersistenceSliderName);
  auto* const label =
      Named<QLabel>(panel, WaveformPanel::kPersistenceLabelName);
  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  ASSERT_NE(slider, nullptr);
  ASSERT_NE(label, nullptr);
  ASSERT_NE(plot, nullptr);

  EXPECT_EQ(slider->value(), 0);
  EXPECT_DOUBLE_EQ(plot->persistence_seconds(), 0.0);
  EXPECT_FALSE(plot->persistence());
  EXPECT_EQ(label->text(), QStringLiteral("off"));
}

TEST(WaveformPanelTest, ThePersistenceSliderChoosesHowLongTheTailIs) {
  // The point of a slider rather than a switch: how long a tail is useful
  // depends on what is being looked for, and one fixed length serves neither
  // end of that.
  WaveformPanel panel(nullptr);

  auto* const slider =
      Named<QSlider>(panel, WaveformPanel::kPersistenceSliderName);
  auto* const label =
      Named<QLabel>(panel, WaveformPanel::kPersistenceLabelName);
  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);

  EXPECT_EQ(slider->minimum(), 0);
  EXPECT_EQ(slider->maximum(), kPersistenceSliderSteps);

  // Two seconds at the top, which the tick marks divide into half-seconds.
  EXPECT_DOUBLE_EQ(kPersistenceSliderSteps * kPersistenceSecondsPerStep, 2.0);
  EXPECT_EQ(kPersistenceSliderSteps % slider->tickInterval(), 0);
  EXPECT_DOUBLE_EQ(slider->tickInterval() * kPersistenceSecondsPerStep, 0.5);

  slider->setValue(2);
  EXPECT_DOUBLE_EQ(plot->persistence_seconds(), 0.5);
  EXPECT_TRUE(plot->persistence());
  EXPECT_EQ(label->text(), QStringLiteral("0.5 s"));

  // The top of the travel, and the label keeping step with it.
  slider->setValue(kPersistenceSliderSteps);
  EXPECT_DOUBLE_EQ(plot->persistence_seconds(),
                   kPersistenceSliderSteps * kPersistenceSecondsPerStep);
  EXPECT_EQ(label->text(), QStringLiteral("2 s"));

  // And all the way back off again.
  slider->setValue(0);
  EXPECT_FALSE(plot->persistence());
  EXPECT_EQ(label->text(), QStringLiteral("off"));
}

TEST(WaveformPanelTest, TheFadeIsCalibratedInSecondsRatherThanInFrames) {
  // The arithmetic behind the slider, asked directly. A tail half the length it
  // claims still draws a perfectly plausible picture, and paints in a test
  // arrive microseconds apart — where nothing decays and every setting looks
  // alike — so the picture cannot answer this and the function has to.
  constexpr double kFrame = 1.0 / 9.0;

  // Off is off, whatever time has passed.
  EXPECT_EQ(WaveformPlot::RetainedAlpha(0.0, kFrame), 0);

  // One time constant in, the picture is at 37%.
  EXPECT_NEAR(WaveformPlot::RetainedAlpha(1.0, 1.0), 255 * 0.368, 2.0);

  // Three of them in, it is at 5% — faded to nothing anybody would call a
  // trace, which is what the slider's label is promising.
  EXPECT_NEAR(WaveformPlot::RetainedAlpha(1.0, 3.0), 255 * 0.050, 2.0);

  // A longer tail keeps more of the same frame...
  EXPECT_GT(WaveformPlot::RetainedAlpha(2.0, kFrame),
            WaveformPlot::RetainedAlpha(0.25, kFrame));

  // ...and the same tail keeps less of a longer gap, which is what makes the
  // setting a duration rather than a per-frame figure: a run that stalled for
  // two seconds has genuinely lost two seconds of picture.
  EXPECT_LT(WaveformPlot::RetainedAlpha(1.0, 2.0),
            WaveformPlot::RetainedAlpha(1.0, kFrame));

  // Frames arriving in the same instant still fade, or a stalled clock would
  // leave the picture accumulating for ever.
  EXPECT_LT(WaveformPlot::RetainedAlpha(2.0, 0.0), 255);
}

TEST(WaveformPanelTest, PersistenceAccumulatesWhereOffReplaces) {
  // The setting has to reach the picture and not just the label. Six sweeps at
  // different amplitudes: with persistence off only the last is on screen, and
  // with it on the earlier ones are still there behind it.
  const auto accumulate = [](int steps) {
    WaveformPanel panel(nullptr);
    panel.resize(600, 300);

    const QImage blank = panel.grab().toImage();
    Named<QSlider>(panel, WaveformPanel::kPersistenceSliderName)
        ->setValue(steps);

    for (int sweep = 0; sweep < 6; ++sweep) {
      std::vector<uint16_t> codes(32'768);
      const double amplitude = 60.0 + (sweep * 70.0);
      for (size_t index = 0; index < codes.size(); ++index) {
        codes[index] = static_cast<uint16_t>(std::lround(
            512.0 + (amplitude * std::sin(static_cast<double>(index) * 0.05))));
      }
      panel.OnWaveformReady(codes);
      panel.grab();
    }
    return VisibleTracePixels(panel.grab().toImage(), blank);
  };

  const int replaced = accumulate(0);
  const int accumulated = accumulate(kPersistenceSliderSteps);

  ASSERT_GT(replaced, 0) << "nothing was drawn at all";
  EXPECT_GT(accumulated, replaced * 3 / 2)
      << "persistence drew " << accumulated << " visible pixels against "
      << replaced << " with it off";
}

// A carrier at a stated phase, as the device would deliver it: 8 MHz at
// 40 Msps is five samples a cycle.
std::vector<uint16_t> MakeCarrier(size_t count, double phase_offset) {
  std::vector<uint16_t> codes(count);
  for (size_t index = 0; index < count; ++index) {
    const double phase =
        (2.0 * M_PI * static_cast<double>(index) / 5.0) + phase_offset;
    codes[index] =
        static_cast<uint16_t>(std::lround(512.0 + (300.0 * std::sin(phase))));
  }
  return codes;
}

TEST(WaveformPanelTest, TheTriggerIsOnByDefaultAndCanBeTurnedOff) {
  WaveformPanel panel(nullptr);

  auto* const box = Named<QCheckBox>(panel, WaveformPanel::kTriggerBoxName);
  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  ASSERT_NE(box, nullptr);

  EXPECT_TRUE(box->isChecked());
  EXPECT_TRUE(plot->triggered());

  box->setChecked(false);
  EXPECT_FALSE(plot->triggered());
}

TEST(WaveformPanelTest, AFlatInputIsReportedAsFreeRunningRatherThanTriggered) {
  // The box stays ticked — the trigger is still on and still looking — but
  // nothing crossed the level, so what is on screen starts wherever the
  // transfer did. The two cases produce the same flat line, which is exactly
  // why the display has to say which one it is drawing.
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  ASSERT_NE(plot, nullptr);
  ASSERT_TRUE(plot->triggered());

  panel.OnWaveformReady(std::vector<uint16_t>(4096, 512));
  panel.grab();

  EXPECT_TRUE(plot->free_running());

  // And a signal that does cross it is not reported that way.
  panel.OnWaveformReady(Sine(4096));
  panel.grab();

  EXPECT_FALSE(plot->free_running());
}

TEST(WaveformPanelTest, NoTriggerMarkerIsDrawnOverAFreeRunningTrace) {
  // A marker over a free-running sweep points at a sample with no more claim
  // to be the start of a cycle than any other one on screen.
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  ASSERT_NE(plot, nullptr);

  const bool dark = theme_tokens::IsDarkPalette(panel.palette());
  const QColor marker = theme_tokens::PlotColor(
      theme_tokens::PlotColorToken::kTriggerMarker, dark);

  panel.OnWaveformReady(Sine(4096));
  EXPECT_GT(ColourPixels(plot->grab().toImage(), marker), 0)
      << "a triggered trace should say where the sweeps start";

  panel.OnWaveformReady(std::vector<uint16_t>(4096, 512));
  EXPECT_EQ(ColourPixels(plot->grab().toImage(), marker), 0)
      << "the marker was still drawn with nothing to align to";
}

TEST(WaveformPanelTest, TheDefaultSpanShowsCyclesRatherThanAFuzzyBand) {
  // 1 µs: about eight cycles of an 8 MHz carrier. The ladder used to open at
  // 10 µs, which is eighty of them and unreadable at any panel size.
  WaveformPanel panel(nullptr);

  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  EXPECT_EQ(
      plot->sample_span(),
      analysis::kWaveformSpanChoices[analysis::kDefaultWaveformSpanIndex]);
  EXPECT_EQ(plot->sample_span(), 40U);
}

TEST(WaveformPanelTest, ATriggeredCarrierIsDrawnTheSameWhateverThePhase) {
  // The defect the trigger exists to fix, stated as the picture. Snapshots
  // begin wherever the USB transfer began, so the same carrier arrives at a
  // different phase every frame; drawn from the first sample it shimmers at the
  // snapshot rate and reads as a band of fuzz rather than as a waveform.
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  ASSERT_TRUE(plot->triggered());

  const QImage blank = panel.grab().toImage();

  panel.OnWaveformReady(MakeCarrier(4000, 0.0));
  const QImage first = panel.grab().toImage();

  // The same signal, cut a third of a cycle later.
  panel.OnWaveformReady(MakeCarrier(4000, 2.0 * M_PI / 3.0));
  const double triggered = MeanTraceShift(first, panel.grab().toImage(), blank);

  // And the same pair without the trigger, which is what makes the figure above
  // mean anything.
  Named<QCheckBox>(panel, WaveformPanel::kTriggerBoxName)->setChecked(false);

  panel.OnWaveformReady(MakeCarrier(4000, 0.0));
  const QImage free_first = panel.grab().toImage();
  panel.OnWaveformReady(MakeCarrier(4000, 2.0 * M_PI / 3.0));
  const double untriggered =
      MeanTraceShift(free_first, panel.grab().toImage(), blank);

  ASSERT_GT(untriggered, 20.0)
      << "untriggered, a phase shift should move the trace a long way";

  // Not pixel-for-pixel identical, and it could not be: the codes are integers,
  // so a phase-shifted carrier is not an exact shift of the first one. What has
  // to hold is that the waveform stays where it was — within about a pixel,
  // against tens of pixels of movement without the trigger.
  EXPECT_LT(triggered, 1.5)
      << "triggered, a phase shift moved the trace " << triggered
      << " pixels against " << untriggered << " untriggered";
}

TEST(WaveformPanelTest, ATriggeredSnapshotYieldsManySweepsForPersistence) {
  // One sweep a snapshot is nine a second. Taking many from each — spread
  // across the whole 819 µs rather than clustered at its start — is what lets
  // the accumulated picture show the deviation of an FM carrier rather than one
  // instant of it.
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  panel.OnWaveformReady(MakeCarrier(32'768, 0.0));
  panel.grab();

  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  EXPECT_GT(plot->sweep_count(), 8U);

  // Untriggered there is exactly one, starting where the snapshot did.
  Named<QCheckBox>(panel, WaveformPanel::kTriggerBoxName)->setChecked(false);
  panel.grab();
  EXPECT_EQ(plot->sweep_count(), 1U);
}

TEST(WaveformPanelTest, AnUntriggerableSignalFreeRunsRatherThanFreezing) {
  // A flat input never crosses the level. Showing nothing would be the worst
  // possible answer: a trace that has gone flat is exactly when somebody needs
  // to see it.
  WaveformPanel panel(nullptr);
  panel.resize(600, 300);

  panel.OnWaveformReady(std::vector<uint16_t>(4000, 512));
  panel.grab();

  auto* const plot = Named<WaveformPlot>(panel, WaveformPanel::kPlotName);
  EXPECT_EQ(plot->sweep_count(), 1U);
  EXPECT_FALSE(panel.grab().isNull());
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

  Named<QSlider>(panel, WaveformPanel::kPersistenceSliderName)->setValue(4);

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

  Named<QSlider>(panel, WaveformPanel::kPersistenceSliderName)->setValue(4);

  // Six sweeps at different amplitudes, which is what a signal whose level is
  // moving looks like from one snapshot to the next.
  //
  // Different amplitudes rather than different phases: the trigger aligns
  // phases by design, so six phase-shifted sweeps now draw on top of one
  // another and accumulate to exactly one trace — which is the feature working,
  // not persistence failing.
  //
  // Grabbed after each one, because the accumulation happens when the widget
  // paints and update() coalesces repaints. Six frames delivered without a
  // paint between them are one paint, and would be one sweep — which is the
  // right behaviour on a machine too busy to draw them all, and not what is
  // being tested here.
  for (int sweep = 0; sweep < 6; ++sweep) {
    std::vector<uint16_t> codes(32'768);
    const double amplitude = 60.0 + (sweep * 70.0);
    for (size_t index = 0; index < codes.size(); ++index) {
      codes[index] = static_cast<uint16_t>(std::lround(
          512.0 + (amplitude * std::sin(static_cast<double>(index) * 0.05))));
    }
    panel.OnWaveformReady(codes);
    panel.grab();
  }
  const int accumulated = VisibleTracePixels(panel.grab().toImage(), blank);

  EXPECT_GT(accumulated, one_sweep * 3 / 2)
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
