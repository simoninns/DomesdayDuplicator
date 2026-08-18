/************************************************************************

    bufferMonitor.v

    Capture buffer back-pressure instrument
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Watches the capture FIFO and reports how much headroom it had, so that a
    host can tell a capture that was comfortable from one that nearly failed.
    Until this existed the only thing that left the gateware about the buffer
    was the overflow pin, which says a capture has already been damaged and
    says nothing at all about how close the ones that survived came.

    Everything here is an observer. Every port but latch is an output, and
    latch reaches nothing except the registers in this module, so there is no
    path from this instrument back into the FIFO, its pointers, data_available
    or the GPIF handshake. A defect in here can misreport a capture. It cannot
    damage one, and that is the reason this is a module of its own rather than
    a handful of counters added to buffer.v.

    What the numbers will look like, which matters for reading them:
    data_available is raised only once a whole packet is queued and the FX3
    then drains at up to one word per clock while the sampling side writes one
    word every two, so a healthy capture sawtooths between a quarter and a half
    of the FIFO. Occupancy near half is the normal state and not a warning. The
    signal worth watching is the excursion above the packet threshold, because
    that is the FX3 having been late, and the room above it is the whole of
    what a USB stall is paid out of.

    Sampling, not streaming. The link that carries these numbers away moves
    about a byte per 80 us and used_words changes every 12.5 ns, so a host that
    read the counters directly would get a different counter's idea of a
    different instant in every byte. Instead a read latches all of them into a
    shadow bank in one clock, and the host reads the shadow - which then stands
    still until the next read. The interval counters are cleared by the same
    pulse, so each read reports the interval since the previous one, and the
    peak within that interval is the figure that a four-times-a-second sample
    of a 12.5 ns signal could never have found.

    Two counters deliberately do not clear: the lifetime peak and the sticky
    overflow bit. A second reader on the link - a diagnostic tool, a second
    application - consumes intervals that the first one then never sees, and
    those two say what happened regardless of who was reading.

************************************************************************/

