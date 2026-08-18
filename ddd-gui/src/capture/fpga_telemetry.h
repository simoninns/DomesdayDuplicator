/************************************************************************

    fpga_telemetry.h

    What the gateware reports about its own capture buffer
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstdint>
#include <span>

namespace ddd::capture {

// The device's own account of how close its capture buffer came to overflowing.
//
// The FIFO between the ADC and the FX3 is what a USB stall is paid out of, and
// until this existed the host could not see it at all: a capture that peaked at
// 99% on every packet and one that never went above half looked identical from
// here. The gateware keeps the counters, a read samples them into a shadow bank
// so that a reading is coherent, and this is one such reading.
//
// The interval figures — the peak, the events, the drops — cover the time since
// the previous reading, because the read that produced this one is also what
// cleared them. The lifetime figures cover the time since the device was
// opened and are never cleared, which is what makes them the figures to trust
// when something else on the machine has been reading the same registers.
struct FpgaTelemetry {
  // The signature was there. False covers every way of not knowing: no device,
  // gateware that predates this instrument, a device whose FPGA has not
  // finished configuring, or a unit running its recovery gateware, which has no
  // capture buffer to report on.
  bool present = false;

  // The layout the device says it used. Nothing outside ParseFpgaTelemetry
  // should need it; it exists so that a future layout change is a fact this
  // build can notice rather than a silent misreading.
  uint8_t format = 0;

  // The buffer has overflowed at least once since the device was opened. Never
  // cleared by a reading, so it survives a second reader consuming an interval.
  bool overflow_since_open = false;

  // A counter reached its maximum during this interval, so its figure is a
  // floor rather than a count. Set only by a catastrophe: the drop counter
  // saturates after 65535 lost samples.
  bool saturated = false;

  // Increments once per reading, and wraps. Two consecutive readings that
  // differ by more than one mean something else polled in between and consumed
  // an interval that this one therefore never saw.
  uint8_t latch_count = 0;

  // Occupancy at the instant the reading was taken, in words.
  uint16_t used_now = 0;

  // The highest occupancy reached during the interval. The figure that matters:
  // an instantaneous reading four times a second samples a signal that changes
  // every 12.5 nanoseconds and finds nothing.
  uint16_t peak = 0;

  // The highest occupancy reached since the device was opened.
  uint16_t peak_since_open = 0;

  // Distinct overflow bursts during the interval, and the samples they cost. A
  // stall is one event however long it lasts, so these answer different
  // questions: how often, and how much.
  uint16_t overflow_events = 0;
  uint16_t dropped_words = 0;

  // Packets the FX3 took during the interval — the drain rate as the device
  // saw it.
  uint16_t packets_read = 0;

  // Time spent at or above the near-full threshold during the interval, in
  // units of kTelemetryNearFullPrescale samples. Duration rather than
  // amplitude: a spike and a sustained squeeze produce the same peak.
  uint16_t near_full_units = 0;

  // The buffer's dimensions, as the device reports them rather than as this
  // build assumes them. A host that hardcoded these would misreport every
  // reading from a device whose gateware had been resized.
  uint16_t depth_words = 0;
  uint16_t packet_words = 0;
  uint16_t near_full_words = 0;

  // Back pressure, from 0 (none) to 100 (samples lost).
  //
  // Deliberately not raw occupancy. A healthy capture sawtooths between a
  // quarter and a half of the FIFO, because the gateware offers a packet only
  // once a whole one is queued and the FX3 then drains faster than the ADC
  // fills — so a raw scale would idle at half and its top half would be the
  // only part that ever meant anything.
  //
  // The scale is the headroom above the packet threshold, which is exactly what
  // a stall is paid out of. Zero means the buffer never rose above a packet:
  // the FX3 took every packet as soon as it was offered. One hundred means the
  // buffer reached its depth, which is overflow, and any interval that dropped
  // a sample reads 100 whatever the arithmetic says.
  int BackPressurePercent() const;

  // The interval peak as a percentage of the whole buffer, and the occupancy at
  // the instant the reading was taken as the same.
  //
  // These are what a display should show, and BackPressurePercent is what it
  // should judge by. The distinction is the difference between an instrument
  // and a warning light: on a working capture the peak reaches the packet
  // threshold and stops, which is half the buffer, so the pressure figure is
  // zero for hours at a time and a bar drawn from it never moves. A bar that
  // never moves cannot be told from a bar that is broken, and the whole point
  // of this instrument is to be believed when it does move.
  //
  // On this scale the halfway mark is where ordinary use ends: up to half is
  // the sawtooth of a device whose packets are being taken as fast as they are
  // offered, and everything above it is the FX3 having been late.
  int PeakPercentOfDepth() const;
  int UsedPercentOfDepth() const;
};

// Parse a block read from kRegisterTelemetryId.
//
// Returns a default-constructed value — present false — for anything that is
// not a well-formed block, including the all-zero reading a gateware without
// the instrument gives and the all-ones reading of a floating link.
FpgaTelemetry ParseFpgaTelemetry(std::span<const uint8_t> block);

}  // namespace ddd::capture
