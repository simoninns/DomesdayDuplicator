/************************************************************************

    cycloneive_asmiblock.v

    Simulation model of the Cyclone IV active serial access primitive
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    A test fixture, never compiled into a bitstream. It stands in for the
    device primitive asmiBlock instantiates, so that the flash bridge and
    the factory image's boot logic can be linted and simulated with free
    tools.

    Quartus ships a model of this primitive, but it models the tri-stating
    and nothing else: the line that would connect it to a flash is commented
    out in the file it ships. This one puts an EPCS model on the other side
    of the pins, so a testbench that instantiates a design gets a device to
    talk to without having to reach into the design to place one.

    The port names, and the polarity of oe, are the primitive's, taken from
    the model in the Quartus installation: oe high tri-states, oe low
    drives. Nothing here may be tidied into a more sensible convention,
    because the point of the file is to behave as the part does.

    A testbench loads the flash contents through the hierarchy, which is
    where they belong - the contents are what each test is about:

        initial dut.asmi_block_0.flash_0.memory[0] = 8'h44;

************************************************************************/

module cycloneive_asmiblock (
    input dclkin,
    input scein,
    input oe,
    input sdoin,

    output data0out
);

    // The flash sees nothing at all while the block is tri-stated: chip
    // select reads high, which is deasserted, so no transaction can start.
    wire driving = (oe == 1'b0);
    wire dclk = driving ? dclkin : 1'b0;
    wire chip_select_n = driving ? scein : 1'b1;
    wire serial_data_out = driving ? sdoin : 1'b0;

    epcsFlashModel flash_0 (
        .dclk         (dclk),
        .chip_select_n(chip_select_n),
        .data_in      (serial_data_out),
        .data_out     (data0out)
    );

endmodule