module bufferMonitor #(
    parameter integer FifoDepth     = 16384,
    parameter integer PacketWords   = 8192,
    parameter integer NearFullWords = 12288
) (
    input reset_n,
    input clock,

    // The FIFO as buffer.v sees it, all sampled and none of it driven
    input [$clog2(FifoDepth+1)-1:0] used_words,
    input                           write_enable,  // a sample is offered this edge
    input                           overflow,      // and there was no room for it
    input                           is_reading,    // the FX3 took a word this edge

    // One clock wide, from the register bank, when a host begins reading the
    // telemetry block
    input latch,

    // The shadow bank, least significant byte first, as the register map
    // presents it from TELEM_STATUS upwards
    output [127:0] telemetry,

    // The three constants a host needs to interpret the bank, so that it never
    // has to carry a copy of this design's geometry: depth, packet size and the
    // near-full threshold, least significant byte first
    output [47:0] geometry
);

    localparam integer UsedBits = $clog2(FifoDepth + 1);

    // Enough to count words within a packet, which is the prescale that turns
    // words read into packets read
    localparam integer PacketBits = $clog2(PacketWords);

    // Sized constants, made by part-selecting a 32-bit value for the reason
    // given at the head of fifo.v
    localparam [31:0] DepthValue = FifoDepth;
    localparam [31:0] PacketValue = PacketWords;
    localparam [31:0] NearFullValue = NearFullWords;

    localparam [UsedBits-1:0] NearFullThreshold = NearFullValue[UsedBits-1:0];
    localparam [PacketBits-1:0] PacketLastWord = PacketValue[PacketBits-1:0] - 1'b1;

    localparam [UsedBits-1:0] UsedZero = {UsedBits{1'b0}};
    localparam [PacketBits-1:0] PacketZero = {PacketBits{1'b0}};

    // The layout of the shadow bank, reported in the low nibble of the status
    // byte. A host reads this rather than inferring the layout from the
    // gateware's commit, and a change to what any field means changes it.
    localparam [3:0] TelemetryFormat = 4'd1;

    localparam [15:0] CounterMaximum = 16'hFFFF;

    // Live counters -----------------------------------------------------------

    reg [UsedBits-1:0] peak_interval;
    reg [UsedBits-1:0] peak_lifetime;

    reg [15:0] overflow_events;
    reg [15:0] dropped_words;
    reg [15:0] packets_read;
    reg [15:0] near_full_units;

    reg [PacketBits-1:0] packet_word_count;
    reg [7:0] near_full_prescale;
    reg [7:0] latch_counter;

    reg overflow_active;
    reg overflow_sticky;
    reg saturated;

    // An overflow event is a burst, not a cycle. A stall drops every sample
    // offered until the FX3 comes back, and counting those as separate events
    // would report one stall as thousands - so the run is counted once, and
    // dropped_words is what says how long it lasted.
    wire overflow_event = overflow && !overflow_active;

    // A packet is complete on the last word of it. The FX3 reads exactly
    // PacketWords per packet, so counting words and dividing is exact rather
    // than an estimate of what the GPIF did.
    wire packet_complete = is_reading && (packet_word_count == PacketLastWord);

    // Time spent squeezed, sampled at the sampling rate so that it is a count
    // of samples and not of clocks. Prescaled by 256 because a quarter of a
    // second of continuous near-full is ten million samples and this is a
    // 16-bit field; the prescale is free-running across a latch, so at most 255
    // samples of an interval are attributed to the next one.
    wire near_full_sample = write_enable && (used_words >= NearFullThreshold);
    wire near_full_tick = near_full_sample && (near_full_prescale == 8'hFF);

    // Saturation is reported rather than wrapped. A counter that wrapped would
    // report a small number for a catastrophe, which is the one reading that
    // must not be possible.
    wire any_saturation = (overflow_event && (overflow_events == CounterMaximum))
        || (overflow && (dropped_words == CounterMaximum))
        || (near_full_tick && (near_full_units == CounterMaximum));

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            peak_interval      <= UsedZero;
            peak_lifetime      <= UsedZero;
            overflow_events    <= 16'd0;
            dropped_words      <= 16'd0;
            packets_read       <= 16'd0;
            near_full_units    <= 16'd0;
            packet_word_count  <= PacketZero;
            near_full_prescale <= 8'd0;
            latch_counter      <= 8'd0;
            overflow_active    <= 1'b0;
            overflow_sticky    <= 1'b0;
            saturated          <= 1'b0;
        end else begin
            // The peak. Cleared to the occupancy at the latch instant rather
            // than to zero, because that instant belongs to the interval
            // starting here and the peak of an interval that has already
            // reached this level cannot be lower than it.
            if (latch) begin
                peak_interval <= used_words;
            end else if (used_words > peak_interval) begin
                peak_interval <= used_words;
            end

            if (used_words > peak_lifetime) begin
                peak_lifetime <= used_words;
            end

            // The interval counters. Each is cleared by the latch to whatever
            // this cycle contributes, so no event is lost to the clearing and
            // none is counted in two intervals.
            if (latch) begin
                overflow_events <= overflow_event ? 16'd1 : 16'd0;
            end else if (overflow_event && (overflow_events != CounterMaximum)) begin
                overflow_events <= overflow_events + 16'd1;
            end

            if (latch) begin
                dropped_words <= overflow ? 16'd1 : 16'd0;
            end else if (overflow && (dropped_words != CounterMaximum)) begin
                dropped_words <= dropped_words + 16'd1;
            end

            // Packets wrap rather than saturating: a host differences
            // consecutive readings to get a rate, and a saturated count would
            // be a lie about the rate rather than a bounded one about a total.
            if (latch) begin
                packets_read <= packet_complete ? 16'd1 : 16'd0;
            end else if (packet_complete) begin
                packets_read <= packets_read + 16'd1;
            end

            if (latch) begin
                near_full_units <= near_full_tick ? 16'd1 : 16'd0;
            end else if (near_full_tick && (near_full_units != CounterMaximum)) begin
                near_full_units <= near_full_units + 16'd1;
            end

            // Position within a packet and the near-full prescale are phases,
            // not interval counts, so the latch leaves both alone. Clearing
            // them would make every read shift the packet boundary and lose
            // part of a sample count.
            if (is_reading) begin
                if (packet_word_count == PacketLastWord) begin
                    packet_word_count <= PacketZero;
                end else begin
                    packet_word_count <= packet_word_count + 1'b1;
                end
            end

            if (near_full_sample) begin
                near_full_prescale <= near_full_prescale + 8'd1;
            end

            // A burst ends at the first sample that finds room again
            if (overflow) begin
                overflow_active <= 1'b1;
                overflow_sticky <= 1'b1;
            end else if (write_enable) begin
                overflow_active <= 1'b0;
            end

            if (latch) begin
                saturated <= 1'b0;
            end else if (any_saturation) begin
                saturated <= 1'b1;
            end

            if (latch) begin
                latch_counter <= latch_counter + 8'd1;
            end
        end
    end

    // The shadow bank -------------------------------------------------------
    //
    // A second block, and not the same one, so that what a read returns is the
    // counters as they stood *before* the latch cleared them. Non-blocking
    // assignment is what makes that true without a temporary: both blocks see
    // the same values on the latching edge.

    reg [         7:0] shadow_status;
    reg [         7:0] shadow_latch_count;
    reg [UsedBits-1:0] shadow_used_now;
    reg [UsedBits-1:0] shadow_peak_interval;
    reg [UsedBits-1:0] shadow_peak_lifetime;
    reg [        15:0] shadow_overflow_events;
    reg [        15:0] shadow_dropped_words;
    reg [        15:0] shadow_packets_read;
    reg [        15:0] shadow_near_full_units;

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            // Zero, and specifically not the reset values of the live
            // counters, because a host that reads before the first latch must
            // see something it can recognise as "nothing has been sampled yet"
            // rather than a plausible set of measurements.
            shadow_status          <= {4'b0000, TelemetryFormat};
            shadow_latch_count     <= 8'd0;
            shadow_used_now        <= UsedZero;
            shadow_peak_interval   <= UsedZero;
            shadow_peak_lifetime   <= UsedZero;
            shadow_overflow_events <= 16'd0;
            shadow_dropped_words   <= 16'd0;
            shadow_packets_read    <= 16'd0;
            shadow_near_full_units <= 16'd0;
        end else if (latch) begin
            shadow_status          <= {2'b00, saturated, overflow_sticky, TelemetryFormat};
            shadow_latch_count     <= latch_counter + 8'd1;
            shadow_used_now        <= used_words;
            shadow_peak_interval   <= peak_interval;
            shadow_peak_lifetime   <= peak_lifetime;
            shadow_overflow_events <= overflow_events;
            shadow_dropped_words   <= dropped_words;
            shadow_packets_read    <= packets_read;
            shadow_near_full_units <= near_full_units;
        end
    end

    // The occupancies are UsedBits wide and the map presents them as sixteen,
    // which is what lets a host read every field of this bank the same way
    localparam integer UsedPad = 16 - UsedBits;

    assign telemetry = {
        shadow_near_full_units,
        shadow_packets_read,
        shadow_dropped_words,
        shadow_overflow_events,
        {UsedPad{1'b0}},
        shadow_peak_lifetime,
        {UsedPad{1'b0}},
        shadow_peak_interval,
        {UsedPad{1'b0}},
        shadow_used_now,
        shadow_latch_count,
        shadow_status
    };

    assign geometry = {NearFullValue[15:0], PacketValue[15:0], DepthValue[15:0]};

endmodule
