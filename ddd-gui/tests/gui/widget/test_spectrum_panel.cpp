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
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <algorithm>
#include <cmath>
#include <vector>

#include "capture_format.h"
#include "frequency_axis.h"
#include "sample_format.h"
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

bool IsColour(const QImage& shot, int x, int y, const QColor& wanted,
              int tolerance) {
  const QColor pixel(shot.pixel(x, y));
  return std::abs(pixel.red() - wanted.red()) <= tolerance &&
         std::abs(pixel.green() - wanted.green()) <= tolerance &&
         std::abs(pixel.blue() - wanted.blue()) <= tolerance;
}

// The one column, or row, most of a line was drawn in. The markers are drawn
// with axis-aligned dashed lines in their own colour, so the line is the run
// with far more of that colour in it than any other.
int BusiestColumn(const QImage& shot, const QColor& wanted, int tolerance) {
  int best = -1;
  int best_count = 0;
  for (int x = 0; x < shot.width(); ++x) {
    int count = 0;
    for (int y = 0; y < shot.height(); ++y) {
      if (IsColour(shot, x, y, wanted, tolerance)) {
        ++count;
      }
    }
    if (count > best_count) {
      best_count = count;
      best = x;
    }
  }
  return best_count > 2 ? best : -1;
}

int BusiestRow(const QImage& shot, const QColor& wanted, int tolerance) {
  int best = -1;
  int best_count = 0;
  for (int y = 0; y < shot.height(); ++y) {
    int count = 0;
    for (int x = 0; x < shot.width(); ++x) {
      if (IsColour(shot, x, y, wanted, tolerance)) {
        ++count;
      }
    }
    if (count > best_count) {
      best_count = count;
      best = y;
    }
  }
  return best_count > 2 ? best : -1;
}

// Whether anything of the given colour was drawn within `radius` pixels of a
// point. Used for the peak marker, which is a small circle rather than a line.
bool ColourNear(const QImage& shot, const QColor& wanted, const QPointF& at,
                int radius, int tolerance) {
  for (int y = static_cast<int>(at.y()) - radius;
       y <= static_cast<int>(at.y()) + radius; ++y) {
    for (int x = static_cast<int>(at.x()) - radius;
         x <= static_cast<int>(at.x()) + radius; ++x) {
      if (x < 0 || y < 0 || x >= shot.width() || y >= shot.height()) {
        continue;
      }
      if (IsColour(shot, x, y, wanted, tolerance)) {
        return true;
      }
    }
  }
  return false;
}

void FeedCarrier(SpectrumPanel& panel, size_t bin, int frames) {
  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  for (int frame = 0; frame < frames; ++frame) {
    std::vector<double> magnitudes(bins, -80.0);
    magnitudes[bin] = -6.0;
    panel.OnSpectrumReady(magnitudes, magnitudes, magnitudes);
    panel.grab();
  }
}

