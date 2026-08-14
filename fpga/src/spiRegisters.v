/************************************************************************

    spiRegisters.v

    SPI register bank
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The FX3's window into the gateware. It replaces the five dedicated
    configuration lines that carried one bit each, of which only test mode
    was ever used, with a register bank the FX3 reads and writes over a
    private SPI link.

    SPI mode 0, most significant bit first, framed by chip select. A
    transaction is a command byte - direction in bit 7, register address in
    bits 6:0 - followed by data bytes, with the address incrementing after
    each one. The full contract, including why this is SPI rather than the
    I2C an earlier draft specified, is on the "FPGA register interface"
    page of the documentation site.

    Everything here is deliberately in one module. The shift register and
    the registers it reaches are too small to be worth an interface between
    them, and the simulation runner compiles a testbench against exactly one
    source file - so a split would cost the end-to-end test that is the only
    one worth having here.

************************************************************************/

module spiRegisters (
    input reset_n,
    input clock,

    // SPI slave. All three inputs are driven by the FX3 and are
    // asynchronous to clock.
    input  spi_clock,
    input  spi_mosi,
    input  spi_chip_select_n,
    output spi_miso,

    // Register outputs
    output       test_mode,
    output [7:0] leds
);

    // The build this gateware was compiled from, presented read-only at
    // addresses 0x02 to 0x0A. The top level supplies them from version.vh,
    // which the build generates; the defaults here describe a build that
    // cannot name its own commit, which is what a lint or simulation run is.
    //
    // The commit is eight ASCII characters, first character in the most
    // significant byte, padded with nulls. ASCII rather than a packed number
    // because the commit is not always eight characters long - CMake asks git
    // for eight, a Nix build passes seven - and eight bytes of text represent
    // both without inventing or losing a digit.
    parameter [63:0] CommitText = 64'h0000000000000000;
    parameter [7:0] BuildFlags = 8'h00;

    // Input synchronisers ---------------------------------------------------

    reg  [1:0] spi_clock_sync;
    reg  [1:0] spi_mosi_sync;
    reg  [1:0] spi_chip_select_n_sync;

    // Filtered levels, and the previous clock level for edge detection.
    //
    // A new level is accepted only once two consecutive synchronised samples
    // agree, which discards anything narrower than 33 ns. That is aimed at
    // ringing on the two board-to-board headers this link crosses, not at
    // metastability - the synchronisers above are what deal with that. The
    // specification sets a minimum clock phase of 250 ns, so the filter costs
    // at most an eighth of the shortest pulse it has to pass, and the master
    // runs two orders of magnitude slower than that in practice.
    reg        spi_clock_sample;
    reg        spi_chip_select_n_sample;
    reg        spi_clock_level;
    reg        spi_chip_select_n_level;
    reg        spi_clock_level_previous;

    wire       spi_clock_rising = spi_clock_level & ~spi_clock_level_previous;
    wire       spi_clock_falling = ~spi_clock_level & spi_clock_level_previous;

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            spi_clock_sync           <= 2'b00;
            spi_mosi_sync            <= 2'b00;
            spi_chip_select_n_sync   <= 2'b11;
            spi_clock_sample         <= 1'b0;
            spi_chip_select_n_sample <= 1'b1;
            spi_clock_level          <= 1'b0;
            spi_chip_select_n_level  <= 1'b1;
            spi_clock_level_previous <= 1'b0;
        end else begin
            spi_clock_sync           <= {spi_clock_sync[0], spi_clock};
            spi_mosi_sync            <= {spi_mosi_sync[0], spi_mosi};
            spi_chip_select_n_sync   <= {spi_chip_select_n_sync[0], spi_chip_select_n};

            spi_clock_sample         <= spi_clock_sync[1];
            spi_chip_select_n_sample <= spi_chip_select_n_sync[1];

            if (spi_clock_sync[1] == spi_clock_sample) begin
                spi_clock_level <= spi_clock_sync[1];
            end

            if (spi_chip_select_n_sync[1] == spi_chip_select_n_sample) begin
                spi_chip_select_n_level <= spi_chip_select_n_sync[1];
            end

            spi_clock_level_previous <= spi_clock_level;
        end
    end

    // Register bank ---------------------------------------------------------

    reg [7:0] test_mode_register;
    reg [7:0] led_register;

    // Unmapped addresses read as zero. That is what lets the map grow without
    // the host having to probe for what exists: it reads MAP_VERSION, which is
    // a positive statement of what this gateware implements.
    function [7:0] read_register;
        input [6:0] read_address;
        begin
            case (read_address)
                7'h00:   read_register = 8'h44;  // ID
                7'h01:   read_register = 8'h01;  // MAP_VERSION
                7'h02:   read_register = BuildFlags;
                7'h03:   read_register = CommitText[63:56];
                7'h04:   read_register = CommitText[55:48];
                7'h05:   read_register = CommitText[47:40];
                7'h06:   read_register = CommitText[39:32];
                7'h07:   read_register = CommitText[31:24];
                7'h08:   read_register = CommitText[23:16];
                7'h09:   read_register = CommitText[15:8];
                7'h0A:   read_register = CommitText[7:0];
                7'h10:   read_register = test_mode_register;
                7'h11:   read_register = led_register;
                default: read_register = 8'h00;
            endcase
        end
    endfunction

    // SPI transfer ----------------------------------------------------------

    reg  [6:0] shift_in;  // bits of the arriving byte, less the one still on the wire
    reg  [7:0] shift_out;  // the byte being clocked out, most significant bit first
    reg  [2:0] bit_count;  // bits of the current byte received so far
    reg        command_received;  // the command byte has arrived, so data bytes follow
    reg        read_transfer;  // the command asked to read
    reg  [6:0] address;  // register the next data byte reads or writes
    reg        miso_out;

    // The byte as it stands once the bit currently on spi_mosi is taken in
    wire [7:0] shift_in_next = {shift_in, spi_mosi_sync[1]};

    // A read has to fetch the next byte as the current one completes, so that
    // its first bit is on the wire before the master clocks it out
    wire [6:0] address_next = address + 7'd1;

    assign spi_miso  = miso_out;

    // Any non-zero value means on, so that a host writing 1 and a host writing
    // 0xFF agree about what they asked for
    assign test_mode = (test_mode_register != 8'h00);
    assign leds      = led_register;

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            shift_in           <= 7'd0;
            shift_out          <= 8'h00;
            bit_count          <= 3'd0;
            command_received   <= 1'b0;
            read_transfer      <= 1'b0;
            address            <= 7'h00;
            miso_out           <= 1'b0;

            test_mode_register <= 8'h00;

            // One LED lit, which says "configured and running, but the FX3 has
            // not written here yet". An unconfigured FPGA shows none, because
            // its pins are high-Z, and the firmware overwrites this within a
            // second of enumerating - so the board distinguishes three states
            // on hardware whose only other diagnostic is a UART header.
            led_register       <= 8'h01;
        end else if (spi_chip_select_n_level) begin
            // Chip select is deasserted, so no transfer is in progress.
            //
            // Clearing the transfer state here rather than when the next one
            // starts is what makes a transfer that is cut short - by an FX3
            // reset, or by a board powering up mid-byte - leave nothing behind.
            // A partly received data byte cannot reach a register, because a
            // register is only written on the eighth bit of a byte.
            shift_in         <= 7'd0;
            shift_out        <= 8'h00;
            bit_count        <= 3'd0;
            command_received <= 1'b0;
            miso_out         <= 1'b0;
        end else begin
            if (spi_clock_rising) begin
                shift_in  <= shift_in_next[6:0];
                bit_count <= bit_count + 3'd1;

                if (bit_count == 3'd7) begin
                    if (!command_received) begin
                        command_received <= 1'b1;
                        read_transfer <= shift_in_next[7];
                        address <= shift_in_next[6:0];

                        // Nothing meaningful goes out during the command byte,
                        // so a read's first returned byte is always zero and the
                        // register contents start with the second
                        shift_out <= shift_in_next[7] ? read_register(shift_in_next[6:0]) : 8'h00;
                    end else begin
                        if (!read_transfer) begin
                            case (address)
                                7'h10: test_mode_register <= shift_in_next;
                                7'h11: led_register <= shift_in_next;
                                default: begin
                                    // Read-only and unmapped addresses: the
                                    // write is discarded. SPI has no way to
                                    // refuse a byte and nothing here pretends
                                    // otherwise.
                                end
                            endcase
                        end

                        address   <= address_next;
                        shift_out <= read_transfer ? read_register(address_next) : 8'h00;
                    end
                end
            end

            if (spi_clock_falling) begin
                // Mode 0: the slave changes its output on the falling edge, so
                // the bit has a full half period to settle before the master
                // samples it on the rise.
                miso_out  <= shift_out[7];
                shift_out <= {shift_out[6:0], 1'b0};
            end
        end
    end

endmodule
