/************************************************************************

    statistics_presenter.cpp

    Turning the statistics block into the sentences a user reads
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "statistics_presenter.h"

#include <QCoreApplication>

#include "gain_choices.h"
#include "sample_format.h"

namespace ddd::gui {
namespace {

const char* kNoValue = "—";

QString Translate(const char* text) {
  return QCoreApplication::translate("StatisticsPresenter", text);
}

QString None() { return QString::fromUtf8(kNoValue); }

QString DescribeSequenceState(capture::SequenceState state) {
  switch (state) {
    case capture::SequenceState::kSynchronising:
      return Translate("Synchronising");
    case capture::SequenceState::kRunning:
      return Translate("Verified — no samples lost");
    case capture::SequenceState::kDisabled:
      return Translate(
          "Not available — this gateware does not send sequence markers");
    case capture::SequenceState::kFailed:
      return Translate("Broken — samples have been lost");
  }
  return Translate("Unknown");
}

QString DescribeLinkSpeed(capture::DeviceSpeed speed) {
  if (speed == capture::DeviceSpeed::kUnknown) {
    return None();
  }
  return QString::fromUtf8(capture::DeviceSpeedName(speed));
}

}  // namespace

QString FormatThroughput(double bytes_per_second) {
  if (bytes_per_second <= 0.0) {
    return None();
  }

  const double megabytes = bytes_per_second / (1024.0 * 1024.0);
  const double megasamples = bytes_per_second /
                             static_cast<double>(capture::kBytesPerSample) /
                             1'000'000.0;

  return Translate("%1 MB/s  (%2 Msps)")
      .arg(megabytes, 0, 'f', 1)
      .arg(megasamples, 0, 'f', 2);
}

QString FormatAmplitude(const capture::SampleMetricsSnapshot& metrics,
                        const analysis::FrontEndGain& gain) {
  if (metrics.sample_count == 0) {
    return None();
  }

  const double span = static_cast<double>(metrics.recent_maximum_value) -
                      static_cast<double>(metrics.recent_minimum_value);
  const double proportion =
      span / static_cast<double>(capture::kMaximumSampleValue) * 100.0;

  if (!gain.declared()) {
    return Translate("%1 to %2 of 1023  (%3% of range)")
        .arg(metrics.recent_minimum_value)
        .arg(metrics.recent_maximum_value)
        .arg(proportion, 0, 'f', 1);
  }

  return Translate("%1 to %2 of 1023  (%3% of range, %4 mV p-p)")
      .arg(metrics.recent_minimum_value)
      .arg(metrics.recent_maximum_value)
      .arg(proportion, 0, 'f', 1)
      .arg(gain.CodeSpanToInputMillivolts(span), 0, 'f', 0);
}

QString FormatElapsed(double seconds) {
  if (seconds < 0.0) {
    return None();
  }

  if (seconds < 60.0) {
    return Translate("%1 s").arg(seconds, 0, 'f', 1);
  }

  const auto whole = static_cast<qint64>(seconds);
  const qint64 hours = whole / 3600;
  const qint64 minutes = (whole % 3600) / 60;
  const qint64 remainder = whole % 60;

  return Translate("%1:%2:%3")
      .arg(hours)
      .arg(minutes, 2, 10, QLatin1Char('0'))
      .arg(remainder, 2, 10, QLatin1Char('0'));
}

QString FormatByteSize(uint64_t bytes) {
  constexpr double kGigabyte = 1.0e9;
  constexpr double kMegabyte = 1.0e6;

  if (static_cast<double>(bytes) >= kGigabyte) {
    return Translate("%1 GB").arg(static_cast<double>(bytes) / kGigabyte, 0,
                                  'f', 1);
  }
  return Translate("%1 MB").arg(static_cast<double>(bytes) / kMegabyte, 0, 'f',
                                0);
}

QString FormatEncoderBacklog(uint64_t samples_pending) {
  const double seconds = static_cast<double>(samples_pending) /
                         static_cast<double>(capture::kSampleRateHz);

  return Translate("%1 ms  (%L2 samples)")
      .arg(seconds * 1000.0, 0, 'f', 1)
      .arg(samples_pending);
}

QString FormatSpaceRemaining(const capture::FreeSpace& space) {
  if (!space.known) {
    return None();
  }

  // The time first. "412 GB free" does not answer the question somebody
  // actually has, which is whether this will last the side they are about to
  // play.
  return Translate("%1 of capture  (%2 free)")
      .arg(FormatElapsed(
          capture::CaptureSecondsRemaining(space.bytes_available)))
      .arg(FormatByteSize(space.bytes_available));
}

StatisticsView PresentIdleStatistics(const analysis::FrontEndGain& gain,
                                     capture::DeviceSpeed speed,
                                     const capture::FreeSpace& space) {
  StatisticsView view;
  view.throughput = None();
  view.integrity = None();
  view.buffer = None();
  view.signal_level = None();
  view.extremes = None();
  view.clipping = None();
  view.transfers = None();
  view.samples = None();
  view.elapsed = None();
  view.bytes_written = None();
  view.encoder_backlog = None();
  view.space_remaining = FormatSpaceRemaining(space);
  view.link_speed = DescribeLinkSpeed(speed);
  view.front_end_gain = DescribeFrontEndGain(gain.switch_pattern());
  return view;
}

StatisticsView PresentStatistics(const capture::CaptureStats& stats,
                                 const analysis::FrontEndGain& gain,
                                 capture::DeviceSpeed speed,
                                 const capture::FreeSpace& space) {
  StatisticsView view = PresentIdleStatistics(gain, speed, space);

  view.throughput = FormatThroughput(stats.throughput_bytes_per_second);
  view.integrity = DescribeSequenceState(stats.sequence_state);

  if (stats.slot_count > 0) {
    view.buffer_percent =
        static_cast<int>(stats.slots_in_use * 100 / stats.slot_count);
    // The peak is in the text rather than in a second widget because it is the
    // figure that matters after the fact: a capture that was fine except for
    // one stall thirty minutes in reads as perfect from the live value alone.
    view.buffer = Translate("%1 of %2 buffers  (peak %3)")
                      .arg(stats.slots_in_use)
                      .arg(stats.slot_count)
                      .arg(stats.peak_slots_in_use);
  }

  view.signal_level = FormatAmplitude(stats.metrics, gain);

  if (stats.metrics.sample_count > 0) {
    if (!gain.declared()) {
      view.extremes = Translate("%1 to %2 of 1023 since the run started")
                          .arg(stats.metrics.minimum_value)
                          .arg(stats.metrics.maximum_value);
    } else {
      const double span = static_cast<double>(stats.metrics.maximum_value) -
                          static_cast<double>(stats.metrics.minimum_value);
      view.extremes =
          Translate("%1 to %2 of 1023 since the run started  (%3 mV p-p)")
              .arg(stats.metrics.minimum_value)
              .arg(stats.metrics.maximum_value)
              .arg(gain.CodeSpanToInputMillivolts(span), 0, 'f', 0);
    }

    // Whole-capture totals with the most recent buffer beside them. The
    // distinction is what makes the figure usable while adjusting a gain
    // control: the running total records the worst moment since the run began
    // and will not come back down, so on its own it cannot show that a change
    // has helped.
    view.clipping = Translate("%1 low, %2 high  (%3 and %4 in the last buffer)")
                        .arg(stats.metrics.clipped_low_count)
                        .arg(stats.metrics.clipped_high_count)
                        .arg(stats.metrics.recent_clipped_low_count)
                        .arg(stats.metrics.recent_clipped_high_count);

    view.samples = Translate("%L1").arg(stats.metrics.sample_count);
  }

  view.transfers = Translate("%1 transfers, %2 buffers")
                       .arg(stats.transfers_completed)
                       .arg(stats.buffers_processed);

  view.elapsed = FormatElapsed(stats.elapsed_seconds);

  if (stats.bytes_written > 0) {
    view.bytes_written = Translate("%1 MB").arg(
        static_cast<double>(stats.bytes_written) / (1024.0 * 1024.0), 0, 'f',
        1);
  }

  // Only while a writer is attached. Zero pending is a meaningful measurement
  // during a capture and a meaningless one during monitoring — there is no
  // encoder — so the row stays blank rather than reporting a healthy-looking
  // nothing.
  if (stats.writing) {
    view.encoder_backlog = FormatEncoderBacklog(stats.samples_pending);
  }

  return view;
}

}  // namespace ddd::gui
