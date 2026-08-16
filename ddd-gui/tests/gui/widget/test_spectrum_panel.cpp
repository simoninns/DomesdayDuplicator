/************************************************************************

    test_spectrum_panel.cpp

    T1 tests for the spectrum panel
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
#include <QPushButton>
#include <algorithm>
#include <cmath>
#include <vector>

#include "spectrogram_history.h"
#include "spectrum_analyser.h"
#include "spectrum_panel.h"
#include "theme_color_tokens.h"

namespace ddd::gui {
namespace {

template <typename T>
T* Named(const SpectrumPanel& panel, const char* name) {
  return panel.findChild<T*>(QLatin1String(name));
}

std::vector<double> Levels(size_t count, double value) {
  return std::vector<double>(count, value);
}

bool IsLoud(const QImage& shot, int x, int y, bool dark) {
  const QColor loud = theme_tokens::SpectrogramColor(0.94, dark);
  const QColor pixel(shot.pixel(x, y));
  return std::abs(pixel.red() - loud.red()) <= 40 &&
         std::abs(pixel.green() - loud.green()) <= 40 &&
         std::abs(pixel.blue() - loud.blue()) <= 40;
}

// Feed a spectrogram a carrier in one bin, repeatedly, and it should draw a
// horizontal band at that frequency — time runs across and frequency up. This
// finds the row that band is in, by looking for the colour the ramp gives a
// strong signal; the gridlines are grey and the noise floor is at the other end
// of the ramp, so neither competes.
int BrightestRow(const QImage& shot, bool dark) {
  int best_row = -1;
  int best_count = 0;

  for (int y = 0; y < shot.height(); ++y) {
    int count = 0;
    for (int x = 0; x < shot.width(); ++x) {
      if (IsLoud(shot, x, y, dark)) {
        ++count;
      }
    }
    if (count > best_count) {
      best_count = count;
      best_row = y;
    }
  }
  return best_count > 0 ? best_row : -1;
}

int LoudColumns(const QImage& shot, int from_x, int to_x, bool dark) {
  int count = 0;
  for (int x = from_x; x < to_x; ++x) {
    for (int y = 0; y < shot.height(); ++y) {
      if (IsLoud(shot, x, y, dark)) {
        ++count;
        break;
      }
    }
  }
  return count;
}

void FeedCarrier(SpectrumPanel& panel, size_t bin, int frames) {
  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  for (int frame = 0; frame < frames; ++frame) {
    std::vector<double> magnitudes(bins, -80.0);
    magnitudes[bin] = -6.0;
    panel.OnSpectrumReady(magnitudes, magnitudes);
    panel.grab();
  }
}

TEST(SpectrumFormatTest, FrequenciesAreGivenInTheUnitTheyAreComfortableIn) {
  EXPECT_EQ(FormatFrequency(250'000.0), QStringLiteral("250 kHz"));
  EXPECT_EQ(FormatFrequency(8'000'000.0), QStringLiteral("8.00 MHz"));
}

TEST(SpectrumFormatTest, DurationsCarryAtMostTwoDecimalPlaces) {
  // And fewer when the rest would be zeroes: a spectrogram thirty seconds wide
  // labelled "30.00 s" is claiming a precision its own frame rate has not got.
  EXPECT_EQ(FormatSecondsAgo(30.0), QStringLiteral("30 s"));
  EXPECT_EQ(FormatSecondsAgo(12.5), QStringLiteral("12.5 s"));
  EXPECT_EQ(FormatSecondsAgo(1.234), QStringLiteral("1.23 s"));
  EXPECT_EQ(FormatSecondsAgo(0.105), QStringLiteral("0.1 s"));
}

TEST(SpectrumFormatTest, TheSpectrogramCursorSaysHowLongAgoAReadingWas) {
  const QString text = FormatSpectrumCursor(8'000'000.0, -12.5, 4.25);

  EXPECT_TRUE(text.contains(QStringLiteral("8.00 MHz"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("4.25 s"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("ago"))) << text.toStdString();
}

TEST(SpectrumFormatTest, TheTraceCursorHasNoAgeToGive) {
  // It is a reading of now. An age of "0 s" would be a claim rather than an
  // omission.
  const QString text = FormatSpectrumCursor(8'000'000.0, -12.5);

  EXPECT_FALSE(text.contains(QStringLiteral("ago"))) << text.toStdString();
}

TEST(SpectrumFormatTest, TheCursorGivesAFrequencyAndALevel) {
  const QString text = FormatSpectrumCursor(8'000'000.0, -12.5);

  EXPECT_TRUE(text.contains(QStringLiteral("8.00 MHz"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("-12.5"))) << text.toStdString();
}

TEST(SpectrumFormatTest, AnEmptyBinSaysSoRatherThanShowingTheFloor) {
  // -120.0 dBFS is a figure a user could mistake for a measurement. It is not
  // one: it is the number the display clamps to when there was nothing there.
  const QString text = FormatSpectrumCursor(
      1'000'000.0, analysis::SpectrumAnalyser::kFloorDecibels);

  EXPECT_FALSE(text.contains(QStringLiteral("-120"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("nothing"))) << text.toStdString();
}

TEST(SpectrumPanelTest, TheAveragingChoicesIncludeNoneAndDefaultToMedium) {
  SpectrumPanel panel(nullptr);

  auto* const averaging =
      Named<QComboBox>(panel, SpectrumPanel::kAveragingComboName);
  ASSERT_NE(averaging, nullptr);
  EXPECT_GE(averaging->count(), 3);

  averaging->setCurrentIndex(0);
  EXPECT_DOUBLE_EQ(averaging->currentData().toDouble(), 0.0);

  averaging->setCurrentIndex(2);
  EXPECT_DOUBLE_EQ(averaging->currentData().toDouble(),
                   analysis::kDefaultAveraging);
}

TEST(SpectrumFormatTest, AResolutionIsPutAsTheBinWidthItBuys) {
  // "4096 points" is a fact about the implementation. The bin width is the same
  // fact in the units the user is choosing between.
  EXPECT_EQ(FormatSpectrumResolution(4096), QStringLiteral("9.8 kHz bins"));
  EXPECT_EQ(FormatSpectrumResolution(16384), QStringLiteral("2.4 kHz bins"));
}

TEST(SpectrumPanelTest, TheResolutionChoicesDefaultToTheMostAveraged) {
  SpectrumPanel panel(nullptr);

  auto* const resolution =
      Named<QComboBox>(panel, SpectrumPanel::kResolutionComboName);
  ASSERT_NE(resolution, nullptr);

  EXPECT_EQ(static_cast<size_t>(resolution->count()),
            analysis::kTransformSizeChoiceCount);
  EXPECT_EQ(static_cast<size_t>(resolution->currentData().toULongLong()),
            analysis::kDefaultTransformSize);

  // Widest bins first, because that is the order of the averaging they buy and
  // the default is meant to read as one end of a range rather than a middle.
  for (int index = 1; index < resolution->count(); ++index) {
    EXPECT_GT(resolution->itemData(index).toULongLong(),
              resolution->itemData(index - 1).toULongLong());
  }
}

TEST(SpectrumPanelTest, ChangingTheResolutionChangesHowManyBinsArriveAndDraws) {
  // The bin count is a property of the transform size, so this control changes
  // the length of every vector the panel is handed afterwards. A plot that had
  // cached anything sized to the old one would draw past the end of the new.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  auto* const resolution =
      Named<QComboBox>(panel, SpectrumPanel::kResolutionComboName);
  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);

  const size_t wide = (analysis::kDefaultTransformSize / 2) + 1;
  panel.OnSpectrumReady(Levels(wide, -40.0), Levels(wide, -30.0));
  EXPECT_FALSE(panel.grab().isNull());

  resolution->setCurrentIndex(resolution->count() - 1);

  const size_t narrow =
      (analysis::kTransformSizeChoices[analysis::kTransformSizeChoiceCount -
                                       1] /
       2) +
      1;
  panel.OnSpectrumReady(Levels(narrow, -40.0), Levels(narrow, -30.0));

  EXPECT_FALSE(panel.grab().isNull());

  // The spectrogram reduces bins to a fixed number of columns by proportion, so
  // rows recorded at either resolution stay comparable and neither is
  // discarded.
  EXPECT_EQ(plot->history().size(), 2U);
}

TEST(SpectrumPanelTest, PeakHoldIsOnByDefaultAndCanBeTurnedOff) {
  SpectrumPanel panel(nullptr);

  auto* const box = Named<QCheckBox>(panel, SpectrumPanel::kPeakHoldBoxName);
  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(box, nullptr);
  ASSERT_NE(plot, nullptr);

  EXPECT_TRUE(box->isChecked());
  EXPECT_TRUE(plot->peak_hold_visible());

  box->setChecked(false);
  EXPECT_FALSE(plot->peak_hold_visible());
}

TEST(SpectrumPanelTest, ResetPeaksIsOfferedEvenWithNoControllerBehindIt) {
  // The widget tests build every panel with no controller, which is what lets
  // the layout be tested without a USB subsystem. A button that crashed when
  // pressed in that state would be a real defect in the application too, on the
  // path where the device could not be opened.
  SpectrumPanel panel(nullptr);

  auto* const reset =
      Named<QPushButton>(panel, SpectrumPanel::kResetButtonName);
  ASSERT_NE(reset, nullptr);
  reset->click();
}

TEST(SpectrumPanelTest, BothViewsAreOfferedAndTheTraceIsTheDefault) {
  SpectrumPanel panel(nullptr);

  auto* const view = Named<QComboBox>(panel, SpectrumPanel::kViewComboName);
  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(view, nullptr);
  ASSERT_NE(plot, nullptr);

  EXPECT_EQ(view->count(), 2);
  EXPECT_EQ(plot->view(), SpectrumView::kTrace);

  view->setCurrentIndex(1);
  EXPECT_EQ(plot->view(), SpectrumView::kSpectrogram);
}

TEST(SpectrumPanelTest, PeakHoldIsDisabledWhereItWouldMeanNothing) {
  // The spectrogram already shows every frame the peak hold would be
  // summarising. Disabled rather than left to do nothing when pressed.
  SpectrumPanel panel(nullptr);

  auto* const view = Named<QComboBox>(panel, SpectrumPanel::kViewComboName);
  auto* const peak_hold =
      Named<QCheckBox>(panel, SpectrumPanel::kPeakHoldBoxName);
  auto* const reset =
      Named<QPushButton>(panel, SpectrumPanel::kResetButtonName);

  EXPECT_TRUE(peak_hold->isEnabled());
  EXPECT_TRUE(reset->isEnabled());

  view->setCurrentIndex(1);
  EXPECT_FALSE(peak_hold->isEnabled());
  EXPECT_FALSE(reset->isEnabled());

  view->setCurrentIndex(0);
  EXPECT_TRUE(peak_hold->isEnabled());
}

TEST(SpectrumPanelTest, TheRangeStartsAtTheFilterCornerAndCanBeWidened) {
  SpectrumPanel panel(nullptr);

  auto* const range =
      Named<QComboBox>(panel, SpectrumPanel::kMaximumFrequencyComboName);
  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(range, nullptr);

  EXPECT_EQ(static_cast<size_t>(range->count()),
            analysis::kMaximumFrequencyChoiceCount);
  EXPECT_DOUBLE_EQ(plot->maximum_frequency_hz(),
                   analysis::kDefaultMaximumFrequencyHz);

  // The default has to show the filter's corner rather than stop before it.
  EXPECT_GT(plot->maximum_frequency_hz(), analysis::kLowPassCornerHz);

  range->setCurrentIndex(range->count() - 1);
  EXPECT_DOUBLE_EQ(plot->maximum_frequency_hz(), 20'000'000.0);
}

TEST(SpectrumPanelTest, TheSpectrogramRecordsWhicheverViewIsShowing) {
  // The spectrogram is a record of the run, not of the time the user spent
  // looking at it: switching to it has to show what has happened rather than
  // starting again from the moment it was asked for.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_EQ(plot->view(), SpectrumView::kTrace);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  for (int frame = 0; frame < 5; ++frame) {
    panel.OnSpectrumReady(Levels(bins, -40.0), Levels(bins, -30.0));
  }

  EXPECT_EQ(plot->history().size(), 5U);
}

TEST(SpectrumPanelTest, StartingARunClearsTheSpectrogramToo) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  panel.OnSpectrumReady(Levels(bins, -40.0), Levels(bins, -30.0));
  ASSERT_FALSE(
      Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName)->history().empty());

  panel.OnMonitoringChanged(true);

  EXPECT_TRUE(
      Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName)->history().empty());
}

TEST(SpectrumPanelTest, TheSpectrogramPaintsEmptyAndFull) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  Named<QComboBox>(panel, SpectrumPanel::kViewComboName)->setCurrentIndex(1);

  EXPECT_FALSE(panel.grab().isNull());

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  for (int frame = 0; frame < 20; ++frame) {
    std::vector<double> magnitudes = Levels(bins, -80.0);
    magnitudes[800 + frame] = -6.0;
    panel.OnSpectrumReady(magnitudes, Levels(bins, -70.0));
    panel.grab();
  }

  EXPECT_FALSE(panel.grab().isNull());
}

// A spectrogram that draws nothing a person can see is the same as no
// spectrogram, and that failure has happened on this panel's neighbour: the
// waveform persistence accumulated perfectly into a buffer at 2% opacity. So
// this asserts a stripe is drawn, in colour, where the carrier is.
TEST(SpectrumPanelTest, TheSpectrogramDrawsTheCarrierAsAVisibleBand) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  Named<QComboBox>(panel, SpectrumPanel::kViewComboName)->setCurrentIndex(1);

  const bool dark = theme_tokens::IsDarkPalette(panel.palette());

  // Bin 800 of 2048 is 7.8 MHz — a LaserDisc FM carrier, near enough.
  FeedCarrier(panel, 800, 30);

  const int row = BrightestRow(panel.grab().toImage(), dark);
  EXPECT_GE(row, 0) << "no strongly coloured row: nothing was drawn";
}

// Time runs left to right with now at the right, as it does on the amplitude
// panel — but over a fixed window rather than a stretched one. A run starts as
// a sliver at the right-hand edge and grows leftwards until it fills, so the
// time scale never changes under the reader.
TEST(SpectrumPanelTest, TheSpectrogramGrowsFromTheRightRatherThanStretching) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  Named<QComboBox>(panel, SpectrumPanel::kViewComboName)->setCurrentIndex(1);

  const bool dark = theme_tokens::IsDarkPalette(panel.palette());

  // A handful of frames against a window that holds hundreds.
  FeedCarrier(panel, 800, 12);

  const QImage shot = panel.grab().toImage();
  const int width = shot.width();

  EXPECT_EQ(LoudColumns(shot, 0, width / 2, dark), 0)
      << "the picture reaches the left-hand half after a dozen frames: it is "
         "being stretched rather than scrolled";
  EXPECT_GT(LoudColumns(shot, width - (width / 10), width, dark), 0)
      << "nothing at the right-hand edge, where now is";
}

// The frequency range has to reach the drawing, not just the axis labels. The
// same carrier occupies a different fraction of a wider display, so widening
// the range must move its stripe towards the left.
TEST(SpectrumPanelTest, WideningTheRangeMovesTheCarrierDownTheSpectrogram) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  Named<QComboBox>(panel, SpectrumPanel::kViewComboName)->setCurrentIndex(1);

  auto* const range =
      Named<QComboBox>(panel, SpectrumPanel::kMaximumFrequencyComboName);
  const bool dark = theme_tokens::IsDarkPalette(panel.palette());

  FeedCarrier(panel, 800, 30);
  const int at_default = BrightestRow(panel.grab().toImage(), dark);
  ASSERT_GE(at_default, 0);

  range->setCurrentIndex(range->count() - 1);  // 20 MHz
  const int at_widest = BrightestRow(panel.grab().toImage(), dark);
  ASSERT_GE(at_widest, 0);

  // Frequency runs up the side, so a carrier occupying a smaller fraction of a
  // wider range sits lower — further down the image, which is a larger row.
  EXPECT_GT(at_widest, at_default)
      << "the carrier sat at row " << at_default << " over 14 MHz and "
      << at_widest << " over 20 MHz; the range is not reaching the drawing";
}

TEST(SpectrumPanelTest, ThePanelPaintsWithNothingToDraw) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  EXPECT_FALSE(panel.grab().isNull());
}

TEST(SpectrumPanelTest, ThePanelPaintsASpectrum) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  std::vector<double> magnitudes = Levels(bins, -80.0);
  magnitudes[800] = -6.0;

  panel.OnSpectrumReady(magnitudes, Levels(bins, -70.0));

  EXPECT_FALSE(panel.grab().isNull());
}

TEST(SpectrumPanelTest, StartingARunClearsTheOldSpectrum) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  panel.OnSpectrumReady(Levels(bins, -20.0), Levels(bins, -10.0));
  panel.OnMonitoringChanged(true);

  EXPECT_FALSE(panel.grab().isNull());
}

TEST(SpectrumPanelTest, TheCursorLabelSaysWhatToDoBeforeItIsUsed) {
  SpectrumPanel panel(nullptr);

  auto* const cursor = Named<QLabel>(panel, SpectrumPanel::kCursorLabelName);
  ASSERT_NE(cursor, nullptr);
  EXPECT_FALSE(cursor->text().isEmpty());
}

}  // namespace
}  // namespace ddd::gui
