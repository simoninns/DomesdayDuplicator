/************************************************************************

    flashBridge.v

    Explicitly-unlocked pass-through to the EPCS configuration flash
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Registers 0x20 to 0x22 of the register map. The FX3 speaks the EPCS
    command set - read identification, erase a sector, program a page - and
    this shifts the bytes. It knows none of those commands and no address
    layout: it asserts chip select when it is told to, shifts a byte when it
    is given one, and hands back the byte that arrived in its place.

    That the gateware stays dumb is the design, not an omission. The FX3
    already has to hold the update state machine, the digests and the
    retry policy; a second, smaller copy of the EPCS command set in fabric
    that could never be updated in the field would be a liability rather
    than a convenience.

    Three properties this module is responsible for:

      - it is inert until deliberately unlocked. Anything that can send a
        register write can reach these addresses, and the flash holds the
        only copy of the gateware, so the unlock sequence is what stands
        between a stray write and an unbootable board;
      - the active serial pins are released whenever it is locked, so a
        gateware that is not talking to the flash is not connected to it;
      - a byte shift completes in a bounded number of clocks and reports
        busy while it runs, so nothing on the far end can hang the link.

    SPI mode 0 at one eighth of the system clock - 10 MHz from 80 MHz -
    which is inside the EPCS64's read timing with room to spare and is two
    orders of magnitude faster than the bit-banged register link that feeds
    it. The flash is never the bottleneck; the FX3's GPIO write rate is.

************************************************************************/

