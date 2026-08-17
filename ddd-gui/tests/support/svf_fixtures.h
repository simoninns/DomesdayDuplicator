/************************************************************************

    svf_fixtures.h

    Programming files to play the SVF player against
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ddd::capture {

// A file written by hand to exercise the grammar rather than a device: every
// statement the player understands, in a shape small enough that a test can
// say what the vector stream should be.
inline constexpr char kGrammarSvf[] = R"(
! A comment, in SVF's own spelling.
// And in the one every other tool also accepts.
TRST ABSENT;
ENDIR IDLE;
ENDDR IDLE;
STATE RESET;
STATE IDLE;
FREQUENCY 1.00E+07 HZ;
SIR 10 TDI (006);
RUNTEST IDLE 8 TCK ENDSTATE IDLE;
SDR 32 TDI (00000000) TDO (00000000) MASK (FFFFFFFF);
)";

// The opening of a real Quartus-emitted provisioning file for this project's
// own board, cut short after the checks that run before anything is written.
//
// It is here because a parser that only ever meets its author's idea of the
// format is a parser that meets the real thing for the first time on a
// bench. What this adds over the fixture above is the shape Quartus actually
// writes: values continued across lines and indented, comments in the middle
// of the file, the same instruction register loaded over and over, and waits
// counted in millions of cycles.
//
// Taken from `quartus_cpf -c -q 4.5MHz -g 3.3 -n p
// DomesdayDuplicatorProvisioning_write_jic.cdf …`, run against the artefacts
// of `nix build .#bitstream`. Altera's licence banner, fourteen lines of it,
// is the only thing dropped.
inline constexpr char kQuartusOpeningSvf[] = R"(
!Quartus Prime SVF converter 25.1
!
!Device #1: EP4CE22 - ./DomesdayDuplicatorProvisioning.jic
!
!NOTE "USERCODE" "0017E24B";
!
!NOTE "CHECKSUM" "798AD5E3";
!
FREQUENCY 4.50E+06 HZ;
!
TRST ABSENT;
ENDDR IDLE;
ENDIR IRPAUSE;
STATE IDLE;
!
!CHECKING SFL IP VERSION
!
SIR 10 TDI (006);
RUNTEST IDLE 8 TCK ENDSTATE IDLE;
SDR 32 TDI (00000000);
SIR 10 TDI (00E);
RUNTEST 8 TCK;
SDR 32 TDI (00000000);
SIR 10 TDI (00C);
RUNTEST 8 TCK;
SDR 4 TDI (0);
SDR 4 TDI (0) TDO (0) MASK (F);
SDR 4 TDI (0) TDO (0);
SDR 4 TDI (0) TDO (E);
SDR 4 TDI (0) TDO (6);
SIR 10 TDI (00E);
RUNTEST 8 TCK;
SDR 13 TDI (1FFF);
RUNTEST 4500000 TCK;
SDR 13 TDI (1FFF) TDO (0800) MASK (0800);
!
!CHECKING SILICON ID
!
SIR 10 TDI (00E);
RUNTEST 8 TCK;
SDR 13 TDI (1111);
SIR 10 TDI (00C);
RUNTEST 8 TCK;
SDR 1 TDI (0);
SDR 68 TDI (0000000000000000F)
	TDO (00000000000000000)
	MASK (00000000000000000);
STATE IDLE;
)";

// What a device has to say for the opening file to get through it: the
// answers to its four-bit reads, then the version bit it waits on, then the
// silicon identifier it does not check. In the order they are asked for,
// because that is the order the player reads them in — anything else and the
// run is expected to stop, which is the other half of what these tests check.
inline std::vector<bool> QuartusOpeningAnswers() {
  struct Answer {
    const char* hex;
    size_t bits;
  };
  static constexpr Answer kAnswers[] = {{"0", 4}, {"0", 4},     {"E", 4},
                                        {"6", 4}, {"0800", 13}, {"0", 68}};

  std::vector<bool> stream;
  for (const Answer& answer : kAnswers) {
    const std::string hex = answer.hex;
    for (size_t index = 0; index < answer.bits; ++index) {
      const size_t nibble = index / 4;
      bool set = false;
      if (nibble < hex.size()) {
        const char digit = hex[hex.size() - 1 - nibble];
        const int value =
            digit <= '9' ? digit - '0' : (digit & ~0x20) - 'A' + 10;
        set = ((value >> (index % 4)) & 1) != 0;
      }
      stream.push_back(set);
    }
  }
  return stream;
}

}  // namespace ddd::capture
