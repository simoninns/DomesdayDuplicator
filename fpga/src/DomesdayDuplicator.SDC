#************************************************************************
#
#    DomesdayDuplicator.SDC
#
#    Timing constraints
#    Domesday Duplicator - LaserDisc RF sampler
#    SPDX-FileCopyrightText: 2018-2026 Simon Inns
#    SPDX-License-Identifier: GPL-3.0-or-later
#
#************************************************************************
#
# Until 2026 this file was three lines: the 50 MHz input clock and the two
# derive_* commands. That constrained the logic inside the FPGA and nothing at
# its pins, which was survivable while the design's own clocks were the only
# things it had to get right and the FX3 link ran at 60 MHz. Moving to one
# 80 MHz clock makes the pins the interesting part: the FX3 interface is the
# fastest thing on the board, and the ADC's sampling clock is now something
# this design generates in the fabric rather than something the PLL hands it.
#
# READ THIS BEFORE TRUSTING A TIMING REPORT
#
# The clock definitions below are facts about the design and are exact. The
# I/O delays are not: they are **deliberately pessimistic bounds**, not
# datasheet values, because the ADS825, 74LVTH541 and CYUSB3014 datasheets
# were not to hand when this file was written.
#
# They are chosen so that the error is in the safe direction. Every external
# delay is set larger than any plausible real value, and every setup and hold
# requirement is set tighter than the published one. So:
#
#   - if timing closes with these numbers, it closes with the real ones, and
#     the result can be believed;
#   - if it does not close, the numbers are the first thing to check, not the
#     design. Replace the values in the block below with the datasheet ones
#     and re-run before changing any logic.
#
# Each value says what it stands for and what to look up. Nothing else in the
# file needs editing to correct them.
#
#************************************************************************


# ---------------------------------------------------------------------------
# Values to confirm against the datasheets
# ---------------------------------------------------------------------------

# The altpll output pin that carries the 80 MHz system clock. This is the
# conventional Quartus path for an altpll instance named pll_generator_0; if a
# future Quartus renames it, every reference below comes from here.
set pll_clock_pin {pll_generator_0|altpll_component|auto_generated|pll1|clk[0]}

# FX3 GPIF II, 16-bit synchronous slave. The FPGA drives PCLK, so these are
# what the FX3 requires of data this design presents to it. CYUSB3014
# publishes about 2 ns setup and 0.5 ns hold; the values here are tighter, so
# meeting them means meeting the real ones.
set fx3_setup 2.5
set fx3_hold 1.0

# What the FX3 does with its own outputs, relative to the same PCLK edge -
# readData is the only one this design times. A clock-to-out window of 1 to
# 8 ns is wider than the part is specified for, which makes the constraint
# harder to meet rather than easier.
set fx3_ctl_max 8.0
set fx3_ctl_min 1.0

# The ADC data path: ADS825 clock-to-data, plus two 74LVTH541 buffers between
# the converter and the FPGA (see the hardware guide - they are there to keep
# the ADC's own switching noise off the analogue front end), plus the board
# and header traces. 15 ns is far more than that sum can plausibly be; the
# path has a 25 ns budget, so even this bound leaves room.
set adc_data_max 15.0
set adc_data_min 2.0


# ---------------------------------------------------------------------------
# Clocks
# ---------------------------------------------------------------------------

# The DE0-Nano's 50 MHz oscillator, the only clock that enters the FPGA
create_clock -name CLOCK_50 -period 20.000 [get_ports CLOCK_50]

# The 80 MHz system clock out of the PLL. Everything in the design runs from
# it, so this one command covers every internal path.
derive_pll_clocks

# PCLK to the FX3 is the system clock driven straight onto a pin, so the clock
# at the pin is a one-to-one copy of it. Without this the FX3 databus has no
# clock to be constrained against and the whole interface goes unanalysed.
create_generated_clock -name fx3_pclk \
    -source [get_pins $pll_clock_pin] \
    [get_ports {GPIO1[31]}]

# The ADC's 40 MHz sampling clock: the divide-by-two register's output, driven
# onto a pin. Sourcing it from the register's own clock pin rather than from
# the PLL keeps this correct if the PLL hierarchy is ever renamed.
create_generated_clock -name adc_clock \
    -source [get_pins {adc_clock_divider|clk}] \
    -divide_by 2 \
    [get_ports {GPIO0[33]}]

derive_clock_uncertainty


# ---------------------------------------------------------------------------
# Port groups
# ---------------------------------------------------------------------------

