/************************************************************************

    tb_bufferMonitor.v

    Testbench for the capture buffer instrument (T3)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    An instrument that is wrong is worse than no instrument, because the
    reading it gives is the one a user will act on: a capture that reports
    comfortable headroom and was in fact one packet from dropping samples
    would be taken as evidence that a marginal machine is fine. So the
    properties here are the ones that decide whether a number can be believed:

      - a peak is the peak of its own interval, not of the one before it and
        not of the whole run
      - the lifetime peak and the sticky overflow bit are never cleared by a
        read, so a second reader on the link cannot hide an excursion from
        the first
      - a stall is one overflow event however long it lasts, and the number
        of samples it cost is a separate figure
      - a counter that cannot represent what happened says so rather than
        wrapping, because a wrapped counter reports a small number for a
        catastrophe
      - a latch loses nothing: an event on the latching edge is counted in
        the interval that is starting, not dropped and not counted twice

    The FIFO is modelled rather than instantiated. What is under test is the
    arithmetic over used_words, and driving the occupancy directly is what
    makes it possible to hold the buffer at a level a real capture reaches
    only during a fault.

************************************************************************/

`timescale 1ns / 1ps

module tb_bufferMonitor;

    // The geometry the application image builds, so what is tested is the
    // instrument as it ships rather than a scaled-down version of it
    localparam integer FIFO_DEPTH = 16384;
    localparam integer PACKET_WORDS = 8192;
    localparam integer NEAR_FULL_WORDS = 12288;

    localparam integer USED_BITS = 15;

    // Samples per near-full unit. The counter is prescaled because a quarter
    // of a second of continuous near-full is ten million samples.
    localparam integer NEAR_FULL_PRESCALE = 256;

    reg                     reset_n;
    reg                     clock;

    reg     [USED_BITS-1:0] used_words;
    reg                     write_enable;
    reg                     overflow;
    reg                     is_reading;
    reg                     latch;

    wire    [        127:0] telemetry;
    wire    [         47:0] geometry;

    integer                 errors;
    integer                 i;

    bufferMonitor #(
        .FifoDepth    (FIFO_DEPTH),
        .PacketWords  (PACKET_WORDS),
        .NearFullWords(NEAR_FULL_WORDS)
    ) dut (
        .reset_n     (reset_n),
        .clock       (clock),
        .used_words  (used_words),
        .write_enable(write_enable),
        .overflow    (overflow),
        .is_reading  (is_reading),
        .latch       (latch),
        .telemetry   (telemetry),
        .geometry    (geometry)
    );

    // The shadow bank, named. The map presents these as bytes from TELEM_STATUS
    // upwards, least significant byte first, and a field split across the wrong
    // bytes here would reach a host as a plausible wrong number.
    wire [ 7:0] t_status = telemetry[7:0];
    wire [ 7:0] t_latch_count = telemetry[15:8];
    wire [15:0] t_used_now = telemetry[31:16];
    wire [15:0] t_peak = telemetry[47:32];
    wire [15:0] t_peak_lifetime = telemetry[63:48];
    wire [15:0] t_overflows = telemetry[79:64];
    wire [15:0] t_dropped = telemetry[95:80];
    wire [15:0] t_packets = telemetry[111:96];
    wire [15:0] t_near_full = telemetry[127:112];

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

    // One system clock cycle with the buffer in a stated condition
    task cycle;
        input [15:0] occupancy;
        input write_now;
        input overflow_now;
        input read_now;
        input latch_now;
        begin
            used_words   = occupancy[USED_BITS-1:0];
            write_enable = write_now;
            overflow     = overflow_now;
            is_reading   = read_now;
            latch        = latch_now;
            @(posedge clock);
            #1;
        end
    endtask

    // Hold the buffer at one occupancy for a while, writing at the sampling
    // rate: one sample every second cycle, which is 40 MSPS against an 80 MHz
    // clock.
    task hold;
        input [15:0] occupancy;
        input integer samples;
        begin
            for (i = 0; i < samples; i = i + 1) begin
                cycle(occupancy, 1'b1, 1'b0, 1'b0, 1'b0);
                cycle(occupancy, 1'b0, 1'b0, 1'b0, 1'b0);
            end
        end
    endtask

    // Take a reading, exactly as a host does: one clock of latch, and the
    // shadow bank stands still afterwards until the next one.
    task take_reading;
        begin
            cycle(used_words, 1'b0, 1'b0, 1'b0, 1'b1);
            cycle(used_words, 1'b0, 1'b0, 1'b0, 1'b0);
        end
    endtask

    // A stall: `samples` samples offered to a full FIFO, none of which fit
    task stall;
        input integer samples;
        begin
            for (i = 0; i < samples; i = i + 1) begin
                cycle(FIFO_DEPTH[15:0], 1'b1, 1'b1, 1'b0, 1'b0);
                cycle(FIFO_DEPTH[15:0], 1'b0, 1'b0, 1'b0, 1'b0);
            end
        end
    endtask

    initial begin
        errors       = 0;
        used_words   = 15'd0;
        write_enable = 1'b0;
        overflow     = 1'b0;
        is_reading   = 1'b0;
        latch        = 1'b0;
        reset_n      = 1'b0;

        @(posedge clock);
        #1 reset_n = 1'b1;

        // --- Geometry -------------------------------------------------------
        //
        // The three constants a host reads instead of carrying a copy of this
        // design's dimensions. A host that hardcoded them would misreport
        // every reading from a device whose gateware had been resized.
        check({16'd0, geometry[15:0]}, FIFO_DEPTH, "geometry: FIFO depth");
        check({16'd0, geometry[31:16]}, PACKET_WORDS, "geometry: packet words");
        check({16'd0, geometry[47:32]}, NEAR_FULL_WORDS, "geometry: near-full threshold");

        // --- Before the first reading ---------------------------------------
        //
        // Zero everywhere but the format version. A host that reads before
        // anything has been sampled must see something it can recognise as
        // "nothing measured yet" rather than a plausible set of figures.
        check({24'd0, t_status}, 32'h01, "status before the first latch");
        check({24'd0, t_latch_count}, 32'd0, "latch count before the first latch");
        check({16'd0, t_used_now}, 32'd0, "occupancy before the first latch");
        check({16'd0, t_peak}, 32'd0, "peak before the first latch");

        // --- A quiet interval ------------------------------------------------
        //
        // The sawtooth of a healthy capture: filling towards the packet
        // threshold and drained back down again, twice.
        hold(16'd2000, 20);
        hold(16'd6000, 20);
        hold(16'd8100, 20);
        hold(16'd3000, 20);
        cycle(16'd1500, 1'b0, 1'b0, 1'b0, 1'b0);
        take_reading;

        check({16'd0, t_peak}, 32'd8100, "peak of a quiet interval");
        check({16'd0, t_used_now}, 32'd1500, "occupancy at the latch instant");
        check({16'd0, t_peak_lifetime}, 32'd8100, "lifetime peak after one interval");
        check({16'd0, t_overflows}, 32'd0, "no overflows in a quiet interval");
        check({16'd0, t_dropped}, 32'd0, "nothing dropped in a quiet interval");
        check({24'd0, t_latch_count}, 32'd1, "latch count after one reading");
        check({24'd0, t_status}, 32'h01, "status after a quiet interval");

        // --- The peak belongs to its own interval -----------------------------
        //
        // The figure a user acts on. A peak that carried over would report a
        // stall that has already been recovered from as a stall that is still
        // happening, and every interval after a bad one would read as bad.
        hold(16'd4000, 20);
        cycle(16'd900, 1'b0, 1'b0, 1'b0, 1'b0);
        take_reading;
        check({16'd0, t_peak}, 32'd4000, "the peak of the second interval");

        // ... and the lifetime peak does not clear, so the excursion above is
        // still readable after the interval that contained it has gone
        check({16'd0, t_peak_lifetime}, 32'd8100, "the lifetime peak survives a reading");

        // --- Near-full time ---------------------------------------------------
        //
        // Amplitude does not distinguish a spike from a squeeze. This is the
        // figure that does, and it is counted in samples rather than clocks.
        hold(NEAR_FULL_WORDS[15:0], NEAR_FULL_PRESCALE * 3);
        take_reading;
        check({16'd0, t_near_full}, 32'd3, "near-full units for three prescale periods");
        check({16'd0, t_peak}, NEAR_FULL_WORDS, "peak while held at the threshold");

        // One word below the threshold is not near-full. The comparison is
        // inclusive, and off by one here would misreport every capture.
        hold(NEAR_FULL_WORDS[15:0] - 16'd1, NEAR_FULL_PRESCALE * 2);
        take_reading;
        check({16'd0, t_near_full}, 32'd0, "one word below the threshold is not near-full");

        // --- A stall ----------------------------------------------------------
        //
        // One event however long it lasts, and a separate count of what it
        // cost. Counting each dropped sample as an event would report a single
        // stall as thousands of them.
        // A burst ends at the first sample that finds room, so the recovery is
        // written rather than merely waited for. Idling does not end a stall:
        // nothing has been offered, so nothing has yet succeeded.
        stall(40);
        hold(16'd8000, 2);
        take_reading;
        check({16'd0, t_overflows}, 32'd1, "one stall is one overflow event");
        check({16'd0, t_dropped}, 32'd40, "and the samples it cost");
        check({24'd0, t_status}, 32'h11, "the sticky overflow bit is set");

        // The sticky bit is what tells a host that a capture was damaged at
        // some point, whoever was reading at the time
        take_reading;
        check({16'd0, t_overflows}, 32'd0, "the event count clears with the interval");
        check({24'd0, t_status}, 32'h11, "the sticky overflow bit does not clear");

        // Two stalls with a recovery between them are two events. The run has
        // to end on a sample that found room, which is what a recovery is.
        stall(3);
        hold(16'd4000, 4);
        stall(5);
        hold(16'd4000, 2);
        take_reading;
        check({16'd0, t_overflows}, 32'd2, "two stalls are two events");
        check({16'd0, t_dropped}, 32'd8, "and eight dropped samples between them");

        // --- Packets read -------------------------------------------------------
        //
        // The drain rate, from the device's own point of view. The FX3 reads
        // exactly one packet at a time, so this counts words and divides
        // rather than estimating what the GPIF did.
        take_reading;
        for (i = 0; i < PACKET_WORDS; i = i + 1) begin
            cycle(16'd8192, 1'b0, 1'b0, 1'b1, 1'b0);
        end
        take_reading;
        check({16'd0, t_packets}, 32'd1, "one packet read");

        // A partial packet is not a packet. A count that rounded up would
        // report a drain that did not happen.
        for (i = 0; i < PACKET_WORDS - 1; i = i + 1) begin
            cycle(16'd8192, 1'b0, 1'b0, 1'b1, 1'b0);
        end
        take_reading;
        check({16'd0, t_packets}, 32'd0, "a partial packet is not counted");

        // ... and the position within a packet is a phase, not an interval
        // count, so the reading above did not move the boundary: one more word
        // completes the packet the reading interrupted.
        cycle(16'd8192, 1'b0, 1'b0, 1'b1, 1'b0);
        take_reading;
        check({16'd0, t_packets}, 32'd1, "a reading does not shift the packet boundary");

        // --- An event on the latching edge --------------------------------------
        //
        // The edge case that decides whether a poll can lose a measurement.
        // The sample offered on the latching edge belongs to the interval that
        // is starting: it must not vanish, and it must not be counted twice.
        take_reading;
        cycle(FIFO_DEPTH[15:0], 1'b1, 1'b1, 1'b0, 1'b1);
        cycle(FIFO_DEPTH[15:0], 1'b0, 1'b0, 1'b0, 1'b0);
        check({16'd0, t_dropped}, 32'd0, "the latching edge's drop is not in the old interval");

        take_reading;
        check({16'd0, t_dropped}, 32'd1, "the latching edge's drop is in the new one");
        hold(16'd4000, 2);

        // --- Saturation -----------------------------------------------------------
        //
        // A wrapped counter reports a small number for a catastrophe, which is
        // the one reading that must not be possible. So the counter stops and
        // the status byte says it stopped.
        take_reading;
        stall(70000);
        hold(16'd8000, 2);
        take_reading;
        check({16'd0, t_dropped}, 32'hFFFF, "a saturating counter stops at its maximum");
        check({24'd0, t_status}, 32'h31, "and the status byte says it saturated");

        // The saturation bit is an interval flag: the next reading is a fresh
        // measurement and must not inherit it.
        hold(16'd4000, 4);
        take_reading;
        check({24'd0, t_status}, 32'h11, "the saturation bit clears with the interval");

        // --- Reset --------------------------------------------------------------
        //
        // reset_n is the FX3's and drops when the host closes the device. The
        // next capture must not begin with the last one's history, and in
        // particular not with its sticky overflow bit.
        #1 reset_n = 1'b0;
        @(posedge clock);
        #1 reset_n = 1'b1;
        @(posedge clock);
        #1;

        check({24'd0, t_status}, 32'h01, "reset clears the sticky overflow bit");
        check({16'd0, t_peak_lifetime}, 32'd0, "reset clears the lifetime peak");
        check({24'd0, t_latch_count}, 32'd0, "reset clears the latch count");

        hold(16'd5000, 10);
        take_reading;
        check({16'd0, t_peak}, 32'd5000, "the instrument works after a reset");

        if (errors == 0) begin
            $display("tb_bufferMonitor: PASS");
        end else begin
            $display("tb_bufferMonitor: FAIL (%0d errors)", errors);
        end

        if (errors != 0) begin
            $fatal(1, "tb_bufferMonitor failed");
        end
        $finish;
    end

endmodule
