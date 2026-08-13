/************************************************************************

    front_end_gain.h

    The board's RF gain switch, as declared by the user
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ddd::analysis {

// What turns a sample value into a voltage, and why the user has to say.
//
// The Domesday Duplicator's analogue front end is an OPA690 whose feedback
// resistance is set by SW401, a four-way DIP switch selecting 1 kΩ5, 1 kΩ,
// 680 Ω and 560 Ω in parallel against a fixed 200 Ω. The gain is therefore
// 1 + Rf‖ / 200, and the largest input the board can take without clipping is
// the ADC's 2 V p-p full scale divided by it.
//
// SW401 is mechanical and has no electrical path to the FPGA or the FX3.
// Nothing in the sample stream, the USB descriptors or the vendor requests
// reveals its position, so the application cannot discover it and must be told.
// Everything below follows from that one fact:
//
//   - There is no default gain. The undeclared state is a real state, and while
//     the setting is in it no display may show a voltage. A plausible default
//     would produce authoritative-looking millivolt figures wrong by up to a
//     factor of four, with nothing on screen to reveal it — worse than showing
//     nothing at all.
//
//   - Nothing here reaches the engine. This is a display calibration: samples
//     are stored and written as the 10-bit codes the converter produced, and
//     the conversion happens where they are drawn. A user who realises they
//     declared the wrong switches can correct it and every figure already on
//     screen re-scales, because nothing was ever stored in the derived units.
//
//   - Clipping does not depend on it. A clipped sample is one whose code has
//     reached 0 or 1023, which is a property of the converter; clip counts stay
//     correct whether the declaration is absent, right or wrong.

// The four feedback resistors, in switch order. Switch 1 is the first.
inline constexpr double kSwitchResistanceOhms[] = {1500.0, 1000.0, 680.0,
                                                   560.0};

inline constexpr size_t kSwitchCount =
    sizeof(kSwitchResistanceOhms) / sizeof(kSwitchResistanceOhms[0]);

// The fixed leg of the divider, R405 on the board.
inline constexpr double kGainResistanceOhms = 200.0;

// Full scale at the amplifier's output, which is what the converter spans.
inline constexpr double kAdcFullScaleMillivoltsPeakToPeak = 2000.0;

// Distinct codes a 10-bit converter produces. 1024 rather than 1023: the codes
// divide the full-scale span into that many steps.
inline constexpr int kAdcCodeCount = 1024;

// The code representing zero once the signal is centred.
inline constexpr double kAdcMidScaleCode = 512.0;

// Switch patterns are a bit per switch, switch 1 in the highest bit, so the
// pattern reads left to right as the switches sit on the board.
inline constexpr uint8_t kUndeclaredSwitchPattern = 0;
inline constexpr uint8_t kMaximumSwitchPattern = 0x0F;

// A declared front-end gain, or the absence of one.
//
// Zero is not a switch pattern: with all four switches open there is no
// feedback path and the amplifier has no defined gain. That makes it free to
// carry the undeclared state, so the whole setting is one small integer with no
// separate "is it set" flag to get out of step with it.
class FrontEndGain {
 public:
  // Undeclared.
  FrontEndGain() = default;

  // A pattern outside 1..15 yields the undeclared state rather than an error:
  // this is fed from a settings file that may have been written by hand or by
  // another version, and the safe reading of a value that makes no sense is
  // that nothing has been declared.
  static FrontEndGain FromSwitchPattern(uint8_t pattern);

  bool declared() const { return pattern_ != kUndeclaredSwitchPattern; }

  uint8_t switch_pattern() const { return pattern_; }

  // The parallel combination of the closed switches' resistors. Zero when
  // undeclared.
  double FeedbackResistanceOhms() const;

  // 1 + Rf‖ / 200. Zero when undeclared — deliberately not 1, so a caller that
  // forgot to check declared() produces an obviously broken figure rather than
  // a plausible one.
  double Gain() const;

  // Millivolts at the BNC per converter code. Zero when undeclared.
  double MillivoltsPerCode() const;

  // A code as a voltage at the BNC, relative to mid-scale. Zero when
  // undeclared.
  double CodeToInputMillivolts(double code) const;

  // A span of codes as a peak-to-peak input voltage. Zero when undeclared.
  double CodeSpanToInputMillivolts(double code_span) const;

  // The largest input the board can take at this gain without clipping.
  double FullScaleInputMillivoltsPeakToPeak() const;

  bool operator==(const FrontEndGain& other) const {
    return pattern_ == other.pattern_;
  }
  bool operator!=(const FrontEndGain& other) const { return !(*this == other); }

 private:
  uint8_t pattern_ = kUndeclaredSwitchPattern;
};

// The switch block as it looks on the board: one digit per switch, switch 1
// first, 1 for closed and 0 for open — so "1000" is switch 1 alone and "1010"
// is switches 1 and 3. Empty when nothing is declared.
//
// All four digits, always, rather than naming only the closed ones. A DIP
// switch is read by looking at it, and what a user sees is four positions of
// which some are up; a list of the closed ones makes them count along the block
// to check, which is exactly the error this is meant to prevent.
std::string DescribeSwitchPattern(uint8_t pattern);

}  // namespace ddd::analysis