# Everything the FX3 samples on a PCLK edge: the 16-bit databus, and the two
# flags it reads alongside it.
set fx3_synchronous_outputs [get_ports { \
    GPIO1[2]  GPIO1[4]  GPIO1[6]  GPIO1[8]  GPIO1[10] GPIO1[12] GPIO1[14] \
    GPIO1[16] GPIO1[18] GPIO1[20] GPIO1[22] GPIO1[24] GPIO1[26] GPIO1[28] \
    GPIO1[30] GPIO1[32] \
    GPIO1[27] \
    GPIO1[21] }]

# readData, the one FX3 output this design captures on a clock edge
set fx3_synchronous_inputs [get_ports {GPIO1[25]}]

# The 10-bit ADC databus
set adc_data_inputs [get_ports { \
    GPIO0[23] GPIO0[24] GPIO0[25] GPIO0[26] GPIO0[27] \
    GPIO0[28] GPIO0[29] GPIO0[30] GPIO0[31] GPIO0[32] }]


# ---------------------------------------------------------------------------
# The FX3 interface
# ---------------------------------------------------------------------------
#
# Source-synchronous, with this design as the source: it drives PCLK and the
# data together, and the FX3 samples both. The setup requirement becomes the
# maximum output delay and the hold requirement the minimum, negated.
#
# This is the fastest interface on the board and the one the move to 80 MHz
# puts most pressure on, so it is the report to read first.

set_output_delay -clock fx3_pclk -max $fx3_setup $fx3_synchronous_outputs
set_output_delay -clock fx3_pclk -min [expr {0 - $fx3_hold}] $fx3_synchronous_outputs

set_input_delay -clock fx3_pclk -max $fx3_ctl_max $fx3_synchronous_inputs
set_input_delay -clock fx3_pclk -min $fx3_ctl_min $fx3_synchronous_inputs


# ---------------------------------------------------------------------------
# The ADC interface
# ---------------------------------------------------------------------------
#
# The FPGA drives the ADC's clock and reads the result back, so this is
# source-synchronous with the FPGA as the source of the clock and the ADC as
# the source of the data.

set_input_delay -clock adc_clock -max $adc_data_max $adc_data_inputs
set_input_delay -clock adc_clock -min $adc_data_min $adc_data_inputs

# The sample the ADC launches on one of its clock edges is captured a whole
# ADC period later, not on the next system clock edge - dataGenerator only
# takes a sample when sample_enable is high, which is every second system
# clock edge, and those are exactly the edges the ADC clock rises on.
#
# Without this the analysis would allow 12.5 ns for a path the design gives
# 25 ns, and would report a failure that does not exist. The hold multicycle
# is one less than the setup multicycle, which is what keeps the hold check
# against the same edge it was against before the exception.
set_multicycle_path -setup 2 -from [get_clocks adc_clock] -to [get_clocks $pll_clock_pin]
set_multicycle_path -hold 1 -from [get_clocks adc_clock] -to [get_clocks $pll_clock_pin]


# ---------------------------------------------------------------------------
# Paths that are not timed, and why
# ---------------------------------------------------------------------------

# The SPI register link. Mode 0, and the master is the FX3 bit-banging GPIO
# through register writes, so the fastest it can go is two orders of magnitude
# below the 2 MHz the slave is specified for. All three inputs go through
# two-flop synchronisers and glitch filters in spiRegisters, which is what
# makes them safe to be asynchronous; timing them against the system clock
# would be constraining a relationship that does not exist.
set_false_path -from [get_ports {GPIO1[17]}]
set_false_path -from [get_ports {GPIO1[15]}]
set_false_path -from [get_ports {GPIO1[11]}]
set_false_path -to [get_ports {GPIO1[13]}]

# nReset from the FX3. It asserts asynchronously by design - a reset that
# needed a clock edge would not work when there is no clock - and the top
# level synchronises its release before any logic sees it, so recovery and
# removal analysis on the pin itself has nothing to say.
set_false_path -from [get_ports {GPIO1[7]}]

# The status LEDs. Driven from the register bank for a human to look at.
set_false_path -to [get_ports {LED[*]}]

# FX3 control lines the board wires up and this design does not use. They are
# tied off or ignored; the pin assignments are fixed by the PCB.
set_false_path -from [get_ports {GPIO1[23]}]
set_false_path -from [get_ports {GPIO1[9]}]
set_false_path -to [get_ports {GPIO1[19]}]
set_false_path -to [get_ports {GPIO1[5]}]
set_false_path -to [get_ports {GPIO1[3]}]
