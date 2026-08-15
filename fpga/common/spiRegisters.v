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

    The shift register and the small registers it reaches are deliberately
    in one module: they are too small to be worth an interface between them,
    and the simulation runner compiles a testbench against exactly one
    source file - so a split would cost the end-to-end test that is the only
    one worth having here.

    What is *not* in here is the flash bridge and the reconfiguration
    control at 0x20 to 0x23. Those are a second SPI master and a device
    primitive respectively, they are the only registers whose writes have an
    effect outside this module, and one of them must not auto-increment.
    They reach the map through the four-byte window below, so this module
    stays what it says it is - a shift register and a register bank - and
    both images get the same one.

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
    output [7:0] leds,

    // The 0x20 to 0x23 window. A write pulses window_write for one clock
    // with the byte and the low two bits of the address; reads are answered
    // combinationally from window_read_data, least significant byte 0x20.
    output        window_write,
    output [ 1:0] window_address,
    output [ 7:0] window_write_data,
    input  [31:0] window_read_data,

    // BENCH DIAGNOSTIC. The remote update block's own account of itself,
    // presented read-only at 0x30 to 0x37, least significant byte first.
    // Reads of unmapped addresses return zero, so gateware without this
    // reads as zero here and a host can tell the two apart by the
    // signature in the top two bytes.
    input [63:0] diagnostics,

    // One clock high for each data byte of a framed transaction that
    // completes. This is what the application image tickles the
    // reconfiguration watchdog with: it says the fabric decoded something,
    // which is the one thing neither the configuration CRC nor the boot
    // block's CRC can establish.
    output transaction_decoded
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

    // Which of the two images this bank is compiled into, reported at 0x0B.
    // The default is the application image, because a build that has not
    // said is the capture gateware; the factory image overrides it and its
    // top level is the only place that does.
    parameter [7:0] ImageRole = 8'h01;

    // The register map this bank implements, reported at 0x01. Version 2
    // adds IMAGE_ROLE and the 0x20 to 0x23 window; everything version 1
    // defined is unchanged, and the identity block is frozen across all
    // versions.
    localparam [7:0] MapVersion = 8'h02;

    // BRIDGE_DATA is a port rather than a location: each write shifts a byte
    // out to the EPCS and latches the byte that came back. The address
    // post-increment, which is what makes the identity block one
    // transaction, is exactly wrong for it - a run of writes to one address
    // is how a multi-byte flash transaction is expressed.
    localparam [6:0] BridgeDataAddress = 7'h22;

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
                7'h01:   read_register = MapVersion;
                7'h02:   read_register = BuildFlags;
                7'h03:   read_register = CommitText[63:56];
                7'h04:   read_register = CommitText[55:48];
                7'h05:   read_register = CommitText[47:40];
                7'h06:   read_register = CommitText[39:32];
                7'h07:   read_register = CommitText[31:24];
                7'h08:   read_register = CommitText[23:16];
                7'h09:   read_register = CommitText[15:8];
                7'h0A:   read_register = CommitText[7:0];
                7'h0B:   read_register = ImageRole;
                7'h10:   read_register = test_mode_register;
                7'h11:   read_register = led_register;
                7'h20:   read_register = window_read_data[7:0];
                7'h21:   read_register = window_read_data[15:8];
                7'h22:   read_register = window_read_data[23:16];
                7'h23:   read_register = window_read_data[31:24];
                7'h30:   read_register = diagnostics[7:0];
                7'h31:   read_register = diagnostics[15:8];
                7'h32:   read_register = diagnostics[23:16];
                7'h33:   read_register = diagnostics[31:24];
                7'h34:   read_register = diagnostics[39:32];
                7'h35:   read_register = diagnostics[47:40];
                7'h36:   read_register = diagnostics[55:48];
                7'h37:   read_register = diagnostics[63:56];
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

    // The 0x20 to 0x23 window, and the data-byte pulse. Registered rather
    // than decoded combinationally from the shift register, so that what
    // leaves this module is one clock wide however long the SPI clock phase
    // was that produced it.
    reg        window_write_pulse;
    reg  [1:0] window_write_address;
    reg  [7:0] window_write_byte;
    reg        data_byte_complete;

    // The byte as it stands once the bit currently on spi_mosi is taken in
    wire [7:0] shift_in_next = {shift_in, spi_mosi_sync[1]};

    // A read has to fetch the next byte as the current one completes, so that
    // its first bit is on the wire before the master clocks it out.
    // BRIDGE_DATA is the one address that does not move on, for the reason
    // given where BridgeDataAddress is declared.
    wire [6:0] address_next = (address == BridgeDataAddress) ? address : address + 7'd1;

    // The window covers 0x20 to 0x23, which is 0x20 plus two bits
    wire       address_in_window = (address[6:2] == 5'b01000);

    assign spi_miso            = miso_out;

    // Any non-zero value means on, so that a host writing 1 and a host writing
    // 0xFF agree about what they asked for
    assign test_mode           = (test_mode_register != 8'h00);
    assign leds                = led_register;

    assign window_write        = window_write_pulse;
    assign window_address      = window_write_address;
    assign window_write_data   = window_write_byte;
    assign transaction_decoded = data_byte_complete;

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            shift_in             <= 7'd0;
            shift_out            <= 8'h00;
            bit_count            <= 3'd0;
            command_received     <= 1'b0;
            read_transfer        <= 1'b0;
            address              <= 7'h00;
            miso_out             <= 1'b0;

            window_write_pulse   <= 1'b0;
            window_write_address <= 2'd0;
            window_write_byte    <= 8'h00;
            data_byte_complete   <= 1'b0;

            test_mode_register   <= 8'h00;

            // One LED lit, which says "configured and running, but the FX3 has
            // not written here yet". An unconfigured FPGA shows none, because
            // its pins are high-Z, and the firmware overwrites this within a
            // second of enumerating - so the board distinguishes three states
            // on hardware whose only other diagnostic is a UART header.
            led_register         <= 8'h01;
        end else if (spi_chip_select_n_level) begin
            // Chip select is deasserted, so no transfer is in progress.
            //
            // Clearing the transfer state here rather than when the next one
            // starts is what makes a transfer that is cut short - by an FX3
            // reset, or by a board powering up mid-byte - leave nothing behind.
            // A partly received data byte cannot reach a register, because a
            // register is only written on the eighth bit of a byte.
            shift_in           <= 7'd0;
            shift_out          <= 8'h00;
            bit_count          <= 3'd0;
            command_received   <= 1'b0;
            miso_out           <= 1'b0;

            window_write_pulse <= 1'b0;
            data_byte_complete <= 1'b0;
        end else begin
            // Both of these are one clock wide, so they are cleared on every
            // clock and raised only by the byte that earned them
            window_write_pulse <= 1'b0;
            data_byte_complete <= 1'b0;

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
                        // A whole data byte of a framed transaction has
                        // arrived, which is the fabric proving it decoded
                        // something. Reads count as much as writes: the
                        // firmware's identity read during start-up is what
                        // tickles the watchdog on a device with no host.
                        data_byte_complete <= 1'b1;

                        if (!read_transfer) begin
                            case (address)
                                7'h10: test_mode_register <= shift_in_next;
                                7'h11: led_register <= shift_in_next;
                                default: begin
                                    if (address_in_window) begin
                                        window_write_pulse   <= 1'b1;
                                        window_write_address <= address[1:0];
                                        window_write_byte    <= shift_in_next;
                                    end

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
