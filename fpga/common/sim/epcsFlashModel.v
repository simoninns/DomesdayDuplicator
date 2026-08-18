/************************************************************************

    epcsFlashModel.v

    Simulation model of the EPCS64 configuration flash
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    A test fixture, never compiled into a bitstream. It sits behind
    cycloneive_asmiblock's simulation model so that anything reading the
    flash - the boot logic, and a testbench driving the bridge by hand -
    has something to read.

    It implements the two commands this gateware issues and nothing else:
    read bytes (0x03, followed by a 24-bit address) and read the silicon
    identifier (0xAB). Erase and program are deliberately absent, because
    no gateware in this repository issues them: the FX3 does, over the same
    bridge, and its half of that conversation is tested on the host side
    where the arithmetic lives.

    Only a window of the address space is modelled - the boot block is what
    the boot logic reads, and one sector of storage is enough for that.
    Every address outside the window reads as 0xFF, which is what erased
    flash reads as, so a test that gets an address wrong sees the same thing
    the real device would show it.

************************************************************************/

module epcsFlashModel (
    input dclk,
    input chip_select_n,
    input data_in,

    output data_out
);

    // The modelled window. The default covers the boot block; a testbench
    // that cares about somewhere else overrides the address.
    parameter [23:0] BaseAddress = 24'h100000;

    // Fixed at one page, which is what the offset below is indexed with
    localparam integer WindowBytes = 256;

    // EPCS64's response to 0xAB
    parameter [7:0] SiliconId = 8'h16;

    localparam [7:0] CommandRead = 8'h03;
    localparam [7:0] CommandReadSiliconId = 8'hAB;

    reg     [ 7:0] memory       [0:WindowBytes-1];

    reg     [ 7:0] command;
    reg     [23:0] address;
    reg     [ 7:0] shift_out;
    reg     [ 7:0] shift_in;
    // Wide enough that a long read never wraps it: a byte boundary is
    // recognised by the low three bits, and the address phase by the count
    reg     [15:0] bit_count;
    reg            data_out_reg;

    integer        index;

    initial begin
        for (index = 0; index < WindowBytes; index = index + 1) begin
            memory[index] = 8'hFF;
        end

        command      = 8'h00;
        address      = 24'h000000;
        shift_out    = 8'hFF;
        shift_in     = 8'h00;
        bit_count    = 16'd0;
        data_out_reg = 1'b1;
    end

    assign data_out = data_out_reg;

    // The byte at an address, or erased flash if it is outside the window
    function [7:0] byte_at;
        input [23:0] byte_address;
        reg [23:0] offset;
        begin
            offset = byte_address - BaseAddress;

            if (byte_address >= BaseAddress && offset < 24'd256) begin
                byte_at = memory[offset[7:0]];
            end else begin
                byte_at = 8'hFF;
            end
        end
    endfunction

    always @(negedge chip_select_n) begin
        command   = 8'h00;
        bit_count = 16'd0;
        shift_in  = 8'h00;
        shift_out = 8'hFF;
    end

    // Mode 0: the device samples on the rising edge and changes its output
    // on the falling one, exactly as the bridge on the other side expects.
    always @(posedge dclk) begin
        if (!chip_select_n) begin
            shift_in  = {shift_in[6:0], data_in};
            bit_count = bit_count + 16'd1;

            if (bit_count[2:0] == 3'd0) begin
                if (bit_count == 16'd8) begin
                    command = shift_in;

                    if (command == CommandReadSiliconId) begin
                        // Three don't-care bytes, then the identifier
                        // repeated for as long as the master keeps clocking
                        shift_out = 8'hFF;
                    end
                end else if (command == CommandRead) begin
                    case (bit_count)
                        16'd16: address[23:16] = shift_in;
                        16'd24: address[15:8] = shift_in;
                        16'd32: begin
                            address[7:0] = shift_in;
                            shift_out    = byte_at(address);
                        end
                        default: begin
                            if (bit_count > 16'd32) begin
                                address   = address + 24'd1;
                                shift_out = byte_at(address);
                            end
                        end
                    endcase
                end else if (command == CommandReadSiliconId) begin
                    if (bit_count >= 16'd32) begin
                        shift_out = SiliconId;
                    end
                end
            end
        end
    end

    always @(negedge dclk) begin
        if (!chip_select_n) begin
            data_out_reg = shift_out[7];
            shift_out    = {shift_out[6:0], 1'b1};
        end
    end

endmodule
