/************************************************************************

    test_amplitude_panel.cpp

    T1 tests for the amplitude history panel
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include <gtest/gtest.h>

#include <QColor>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <vector>

#include "amplitude_panel.h"
#include "front_end_gain.h"
#include "monitor_tap.h"
#include "signal_levels.h"
#include "theme_color_tokens.h"

namespace ddd::gui {
namespace {

template <typename T>
T* Named(const AmplitudePanel& panel, const char* name) {
  return panel.findChild<T*>(QLatin1String(name));
}

capture::CaptureStats Stats(double elapsed, uint16_t minimum, uint16_t maximum,
                            double rms = 200.0, uint64_t clipped = 0) {
  capture::CaptureStats stats;
  stats.elapsed_seconds = elapsed;
  stats.metrics.sample_count = 1'000'000;
  stats.metrics.recent_minimum_value = minimum;
  stats.metrics.recent_maximum_value = maximum;
  stats.metrics.recent_rms = rms;
  stats.metrics.clipped_high_count = clipped;
  return stats;
}

// Feeds statistics at the panel's own 20 Hz update rate for a stated duration,
// which is what turns them into history points. Returns the clock it reached,
// so a second call can carry on from where the first stopped rather than
// sending time backwards.
double Feed(AmplitudePanel& panel, double from_seconds, double duration,
            uint16_t minimum, uint16_t maximum, uint64_t clipped = 0) {
  double elapsed = from_seconds;
  const int steps = static_cast<int>(duration * 20.0);
  for (int step = 1; step <= steps; ++step) {
    elapsed = from_seconds + (static_cast<double>(step) / 20.0);
    panel.OnStatsUpdated(Stats(elapsed, minimum, maximum, 200.0, clipped));
  }
  return elapsed;
}

// The plot on its own, without the panel's controls around it.
//
// Grabbing the whole panel picks up the summary label and the Clear button, and
// a button's own chrome is close enough to a gridline's grey to be counted as
// one — which moves the apparent centre of the plot and makes a symmetrical
// drawing look lopsided. Ask the plot directly and the question is only about
// the plot.
QImage PlotImage(AmplitudePanel& panel) {
  panel.grab();  // lays the panel out, so the plot has its final size
  auto* const plot =
      panel.findChild<AmplitudePlot*>(QLatin1String(AmplitudePanel::kPlotName));
  return plot->grab().toImage();
}

// The rows on which a colour is drawn right across the plot — a rule line
// rather than a label in the same colour. Identified by width rather than by
// position, so the test knows nothing about the panel's internal margins.
std::vector<int> RuleRows(const QImage& shot, const QColor& wanted,
                          int minimum_width = 50) {
  const int kRuleWidthPixels = minimum_width;

  std::vector<int> rows;
  for (int y = 0; y < shot.height(); ++y) {
    int matched = 0;
    for (int x = 0; x < shot.width(); ++x) {
      const QColor pixel(shot.pixel(x, y));
      if (std::abs(pixel.red() - wanted.red()) <= 12 &&
          std::abs(pixel.green() - wanted.green()) <= 12 &&
          std::abs(pixel.blue() - wanted.blue()) <= 12) {
        ++matched;
      }
    }
    if (matched >= kRuleWidthPixels) {
      rows.push_back(y);
    }
  }
  return rows;
}

TEST(AmplitudeFormatTest, AnEmptyHistorySaysSoRatherThanShowingZeroes) {
  const analysis::AmplitudeHistory history;

  EXPECT_EQ(FormatAmplitudeSummary(history, analysis::FrontEndGain()),
            QStringLiteral("Nothing recorded yet"));
}

TEST(AmplitudeFormatTest, WithNoDeclarationTheSummaryStaysInCodes) {
  analysis::AmplitudeHistory history(10);
  analysis::AmplitudePoint point;
  point.minimum_code = 100;
  point.maximum_code = 900;
  history.Append(point);

  const QString text =
      FormatAmplitudeSummary(history, analysis::FrontEndGain());

  EXPECT_TRUE(text.contains(QStringLiteral("100"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("900"))) << text.toStdString();
  EXPECT_FALSE(text.contains(QStringLiteral("mV"))) << text.toStdString();
}

TEST(AmplitudeFormatTest, WithADeclarationTheSummaryCarriesTheVoltage) {
  analysis::AmplitudeHistory history(10);
  analysis::AmplitudePoint point;
  point.minimum_code = 0;
  point.maximum_code = 1023;
  history.Append(point);

  // Switches 2, gain 6.00: the whole converter is 333 mV p-p at the BNC, and
  // 1023 codes of it is a shade under.
  const QString text = FormatAmplitudeSummary(
      history, analysis::FrontEndGain::FromSwitchPattern(0b0100));

  EXPECT_TRUE(text.contains(QStringLiteral("333"))) << text.toStdString();
}

TEST(AmplitudeFormatTest, ClippingIsCalledOutRatherThanLeftToBeNoticed) {
  analysis::AmplitudeHistory history(10);
  analysis::AmplitudePoint point;
  point.minimum_code = 0;
  point.maximum_code = 1023;
  point.clipped_count = 42;
  history.Append(point);

  const QString text =
      FormatAmplitudeSummary(history, analysis::FrontEndGain());

  EXPECT_TRUE(text.contains(QStringLiteral("42"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("clipped"))) << text.toStdString();
}

TEST(AmplitudePanelTest, StatisticsBecomeHistoryPoints) {
  AmplitudePanel panel(nullptr);

  EXPECT_TRUE(panel.history().empty());

  Feed(panel, 0.0, 1.0, 300, 700);

  // Ten points a second, so a second of updates is about ten points. Not
  // asserted exactly: the sampler closes an interval when an update crosses its
  // boundary, and the last one may still be open.
  EXPECT_GE(panel.history().size(), 8u);
  EXPECT_LE(panel.history().size(), 11u);
}

TEST(AmplitudePanelTest, TheHistoryHoldsTheExtremesThatWereSeen) {
  AmplitudePanel panel(nullptr);

  const double clock = Feed(panel, 0.0, 0.5, 400, 600);
  Feed(panel, clock, 0.5, 100, 900);

  EXPECT_EQ(panel.history().TroughCode(), 100);
  EXPECT_EQ(panel.history().PeakCode(), 900);
}

TEST(AmplitudePanelTest, StartingARunClearsTheHistory) {
  AmplitudePanel panel(nullptr);

  Feed(panel, 0.0, 1.0, 300, 700);
  ASSERT_FALSE(panel.history().empty());

  panel.OnMonitoringChanged(true);

  EXPECT_TRUE(panel.history().empty());
}

TEST(AmplitudePanelTest, StoppingLeavesTheHistoryOnScreen) {
  // The record of what just happened. Clearing it at the end would throw away
  // the evidence at exactly the moment somebody wanted to look at it.
  AmplitudePanel panel(nullptr);

  Feed(panel, 0.0, 1.0, 300, 700);
  const size_t recorded = panel.history().size();

  panel.OnMonitoringChanged(false);

  EXPECT_EQ(panel.history().size(), recorded);
}

// The property the whole gain design rests on: the history is stored in codes,
// so a correction to the declaration re-labels what was already recorded rather
// than invalidating it.
TEST(AmplitudePanelTest, CorrectingTheGainRelabelsHistoryAlreadyRecorded) {
  AmplitudePanel panel(nullptr);
  panel.resize(600, 300);

  Feed(panel, 0.0, 2.0, 0, 1023);
  const size_t recorded = panel.history().size();
  ASSERT_GT(recorded, 0u);

  auto* const summary = Named<QLabel>(panel, AmplitudePanel::kSummaryLabelName);
  ASSERT_NE(summary, nullptr);
  ASSERT_FALSE(summary->text().contains(QStringLiteral("mV")));

  panel.SetFrontEndGain(analysis::FrontEndGain::FromSwitchPattern(0b0100));

  EXPECT_TRUE(summary->text().contains(QStringLiteral("333")))
      << summary->text().toStdString();
  EXPECT_EQ(panel.history().size(), recorded);

  // And again, to a different setting: switches 1 is a gain of 8.5, so the same
  // codes are now 235 mV p-p. Nothing was re-acquired.
  panel.SetFrontEndGain(analysis::FrontEndGain::FromSwitchPattern(0b1000));

  EXPECT_TRUE(summary->text().contains(QStringLiteral("235")))
      << summary->text().toStdString();
  EXPECT_EQ(panel.history().size(), recorded);
}

TEST(AmplitudePanelTest, TheDeclaredGainIsShownBesideTheFigures) {
  // So that a number on screen always says what it was computed with.
  AmplitudePanel panel(nullptr);

  auto* const label = Named<QLabel>(panel, AmplitudePanel::kGainLabelName);
  ASSERT_NE(label, nullptr);
  EXPECT_TRUE(label->text().contains(QStringLiteral("Not declared")))
      << label->text().toStdString();

  panel.SetFrontEndGain(analysis::FrontEndGain::FromSwitchPattern(0b1010));

  EXPECT_TRUE(label->text().contains(QStringLiteral("1010")))
      << label->text().toStdString();
}

// The nominal capture level is 75% of the converter's range, and the signal
// swings both ways about 0 V — so the bound has to be marked on both sides. A
// mark on one side only would say nothing about the half of the waveform that
// was already closer to the rail.
//
// Checked against the panel's own gridlines rather than against pixel numbers,
// so it survives any change to the margins: the top and bottom gridlines are
// the ends of the code range, and the nominal marks have to sit the nominal
// proportion of the way from the middle to each of them.
//
// That proportion is read from the same constant the panel draws with, so this
// deliberately does not pin the 75% — it pins the drawing. The figure itself is
// pinned in tests/analysis/test_signal_levels.cpp, which is where it is
// decided.
TEST(AmplitudePanelTest, TheNominalLevelIsMarkedOnBothSidesOfZero) {
  AmplitudePanel panel(nullptr);
  panel.resize(600, 300);

  const bool dark = theme_tokens::IsDarkPalette(panel.palette());
  const QImage shot = PlotImage(panel);

  const std::vector<int> grid =
      RuleRows(shot, theme_tokens::GridLine(panel.palette()));
  ASSERT_GE(grid.size(), 2U) << "no gridlines to measure against";

  const double top = grid.front();
  const double bottom = grid.back();
  const double centre = (top + bottom) / 2.0;

  const std::vector<int> marks =
      RuleRows(shot, theme_tokens::PlotColor(
                         theme_tokens::PlotColorToken::kNominalLimit, dark));

  ASSERT_EQ(marks.size(), 2U)
      << "expected one nominal mark above mid-scale and one below";

  const double upper = marks.front();
  const double lower = marks.back();

  // Symmetrical about mid-scale, because the two codes are.
  EXPECT_NEAR(centre - upper, lower - centre, 2.0);

  // And three quarters of the way out, not at the ends: these are a nominal
  // level with headroom above it, not the clip levels.
  EXPECT_NEAR(centre - upper, analysis::kNominalPeakProportion * (centre - top),
              2.0);
  EXPECT_NEAR(lower - centre,
              analysis::kNominalPeakProportion * (bottom - centre), 2.0);
}

// Drawn before there is anything to judge against them, because they are a
// scale reference as much as a warning.
TEST(AmplitudePanelTest, TheNominalMarksAreThereBeforeAnySignalIs) {
  AmplitudePanel panel(nullptr);
  panel.resize(600, 300);

  ASSERT_TRUE(panel.history().empty());

  const QColor nominal =
      theme_tokens::PlotColor(theme_tokens::PlotColorToken::kNominalLimit,
                              theme_tokens::IsDarkPalette(panel.palette()));

  EXPECT_EQ(RuleRows(PlotImage(panel), nominal).size(), 2U);
}

// The RMS trace measures a magnitude, and the signal it measures swings both
// ways about 0 V. Drawn on one side only it reads as though the waveform were
// sitting above the centre line, which is exactly what it is not doing.
TEST(AmplitudePanelTest, TheRmsTraceIsDrawnOnBothSidesOfZero) {
  AmplitudePanel panel(nullptr);
  panel.resize(600, 300);

  // A steady level, so both traces are straight lines running the full width
  // and can be found by colour.
  Feed(panel, 0.0, 3.0, 300, 700);
  ASSERT_FALSE(panel.history().empty());

  const QColor trace =
      theme_tokens::PlotColor(theme_tokens::PlotColorToken::kAmplitudeTrace,
                              theme_tokens::IsDarkPalette(panel.palette()));

  const QImage shot = PlotImage(panel);

  const std::vector<int> rows = RuleRows(shot, trace);
  ASSERT_FALSE(rows.empty()) << "no RMS trace was drawn at all";

  // Two traces, above and below the centre, and symmetrical about it. Compared
  // through the gridlines so the check knows nothing about the panel's margins.
  const std::vector<int> grid =
      RuleRows(shot, theme_tokens::GridLine(panel.palette()));
  ASSERT_GE(grid.size(), 2U);
  const double centre = (grid.front() + grid.back()) / 2.0;

  const double above = rows.front();
  const double below = rows.back();

  EXPECT_LT(above, centre) << "nothing above the centre line";
  EXPECT_GT(below, centre) << "nothing below the centre line";
  EXPECT_NEAR(centre - above, below - centre, 3.0);
}

TEST(AmplitudePanelTest, TheHistoryCanBeClearedWithoutStoppingTheRun) {
  // For after a cable or a gain setting has been changed, when what came before
  // is no longer what is being measured.
  AmplitudePanel panel(nullptr);

  Feed(panel, 0.0, 1.0, 300, 700);
  ASSERT_FALSE(panel.history().empty());

  auto* const clear =
      Named<QPushButton>(panel, AmplitudePanel::kClearButtonName);
  ASSERT_NE(clear, nullptr);
  clear->click();

  EXPECT_TRUE(panel.history().empty());
}

TEST(AmplitudePanelTest, ClearingResetsTheSamplerWithTheHistory) {
  // Left alone, the sampler would still hold the clip total it last saw, and
  // the first point after a clear would report the difference against a run of
  // history that no longer exists.
  AmplitudePanel panel(nullptr);

  const double clock = Feed(panel, 0.0, 1.0, 0, 1023, 5000);
  Named<QPushButton>(panel, AmplitudePanel::kClearButtonName)->click();

  Feed(panel, clock, 1.0, 400, 600, 5000);

  ASSERT_FALSE(panel.history().empty());
  EXPECT_EQ(panel.history().TotalClipped(), 0U)
      << "clip counts carried across a clear";
}

TEST(AmplitudePanelTest, TheSpanIsEverythingHeldUntilItIsAskedToMatch) {
  AmplitudePanel panel(nullptr);

  auto* const span = Named<QComboBox>(panel, AmplitudePanel::kSpanComboName);
  auto* const plot =
      panel.findChild<AmplitudePlot*>(QLatin1String(AmplitudePanel::kPlotName));
  ASSERT_NE(span, nullptr);
  ASSERT_NE(plot, nullptr);

  EXPECT_EQ(span->count(), 2);
  EXPECT_LE(plot->window_seconds(), 0.0) << "a window was applied unasked";

  panel.SetMatchedWindowSeconds(31.5);
  EXPECT_LE(plot->window_seconds(), 0.0)
      << "the spectrogram's window was adopted without being chosen";

  span->setCurrentIndex(1);
  EXPECT_DOUBLE_EQ(plot->window_seconds(), 31.5);
}

TEST(AmplitudePanelTest, MatchingBeforeTheSpectrogramKnowsItsWindowShowsAll) {
  // The spectrogram needs two frames to measure its own frame rate, and until
  // it has there is no span to match. Everything held is a better answer than
  // nothing at all.
  AmplitudePanel panel(nullptr);

  Named<QComboBox>(panel, AmplitudePanel::kSpanComboName)->setCurrentIndex(1);

  auto* const plot =
      panel.findChild<AmplitudePlot*>(QLatin1String(AmplitudePanel::kPlotName));
  EXPECT_LE(plot->window_seconds(), 0.0);

  panel.SetMatchedWindowSeconds(12.0);
  EXPECT_DOUBLE_EQ(plot->window_seconds(), 12.0);
}

// The point of matching: a moment on one panel lines up with the same moment on
// the other. That only holds if a point's distance from the right-hand edge is
// how long ago it was, rather than where it fell in the list.
TEST(AmplitudePanelTest, ANarrowedSpanDrawsOnlyTheRecentPast) {
  AmplitudePanel panel(nullptr);
  panel.resize(600, 300);

  // Ten seconds of quiet, then two seconds of a much wider signal.
  const double clock = Feed(panel, 0.0, 10.0, 480, 540);
  Feed(panel, clock, 2.0, 100, 900);

  auto* const plot =
      panel.findChild<AmplitudePlot*>(QLatin1String(AmplitudePanel::kPlotName));

  // A panel with nothing in it, to subtract: the gridlines, the nominal marks
  // and the axis are in both, and what is left is the signal.
  AmplitudePanel blank(nullptr);
  blank.resize(600, 300);
  const QImage baseline = PlotImage(blank);

  // Measured in one column near the left-hand edge, which is the oldest moment
  // on display. Counting the whole plot would count the union of both parts and
  // come out the same either way — which is what the first version of this test
  // did, and it passed against a panel that ignored the span entirely.
  //
  // Measured as "not the background" rather than as the envelope's own colour,
  // because the envelope is drawn semi-transparent: what reaches the screen is
  // a blend, and matching the token colour finds only the odd opaque pixel.
  const auto height_at_left = [&] {
    const QImage shot = PlotImage(panel);
    constexpr int kJustInsideThePlot = 70;

    int count = 0;
    for (int y = 0; y < shot.height() && y < baseline.height(); ++y) {
      if (shot.pixel(kJustInsideThePlot, y) !=
          baseline.pixel(kJustInsideThePlot, y)) {
        ++count;
      }
    }
    return count;
  };

  // Showing everything, the left edge is twelve seconds ago — the quiet part.
  plot->SetWindowSeconds(0.0);
  const int all = height_at_left();

  // Narrowed to the last two seconds, the left edge is inside the wide part.
  plot->SetWindowSeconds(2.0);
  const int narrowed = height_at_left();

  EXPECT_GT(all, 0) << "no signal drawn at all";
  EXPECT_GT(narrowed, all * 2)
      << "the signal at the left edge was " << all
      << " pixels tall showing everything and " << narrowed
      << " narrowed to the recent past; the span is not reaching the drawing";
}

TEST(AmplitudePanelTest, ThePanelPaintsEmptyAndFull) {
  AmplitudePanel panel(nullptr);
  panel.resize(600, 300);

  EXPECT_FALSE(panel.grab().isNull());

  Feed(panel, 0.0, 2.0, 0, 1023, 5);
  EXPECT_FALSE(panel.grab().isNull());
}

TEST(AmplitudePanelTest, ARunWithNoSamplesYetRecordsNothing) {
  AmplitudePanel panel(nullptr);

  capture::CaptureStats stats;
  stats.elapsed_seconds = 5.0;
  panel.OnStatsUpdated(stats);

  EXPECT_TRUE(panel.history().empty());
}

}  // namespace
}  // namespace ddd::gui
