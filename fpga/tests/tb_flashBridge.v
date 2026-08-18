/************************************************************************

    tb_flashBridge.v

    Testbench for the EPCS pass-through (T3)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Two things are worth testing here and they are not the same thing.

    The first is the lock. The registers this module answers are reachable
    by anything that can send a register write, and the flash behind them
    holds the only copy of the gateware, so "inert until deliberately
    unlocked" is a safety property rather than a convenience - and a lock
    that can be opened by accident is not a lock. The tests below try to
    open it by accident.

    The second is that a byte written to BRIDGE_DATA comes back as the byte
    the flash sent in its place. That is checked against a model of the
    EPCS64 rather than against a loopback, so the mode-0 edges have to be
    the right way round: a bridge that sampled on the wrong edge would pass
    a loopback test perfectly and read nothing but zeros from a real part.

************************************************************************/

`timescale 1ns / 1ps

module tb_flashBridge;

    localparam [1:0] ADDRESS_UNLOCK = 2'd0;
    localparam [1:0] ADDRESS_CONTROL = 2'd1;
    localparam [1:0] ADDRESS_DATA = 2'd2;

    localparam [7:0] READ_COMMAND = 8'h03;

    // Where the model answers, and what the first two bytes there are
    localparam [23:0] FLASH_BASE = 24'h100000;
    localparam [7:0] FLASH_BYTE_0 = 8'h5A;
    localparam [7:0] FLASH_BYTE_1 = 8'hC3;

    reg           reset_n;
    reg           clock;

    reg           window_write;
    reg     [1:0] window_address;
    reg     [7:0] window_write_data;

    wire    [7:0] unlock_read;
    wire    [7:0] control_read;
    wire    [7:0] data_read;

    wire          flash_clock;
    wire          flash_chip_select_n;
    wire          flash_data_out;
    wire          flash_data_in;
    wire          flash_drive;

    integer       errors;

    flashBridge dut (
        .reset_n            (reset_n),
        .clock              (clock),
        .window_write       (window_write),
        .window_address     (window_address),
        .window_write_data  (window_write_data),
        .unlock_read        (unlock_read),
        .control_read       (control_read),
        .data_read          (data_read),
        .flash_clock        (flash_clock),
        .flash_chip_select_n(flash_chip_select_n),
        .flash_data_out     (flash_data_out),
        .flash_data_in      (flash_data_in),
        .flash_drive        (flash_drive)
    );

    // The bridge is tested against a model of the part it will meet. The
    // chip select the model sees is gated by flash_drive, exactly as
    // asmiBlock gates it, so a locked bridge is not merely ignored here -
    // it is disconnected.
    epcsFlashModel #(
        .BaseAddress(FLASH_BASE)
    ) flash (
        .dclk         (flash_drive ? flash_clock : 1'b0),
        .chip_select_n(flash_drive ? flash_chip_select_n : 1'b1),
        .data_in      (flash_drive ? flash_data_out : 1'b0),
        .data_out     (flash_data_in)
    );

    // 80 MHz system clock
    initial begin
        clock = 1'b0;
    end
    always begin
        #6.25 clock = ~clock;
    end

    task check;
        input [31:0] got;
        input [31:0] want;
        input [511:0] what;
        begin
            if (got !== want) begin
                $display("FAIL: %0s: got %h, expected %h (t=%0t)", what, got, want, $time);
                errors = errors + 1;
            end
        end
    endtask

    task write_register;
        input [1:0] address;
        input [7:0] value;
        begin
            @(negedge clock);
            window_write      = 1'b1;
            window_address    = address;
            window_write_data = value;
            @(negedge clock);
            window_write = 1'b0;
        end
    endtask

    // Write a byte to BRIDGE_DATA and wait for the shift to finish, which
    // is what the FX3 does through two register transactions and what the
    // boot logic does by watching the busy bit.
    task shift_byte;
        input [7:0] value;
        begin
            write_register(ADDRESS_DATA, value);

            // The bridge raises busy on the clock edge in the middle of the
            // write above, so by here it is already up. Both waits are
            // level-sensitive rather than edge-sensitive, so this is right
            // whether the rise has happened yet or not - an edge wait would
            // hang on the one it had already missed.
            wait (control_read[1] == 1'b1);
            wait (control_read[1] == 1'b0);
        end
    endtask

    task unlock;
        begin
            write_register(ADDRESS_UNLOCK, 8'h44);
            write_register(ADDRESS_UNLOCK, 8'h44);
            write_register(ADDRESS_UNLOCK, 8'h55);
            write_register(ADDRESS_UNLOCK, 8'hAA);
        end
    endtask

    initial begin
        errors            = 0;

        reset_n           = 1'b0;
        window_write      = 1'b0;
        window_address    = 2'd0;
        window_write_data = 8'h00;

        repeat (4) @(posedge clock);
        #1 reset_n = 1'b1;
        repeat (4) @(posedge clock);

        // After time zero, so that the model's own initialisation - which
        // fills its window with the 0xFF an erased flash reads as - cannot
        // land on top of these.
        flash.memory[0] = FLASH_BYTE_0;
        flash.memory[1] = FLASH_BYTE_1;

        // --- Inert until unlocked ---
        check(unlock_read, 8'h00, "the bridge is locked out of reset");
        check(flash_drive, 1'b0, "the active serial pins are released while locked");

        write_register(ADDRESS_CONTROL, 8'h01);
        check(control_read[0], 1'b0, "chip select cannot be asserted while locked");

        write_register(ADDRESS_DATA, 8'hAB);
        repeat (8) @(posedge clock);
        check(control_read[1], 1'b0, "a byte written while locked does not shift");

        // --- The sequence has to be the sequence ---
        //
        // Three quarters of it, then something else, leaves the bridge
        // locked and the matcher back at the start - so an unlock that is
        // interrupted cannot be completed by a later stray write.
        write_register(ADDRESS_UNLOCK, 8'h44);
        write_register(ADDRESS_UNLOCK, 8'h44);
        write_register(ADDRESS_UNLOCK, 8'h55);
        write_register(ADDRESS_UNLOCK, 8'h00);
        check(unlock_read, 8'h00, "a wrong byte leaves the bridge locked");

        write_register(ADDRESS_UNLOCK, 8'hAA);
        check(unlock_read, 8'h00, "the last byte alone does not unlock");

        // --- The whole sequence does ---
        unlock;
        check(unlock_read, 8'h01, "the sequence unlocks the bridge");
        check(flash_drive, 1'b1, "the pins are driven once unlocked");

        // --- A read through the bridge ---
        //
        // The EPCS read command, three address bytes, then two dummy bytes
        // that clock the flash's answer back out.
        write_register(ADDRESS_CONTROL, 8'h01);
        check(control_read[0], 1'b1, "chip select asserts once unlocked");

        shift_byte(READ_COMMAND);
        shift_byte(FLASH_BASE[23:16]);
        shift_byte(FLASH_BASE[15:8]);
        shift_byte(FLASH_BASE[7:0]);

        shift_byte(8'hFF);
        check(data_read, FLASH_BYTE_0, "the first byte read from the flash");

        shift_byte(8'hFF);
        check(data_read, FLASH_BYTE_1, "the second byte, from the flash's own address counter");

        write_register(ADDRESS_CONTROL, 8'h00);
        check(control_read[0], 1'b0, "chip select deasserts");

        // --- Relocking ---
        //
        // Any write to the unlock register closes the bridge, so a host
        // that has finished can say so in one byte rather than having to
        // know a second magic value.
        write_register(ADDRESS_UNLOCK, 8'h00);
        check(unlock_read, 8'h00, "a write to the unlock register relocks");
        check(flash_drive, 1'b0, "the pins are released again");

        // --- And reset does the same ---
        unlock;
        check(unlock_read, 8'h01, "unlocked again");
        #1 reset_n = 1'b0;
        repeat (4) @(posedge clock);
        #1 reset_n = 1'b1;
        repeat (4) @(posedge clock);
        check(unlock_read, 8'h00, "reset relocks the bridge");

        if (errors == 0) begin
            $display("tb_flashBridge: PASS");
        end else begin
            $display("tb_flashBridge: FAIL (%0d errors)", errors);
        end

        if (errors != 0) begin
            $fatal(1, "tb_flashBridge failed");
        end
        $finish;
    end

endmodule
