/************************************************************************

    tb_fifo.v

    Testbench for the single-clock FIFO buffer (T3)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The FIFO carries every sample the instrument produces, and none of its
    failure modes announce themselves: a word duplicated, dropped or handed
    over one cycle late is a capture that completes normally and is wrong from
    that point on. So the properties checked here are ordering and occupancy,
    exhaustively, rather than that the module runs.

    Three depths, because the depth is what the interesting logic is written
    in terms of:

      6      not a power of two, which is the case the pointers wrap on a
             compare to support - a design that wrapped by letting the pointer
             overflow would pass every other test here and corrupt this one
      8      a power of two, where the compare and an overflow agree
      16384  the depth the instrument is built with

    Each depth is an instance of fifo_case, which runs the whole sequence
    against its own clock and reports its own error count. tb_fifo waits for
    all three and adds them up, so one depth failing does not hide the others.

************************************************************************/

`timescale 1ns / 1ps

// One full run of the checks against a FIFO of a given depth.
module fifo_case #(
    parameter integer Depth = 8
) (
    output reg        done,
    output reg [31:0] errors
);

    localparam integer DATA_WIDTH = 16;
    localparam integer COUNT_BITS = $clog2(Depth + 1);

    // Sized forms of the depth, made the same way fifo.v makes them
    localparam [31:0] DEPTH_VALUE = Depth;
    localparam [COUNT_BITS-1:0] DEPTH_WORDS = DEPTH_VALUE[COUNT_BITS-1:0];
    localparam [COUNT_BITS-1:0] COUNT_ZERO = {COUNT_BITS{1'b0}};

    reg                      reset_n;
    reg                      clock;
    reg                      write_request;
    reg                      read_request;
    reg     [DATA_WIDTH-1:0] data_in;

    wire    [DATA_WIDTH-1:0] data_out;
    wire                     empty;
    wire                     full;
    wire    [COUNT_BITS-1:0] used_words;

    integer                  i;
    integer                  half;
    reg     [DATA_WIDTH-1:0] value;

    fifo #(
        .DataWidth(DATA_WIDTH),
        .Depth    (Depth)
    ) dut (
        .reset_n      (reset_n),
        .clock        (clock),
        .write_request(write_request),
        .data_in      (data_in),
        .read_request (read_request),
        .data_out     (data_out),
        .empty        (empty),
        .full         (full),
        .used_words   (used_words)
    );

    // 80 MHz — 12.5 ns period. The rate the single-clock design runs at, so
    // that a waveform from this testbench is read in the same units as one
    // from the instrument.
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
                $display("FAIL: depth %0d: %0s: got %0d, expected %0d (t=%0t)", Depth, what, got,
                         want, $time);
                errors = errors + 32'd1;
            end
        end
    endtask

    // One clock cycle with the given request lines, settling afterwards so
    // that the caller sees the state the edge produced.
    //
    // Show-ahead means a read is an acknowledgement of a word already on
    // data_out, so a caller reading a word checks data_out *before* calling
    // this with do_read set, not after.
    task cycle;
        input do_write;
        input [DATA_WIDTH-1:0] write_value;
        input do_read;
        begin
            write_request = do_write;
            data_in       = write_value;
            read_request  = do_read;
            @(posedge clock);
            #1;
        end
    endtask

    // The flags are a decode of the occupancy count and must agree with it on
    // every cycle, not merely at the points the sequence below looks at them.
    // Sampled at #2 so that it is behind the stimulus, which drives and checks
    // at #1.
    always @(posedge clock) begin
        #2;
        if (reset_n === 1'b1) begin
            if (empty !== (used_words == COUNT_ZERO)) begin
                $display("FAIL: depth %0d: empty disagrees with used_words %0d (t=%0t)", Depth,
                         used_words, $time);
                errors = errors + 32'd1;
            end

            if (full !== (used_words == DEPTH_WORDS)) begin
                $display("FAIL: depth %0d: full disagrees with used_words %0d (t=%0t)", Depth,
                         used_words, $time);
                errors = errors + 32'd1;
            end

            if (used_words > DEPTH_WORDS) begin
                $display("FAIL: depth %0d: used_words %0d exceeds the depth (t=%0t)", Depth,
                         used_words, $time);
                errors = errors + 32'd1;
            end
        end
    end

    initial begin
        done          = 1'b0;
        errors        = 32'd0;
        write_request = 1'b0;
        read_request  = 1'b0;
        data_in       = {DATA_WIDTH{1'b0}};
        reset_n       = 1'b0;
        half          = Depth / 2;

        @(posedge clock);
        #1 reset_n = 1'b1;

        // --- After reset --------------------------------------------------
        check(empty, 32'd1, "empty after reset");
        check(full, 32'd0, "full after reset");
        check(used_words, 32'd0, "used_words after reset");

        // --- A read while empty is ignored --------------------------------
        // Not merely harmless: if it moved the read pointer, the first word
        // written afterwards would never be handed out, and the pointers would
        // stay one apart for the rest of the capture.
        for (i = 0; i < 3; i = i + 1) begin
            cycle(1'b0, {DATA_WIDTH{1'b0}}, 1'b1);
            check(used_words, 32'd0, "used_words after a read while empty");
            check(empty, 32'd1, "empty after a read while empty");
        end

        // --- Show-ahead ---------------------------------------------------
        // One word into an empty queue must appear on data_out on the next
        // cycle, with nothing asked of the read side. This is also the case
        // the write-to-read bypass exists for: the word is being read ahead to
        // on the same edge it is written, so the memory cannot supply it.
        cycle(1'b1, 16'hA5A5, 1'b0);
        check(empty, 32'd0, "empty after one write");
        check(used_words, 32'd1, "used_words after one write");
        check(data_out, 32'h0000A5A5, "data_out shows the word with no read request");

        // Holding it must not consume it, and the value must survive the
        // handover from the bypass register to the memory output.
        for (i = 0; i < 3; i = i + 1) begin
            cycle(1'b0, {DATA_WIDTH{1'b0}}, 1'b0);
            check(data_out, 32'h0000A5A5, "data_out holds an unacknowledged word");
            check(used_words, 32'd1, "used_words holds an unacknowledged word");
        end

        cycle(1'b0, {DATA_WIDTH{1'b0}}, 1'b1);
        check(empty, 32'd1, "empty once the word is acknowledged");
        check(used_words, 32'd0, "used_words once the word is acknowledged");

        // --- Fill to full -------------------------------------------------
        for (i = 0; i < Depth; i = i + 1) begin
            value = i[DATA_WIDTH-1:0];
            cycle(1'b1, value, 1'b0);

            check(used_words, i + 1, "used_words while filling");
            check(empty, 32'd0, "empty while filling");
            check(data_out, 32'd0, "the head does not move while filling");

            if (i == Depth - 1) begin
                check(full, 32'd1, "full on the last word of a fill");
            end else begin
                check(full, 32'd0, "full before the last word of a fill");
            end
        end

        // --- A write while full is discarded ------------------------------
        // The alternative — overwriting the oldest word — would keep the
        // occupancy correct and silently reorder the capture.
        for (i = 0; i < 3; i = i + 1) begin
            cycle(1'b1, 16'hDEAD, 1'b0);
            check(used_words, Depth, "used_words after a write while full");
            check(full, 32'd1, "full after a write while full");
        end

        // --- Drain, in order ----------------------------------------------
        // read_request is held for every cycle of this loop, which is how the
        // FX3 reads a packet: one word per clock with no gaps. A discarded
        // 0xDEAD would show up here as a word out of sequence.
        for (i = 0; i < Depth; i = i + 1) begin
            value = i[DATA_WIDTH-1:0];
            check(data_out, {16'd0, value}, "word order while draining");
            check(used_words, Depth - i, "used_words while draining");
            cycle(1'b0, {DATA_WIDTH{1'b0}}, 1'b1);
        end
        check(empty, 32'd1, "empty after draining");
        check(used_words, 32'd0, "used_words after draining");

        // --- Wrap ----------------------------------------------------------
        // The fill and drain above both started and ended at address zero, so
        // they say nothing about the wrap. Move the pointers off zero first,
        // then a full fill and drain has to cross the end of the memory.
        for (i = 0; i < 3; i = i + 1) begin
            value = 16'h1000 + i[DATA_WIDTH-1:0];
            cycle(1'b1, value, 1'b0);
        end
        for (i = 0; i < 3; i = i + 1) begin
            value = 16'h1000 + i[DATA_WIDTH-1:0];
            check(data_out, {16'd0, value}, "word order before the wrap");
            cycle(1'b0, {DATA_WIDTH{1'b0}}, 1'b1);
        end
        check(empty, 32'd1, "empty before the wrap");

        for (i = 0; i < Depth; i = i + 1) begin
            value = 16'h2000 + i[DATA_WIDTH-1:0];
            cycle(1'b1, value, 1'b0);
        end
        check(full, 32'd1, "full after filling across the wrap");

        for (i = 0; i < Depth; i = i + 1) begin
            value = 16'h2000 + i[DATA_WIDTH-1:0];
            check(data_out, {16'd0, value}, "word order across the wrap");
            check(used_words, Depth - i, "used_words across the wrap");
            cycle(1'b0, {DATA_WIDTH{1'b0}}, 1'b1);
        end
        check(empty, 32'd1, "empty after draining across the wrap");

        // --- Simultaneous read and write ----------------------------------
        // The steady state the instrument runs in: the FX3 is draining while
        // the ADC is still writing. The occupancy must not drift.
        for (i = 0; i < half; i = i + 1) begin
            value = 16'h3000 + i[DATA_WIDTH-1:0];
            cycle(1'b1, value, 1'b0);
        end
        check(used_words, half, "used_words after a half fill");

        for (i = 0; i < half; i = i + 1) begin
            value = 16'h3000 + i[DATA_WIDTH-1:0];
            check(data_out, {16'd0, value}, "word order during simultaneous read and write");
            check(used_words, half, "used_words during simultaneous read and write");

            value = 16'h3000 + half[DATA_WIDTH-1:0] + i[DATA_WIDTH-1:0];
            cycle(1'b1, value, 1'b1);
        end

        for (i = 0; i < half; i = i + 1) begin
            value = 16'h3000 + half[DATA_WIDTH-1:0] + i[DATA_WIDTH-1:0];
            check(data_out, {16'd0, value}, "word order after simultaneous read and write");
            cycle(1'b0, {DATA_WIDTH{1'b0}}, 1'b1);
        end
        check(empty, 32'd1, "empty after simultaneous read and write");

        // --- The bypass, every cycle ---------------------------------------
        // Held at one word deep with both requests asserted, the word being
        // written is always the word about to be read, so every cycle takes
        // the write-to-read bypass rather than the memory output. Twice the
        // depth, so it wraps while doing it.
        cycle(1'b1, 16'h4000, 1'b0);
        check(used_words, 32'd1, "used_words with one word queued");

        for (i = 0; i < 2 * Depth; i = i + 1) begin
            value = 16'h4000 + i[DATA_WIDTH-1:0];
            check(data_out, {16'd0, value}, "word order through the write-to-read bypass");
            check(used_words, 32'd1, "used_words through the write-to-read bypass");

            value = 16'h4001 + i[DATA_WIDTH-1:0];
            cycle(1'b1, value, 1'b1);
        end

        value = 16'h4000 + (2 * Depth);
        check(data_out, {16'd0, value}, "the last word out of the bypass run");
        cycle(1'b0, {DATA_WIDTH{1'b0}}, 1'b1);
        check(empty, 32'd1, "empty after the bypass run");

        // --- Reset while occupied ------------------------------------------
        // reset_n is the FX3's, and drops when the host closes the device. A
        // queue that came back holding the previous capture's samples would
        // put them at the front of the next one.
        for (i = 0; i < 3; i = i + 1) begin
            value = 16'h5000 + i[DATA_WIDTH-1:0];
            cycle(1'b1, value, 1'b0);
        end
        check(used_words, 32'd3, "used_words before reset");

        reset_n = 1'b0;
        #1;
        check(empty, 32'd1, "reset asserts empty asynchronously");
        check(used_words, 32'd0, "reset clears used_words asynchronously");

        @(posedge clock);
        #1 reset_n = 1'b1;

        cycle(1'b1, 16'h6000, 1'b0);
        check(data_out, 32'h00006000, "data_out after a reset");
        check(used_words, 32'd1, "used_words after a reset");

        done = 1'b1;
    end

endmodule

module tb_fifo;

    wire           done_wrap;
    wire           done_power_of_two;
    wire           done_instrument;

    wire    [31:0] errors_wrap;
    wire    [31:0] errors_power_of_two;
    wire    [31:0] errors_instrument;

    integer        errors;

    fifo_case #(
        .Depth(6)
    ) case_wrap (
        .done  (done_wrap),
        .errors(errors_wrap)
    );

    fifo_case #(
        .Depth(8)
    ) case_power_of_two (
        .done  (done_power_of_two),
        .errors(errors_power_of_two)
    );

    fifo_case #(
        .Depth(16384)
    ) case_instrument (
        .done  (done_instrument),
        .errors(errors_instrument)
    );

    initial begin
        wait (done_wrap && done_power_of_two && done_instrument);

        errors = errors_wrap + errors_power_of_two + errors_instrument;

        if (errors == 0) begin
            $display("tb_fifo: PASS");
        end else begin
            $display("tb_fifo: FAIL (%0d errors)", errors);
        end

        if (errors != 0) begin
            $fatal(1, "tb_fifo failed");
        end
        $finish;
    end

endmodule
