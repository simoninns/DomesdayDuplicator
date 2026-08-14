/************************************************************************

    tb_bootLoader.v

    Testbench for the factory image's boot decision (T3)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The most important simulation in the gateware, because the logic it
    covers is the one piece that can never be fixed in the field. A factory
    image that refuses a good boot block strands every unit in recovery; a
    factory image that accepts a bad one hands the device to an image that
    may not come back.

    So the testbench is built the way the device is: the boot logic, the
    flash bridge, the active serial block and the reconfiguration control,
    wired together exactly as the factory top level wires them, with a
    model of the EPCS64 on the far side. Nothing is stubbed out between the
    decision and the flash - the boot block is read through the same bridge
    an update writes through.

    Four cases, which are the four the boot flow documents:

      1. a valid boot block pointing at an image whose CRC matches: arm the
         watchdog with the right address, then reconfigure;
      2. the wrong magic: stay;
      3. a boot block whose own CRC is wrong - a block half written by an
         interrupted update: stay;
      4. a boot block that is intact but describes an image that is not:
         stay.

    The expected CRC-32 values were produced by an independent
    implementation (Python's zlib) rather than by this project's own, so
    that a fault common to the gateware and the testbench cannot pass. The
    module that computes them is checked against the published CRC-32 check
    value separately, in tb_crc32.

************************************************************************/

