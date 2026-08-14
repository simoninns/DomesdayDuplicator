/************************************************************************

    tb_spiRegisters.v

    Testbench for the SPI register bank (T3)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    This is the only test the register interface gets that does not need
    hardware, and it is the reason the bank is one module rather than a
    slave and a register file with an interface between them: what is worth
    testing here is the whole path, from an edge on a pin to a register that
    changed.

    The bus is driven at 2 MHz, the fastest the specification allows, so the
    input synchronisers and the edge filter are exercised at their tightest.
    The master the FX3 will run is bit-banged and two orders of magnitude
    slower, so anything that passes here has a very large margin in the
    thing that will actually talk to it.

************************************************************************/

`timescale 1ns / 1ps

module tb_spiRegisters;

    // 2 MHz SPI: a 250 ns half period, which is the specification's minimum
    // clock phase. Chip select gets the 1 us of setup and hold the
    // specification asks for.
    localparam integer HALF_BIT = 250;
    localparam integer CS_TIME = 1000;

    // "7713495d" as eight ASCII bytes, and a dirty build of a known commit
    localparam [63:0] COMMIT_TEXT = 64'h3737313334393564;
    localparam [7:0] BUILD_FLAGS = 8'h03;

    reg           reset_n;
    reg           clock;

    reg           spi_clock;
    reg           spi_mosi;
    reg           spi_chip_select_n;
    wire          spi_miso;

    wire          test_mode;
    wire    [7:0] leds;

    integer       errors;
    integer       bit_index;
    integer       read_index;

    reg     [7:0] spi_received;
    reg     [7:0] read_data         [0:15];

    spiRegisters #(
        .CommitText(COMMIT_TEXT),
        .BuildFlags(BUILD_FLAGS)
    ) dut (
        .reset_n          (reset_n),
        .clock            (clock),
        .spi_clock        (spi_clock),
        .spi_mosi         (spi_mosi),
        .spi_chip_select_n(spi_chip_select_n),
        .spi_miso         (spi_miso),
        .test_mode        (test_mode),
        .leds             (leds)
    );

    // 60 MHz FX3 system clock — 16.667 ns period
    initial begin
        clock = 1'b0;
    end
    always begin
        #8.333 clock = ~clock;
    end

    task check;
        input [31:0] got;
        input [31:0] want;
        input [255:0] what;
        begin
            if (got !== want) begin
                $display("FAIL: %0s: got %h, expected %h (t=%0t)", what, got, want, $time);
                errors = errors + 1;
            end
        end
    endtask

    // SPI mode 0 master: the clock idles low, data is presented while it is
    // low and sampled on the rising edge.
    task spi_byte;
        input [7:0] send;
        begin
            spi_received = 8'h00;
            for (bit_index = 7; bit_index >= 0; bit_index = bit_index - 1) begin
                spi_mosi = send[bit_index];
                #HALF_BIT;
                spi_clock               = 1'b1;
                spi_received[bit_index] = spi_miso;
                #HALF_BIT;
                spi_clock = 1'b0;
            end
        end
    endtask

    // Clock only the top four bits, leaving a byte half delivered
    task spi_half_byte;
        input [7:0] send;
        begin
            for (bit_index = 7; bit_index >= 4; bit_index = bit_index - 1) begin
                spi_mosi = send[bit_index];
                #HALF_BIT;
                spi_clock = 1'b1;
                #HALF_BIT;
                spi_clock = 1'b0;
            end
        end
    endtask

    task spi_select;
        begin
            spi_chip_select_n = 1'b0;
            #CS_TIME;
        end
    endtask

    task spi_deselect;
        begin
            #CS_TIME;
            spi_chip_select_n = 1'b1;
            #CS_TIME;
        end
    endtask

    // Read count bytes from start_address into read_data
    task spi_read;
        input [6:0] start_address;
        input [7:0] count;
        begin
            spi_select;
            spi_byte({1'b1, start_address});
            for (read_index = 0; read_index < count; read_index = read_index + 1) begin
                spi_byte(8'h00);
                read_data[read_index] = spi_received;
            end
            spi_deselect;
        end
    endtask

    task spi_write_one;
        input [6:0] start_address;
        input [7:0] value;
        begin
            spi_select;
            spi_byte({1'b0, start_address});
            spi_byte(value);
            spi_deselect;
        end
    endtask

    task spi_write_two;
        input [6:0] start_address;
        input [7:0] first;
        input [7:0] second;
        begin
            spi_select;
            spi_byte({1'b0, start_address});
            spi_byte(first);
            spi_byte(second);
            spi_deselect;
        end
    endtask

    initial begin
        errors            = 0;

        reset_n           = 1'b0;
        spi_clock         = 1'b0;
        spi_mosi          = 1'b0;
        spi_chip_select_n = 1'b1;

        repeat (4) @(posedge clock);
        #1 reset_n = 1'b1;
        repeat (4) @(posedge clock);

        // --- Reset values ---
        //
        // One LED lit says "gateware running, FX3 has not written here yet",
        // which is the state that distinguishes a live but unconfigured board
        // from an unconfigured FPGA. Test mode off means a board that has just
        // come out of reset captures real samples.
        check(leds, 8'h01, "LED register reset value");
        check(test_mode, 1'b0, "test mode is off after reset");

        // --- Identity block ---
        //
        // Eleven bytes in one transaction: the signature, the map version, the
        // build flags and eight ASCII characters of commit.
        spi_read(7'h00, 8'd11);
        check(read_data[0], 8'h44, "ID register");
        check(read_data[1], 8'h01, "map version");
        check(read_data[2], BUILD_FLAGS, "build flags");
        check(read_data[3], 8'h37, "commit character 0");
        check(read_data[4], 8'h37, "commit character 1");
        check(read_data[5], 8'h31, "commit character 2");
        check(read_data[6], 8'h33, "commit character 3");
        check(read_data[7], 8'h34, "commit character 4");
        check(read_data[8], 8'h39, "commit character 5");
        check(read_data[9], 8'h35, "commit character 6");
        check(read_data[10], 8'h64, "commit character 7");

        // --- Test mode ---
        spi_write_one(7'h10, 8'h01);
        check(test_mode, 1'b1, "test mode on after writing 1");
        spi_read(7'h10, 8'd1);
        check(read_data[0], 8'h01, "test mode reads back");

        // Any non-zero value means on, so that a host writing 1 and a host
        // writing 0xFF agree about what they asked for
        spi_write_one(7'h10, 8'hFF);
        check(test_mode, 1'b1, "test mode on after writing 0xFF");

        spi_write_one(7'h10, 8'h00);
        check(test_mode, 1'b0, "test mode off after writing 0");

        // --- LEDs ---
        spi_write_one(7'h11, 8'hA5);
        check(leds, 8'hA5, "LED register drives the LEDs");
        spi_read(7'h11, 8'd1);
        check(read_data[0], 8'hA5, "LED register reads back");

        // --- Address auto-increment on a write ---
        //
        // One transaction setting both control registers, which is what makes
        // the identity block a single read rather than seven.
        spi_write_two(7'h10, 8'h01, 8'h3C);
        check(test_mode, 1'b1, "auto-increment wrote test mode");
        check(leds, 8'h3C, "auto-increment wrote the LED register");
        spi_write_one(7'h10, 8'h00);

        // --- Unmapped addresses read as zero ---
        //
        // This is what lets the map grow: a host that reads an address this
        // gateware does not implement gets zero rather than nonsense, and
        // learns what is implemented from the map version instead.
        spi_read(7'h20, 8'd1);
        check(read_data[0], 8'h00, "unmapped address reads zero");

        // --- Writes to read-only registers are discarded ---
        //
        // SPI cannot refuse a byte, so the write is accepted off the wire and
        // dropped. What must not happen is the identity block changing.
        spi_write_one(7'h00, 8'hFF);
        spi_read(7'h00, 8'd1);
        check(read_data[0], 8'h44, "ID register survives a write");

        // --- The address wraps rather than saturating ---
        spi_read(7'h7F, 8'd2);
        check(read_data[0], 8'h00, "unmapped 0x7F reads zero");
        check(read_data[1], 8'h44, "address wrapped to the ID register");

        // --- A byte cut short by chip select is discarded ---
        //
        // The whole recovery mechanism is that deasserting chip select returns
        // the slave to idle from any state. An FX3 that resets mid-transfer,
        // or a board powering up while the lines float, must not be able to
        // leave half a byte in a register.
        spi_write_one(7'h11, 8'h81);
        spi_select;
        spi_byte(8'h10);  // write to test mode
        spi_half_byte(8'hFF);  // four bits of a data byte, then nothing
        spi_deselect;
        check(test_mode, 1'b0, "a half-delivered byte did not reach test mode");

        // --- And the interface still works afterwards ---
        //
        // An abandoned transfer that left the slave mid-byte would show up
        // here as a command byte read four bits out of step.
        spi_read(7'h11, 8'd1);
        check(read_data[0], 8'h81, "the LED register is intact after an abandoned transfer");
        spi_write_one(7'h10, 8'h01);
        check(test_mode, 1'b1, "a write still works after an abandoned transfer");

        // --- Reset returns the registers to their defaults ---
        #1 reset_n = 1'b0;
        repeat (4) @(posedge clock);
        #1 reset_n = 1'b1;
        repeat (4) @(posedge clock);
        check(leds, 8'h01, "reset restores the LED register");
        check(test_mode, 1'b0, "reset clears test mode");

        if (errors == 0) begin
            $display("tb_spiRegisters: PASS");
        end else begin
            $display("tb_spiRegisters: FAIL (%0d errors)", errors);
        end

        if (errors != 0) begin
            $fatal(1, "tb_spiRegisters failed");
        end
        $finish;
    end

endmodule
