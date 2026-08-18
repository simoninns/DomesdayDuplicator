/************************************************************************

    tb_crc32.v

    Testbench for the boot block's checksum (T3)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The whole value of this module is that it computes the same number a
    host's library computes, so the test is the published check value for
    CRC-32 and not a number this project produced for itself: the nine
    characters "123456789" give 0xCBF43926, which is the value every
    catalogue of CRC parameters lists for this polynomial and which
    Python's zlib, PNG and gzip all agree on.

    That matters more than it looks. The boot block is written by a host
    and checked in fabric, so a CRC that were merely self-consistent would
    pass every test in this repository and reject every real boot block.

************************************************************************/

`timescale 1ns / 1ps

module tb_crc32;

    // The published check value: CRC-32 of the ASCII characters 1 to 9
    localparam [31:0] CHECK_VALUE = 32'hCBF43926;

    // CRC-32 of no bytes at all. The initial value inverted, which is what
    // a host library returns for an empty buffer.
    localparam [31:0] EMPTY_VALUE = 32'h00000000;

    reg            reset_n;
    reg            clock;
    reg            data_valid;
    reg     [ 7:0] data;
    reg            restart;

    wire    [31:0] crc;

    integer        errors;
    integer        index;

    reg     [ 7:0] check_string[0:8];

    crc32 dut (
        .reset_n   (reset_n),
        .clock     (clock),
        .data_valid(data_valid),
        .data      (data),
        .restart   (restart),
        .crc       (crc)
    );

    // 80 MHz, the system clock the factory image runs this at
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

    task feed;
        input [7:0] value;
        begin
            @(negedge clock);
            data       = value;
            data_valid = 1'b1;
            @(negedge clock);
            data_valid = 1'b0;
        end
    endtask

    initial begin
        errors          = 0;

        check_string[0] = "1";
        check_string[1] = "2";
        check_string[2] = "3";
        check_string[3] = "4";
        check_string[4] = "5";
        check_string[5] = "6";
        check_string[6] = "7";
        check_string[7] = "8";
        check_string[8] = "9";

        reset_n         = 1'b0;
        data_valid      = 1'b0;
        data            = 8'h00;
        restart         = 1'b0;

        repeat (4) @(posedge clock);
        #1 reset_n = 1'b1;
        repeat (4) @(posedge clock);

        // --- Nothing folded in yet ---
        check(crc, EMPTY_VALUE, "the CRC of no bytes");

        // --- The published check value ---
        for (index = 0; index < 9; index = index + 1) begin
            feed(check_string[index]);
        end
        check(crc, CHECK_VALUE, "CRC-32 of 123456789");

        // --- Restart, and the same bytes again ---
        //
        // The boot logic folds two runs of bytes through one instance - the
        // twenty header bytes, then the whole application image - so a
        // restart that did not fully clear the state would produce a header
        // check that passed and an image check that never could.
        @(negedge clock);
        restart = 1'b1;
        @(negedge clock);
        restart = 1'b0;
        check(crc, EMPTY_VALUE, "restart returns the CRC to its initial state");

        for (index = 0; index < 9; index = index + 1) begin
            feed(check_string[index]);
        end
        check(crc, CHECK_VALUE, "the same value again after a restart");

        // --- A restart that arrives with a byte ---
        //
        // Which is how the boot logic uses it when it starts a read: the
        // byte presented on the restart clock is the first byte of the new
        // run, not the last byte of the old one.
        @(negedge clock);
        restart    = 1'b1;
        data       = "1";
        data_valid = 1'b1;
        @(negedge clock);
        restart    = 1'b0;
        data_valid = 1'b0;

        for (index = 1; index < 9; index = index + 1) begin
            feed(check_string[index]);
        end
        check(crc, CHECK_VALUE, "a restart carrying the first byte");

        // --- One byte at a time is the only mode there is ---
        @(negedge clock);
        restart = 1'b1;
        @(negedge clock);
        restart = 1'b0;
        feed(8'h00);
        check(crc, 32'hD202EF8D, "CRC-32 of a single zero byte");

        if (errors == 0) begin
            $display("tb_crc32: PASS");
        end else begin
            $display("tb_crc32: FAIL (%0d errors)", errors);
        end

        if (errors != 0) begin
            $fatal(1, "tb_crc32 failed");
        end
        $finish;
    end

endmodule
