/************************************************************************

    tb_buffer.v

    Testbench for the data buffer module (T3)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    This module had no testbench for seven years, and could not have had one:
    it was two Altera dcfifo instances, and dcfifo has no free simulation
    model, so the whole capture path was covered only by running the
    instrument. Replacing the IP with fifo.v is what makes this file possible,
    and it is most of the reason the replacement is worth doing.

    The properties here are the ones a capture depends on and that no waveform
    makes obvious:

      - data_available is never a lie. It is raised only when a whole packet
        can be read without the FX3 waiting, so the case that matters is the
        one just below the threshold, where a buffer that rounded up would
        hand the FX3 8191 words and a cycle of whatever came next.
      - it is held for exactly one packet, because the GPIF II state machine
        on the other side of the pin was designed against a flag that behaved
        that way.
      - samples come out in the order they went in, across packet boundaries
        and across an overflow.
      - an overflow drops the samples that do not fit and keeps what is
        already captured, rather than the reverse.

    The clock is 80 MHz and the writer runs one sample every second cycle,
    which is the 40 MSPS the instrument samples at. Nothing here is scaled
    down: the packet is 8192 words and the FIFO is 16384, as built.

************************************************************************/

`timescale 1ns / 1ps

module tb_buffer;

    // Must match the localparams in buffer.v. They are not parameters of the
    // module because they are not free: 8192 words is the FX3's DMA buffer and
    // one 16 KiB USB 3 bulk endpoint buffer.
    localparam integer PACKET_WORDS = 8192;
    localparam integer FIFO_DEPTH = 16384;

    // buffer.v holds the error flag for 2000 cycles. It raises the flag and
    // zeroes the counter on the overflow edge, then counts 0, 1, ... and
    // clears the flag on the edge at which the counter has reached 2000 — so
    // the flag is high for 2001 edges after the overflow.
    localparam integer ERROR_HOLD_EDGES = 2001;

    reg            reset_n;
    reg            clock;
    reg            write_enable;
    reg            is_reading;
    reg     [15:0] data_in;

    wire    [15:0] data_out;
    wire           data_available;
    wire           buffer_error;

    integer        errors;
    integer        i;
    integer        j;
    integer        held;

    // The next value the writer will present, and the value the reader expects
    // next. A FIFO means these are the same sequence, offset by whatever is
    // still queued.
    reg     [15:0] write_value;
    reg     [15:0] read_value;

    buffer dut (
        .reset_n       (reset_n),
        .clock         (clock),
        .write_enable  (write_enable),
        .data_in       (data_in),
        .is_reading    (is_reading),
        .data_out      (data_out),
        .data_available(data_available),
        .buffer_error  (buffer_error)
    );

    // 80 MHz — 12.5 ns period
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
                $display("FAIL: %0s: got %0d, expected %0d (t=%0t)", what, got, want, $time);
                errors = errors + 1;
            end
        end
    endtask

    // One system clock cycle. The writer presents write_value and advances it
    // only when the write is offered, so a caller that stops writing does not
    // leave a hole in the sequence.
    task cycle;
        input write_now;
        input read_now;
        begin
            write_enable = write_now;
            data_in      = write_value;
            is_reading   = read_now;
            @(posedge clock);
            #1;
            if (write_now) begin
                write_value = write_value + 16'd1;
            end
        end
    endtask

    // Offer `count` samples at the instrument's cadence: one word every second
    // cycle, which is 40 MSPS against an 80 MHz clock.
    task write_samples;
        input integer count;
        begin
            for (i = 0; i < count; i = i + 1) begin
                cycle(1'b1, 1'b0);
                cycle(1'b0, 1'b0);
            end
        end
    endtask

    // Idle for `count` cycles, neither reading nor writing
    task idle;
        input integer count;
        begin
            for (i = 0; i < count; i = i + 1) begin
                cycle(1'b0, 1'b0);
            end
        end
    endtask

    // Read one whole packet, the way the FX3 does: one word per cycle with no
    // gaps, while the sampling side keeps writing underneath.
    task read_packet;
        input writer_running;
        begin
            for (j = 0; j < PACKET_WORDS; j = j + 1) begin
                check({16'd0, data_out}, {16'd0, read_value}, "packet word order");
                check(data_available, 32'd1, "data_available is held for the whole packet");
                check(buffer_error, 32'd0, "buffer_error during an ordinary packet");

                // The writer produces one word every second cycle
                cycle(writer_running && ((j % 2) == 0), 1'b1);
                read_value = read_value + 16'd1;
            end
        end
    endtask

    initial begin
        errors       = 0;
        write_enable = 1'b0;
        is_reading   = 1'b0;
        data_in      = 16'd0;
        write_value  = 16'd0;
        read_value   = 16'd0;
        reset_n      = 1'b0;

        @(posedge clock);
        #1 reset_n = 1'b1;

        // --- After reset --------------------------------------------------
        check(data_available, 32'd0, "data_available after reset");
        check(buffer_error, 32'd0, "buffer_error after reset");

        // --- The threshold --------------------------------------------------
        // One word short of a packet the flag must stay down. This is the
        // check the whole module exists to pass: the old ping-pong buffer got
        // it right by construction, because a buffer was either full or it was
        // not, and a threshold comparison has to be got right on purpose.
        write_value = 16'h0000;
        read_value  = 16'h0000;
        write_samples(PACKET_WORDS - 1);
        idle(4);
        check(data_available, 32'd0, "data_available one word short of a packet");

        // The word that completes the packet. data_available is registered off
        // the occupancy, so it follows an edge later.
        write_samples(1);
        idle(4);
        check(data_available, 32'd1, "data_available once a whole packet is queued");
        check(buffer_error, 32'd0, "buffer_error after an ordinary fill");

        // --- One packet, drained --------------------------------------------
        // Nothing is written during this one, so the buffer ends empty and the
        // flag must fall on the last word rather than at some point after it.
        read_packet(1'b0);
        check(data_available, 32'd0, "data_available falls at the end of a packet");

        // A packet is exactly PACKET_WORDS long. If the flag were still up the
        // FX3 would be entitled to another packet that is not there.
        idle(8);
        check(data_available, 32'd0, "data_available stays down with an empty buffer");
        check(buffer_error, 32'd0, "buffer_error after a clean packet");

        // --- Steady state ---------------------------------------------------
        // Three packets with the writer running throughout, which is what a
        // capture is. The ordering checks in read_packet carry across the
        // boundaries, so a word lost or repeated at a packet edge fails here.
        write_value = 16'h4000;
        read_value  = 16'h4000;

        for (j = 0; j < 3; j = j + 1) begin
            // Fill to the threshold. The writer is running during the read
            // below as well, so after the first packet this tops up what the
            // previous read did not consume.
            while (data_available !== 1'b1) begin
                cycle(1'b1, 1'b0);
                cycle(1'b0, 1'b0);
            end

            read_packet(1'b1);
        end
        check(buffer_error, 32'd0, "buffer_error across three steady-state packets");

        // --- Overflow -------------------------------------------------------
        // The reader stops, as it does when the USB 3 host stalls. Everything
        // up to the depth of the FIFO is kept; what does not fit is dropped.
        reset_n = 1'b0;
        @(posedge clock);
        #1 reset_n = 1'b1;

        write_value = 16'h8000;
        read_value  = 16'h8000;

        // Fill to exactly full. The flag must not have gone up on the way.
        write_samples(FIFO_DEPTH);
        check(buffer_error, 32'd0, "buffer_error while filling to exactly full");

        // The next sample has nowhere to go
        write_samples(1);
        check(buffer_error, 32'd1, "buffer_error once a sample is dropped");

        // Those dropped samples must not displace what is already captured, so
        // the survivors are the first FIFO_DEPTH words and nothing else. Drop
        // several more to be sure it is not just the first that is refused.
        write_samples(7);

        // --- The error hold -------------------------------------------------
        // The FX3 samples this pin about once per packet, so a flag that
        // lasted one cycle would be invisible to it. Count how long it is held.
        //
        // The last drop is offered with a bare cycle rather than through
        // write_samples, which would follow it with the idle half of a sample
        // period and start the count an edge after the flag went up.
        cycle(1'b1, 1'b0);

        held = 0;
        while (buffer_error === 1'b1) begin
            cycle(1'b0, 1'b0);
            held = held + 1;
        end
        check(held, ERROR_HOLD_EDGES, "buffer_error hold in clock cycles");

        // A second overflow must be held just as long. The module this
        // replaced never cleared its hold counter, so every overflow after the
        // first raised the flag for a single cycle — invisible in practice,
        // and the reason a stalled capture could look clean.
        cycle(1'b1, 1'b0);
        check(buffer_error, 32'd1, "buffer_error on a second overflow");

        held = 0;
        while (buffer_error === 1'b1) begin
            cycle(1'b0, 1'b0);
            held = held + 1;
        end
        check(held, ERROR_HOLD_EDGES, "second buffer_error hold in clock cycles");

        // --- What survived an overflow --------------------------------------
        // Drain the lot. The FIFO is still full, so this is two whole packets,
        // and every word must be one of the ones written before the drop.
        check(data_available, 32'd1, "data_available with a full buffer");
        read_packet(1'b0);
        idle(4);
        check(data_available, 32'd1, "data_available for the second queued packet");
        read_packet(1'b0);
        idle(4);
        check(data_available, 32'd0, "data_available once the overflowed buffer is drained");

        // --- Reset while occupied -------------------------------------------
        // reset_n is the FX3's and drops when the host closes the device. The
        // next capture must not begin with the tail of the last one.
        write_value = 16'hC000;
        read_value  = 16'hC000;
        write_samples(PACKET_WORDS);
        idle(4);
        check(data_available, 32'd1, "data_available before reset");

        reset_n = 1'b0;
        #1;
        check(data_available, 32'd0, "reset clears data_available asynchronously");
        check(buffer_error, 32'd0, "reset clears buffer_error asynchronously");

        @(posedge clock);
        #1 reset_n = 1'b1;

        // The samples that were queued are gone, so a whole packet has to be
        // written again before the flag can come back.
        write_value = 16'hE000;
        read_value  = 16'hE000;
        write_samples(PACKET_WORDS - 1);
        idle(4);
        check(data_available, 32'd0, "the buffer is empty after a reset");

        write_samples(1);
        idle(4);
        check(data_available, 32'd1, "the buffer works after a reset");
        read_packet(1'b0);
        check(data_available, 32'd0, "the packet after a reset is a whole packet");

        if (errors == 0) begin
            $display("tb_buffer: PASS");
        end else begin
            $display("tb_buffer: FAIL (%0d errors)", errors);
        end

        if (errors != 0) begin
            $fatal(1, "tb_buffer failed");
        end
        $finish;
    end

endmodule
