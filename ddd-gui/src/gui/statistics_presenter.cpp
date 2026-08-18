/************************************************************************

    statistics_presenter.cpp

    Turning the statistics block into the sentences a user reads
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "statistics_presenter.h"

#include <QCoreApplication>
#include <algorithm>

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

// The caption on the back-pressure bar.
//
// Words rather than percentages, because the words are what say the buffer is
// being used at all. A capture that is working fills it to the packet threshold
// on every packet the FX3 takes — 8194 of 16384 words on the hardware this was
// written against — and a caption that reported that as "0%" was true, useless,
// and indistinguishable from an instrument that was not reading anything.
//
// Four cases. The difference between the first two is the point of them: a
// device that cannot report its buffer must not look like a device whose buffer
// is untroubled. Gateware that predates the instrument is perfectly good at
// capturing, so that is a statement about the display and not a fault.
QString DescribeBackPressure(const capture::CaptureStats& stats) {
  const capture::FpgaTelemetry& device = stats.device_buffer;

  if (!device.present) {
    return Translate("Not reported by this gateware");
  }

  if (device.peak == 0 && device.packets_read == 0) {
    // Nothing has moved at all: the device is attached and answering, and its
    // buffer is neither filling nor draining.
    return Translate("Idle");
  }

  if (stats.device_overflow_events > 0) {
    // Once samples have been lost the percentages have stopped being the
    // interesting numbers, so the caption becomes the damage instead.
    return Translate("%1 overflows, %2 samples lost")
        .arg(stats.device_overflow_events)
        .arg(stats.device_dropped_words);
  }

  if (device.BackPressurePercent() > 0) {
    // Above the packet threshold: the FX3 was late and the buffer is into the
    // room a stall is paid out of. This is the reading worth noticing, and it
    // says so in words rather than leaving the bar to be interpreted.
    return Translate("%1 of %2 words — %3% into the reserve")
        .arg(device.peak)
        .arg(device.depth_words)
        .arg(device.BackPressurePercent());
  }

  // The ordinary case, and the one that has to look ordinary.
  //
  // The occupancy at the instant of the reading leads, because it is the figure
  // that changes: it is the sawtooth sampled at whatever phase the reading
  // caught it in. The peak follows it and is deliberately dull — a working
  // capture fills to the packet threshold and is drained from there, so the
  // peak is that threshold plus the two words the FPGA adds while the FX3 is
  // starting, on every interval, for ever. That figure only becomes news when
  // it stops being constant, which is exactly when the caption below takes
  // over.
  return Translate("now %1, peak %2 of %3")
      .arg(device.used_now)
      .arg(device.peak)
      .arg(device.depth_words);
}

// What is behind the bar, for the tooltip.
QString DetailBackPressure(const capture::CaptureStats& stats) {
  const capture::FpgaTelemetry& device = stats.device_buffer;

  if (!device.present) {
    return QString();
  }

  return Translate(
             "Peak %1 of %2 words this reading, %3 since the device was "
             "opened.\n"
             "A packet is offered at %4 words and taken immediately, so on a "
             "capture that is keeping up the peak is that figure plus a word "
             "or two on every reading — a peak that stops being constant is "
             "the host having been late.\n"
             "%5 packets taken since the last reading; %6 overflows and %7 "
             "samples lost this run.")
      .arg(device.peak)
      .arg(device.depth_words)
      .arg(device.peak_since_open)
      .arg(device.packet_words)
      .arg(device.packets_read)
      .arg(stats.device_overflow_events)
      .arg(stats.device_dropped_words);
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

QString FormatCount(uint64_t value) {
  // Powers of a thousand, not of 1024. These are counts of events and samples
  // rather than quantities of memory, and a user reading "40 M samples" against
  // a device specified at 40 million a second expects the two to line up.
  struct Unit {
    double scale;
    const char* suffix;
  };
  static constexpr Unit kUnits[] = {
      {1.0e12, "T"}, {1.0e9, "G"}, {1.0e6, "M"}, {1.0e3, "k"}};

  const auto exact = static_cast<double>(value);

  for (const Unit& unit : kUnits) {
    if (exact < unit.scale) {
      continue;
    }

    const double scaled = exact / unit.scale;

    // Three significant figures throughout, so the width of the figure barely
    // changes as it climbs and the eye is not dragged sideways every time it
    // crosses a power of ten.
    const int decimals = scaled < 10.0 ? 2 : (scaled < 100.0 ? 1 : 0);
    return Translate("%1 %2")
        .arg(scaled, 0, 'f', decimals)
        .arg(QLatin1String(unit.suffix));
  }

  return QString::number(value);
}

QString FormatEncoderBacklog(uint64_t samples_pending,
                             uint32_t sample_rate_hz) {
  if (sample_rate_hz == 0) {
    sample_rate_hz = capture::kSampleRateHz;
  }
  const double seconds = static_cast<double>(samples_pending) /
                         static_cast<double>(sample_rate_hz);

  return Translate("%1 ms  (%L2 samples)")
      .arg(seconds * 1000.0, 0, 'f', 1)
      .arg(samples_pending);
}

QString FormatSpaceRemaining(const capture::FreeSpace& space,
                             double bytes_per_second) {
  if (!space.known) {
    return None();
  }

  // The time first. "412 GB free" does not answer the question somebody
  // actually has, which is whether this will last the side they are about to
  // play.
  return Translate("%1 of capture  (%2 free)")
      .arg(FormatElapsed(capture::CaptureSecondsRemaining(space.bytes_available,
                                                          bytes_per_second)))
      .arg(FormatByteSize(space.bytes_available));
}

StatisticsView PresentIdleStatistics(const analysis::FrontEndGain& gain,
                                     capture::DeviceSpeed speed,
                                     const capture::FreeSpace& space,
                                     double bytes_per_second) {
  StatisticsView view;
  view.throughput = None();
  view.integrity = None();
  view.buffer = None();
  view.back_pressure = None();
  view.signal_level = None();
  view.extremes = None();
  view.clipping = None();
  view.transfers = None();
  view.samples = None();
  view.elapsed = None();
  view.bytes_written = None();
  view.encoder_backlog = None();
  view.space_remaining = FormatSpaceRemaining(space, bytes_per_second);
  view.link_speed = DescribeLinkSpeed(speed);
  view.front_end_gain = DescribeFrontEndGain(gain.switch_pattern());
  return view;
}

StatisticsView PresentStatistics(const capture::CaptureStats& stats,
                                 const analysis::FrontEndGain& gain,
                                 capture::DeviceSpeed speed,
                                 const capture::FreeSpace& space,
                                 double bytes_per_second,
                                 uint32_t sample_rate_hz) {
  StatisticsView view =
      PresentIdleStatistics(gain, speed, space, bytes_per_second);

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

  // The bar is how full the buffer got, not how much trouble it was in. See
  // StatisticsView::back_pressure_percent: the trouble figure is zero for hours
  // on a working capture, and a bar that never leaves zero is one nobody
  // believes when it finally moves.
  view.back_pressure_percent = stats.device_buffer.PeakPercentOfDepth();
  view.back_pressure = DescribeBackPressure(stats);
  view.back_pressure_detail = DetailBackPressure(stats);

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

    view.samples = FormatCount(stats.metrics.sample_count);
  }

  view.transfers = Translate("%1 transfers, %2 buffers")
                       .arg(FormatCount(stats.transfers_completed))
                       .arg(FormatCount(stats.buffers_processed));

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
    view.encoder_backlog =
        FormatEncoderBacklog(stats.samples_pending, sample_rate_hz);
  }

  return view;
}

int BackPressureHold::Apply(int sample) {
  const int decayed = displayed_ * kDecayNumerator / kDecayDenominator;
  displayed_ = std::max(sample, decayed);
  return displayed_;
}

}  // namespace ddd::gui
