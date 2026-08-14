/************************************************************************

    buffer.v

    Data buffer module
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2018-2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Sits between the sampling side, which produces one word every second
    system-clock cycle, and the FX3, which takes them away a packet at a time
    and can be stalled for a long time by the USB 3 host. The buffer is what
    lets the FX3 fall behind and catch up without a sample being lost.

    One FIFO, not the ping-pong pair this module used to be. The pair existed
    because a dual-clock FIFO cannot report an occupancy that is exact - the
    read side sees the write side's word count through a synchroniser chain,
    several cycles stale - so "is a whole packet ready" had to be answered by
    filling one buffer completely and swapping. With a single clock the count
    is exact on the cycle, so the question is a comparison and the second
    buffer has nothing to do.

    Two consequences worth stating, because both are improvements rather than
    translations:

    An overflow now drops the samples that do not fit. The old module reacted
    by asynchronously clearing the whole 8192-word buffer the FX3 had not
    finished with, so a stall that cost one sample threw away up to 8192 that
    had already been captured. The sequence numbers dataGenerator stamps into
    the stream let the host see the gap either way, so the only difference is
    how much is lost.

    The error flag is held for a fixed number of cycles so the FX3 cannot miss
    it, which is what the old module intended - but its hold counter was never
    cleared, so only the *first* overflow of a session was held. Every one
    after it raised the flag for a single cycle. That is fixed here.

************************************************************************/

module buffer (
    input reset_n,
    input clock,

    // One assertion per sample. The sampling side runs at half the system
    // clock, so this is high every second cycle.
    input        write_enable,
    input [15:0] data_in,

    // High for each cycle the FX3 is taking a word off the databus
    input         is_reading,
    output [15:0] data_out,

    output reg data_available,
    output reg buffer_error
);

    // The packet size, in words, and the same number three places agree on:
    // the FX3's DMA buffer, the count in fx3StateMachine, and one 16 KiB USB 3
    // bulk endpoint buffer. Changing it here alone breaks the capture.
    localparam integer PacketWords = 8192;

    // Twice the packet size. The headroom above the threshold is what a USB
    // stall is paid for out of: 8192 words at 40 MSPS is 205 us of grace, the
    // same as the old pair of buffers gave, in the same total memory.
    localparam integer FifoDepth = 16384;

    localparam integer UsedBits = $clog2(FifoDepth + 1);
    localparam integer PacketBits = $clog2(PacketWords + 1);

    // Sized constants, made by part-selecting a 32-bit value for the reason
    // given at the head of fifo.v
    localparam [31:0] PacketValue = PacketWords;
    localparam [UsedBits-1:0] PacketThreshold = PacketValue[UsedBits-1:0];
    localparam [PacketBits-1:0] PacketCount = PacketValue[PacketBits-1:0];
    localparam [PacketBits-1:0] PacketZero = {PacketBits{1'b0}};
    localparam [PacketBits-1:0] PacketOne = {{(PacketBits - 1) {1'b0}}, 1'b1};

    // How long the error flag is held, in system clock cycles. 2000 cycles at
    // 80 MHz is 25 us, which is what 1000 cycles of the old 40 MHz write clock
    // came to - the FX3 samples this pin per packet, so the flag has to
    // outlast a packet's worth of indifference.
    localparam [11:0] ErrorHoldCycles = 12'd2000;
    localparam [11:0] ErrorHoldZero = 12'd0;
    localparam [11:0] ErrorHoldOne = 12'd1;

    wire [UsedBits-1:0] used_words;
    wire                fifo_full;

    // A write that arrives with the FIFO full is the overflow. The FIFO
    // discards it - that is its stated contract, so gating the request here as
    // well would be a second copy of the same decision - and this is only
    // what raises the flag about it.
    wire                overflow = write_enable && fifo_full;

    fifo #(
        .DataWidth(16),
        .Depth    (FifoDepth)
    ) fifo_0 (
        .reset_n      (reset_n),
        .clock        (clock),
        .write_request(write_enable),
        .data_in      (data_in),
        .read_request (is_reading),
        .data_out     (data_out),
        .full         (fifo_full),
        .used_words   (used_words)
    );

    // Packet availability ---------------------------------------------------
    //
    // data_available states that a whole packet can be read without the FX3
    // ever having to wait, so it is raised only once a whole packet is queued
    // and then held for the length of that packet. Holding it is what the
    // ping-pong pair did - it set the flag when a buffer filled and cleared it
    // when that buffer emptied - and the GPIF II state machine on the other
    // side of the pin was designed against that. Dropping the flag the moment
    // the occupancy fell back below a packet would be a truthful signal and a
    // different contract.
    reg [PacketBits-1:0] packet_remaining;

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            data_available   <= 1'b0;
            packet_remaining <= PacketZero;
        end else if (!data_available) begin
            if (used_words >= PacketThreshold) begin
                data_available   <= 1'b1;
                packet_remaining <= PacketCount;
            end
        end else if (is_reading) begin
            if (packet_remaining == PacketOne) begin
                data_available   <= 1'b0;
                packet_remaining <= PacketZero;
            end else begin
                packet_remaining <= packet_remaining - PacketOne;
            end
        end
    end

    // The overflow flag -----------------------------------------------------

    reg [11:0] error_hold;

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            buffer_error <= 1'b0;
            error_hold   <= ErrorHoldZero;
        end else if (overflow) begin
            // Restarting the count on every overflow, rather than only on the
            // first, is the fix described in the header
            buffer_error <= 1'b1;
            error_hold   <= ErrorHoldZero;
        end else if (buffer_error) begin
            if (error_hold >= ErrorHoldCycles) begin
                buffer_error <= 1'b0;
                error_hold   <= ErrorHoldZero;
            end else begin
                error_hold <= error_hold + ErrorHoldOne;
            end
        end
    end

endmodule
