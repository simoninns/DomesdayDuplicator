#************************************************************************
#
#    DomesdayDuplicatorFactory.SDC
#
#    Timing constraints for the factory image
#    Domesday Duplicator - LaserDisc RF sampler
#    SPDX-FileCopyrightText: 2026 Simon Inns
#    SPDX-License-Identifier: GPL-3.0-or-later
#
#************************************************************************
#
# Much shorter than the application image's, and the reason is the whole
# design: this image has no capture path. There is no ADC interface to time
# and nothing is presented on the GPIF databus, so the two interfaces that
# make the application image's constraints interesting are not here.
#
# What is left is the 80 MHz clock everything runs from, the PCLK copy of it
# that leaves for the FX3, and a set of pins that are deliberately not timed -
# each one named below with the reason, because "not constrained" and "nobody
# thought about it" look identical in a timing report.
#
#************************************************************************


# The altpll output pin carrying the 80 MHz system clock. Same instance name
# as the application image, because it is the same PLL instantiation.
set pll_clock_pin {pll_generator_0|altpll_component|auto_generated|pll1|clk[0]}


# ---------------------------------------------------------------------------
# Clocks
# ---------------------------------------------------------------------------

# The DE0-Nano's 50 MHz oscillator, the only clock that enters the FPGA
create_clock -name CLOCK_50 -period 20.000 [get_ports CLOCK_50]

# The 80 MHz system clock out of the PLL. Everything in this image runs from
# it: the register bank, the flash bridge and the boot logic.
derive_pll_clocks

# PCLK to the FX3 is the system clock driven straight onto a pin. This image
# never presents data alongside it, but it keeps the clock running - the FX3's
# GPIF II is a synchronous slave and a slave with no clock cannot be told that
# there is nothing to collect - so the clock is still defined at the pin.
create_generated_clock -name fx3_pclk \
    -source [get_pins $pll_clock_pin] \
    [get_ports {GPIO1[31]}]

# The remote update block's clock, a divide-by-four of the system clock made in
# the fabric by remoteUpdate. It is divided because the block is specified for a
# 25 ns minimum period and 10.1 ns minimum high and low times on this device, so
# the 80 MHz system clock violates it by a factor of two; Quartus checks this,
# and without the constraint below it would check the divided clock against the
# system clock's pulse widths and report a violation that is not there.
create_generated_clock -name update_clock \
    -source [get_pins $pll_clock_pin] \
    -divide_by 4 \
    [get_registers {*update_clock}]

derive_clock_uncertainty


# ---------------------------------------------------------------------------
# Paths that are not timed, and why
# ---------------------------------------------------------------------------

# The SPI register link. Mode 0, bit-banged by the FX3 through GPIO writes, so
# two orders of magnitude below the 2 MHz the slave is specified for. All three
# inputs go through the synchronisers and glitch filters in spiRegisters, which
# is what makes them safe to be asynchronous.
set_false_path -from [get_ports {GPIO1[17]}]
set_false_path -from [get_ports {GPIO1[15]}]
set_false_path -from [get_ports {GPIO1[11]}]
set_false_path -to [get_ports {GPIO1[13]}]

# nReset from the FX3. Asserts asynchronously by design and its release is
# synchronised in the top level before any register sees it.
set_false_path -from [get_ports {GPIO1[7]}]

# The status LEDs, for a human to look at.
set_false_path -to [get_ports {LED[*]}]

# The GPIF interface, held at a constant by this image. dataAvailable and
# bufferError are low for as long as it runs, and the sixteen databus pins are
# held at zero rather than left floating, so there is nothing here that a clock
# edge decides. They are named anyway: a pin the top level drives and the
# constraints never mention is indistinguishable from one nobody considered.
set_false_path -to [get_ports {GPIO1[27]}]
set_false_path -to [get_ports {GPIO1[21]}]
set_false_path -to [get_ports {GPIO1[19]}]
set_false_path -to [get_ports {GPIO1[5]}]
set_false_path -to [get_ports {GPIO1[3]}]

set_false_path -to [get_ports { \
    GPIO1[2]  GPIO1[4]  GPIO1[6]  GPIO1[8]  GPIO1[10] GPIO1[12] GPIO1[14] \
    GPIO1[16] GPIO1[18] GPIO1[20] GPIO1[22] GPIO1[24] GPIO1[26] GPIO1[28] \
    GPIO1[30] GPIO1[32] }]

# The active serial pins the flash bridge reaches through asmiBlock are not
# constrained here and cannot be: they are not user I/O, they belong to the
# configuration circuitry, and they do not appear in the pin assignments at
# all. The bridge clocks them at one eighth of the system clock, which is the
# constraint that matters and it is in the logic rather than in this file.