// The frequency of one bin of a spectrum with `bins` of them, DC to Nyquist.
double BinFrequency(size_t bin, size_t bins) {
  return static_cast<double>(bin) / static_cast<double>(bins - 1) *
         (static_cast<double>(capture::kSampleRateHz) / 2.0);
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

TEST(SpectrumFormatTest, TheReadoutQuotesTheBandwidthAndNotTheBinSpacing) {
  // The two differ by half again, and only one of them is what the noise floor
  // follows. A panel that quoted 9.8 kHz here while collecting from 14.6 would
  // be understating its own bandwidth by 1.8 dB — the kind of wrong that reads
  // as a measurement.
  const QString text =
      FormatResolutionBandwidth(4096, 15, capture::kSampleRateHz);

  EXPECT_TRUE(text.contains(QStringLiteral("14.6 kHz"))) << text.toStdString();
  EXPECT_FALSE(text.contains(QStringLiteral("9.8"))) << text.toStdString();
  EXPECT_TRUE(text.contains(QStringLiteral("15"))) << text.toStdString();
}

TEST(SpectrumFormatTest, TheReadoutLeavesOutASegmentCountNobodyStated) {
  // Zero segments is "nobody said", not "averaged none of them". Printing
  // "0 avg" would be the display inventing a figure about its own confidence.
  const QString text =
      FormatResolutionBandwidth(4096, 0, capture::kSampleRateHz);

  EXPECT_TRUE(text.contains(QStringLiteral("14.6 kHz"))) << text.toStdString();
  EXPECT_FALSE(text.contains(QStringLiteral("avg"))) << text.toStdString();
  EXPECT_FALSE(text.contains(QStringLiteral("0 "))) << text.toStdString();
}

TEST(SpectrumFormatTest, TheReadoutFollowsTheResolutionItWasGiven) {
  // 16,384 points is four times the segment length and a quarter of the
  // bandwidth, which is the whole of what the Resolution control buys.
  EXPECT_TRUE(FormatResolutionBandwidth(16384, 3, capture::kSampleRateHz)
                  .contains(QStringLiteral("3.7 kHz")))
      << FormatResolutionBandwidth(16384, 3, capture::kSampleRateHz)
             .toStdString();
}

TEST(SpectrumFormatTest, EveryLevelSaysWhatItIsRelativeTo) {
  // dB on its own is a ratio with the other half missing. Every level this
  // panel states is against a full-scale sine, and the scale says so.
  EXPECT_EQ(FormatLevelTick(0.0), QStringLiteral("0 dBFS"));
  EXPECT_EQ(FormatLevelTick(-100.0), QStringLiteral("-100 dBFS"));
}

TEST(SpectrumPanelTest, TheLevelScaleIsWideEnoughForItsWidestLabel) {
  // This was fixed at 46 pixels and clipped "-100 dBFS" to "-100 dB", which on
  // a display whose business is stating levels is not a cosmetic fault. Now
  // measured from the font, so it holds at whatever size the desktop is using.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(plot, nullptr);

  const QFontMetrics metrics(plot->font());
  EXPECT_GE(plot->PlotArea().left(),
            metrics.horizontalAdvance(FormatLevelTick(-100.0)));
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
  EXPECT_EQ(FormatSpectrumResolution(4096, capture::kSampleRateHz),
            QStringLiteral("9.8 kHz bins"));
  EXPECT_EQ(FormatSpectrumResolution(16384, capture::kSampleRateHz),
            QStringLiteral("2.4 kHz bins"));
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
  panel.OnSpectrumReady(Levels(wide, -40.0), Levels(wide, -30.0),
                        Levels(wide, -40.0));
  EXPECT_FALSE(panel.grab().isNull());

  resolution->setCurrentIndex(resolution->count() - 1);

  const size_t narrow =
      (analysis::kTransformSizeChoices[analysis::kTransformSizeChoiceCount -
                                       1] /
       2) +
      1;
  panel.OnSpectrumReady(Levels(narrow, -40.0), Levels(narrow, -30.0),
                        Levels(narrow, -40.0));

  EXPECT_FALSE(panel.grab().isNull());

  // The spectrogram reduces bins to a fixed number of columns by proportion, so
  // rows recorded at either resolution stay comparable and neither is
  // discarded.
  EXPECT_EQ(plot->history().size(), 2U);
}

TEST(SpectrumFormatTest, AxisTicksAreLabelledInMegahertzAcrossDecades) {
  // A log axis reaches below a megahertz, where an integer label would round
  // every mark in the bottom decade to the same "0".
  EXPECT_EQ(FormatAxisTick(0.0), QStringLiteral("0"));
  EXPECT_EQ(FormatAxisTick(100'000.0), QStringLiteral("0.1"));
  EXPECT_EQ(FormatAxisTick(500'000.0), QStringLiteral("0.5"));
  EXPECT_EQ(FormatAxisTick(1'000'000.0), QStringLiteral("1"));
  EXPECT_EQ(FormatAxisTick(10'000'000.0), QStringLiteral("10"));
  EXPECT_EQ(FormatAxisTick(14'000'000.0), QStringLiteral("14"));
}

TEST(SpectrumPanelTest, TheFrequencyAxisIsLogarithmicUnlessAskedOtherwise) {
  SpectrumPanel panel(nullptr);

  auto* const box =
      Named<QCheckBox>(panel, SpectrumPanel::kLogFrequencyBoxName);
  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(box, nullptr);

  EXPECT_TRUE(box->isChecked());
  EXPECT_EQ(plot->frequency_scale(), analysis::FrequencyScale::kLogarithmic);

  box->setChecked(false);
  EXPECT_EQ(plot->frequency_scale(), analysis::FrequencyScale::kLinear);

  box->setChecked(true);
  EXPECT_EQ(plot->frequency_scale(), analysis::FrequencyScale::kLogarithmic);
}

TEST(SpectrumPanelTest, TheScaleReachesTheSpectrogramAndNotJustTheTrace) {
  // Both views are pictures of the same measurement in the same panel. A trace
  // spaced by decade beside a waterfall spaced evenly would put the same
  // carrier in two different places, and the reader has no way to tell which
  // one to believe.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  Named<QComboBox>(panel, SpectrumPanel::kViewComboName)->setCurrentIndex(1);

  auto* const box =
      Named<QCheckBox>(panel, SpectrumPanel::kLogFrequencyBoxName);
  const bool dark = theme_tokens::IsDarkPalette(panel.palette());

  // Bin 800 of 2048 is 7.8 MHz — a LaserDisc FM carrier, near enough.
  FeedCarrier(panel, 800, 30);
  const int logarithmic = BrightestRow(panel.grab().toImage(), dark);
  ASSERT_GE(logarithmic, 0);

  box->setChecked(false);
  const int linear = BrightestRow(panel.grab().toImage(), dark);
  ASSERT_GE(linear, 0);

  // 7.8 MHz is 56% of the way up a linear axis to 14 MHz and 88% of the way up
  // a logarithmic one, so the log view draws it markedly higher — a smaller row
  // number, frequency running up the side.
  EXPECT_LT(logarithmic, linear)
      << "the carrier sat at row " << logarithmic << " on a log axis and "
      << linear << " on a linear one; the scale is not reaching the picture";
}

TEST(SpectrumPanelTest, TheCursorReadsTheLevelThatWasDrawnUnderIt) {
  // The failure this exists to catch is silent. The cursor used to work out its
  // own bin from the pointer's position while the trace worked out its own from
  // the column: on a linear axis the two agreed by coincidence, and on a
  // logarithmic one they would drift apart by an amount that varies across the
  // width — which reads as a measurement rather than as a fault.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  panel.show();

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(plot, nullptr);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  constexpr size_t kCarrierBin = 800;

  std::vector<double> magnitudes(bins, -80.0);
  magnitudes[kCarrierBin] = -6.0;
  panel.OnSpectrumReady(magnitudes, magnitudes, magnitudes);

  QSignalSpy moved(plot, &SpectrumPlot::CursorMoved);
  ASSERT_TRUE(moved.isValid());

  // Point at the column the carrier is drawn in, worked out through the axis
  // the plot is using rather than by assuming a scale.
  const analysis::FrequencyAxis axis = plot->Axis();
  const double carrier_hz = BinFrequency(kCarrierBin, bins);

  // The plot's own area, inside the scale it reserves on the left and the axis
  // it reserves below. Asked for rather than reconstructed: the scale's width
  // follows from the font, and a test that assumed a number for it would fail
  // on the next machine with a different one.
  const QRectF area = plot->PlotArea();
  ASSERT_GT(area.width(), 100);

  const double x =
      area.left() + std::floor(axis.ProportionOf(carrier_hz) * area.width());

  // Delivered straight to the widget rather than synthesised through the
  // platform: an offscreen plugin has no pointer to move, and a test that
  // quietly sent nothing would pass by never checking anything.
  const QPointF where(x, area.center().y());
  QMouseEvent move(QEvent::MouseMove, where, plot->mapToGlobal(where),
                   Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(plot, &move);

  ASSERT_FALSE(moved.isEmpty());
  const QList<QVariant> reading = moved.takeLast();

  // The frequency under the pointer is the carrier's, to within the width of
  // one pixel column at that point on the axis.
  const double reported_hz = reading.at(0).toDouble();
  const double column_hz =
      axis.FrequencyAt(static_cast<double>(x + 1 - area.left()) /
                       area.width()) -
      axis.FrequencyAt(static_cast<double>(x - area.left()) / area.width());
  EXPECT_NEAR(reported_hz, carrier_hz, std::max(column_hz, 1.0) * 2.0);

  // And the level is the carrier's, not the floor either side of it.
  EXPECT_NEAR(reading.at(1).toDouble(), -6.0, 0.01)
      << "the cursor read " << reading.at(1).toDouble()
      << " dB where the trace drew the carrier at -6 dB";
}

TEST(SpectrumPanelTest, TheSpectrogramRecordsTheSnapshotNotTheAveragedTrace) {
  // The defect this is here for: rows used to be appended from the same vector
  // the trace draws, which is averaged across snapshots. At the heavy setting
  // that filter's time constant is most of a second — a third of a minute-wide
  // waterfall — so a transient was smeared across several rows of a display
  // whose whole purpose is saying when things happened.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;

  // What an averaged trace and an unaveraged snapshot look like when a
  // transient has just arrived: the trace has barely moved, the snapshot is all
  // the way up.
  const std::vector<double> smoothed = Levels(bins, -70.0);
  const std::vector<double> measured = Levels(bins, -10.0);

  panel.OnSpectrumReady(smoothed, smoothed, measured);

  ASSERT_EQ(plot->history().size(), 1U);
  EXPECT_NEAR(plot->history().At(0, 100), -10.0, 0.001)
      << "the row recorded the trace's averaged level, not the snapshot's";
}

TEST(SpectrumPanelTest, TheContrastControlsBelongToTheSpectrogramAlone) {
  SpectrumPanel panel(nullptr);

  auto* const view = Named<QComboBox>(panel, SpectrumPanel::kViewComboName);
  auto* const reference =
      Named<QComboBox>(panel, SpectrumPanel::kReferenceComboName);
  auto* const range = Named<QComboBox>(panel, SpectrumPanel::kRangeComboName);
  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(reference, nullptr);
  ASSERT_NE(range, nullptr);

  // The full span of what the converter can represent, which assumes nothing
  // about the signal.
  EXPECT_DOUBLE_EQ(plot->spectrogram_reference_db(),
                   kDefaultSpectrogramReferenceDb);
  EXPECT_DOUBLE_EQ(plot->spectrogram_range_db(), kDefaultSpectrogramRangeDb);

  // Offered in the view they colour, and not in the one drawn as a line.
  EXPECT_FALSE(reference->isEnabled());
  view->setCurrentIndex(1);
  EXPECT_TRUE(reference->isEnabled());
  EXPECT_TRUE(range->isEnabled());

  reference->setCurrentIndex(2);
  range->setCurrentIndex(2);
  EXPECT_DOUBLE_EQ(plot->spectrogram_reference_db(), -20.0);
  EXPECT_DOUBLE_EQ(plot->spectrogram_range_db(), 40.0);
}

TEST(SpectrumPanelTest, NarrowingTheRangeBringsOutWhatWasAWashOfOneColour) {
  // History is kept as levels rather than as a picture precisely so that this
  // works: moving the scale re-colours every row already on screen, including
  // the ones recorded before the control was touched.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  Named<QComboBox>(panel, SpectrumPanel::kViewComboName)->setCurrentIndex(1);

  const bool dark = theme_tokens::IsDarkPalette(panel.palette());

  // A carrier well down the scale. Across a hundred decibels from full scale it
  // sits two thirds of the way up the ramp — coloured, but nowhere near what a
  // strong signal gets, which is the whole complaint about a fixed range.
  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  for (int frame = 0; frame < 30; ++frame) {
    std::vector<double> magnitudes(bins, -95.0);
    magnitudes[800] = -32.0;
    panel.OnSpectrumReady(magnitudes, magnitudes, magnitudes);
    panel.grab();
  }

  EXPECT_EQ(BrightestRow(panel.grab().toImage(), dark), -1)
      << "a -32 dB carrier already reads as full scale across a 100 dB range";

  // Reference -30 and a 20 dB range puts the scale at -50 to -30, where the
  // same carrier is near the top of it.
  Named<QComboBox>(panel, SpectrumPanel::kReferenceComboName)
      ->setCurrentIndex(3);
  Named<QComboBox>(panel, SpectrumPanel::kRangeComboName)->setCurrentIndex(3);

  EXPECT_GE(BrightestRow(panel.grab().toImage(), dark), 0)
      << "narrowing the scale did not re-colour the rows already held";
}

TEST(SpectrumPanelTest, ScrollingTheWaterfallDrawsWhatRebuildingItWould) {
  // Frames arrive one at a time and the picture is scrolled to match, which is
  // what keeps an ordinary frame down to one column of colouring. The whole
  // optimisation is only sound if it cannot drift from the picture a full
  // rebuild would produce, so this draws both and compares them pixel for
  // pixel.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  Named<QComboBox>(panel, SpectrumPanel::kViewComboName)->setCurrentIndex(1);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  const auto feed = [&](int frame) {
    std::vector<double> magnitudes(bins, -80.0);
    magnitudes[600 + (frame * 4)] = -12.0;
    panel.OnSpectrumReady(magnitudes, magnitudes, magnitudes);
  };

  // Painted between every frame, so the picture scrolls one column at a time.
  for (int frame = 0; frame < 20; ++frame) {
    feed(frame);
    panel.grab();
  }

  // And then in batches, which is what a widget nobody is looking at does: the
  // frames still arrive, the painting does not, and the scroll has to catch up
  // several columns at once. A one-column-at-a-time test never reaches that
  // path, and an off-by-one in it would look identical.
  for (int batch = 0; batch < 4; ++batch) {
    for (int step = 0; step < 5; ++step) {
      feed(20 + (batch * 5) + step);
    }
    panel.grab();
  }

  const QImage scrolled = panel.grab().toImage();

  // Not a blank picture compared against another blank one.
  ASSERT_GE(
      BrightestRow(scrolled, theme_tokens::IsDarkPalette(panel.palette())), 0);

  // Force the picture to be made again from the history, by moving the axis
  // away and back. Nothing about the data changes.
  auto* const log =
      Named<QCheckBox>(panel, SpectrumPanel::kLogFrequencyBoxName);
  log->setChecked(false);
  panel.grab();
  log->setChecked(true);

  const QImage rebuilt = panel.grab().toImage();

  EXPECT_EQ(scrolled, rebuilt)
      << "the scrolled picture and the rebuilt one differ: the incremental "
         "path has drifted from what the history says";
}

TEST(SpectrumPanelTest, AThemeChangeRecoloursTheRowsAlreadyHeld) {
  // A run that has finished leaves its spectrogram on screen to be looked at.
  // Without this the picture keeps the palette it was coloured in, for ever —
  // there is no next frame coming to invalidate it.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  Named<QComboBox>(panel, SpectrumPanel::kViewComboName)->setCurrentIndex(1);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  for (int frame = 0; frame < 30; ++frame) {
    std::vector<double> magnitudes(bins, -80.0);
    magnitudes[800] = -6.0;
    panel.OnSpectrumReady(magnitudes, magnitudes, magnitudes);
    panel.grab();
  }

  const QImage before = panel.grab().toImage();

  // The other theme, and no new frames at all.
  QPalette swapped = panel.palette();
  const bool dark = theme_tokens::IsDarkPalette(panel.palette());
  swapped.setColor(QPalette::Window,
                   dark ? QColor(245, 245, 245) : QColor(30, 30, 30));
  swapped.setColor(QPalette::Base,
                   dark ? QColor(255, 255, 255) : QColor(20, 20, 20));
  panel.setPalette(swapped);

  EXPECT_NE(before, panel.grab().toImage())
      << "the spectrogram kept the old theme's colours";
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

TEST(SpectrumPanelTest, TheDisplayShowsEverythingTheConverterCanRepresent) {
  // There used to be a control here offering tops of 14 to 20 MHz, because on a
  // linear axis the stretch above the filter's corner was a third of the width
  // spent on the part of the spectrum the hardware has deliberately removed. On
  // a decade axis that stretch is under a tenth of the width, so the display
  // shows all of it and the control is gone.
  SpectrumPanel panel(nullptr);

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(plot, nullptr);

  const double nyquist = static_cast<double>(capture::kSampleRateHz) / 2.0;
  EXPECT_DOUBLE_EQ(plot->Axis().maximum_hz(), nyquist);

  // And the filter's corner is inside it, which is what the range control was
  // originally there to guarantee.
  EXPECT_GT(plot->Axis().maximum_hz(), analysis::kLowPassCornerHz);

  // Fixed on either scale, so switching does not quietly change what is shown.
  Named<QCheckBox>(panel, SpectrumPanel::kLogFrequencyBoxName)
      ->setChecked(false);
  EXPECT_DOUBLE_EQ(plot->Axis().maximum_hz(), nyquist);
}

TEST(SpectrumPlotTest, ADecimatedStreamPutsItsNyquistWhereItReallyIs) {
  // The axis is a property of the stream, not of the converter. At 20 Msps the
  // top of it is 10 MHz, and a plot that kept the converter's own rate would
  // draw a tape's 5 MHz carrier at 10 MHz — a reading twice what is there,
  // presented as a measurement.
  SpectrumPlot plot;
  ASSERT_DOUBLE_EQ(plot.Axis().maximum_hz(),
                   static_cast<double>(capture::kSampleRateHz) / 2.0);

  plot.SetSampleRate(capture::SampleRateHzFor(capture::kTapeDecimationFactor));

  EXPECT_DOUBLE_EQ(plot.Axis().maximum_hz(), 10'000'000.0);

  // And the mapping follows it, which is the part a reader actually sees: the
  // top bin of the transform is Nyquist wherever Nyquist is.
  EXPECT_DOUBLE_EQ(plot.Axis().FrequencyAt(1.0), 10'000'000.0);
}

TEST(SpectrumPlotTest, ChangingTheRateDiscardsHistoryDrawnAgainstTheOldAxis) {
  // A spectrogram column spans DC to Nyquist. Kept across a rate change it
  // would be redrawn under the new axis, moving every carrier already on screen
  // by a factor of two while still presenting itself as the same measurement.
  SpectrumPlot plot;
  plot.resize(600, 300);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  for (int frame = 0; frame < 5; ++frame) {
    plot.SetSpectrum(Levels(bins, -40.0), Levels(bins, -30.0),
                     Levels(bins, -40.0));
  }
  ASSERT_EQ(plot.history().size(), 5U);

  plot.SetSampleRate(capture::SampleRateHzFor(capture::kTapeDecimationFactor));

  EXPECT_EQ(plot.history().size(), 0U);
}

TEST(SpectrumPlotTest, ARateThatHasNotChangedKeepsTheHistory) {
  // Settings arrive whenever anything in them changes, and most of those
  // changes have nothing to do with the rate. Clearing the waterfall because
  // somebody edited the capture folder would be a display that wiped itself for
  // no reason a user could see.
  SpectrumPlot plot;
  plot.resize(600, 300);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  for (int frame = 0; frame < 5; ++frame) {
    plot.SetSpectrum(Levels(bins, -40.0), Levels(bins, -30.0),
                     Levels(bins, -40.0));
  }

  plot.SetSampleRate(capture::kSampleRateHz);

  EXPECT_EQ(plot.history().size(), 5U);
}

TEST(SpectrumFormatTest, ADecimatedStreamHasBinsHalfAsWide) {
  // The bins are a fraction of the rate, so the same transform buys twice the
  // resolution over a decimated stream. A label fixed at the converter's rate
  // would overstate the width of every one of them.
  EXPECT_EQ(FormatSpectrumResolution(4096, capture::kSampleRateHz),
            QStringLiteral("9.8 kHz bins"));
  EXPECT_EQ(FormatSpectrumResolution(
                4096, capture::SampleRateHzFor(capture::kTapeDecimationFactor)),
            QStringLiteral("4.9 kHz bins"));

  // And the same of the bandwidth the corner of the plot states, which is the
  // figure every level on the screen is relative to.
  EXPECT_TRUE(
      FormatResolutionBandwidth(
          4096, 15, capture::SampleRateHzFor(capture::kTapeDecimationFactor))
          .contains(QStringLiteral("7.3 kHz")));
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
    panel.OnSpectrumReady(Levels(bins, -40.0), Levels(bins, -30.0),
                          Levels(bins, -40.0));
  }

  EXPECT_EQ(plot->history().size(), 5U);
}

TEST(SpectrumPanelTest, StartingARunClearsTheSpectrogramToo) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  panel.OnSpectrumReady(Levels(bins, -40.0), Levels(bins, -30.0),
                        Levels(bins, -40.0));
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
    panel.OnSpectrumReady(magnitudes, Levels(bins, -70.0), magnitudes);
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

// The axis has to reach the drawing, not just the labels beside it. With the
// top fixed there is no range to vary, so this checks the stronger property
// directly: the carrier is drawn where the axis says its frequency belongs.
TEST(SpectrumPanelTest, TheCarrierIsDrawnWhereTheAxisPutsItsFrequency) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  Named<QComboBox>(panel, SpectrumPanel::kViewComboName)->setCurrentIndex(1);

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  const bool dark = theme_tokens::IsDarkPalette(panel.palette());

  constexpr size_t kCarrierBin = 800;
  FeedCarrier(panel, kCarrierBin, 30);

  // The plot itself rather than the whole panel, so that the row found and the
  // row computed are in the same coordinates: the panel lays the plot out
  // inside its own margins, and that offset would otherwise read as a mapping
  // error of a handful of pixels.
  const int row = BrightestRow(plot->grab().toImage(), dark);
  ASSERT_GE(row, 0);

  // Where the axis says that frequency sits, in the plot's own coordinates.
  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  const double carrier_hz = BinFrequency(kCarrierBin, bins);

  const QRectF area = plot->PlotArea();
  const double expected =
      area.bottom() - (plot->Axis().ProportionOf(carrier_hz) * area.height());

  EXPECT_NEAR(row, expected, 6.0)
      << "the carrier was drawn at row " << row << " and the axis puts "
      << carrier_hz << " Hz at " << expected;
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

  panel.OnSpectrumReady(magnitudes, Levels(bins, -70.0), magnitudes);

  EXPECT_FALSE(panel.grab().isNull());
}

TEST(SpectrumPanelTest, StartingARunClearsTheOldSpectrum) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  panel.OnSpectrumReady(Levels(bins, -20.0), Levels(bins, -10.0),
                        Levels(bins, -20.0));
  panel.OnMonitoringChanged(true);

  EXPECT_FALSE(panel.grab().isNull());
}

TEST(SpectrumPanelTest, ThePeakMarkerLandsOnTheStrongestCarrier) {
  // The point of the marker is that the strongest thing present can be read
  // without pointing at it, so the test that matters is where it is drawn
  // rather than that it was drawn at all.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  panel.show();

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(plot, nullptr);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  constexpr size_t kCarrierBin = 800;
  constexpr double kCarrierDb = -6.0;

  std::vector<double> magnitudes(bins, -80.0);
  magnitudes[kCarrierBin] = kCarrierDb;

  // Peak hold below the display floor, so the only thing that could be marked
  // is the live trace.
  panel.OnSpectrumReady(magnitudes, Levels(bins, -120.0), magnitudes, 15);

  const QRectF area = plot->PlotArea();
  const double carrier_hz = BinFrequency(kCarrierBin, bins);

  const QPointF expected(
      area.left() +
          std::floor(plot->Axis().ProportionOf(carrier_hz) * area.width()),
      area.bottom() - (((kCarrierDb + 100.0) / 100.0) * area.height()));

  // The window's own text colour: the marker is the instrument speaking rather
  // than another trace, and it says so by not being one of the plot colours.
  EXPECT_TRUE(ColourNear(plot->grab().toImage(),
                         plot->palette().color(QPalette::WindowText), expected,
                         6, 70))
      << "no marker within six pixels of " << expected.x() << ", "
      << expected.y();
}

TEST(SpectrumPanelTest, NothingIsMarkedWhenThereIsNothingAboveTheFloor) {
  // A marker on an empty run would be pointing at the bottom of the scale and
  // calling it the strongest carrier.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  panel.show();

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(plot, nullptr);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  panel.OnSpectrumReady(Levels(bins, -120.0), Levels(bins, -120.0),
                        Levels(bins, -120.0), 15);

  const QImage shot = plot->grab().toImage();
  const QRectF area = plot->PlotArea();
  const QColor ink = plot->palette().color(QPalette::WindowText);

  int found = 0;
  for (int y = static_cast<int>(area.top());
       y < static_cast<int>(area.bottom()); ++y) {
    for (int x = static_cast<int>(area.left());
         x < static_cast<int>(area.right()); ++x) {
      if (IsColour(shot, x, y, ink, 40)) {
        ++found;
      }
    }
  }

  EXPECT_EQ(found, 0) << found << " marker pixels on an empty spectrum";
}

TEST(SpectrumPanelTest, TheFilterCornerIsMarkedWhereTheAxisPutsIt) {
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  panel.show();

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(plot, nullptr);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  panel.OnSpectrumReady(Levels(bins, -80.0), Levels(bins, -120.0),
                        Levels(bins, -80.0), 15);

  const bool dark = theme_tokens::IsDarkPalette(panel.palette());
  const QColor marker = theme_tokens::PlotColor(
      theme_tokens::PlotColorToken::kFilterCorner, dark);

  const int column = BusiestColumn(plot->grab().toImage(), marker, 20);
  ASSERT_GE(column, 0) << "no filter-corner line was drawn";

  const QRectF area = plot->PlotArea();
  const double expected =
      area.left() +
      (plot->Axis().ProportionOf(analysis::kLowPassCornerHz) * area.width());

  EXPECT_NEAR(column, expected, 2.0)
      << "the corner was drawn at " << column << " and the axis puts 13.2 MHz "
      << "at " << expected;
}

TEST(SpectrumPanelTest, TheFilterCornerFollowsTheAxisIntoTheSpectrogram) {
  // Frequency runs up the side there, so the same marker is a horizontal line
  // — and it has to be at the same frequency, because the two views are one
  // measurement and a reader moves between them.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);
  panel.show();

  auto* const view = Named<QComboBox>(panel, SpectrumPanel::kViewComboName);
  ASSERT_NE(view, nullptr);
  view->setCurrentIndex(1);

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(plot, nullptr);
  ASSERT_EQ(plot->view(), SpectrumView::kSpectrogram);

  FeedCarrier(panel, 800, 5);

  const bool dark = theme_tokens::IsDarkPalette(panel.palette());
  const QColor marker = theme_tokens::PlotColor(
      theme_tokens::PlotColorToken::kFilterCorner, dark);

  const int row = BusiestRow(plot->grab().toImage(), marker, 20);
  ASSERT_GE(row, 0) << "no filter-corner line was drawn";

  const QRectF area = plot->PlotArea();
  const double expected =
      area.bottom() -
      (plot->Axis().ProportionOf(analysis::kLowPassCornerHz) * area.height());

  EXPECT_NEAR(row, expected, 2.0);
}

TEST(SpectrumPanelTest, TheReadoutChangesWithTheSegmentCountItIsGiven) {
  // The figure comes from the worker rather than from the panel's own controls,
  // and a signal that carried it but a panel that ignored it would look exactly
  // like this test passing for the wrong reason — hence comparing pictures
  // rather than asserting the string a moment after formatting it.
  SpectrumPanel panel(nullptr);
  panel.resize(600, 300);

  auto* const plot = Named<SpectrumPlot>(panel, SpectrumPanel::kPlotName);
  ASSERT_NE(plot, nullptr);

  const size_t bins = (analysis::kDefaultTransformSize / 2) + 1;
  const std::vector<double> levels = Levels(bins, -80.0);

  panel.OnSpectrumReady(levels, Levels(bins, -120.0), levels, 15);
  const QImage averaged = plot->grab().toImage();

  panel.OnSpectrumReady(levels, Levels(bins, -120.0), levels, 0);
  const QImage silent = plot->grab().toImage();

  EXPECT_NE(averaged, silent)
      << "the panel drew the same thing whether or not it was told how many "
         "segments were averaged";
}

TEST(SpectrumPanelTest, TheCursorLabelSaysWhatToDoBeforeItIsUsed) {
  SpectrumPanel panel(nullptr);

  auto* const cursor = Named<QLabel>(panel, SpectrumPanel::kCursorLabelName);
  ASSERT_NE(cursor, nullptr);
  EXPECT_FALSE(cursor->text().isEmpty());
}

}  // namespace
}  // namespace ddd::gui
