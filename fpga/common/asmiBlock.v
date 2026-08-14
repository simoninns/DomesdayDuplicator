/************************************************************************

    asmiBlock.v

    Access to the EPCS configuration flash pins
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The Cyclone IV's active serial pins - DCLK, nCSO, ASDO and DATA0 - are
    not ordinary I/O. They belong to the configuration circuitry, and the
    only way the fabric reaches them is the asmiblock primitive. Every path
    from this project to the EPCS64 goes through here: the boot block the
    factory image reads, and every byte an update writes.

    The wrapper exists to hold two things that would otherwise be repeated
    at each instantiation, and one of them is a polarity that is easy to get
    backwards: the primitive's oe input tri-states the pins when it is high,
    so "drive the flash" is oe low. It is inverted here, once, where the
    comment can sit next to it.

    The primitive itself has no free simulation model - the one Quartus
    ships is an empty shell that models the tri-stating and nothing else -
    so common/sim/cycloneive_asmiblock.v provides one that a testbench can
    hang a flash model off. That file is a test fixture and is never
    compiled into a bitstream.

************************************************************************/

module asmiBlock (
    // From the fabric to the flash
    input dclk,
    input chip_select_n,
    input serial_data_out,

    // 1 = drive the active serial pins, 0 = leave them alone. The bridge
    // holds this low until it has been unlocked, so a gateware that is not
    // deliberately talking to the flash is not connected to it at all.
    input output_enable,

    // From the flash back to the fabric
    output serial_data_in
);

    cycloneive_asmiblock asmi_block_0 (
        // Inputs
        .dclkin(dclk),
        .scein (chip_select_n),
        .sdoin (serial_data_out),

        // The primitive tri-states when oe is high, which is the opposite
        // way round from every other enable in this design
        .oe(~output_enable),

        // Output
        .data0out(serial_data_in)
    );

endmodule
