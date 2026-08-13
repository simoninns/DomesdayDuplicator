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

  QString signal_level;
  QString extremes;
  QString clipping;

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
StatisticsView PresentStatistics(const capture::CaptureStats& stats,
                                 const analysis::FrontEndGain& gain,
                                 capture::DeviceSpeed speed,
                                 const capture::FreeSpace& space = {});

// The view before anything has run — every measured figure blank, and the facts
// that are known regardless still shown.
StatisticsView PresentIdleStatistics(const analysis::FrontEndGain& gain,
                                     capture::DeviceSpeed speed,
                                     const capture::FreeSpace& space = {});

// A byte count as a size a person reads. GB and not GiB: a drive is sold in the
// former, and a user comparing this figure with what their file manager says
// wants the two to agree.
QString FormatByteSize(uint64_t bytes);

// Samples waiting inside the encoder, as the length of signal they represent.
//
// Time rather than a sample count, because the number that matters is how it
// compares with the ring: a backlog of a fraction of a second is the encoder
// working normally, and one approaching the queue's own depth is the encoder
// about to lose the capture.
QString FormatEncoderBacklog(uint64_t samples_pending);

// How much capture a volume will hold, as a time and a size.
QString FormatSpaceRemaining(const capture::FreeSpace& space);

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