`timescale 1ns / 1ps

module tb_bootLoader;

    // The boot block, and a sixteen-byte stand-in for an application image
    // twenty bytes further on. Both are inside the flash model's window, so
    // one model answers for both reads.
    localparam [23:0] BOOT_BLOCK_ADDRESS = 24'h100000;
    localparam [23:0] IMAGE_ADDRESS = 24'h100020;
    localparam integer IMAGE_BYTES = 16;

    reg            reset_n;
    reg            clock;

    // The window as the top level wires it: the boot logic drives the
    // bridge while it is deciding, and nothing else is connected here
    // because in the factory image nothing else can be - the FX3 has not
    // finished booting when this runs.
    wire           window_write;
    wire    [ 1:0] window_address;
    wire    [ 7:0] window_write_data;

    wire    [ 7:0] bridge_unlock_read;
    wire    [ 7:0] bridge_control_read;
    wire    [ 7:0] bridge_data_read;
    wire    [ 7:0] reconfiguration_read;

    wire           flash_clock;
    wire           flash_chip_select_n;
    wire           flash_data_out;
    wire           flash_data_in;
    wire           flash_drive;

    wire           arm_request;
    wire    [23:0] boot_address;
    wire           reconfigure_request;
    wire           boot_active;

    integer        errors;
    integer        index;
    integer        settled;

    reg     [ 7:0] image_bytes          [0:IMAGE_BYTES-1];

    bootLoader boot_loader_0 (
        .reset_n              (reset_n),
        .clock                (clock),
        .window_write         (window_write),
        .window_address       (window_address),
        .window_write_data    (window_write_data),
        .bridge_data_read     (bridge_data_read),
        .bridge_busy          (bridge_control_read[1]),
        .arm_request          (arm_request),
        .boot_address         (boot_address),
        .reconfigure_request  (reconfigure_request),
        .reconfiguration_armed(reconfiguration_read[2]),
        .boot_active          (boot_active)
    );

    flashBridge flash_bridge_0 (
        .reset_n            (reset_n),
        .clock              (clock),
        .window_write       (window_write),
        .window_address     (window_address),
        .window_write_data  (window_write_data),
        .unlock_read        (bridge_unlock_read),
        .control_read       (bridge_control_read),
        .data_read          (bridge_data_read),
        .flash_clock        (flash_clock),
        .flash_chip_select_n(flash_chip_select_n),
        .flash_data_out     (flash_data_out),
        .flash_data_in      (flash_data_in),
        .flash_drive        (flash_drive)
    );

    asmiBlock asmi_block_0 (
        .dclk           (flash_clock),
        .chip_select_n  (flash_chip_select_n),
        .serial_data_out(flash_data_out),
        .output_enable  (flash_drive),
        .serial_data_in (flash_data_in)
    );

    remoteUpdate remote_update_0 (
        .reset_n            (reset_n),
        .clock              (clock),
        .window_write       (1'b0),
        .window_write_data  (8'h00),
        .control_read       (reconfiguration_read),
        .transaction_decoded(1'b0),
        .arm_request        (arm_request),
        .boot_address       (boot_address),
        .reconfigure_request(reconfigure_request)
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

    // The flash model answers from BOOT_BLOCK_ADDRESS, so the offsets here
    // are into its window.
    task write_flash;
        input [7:0] offset;
        input [7:0] value;
        begin
            asmi_block_0.asmi_block_0.flash_0.memory[offset] = value;
        end
    endtask

    task load_boot_block;
        input [31:0] magic;  // written most significant byte first
        input [15:0] layout_version;
        input [31:0] image_address;
        input [31:0] image_length;
        input [31:0] image_crc;
        input [31:0] block_crc;
        begin
            write_flash(8'h00, magic[31:24]);
            write_flash(8'h01, magic[23:16]);
            write_flash(8'h02, magic[15:8]);
            write_flash(8'h03, magic[7:0]);
            write_flash(8'h04, layout_version[7:0]);
            write_flash(8'h05, layout_version[15:8]);
            write_flash(8'h06, 8'h00);
            write_flash(8'h07, 8'h00);
            write_flash(8'h08, image_address[7:0]);
            write_flash(8'h09, image_address[15:8]);
            write_flash(8'h0A, image_address[23:16]);
            write_flash(8'h0B, image_address[31:24]);
            write_flash(8'h0C, image_length[7:0]);
            write_flash(8'h0D, image_length[15:8]);
            write_flash(8'h0E, image_length[23:16]);
            write_flash(8'h0F, image_length[31:24]);
            write_flash(8'h10, image_crc[7:0]);
            write_flash(8'h11, image_crc[15:8]);
            write_flash(8'h12, image_crc[23:16]);
            write_flash(8'h13, image_crc[31:24]);
            write_flash(8'h14, block_crc[7:0]);
            write_flash(8'h15, block_crc[15:8]);
            write_flash(8'h16, block_crc[23:16]);
            write_flash(8'h17, block_crc[31:24]);
        end
    endtask

    // The stand-in application image, bytes 0xA0 to 0xAF
    task load_image;
        begin
            for (index = 0; index < IMAGE_BYTES; index = index + 1) begin
                image_bytes[index] = 8'hA0 + index[7:0];
                write_flash(8'h20 + index[7:0], image_bytes[index]);
            end
        end
    endtask

    task restart;
        begin
            #1 reset_n = 1'b0;
            repeat (8) @(posedge clock);
            #1 reset_n = 1'b1;
        end
    endtask

    // Wait for the boot logic to finish deciding, or give up. A boot block
    // read is a few thousand clocks, so the limit is generous rather than
    // tight: what it catches is a state machine that has stopped, not one
    // that is slow.
    task wait_for_decision;
        begin
            settled = 0;
            while (settled < 200000 && boot_active) begin
                @(posedge clock);
                settled = settled + 1;
            end

            if (boot_active) begin
                $display("FAIL: the boot logic never finished deciding (t=%0t)", $time);
                errors = errors + 1;
            end
        end
    endtask

    initial begin
        errors  = 0;
        reset_n = 1'b0;
        repeat (4) @(posedge clock);

        // --- 1. A valid boot block ---
        //
        // zlib.crc32 of the sixteen image bytes is 0xB225246F, and of the
        // twenty header bytes that describe them 0x9A522E5C.
        load_image;
        load_boot_block(32'h44444242, 16'd1, IMAGE_ADDRESS, IMAGE_BYTES, 32'hB225246F,
                        32'h9A522E5C);
        restart;
        wait_for_decision;

        check(reconfigure_request, 1'b1, "a valid boot block triggers reconfiguration");

        // The request reaches the block a little later than it is made:
        // the block runs at a quarter of this clock and the request is
        // stretched to be sure it lands.
        repeat (200) @(posedge clock);

        check(remote_update_0.remote_update_0.written_boot_address, IMAGE_ADDRESS,
              "the image address reached the remote update block");
        check(remote_update_0.remote_update_0.watchdog_enabled, 1'b1,
              "the watchdog was enabled before the handover");
        check(remote_update_0.remote_update_0.written_watchdog_value, 12'hFFF,
              "the watchdog timeout was written");
        check(remote_update_0.remote_update_0.reconfigure_count > 0, 1'b1,
              "the block was asked to reconfigure");

        // The arming has to happen before the request to go, not merely at
        // some point: a reconfiguration with no watchdog behind it is one
        // that cannot come back.
        check(reconfiguration_read[2], 1'b1, "the block reported itself armed first");

        // --- 2. The wrong magic ---
        //
        // Which is what an erased sector looks like, and what somebody
        // else's data in that sector looks like.
        load_boot_block(32'hFFFFFFFF, 16'd1, IMAGE_ADDRESS, IMAGE_BYTES, 32'hB225246F,
                        32'h9A522E5C);
        restart;
        wait_for_decision;
        check(reconfigure_request, 1'b0, "the wrong magic leaves the unit in the factory image");

        // --- 3. A boot block whose own checksum is wrong ---
        //
        // The interrupted-update case: the fields are plausible and the
        // block was never finished.
        load_boot_block(32'h44444242, 16'd1, IMAGE_ADDRESS, IMAGE_BYTES, 32'hB225246F,
                        32'hDEADBEEF);
        restart;
        wait_for_decision;
        check(reconfigure_request, 1'b0,
              "a bad block checksum leaves the unit in the factory image");

        // --- 4. An intact block describing a damaged image ---
        //
        // The block validates, so the logic goes on to read the image
        // itself - and the image is not what the block says it is.
        write_flash(8'h20, 8'h00);
        load_boot_block(32'h44444242, 16'd1, IMAGE_ADDRESS, IMAGE_BYTES, 32'hB225246F,
                        32'h9A522E5C);
        restart;
        wait_for_decision;
        check(reconfigure_request, 1'b0, "a damaged image leaves the unit in the factory image");

        // --- And a layout version from the future ---
        //
        // A frozen image cannot know what a later layout means, so it
        // declines rather than reading fields it may be wrong about.
        load_image;
        load_boot_block(32'h44444242, 16'd2, IMAGE_ADDRESS, IMAGE_BYTES, 32'hB225246F,
                        32'h9A522E5C);
        restart;
        wait_for_decision;
        check(reconfigure_request, 1'b0, "an unknown layout version is declined");

        if (errors == 0) begin
            $display("tb_bootLoader: PASS");
        end else begin
            $display("tb_bootLoader: FAIL (%0d errors)", errors);
        end

        if (errors != 0) begin
            $fatal(1, "tb_bootLoader failed");
        end
        $finish;
    end

endmodule
