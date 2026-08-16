/************************************************************************

    statistics_presenter.h

    Turning the statistics block into the sentences a user reads
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <QString>

#include "capture_metatypes.h"
#include "free_space.h"
#include "front_end_gain.h"
#include "monitor_tap.h"
#include "usb_device_info.h"

namespace ddd::gui {

// Every figure the Statistics panel shows, as text, with no widget involved.
//
// Separated from the panel because this is the part that can be wrong. A label
// that says the buffer queue is at 3% when it is at 30%, or that shows a
// whole-capture maximum where a recent one was meant, is a defect a screenshot
// will not reveal and a widget test can only reach through a QApplication and a
// platform plugin. As a value produced by a function it can be checked against
// a pipeline running on synthetic data, which is what the acceptance criteria
// for this panel actually ask for.
struct StatisticsView {
  QString throughput;
  QString integrity;

  QString buffer;
  int buffer_percent = 0;

  // The device's own buffer.
  //
  // The bar is how full it got, as a fraction of the whole buffer, so that
  // ordinary use is visible: a working capture fills it to the packet threshold
  // and no further, which is half, and a bar that sat at zero through all of
  // that could not be told from one that was broken. Half is therefore the mark
  // where ordinary use ends rather than a warning, and the caption is what says
  // which side of it this reading is on.
  QString back_pressure;
  int back_pressure_percent = 0;

  // The figures behind the bar, for the tooltip: what the buffer did, what it
  // has done since the device was opened, and what it cost. Too much for a
  // caption and exactly what somebody who has just noticed the bar move wants.
  QString back_pressure_detail;

  QString signal_level;
  QString extremes;
  QString clipping;

  // Both scaled — see FormatCount. Ninety thousand million samples written out
  // in full is a figure nobody reads.
  QString transfers;
  QString samples;
  QString elapsed;
  QString bytes_written;

  // Samples the sink has taken but not yet committed. Blank while monitoring,
  // because nothing is being written and a backlog of zero would read as a
  // measurement rather than as an absence.
  //
  // The figure that separates "the disk cannot keep up" from "the encoder
  // cannot keep up". Both end as a buffer queue that will not come down, and
  // they have different remedies — a faster drive against a lower compression
  // level — so a user staring at a full queue has no way to choose between them
  // without this.
  QString encoder_backlog;

  // How much longer the destination volume will hold a capture.
  QString space_remaining;

  QString link_speed;
  QString front_end_gain;
};

// The view for a set of statistics.
//
// The gain, the link speed and the free space are passed in rather than read
// from the stats block because none of them is something the pipeline knows:
// one is a declaration the user made, one is a property of the device the run
// was opened on, and one is a property of a volume the engine has no opinion
// about. A default-constructed FreeSpace means "not known", which is what the
// view then says.
StatisticsView PresentStatistics(
    const capture::CaptureStats& stats, const analysis::FrontEndGain& gain,
    capture::DeviceSpeed speed, const capture::FreeSpace& space = {},
    double bytes_per_second = capture::kEstimatedCaptureBytesPerSecond);

// The view before anything has run — every measured figure blank, and the facts
// that are known regardless still shown.
StatisticsView PresentIdleStatistics(
    const analysis::FrontEndGain& gain, capture::DeviceSpeed speed,
    const capture::FreeSpace& space = {},
    double bytes_per_second = capture::kEstimatedCaptureBytesPerSecond);

// The back-pressure bar's fall-off.
//
// A reading covers a quarter of a second and reports the worst moment in it, so
// a single bad interval is a 250 ms flash on a bar that is otherwise still —
// which a user watching a capture will simply not see. This holds the peak and
// lets it fall a fraction at a time, the way a level meter does.
//
// Separate from PresentStatistics because it is the one part of the display
// with memory, and a class with one number in it can be tested for the property
// that matters: a spike is visible for about a second and then gone.
class BackPressureHold {
 public:
  // Numerator and denominator of what survives each reading. Seven tenths per
  // quarter second puts a spike below a tenth of its height after a second.
  static constexpr int kDecayNumerator = 7;
  static constexpr int kDecayDenominator = 10;

  // Take a reading and return what should be shown.
  int Apply(int sample);

  // Back to nothing shown, for the end of a run.
  void Reset() { displayed_ = 0; }

  int displayed() const { return displayed_; }

 private:
  int displayed_ = 0;
};

// A byte count as a size a person reads. GB and not GiB: a drive is sold in the
// former, and a user comparing this figure with what their file manager says
// wants the two to agree.
QString FormatByteSize(uint64_t bytes);

// A plain count, scaled to a unit a person can take in at a glance.
//
// A capture running for a side of a disc reaches ninety thousand million
// samples, and "90,113,472,000" is a figure nobody reads — they count the digit
// groups, get it wrong, and look away. Three significant figures and a k, M, G
// or T is what the number is actually used for: noticing that it is climbing,
// and roughly where it has got to.
//
// Below a thousand the count is given exactly, because down there every digit
// is information: "3 transfers" is a fact about the run and "3.00" is not.
QString FormatCount(uint64_t value);

// Samples waiting inside the encoder, as the length of signal they represent.
//
// Time rather than a sample count, because the number that matters is how it
// compares with the ring: a backlog of a fraction of a second is the encoder
// working normally, and one approaching the queue's own depth is the encoder
// about to lose the capture.
QString FormatEncoderBacklog(uint64_t samples_pending);

// How much capture a volume will hold, as a time and a size.
//
// The rate is passed in because it is not a constant: an uncompressed capture
// costs twice what a FLAC one does and decimation halves it, so the same volume
// holds four times as much of one as of the other. See
// CaptureSettings::EstimatedBytesPerSecond.
QString FormatSpaceRemaining(
    const capture::FreeSpace& space,
    double bytes_per_second = capture::kEstimatedCaptureBytesPerSecond);

// How a throughput figure is put to a user: megabytes per second alongside the
// sample rate it corresponds to.
//
// Both, because they answer different questions. MB/s is what a disk is
// specified in and what tells someone whether their storage can keep up; Msps
// is what the device is specified in and what tells them whether they are
// getting all of the signal. Showing only one leaves the other to be worked out
// with a calculator.
QString FormatThroughput(double bytes_per_second);

// A sample range as a proportion of the 10-bit scale, which is the form the
// number is actually used in: a user adjusting RF gain wants to know how much
// of the range is being used, not what the raw counts are. With the gain
// declared it carries the millivolt figure as well.
QString FormatAmplitude(const capture::SampleMetricsSnapshot& metrics,
                        const analysis::FrontEndGain& gain);

// An elapsed time, in seconds up to a minute and in hours, minutes and seconds
// past it — because a capture runs for a side of a disc, and "5,412.3 s" is not
// a length of time anybody can picture.
QString FormatElapsed(double seconds);

}  // namespace ddd::gui