module flashBridge (
    input reset_n,
    input clock,

    // The 0x20 to 0x22 part of the register window. window_address is the
    // low two bits of the register address, so 0 is BRIDGE_UNLOCK, 1 is
    // BRIDGE_CONTROL and 2 is BRIDGE_DATA. Address 3 belongs to the
    // reconfiguration control and is ignored here.
    input       window_write,
    input [1:0] window_address,
    input [7:0] window_write_data,

    output [7:0] unlock_read,
    output [7:0] control_read,
    output [7:0] data_read,

    // To the EPCS64, through asmiBlock
    output flash_clock,
    output flash_chip_select_n,
    output flash_data_out,
    input  flash_data_in,
    output flash_drive
);

    // Register addresses within the window
    localparam [1:0] AddressUnlock = 2'd0;
    localparam [1:0] AddressControl = 2'd1;
    localparam [1:0] AddressData = 2'd2;

    // The unlock sequence: the identity byte twice, then an alternating
    // pair. Four bytes rather than one because a single magic value is one
    // corrupted transfer away from being written by accident, and because
    // the register address post-increments - so each byte of this sequence
    // is a transaction of its own, and no run of bytes across the map can
    // produce it as a side effect.
    localparam [7:0] UnlockByte0 = 8'h44;
    localparam [7:0] UnlockByte1 = 8'h44;
    localparam [7:0] UnlockByte2 = 8'h55;
    localparam [7:0] UnlockByte3 = 8'hAA;

    // Half a flash clock period, in system clock cycles. Eight cycles to
    // the bit, so 10 MHz from an 80 MHz system clock.
    localparam [1:0] HalfPeriod = 2'd3;

    reg  [1:0] unlock_progress;  // bytes of the sequence matched so far
    reg        unlocked;
    reg        chip_select_asserted;

    reg  [1:0] phase_count;  // system clocks into the current half period
    reg        flash_clock_level;
    reg  [3:0] bits_remaining;
    reg  [7:0] shift_out;
    reg  [7:0] shift_in;
    reg  [7:0] received_byte;
    reg        busy;

    wire       write_unlock = window_write && (window_address == AddressUnlock);
    wire       write_control = window_write && (window_address == AddressControl);
    wire       write_data = window_write && (window_address == AddressData);

    // A shift may only start when the bridge is unlocked, chip select is
    // asserted and nothing is in flight. Anything else is discarded rather
    // than queued: the register link has no way to say "not now", and a
    // queue would make the byte that came back belong to a different write
    // than the one the host thought it was reading.
    wire       start_shift = write_data && unlocked && chip_select_asserted && !busy;

    // Reads. BRIDGE_UNLOCK reports the lock state rather than the sequence
    // position, because the position is not a host's business and telling a
    // caller how far through the sequence it got would turn four bytes into
    // something guessable one byte at a time.
    assign unlock_read         = {7'd0, unlocked};
    assign control_read        = {6'd0, busy, chip_select_asserted};
    assign data_read           = received_byte;

    assign flash_clock         = flash_clock_level;
    assign flash_chip_select_n = ~chip_select_asserted;
    assign flash_data_out      = shift_out[7];
    assign flash_drive         = unlocked;

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            unlock_progress      <= 2'd0;
            unlocked             <= 1'b0;
            chip_select_asserted <= 1'b0;

            phase_count          <= 2'd0;
            flash_clock_level    <= 1'b0;
            bits_remaining       <= 4'd0;
            shift_out            <= 8'h00;
            shift_in             <= 8'h00;
            received_byte        <= 8'h00;
            busy                 <= 1'b0;
        end else begin
            // Unlock ---------------------------------------------------------
            //
            // Any byte that is not the next one expected returns the matcher
            // to the start and locks the bridge. That makes an explicit
            // relock a write of anything at all - zero, by convention - and
            // it means a host that loses track cannot end up half way
            // through the sequence without knowing it.
            if (write_unlock) begin
                // Every write here closes the bridge first and the last
                // byte of the sequence reopens it below, which is what
                // makes an explicit relock a write of anything at all -
                // zero, by convention. Both assignments are non-blocking
                // and the later one wins, so a completed sequence is not
                // undone by the line that starts it.
                unlocked             <= 1'b0;
                chip_select_asserted <= 1'b0;

                case (unlock_progress)
                    2'd0: begin
                        if (window_write_data == UnlockByte0) begin
                            unlock_progress <= 2'd1;
                        end else begin
                            unlock_progress <= 2'd0;
                        end
                    end

                    2'd1: begin
                        if (window_write_data == UnlockByte1) begin
                            unlock_progress <= 2'd2;
                        end else begin
                            unlock_progress <= 2'd0;
                        end
                    end

                    2'd2: begin
                        if (window_write_data == UnlockByte2) begin
                            unlock_progress <= 2'd3;
                        end else begin
                            unlock_progress <= 2'd0;
                        end
                    end

                    2'd3: begin
                        unlock_progress <= 2'd0;

                        if (window_write_data == UnlockByte3) begin
                            unlocked <= 1'b1;
                        end
                    end

                    default: begin
                        unlock_progress <= 2'd0;
                    end
                endcase
            end

            // Chip select ----------------------------------------------------
            //
            // Deasserting mid-transfer is a legitimate thing for a host to
            // do - it is how an EPCS command is abandoned - so it is not
            // gated on busy. What it does not do is stop the shifter, which
            // finishes its bit count into a deselected flash and reports
            // itself idle a few hundred nanoseconds later.
            if (write_control && unlocked) begin
                chip_select_asserted <= window_write_data[0];
            end

            // Byte shift -----------------------------------------------------
            if (start_shift) begin
                shift_out         <= window_write_data;
                shift_in          <= 8'h00;
                bits_remaining    <= 4'd8;
                phase_count       <= 2'd0;
                flash_clock_level <= 1'b0;
                busy              <= 1'b1;
            end else if (busy) begin
                if (phase_count == HalfPeriod) begin
                    phase_count       <= 2'd0;
                    flash_clock_level <= ~flash_clock_level;

                    if (!flash_clock_level) begin
                        // The rising edge. The flash samples what this
                        // module is driving and presents its own bit, which
                        // has had a full half period to settle.
                        shift_in <= {shift_in[6:0], flash_data_in};
                    end else begin
                        // The falling edge ends the bit: the next one goes
                        // out now, so it is stable well before the flash
                        // looks at it.
                        shift_out      <= {shift_out[6:0], 1'b0};
                        bits_remaining <= bits_remaining - 4'd1;

                        // The eighth falling edge ends the byte. Every bit
                        // was taken in on a rising edge, so shift_in is
                        // already complete here.
                        if (bits_remaining == 4'd1) begin
                            busy          <= 1'b0;
                            received_byte <= shift_in;
                        end
                    end
                end else begin
                    phase_count <= phase_count + 2'd1;
                end
            end
        end
    end

endmodule
