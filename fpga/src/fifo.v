/************************************************************************

    fifo.v

    Single-clock FIFO buffer
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The replacement for Altera's dcfifo. Everything in the capture path now
    runs from one clock, so the dual-clock FIFO the design used to need is
    gone, and with it the proprietary IP - which also could not be simulated,
    because dcfifo has no free model and so nothing that instantiated it could
    be put under a testbench.

    Show-ahead, which the dcfifo was configured for as lpm_showahead = "ON":
    data_out already presents the word at the head of the queue, and
    read_request acknowledges it rather than requesting it. The GPIF read path
    depends on this. The FX3 samples the databus on the same clock edge that
    the state machine counts as a read, so a FIFO that produced its word a
    cycle after being asked would hand the FX3 the previous sample on every
    edge of a packet.

    data_out is meaningful only while empty is low. When the queue is empty it
    holds whatever was last read out of the memory, which after reset is
    whatever the M9K powered up with.

    Depth does not have to be a power of two: the pointers wrap on a compare
    against the last address rather than by letting a counter overflow. That
    costs one comparator per pointer and removes a silent failure - a depth
    that is not a power of two would otherwise corrupt data rather than fail
    to compile.

************************************************************************/

module fifo #(
    parameter integer DataWidth = 16,
    parameter integer Depth     = 16384
) (
    input reset_n,
    input clock,

    // Write side. A write while full is discarded; the caller is expected to
    // watch full and decide what to do about it, because only the caller
    // knows whether losing a word is worth reporting.
    input                 write_request,
    input [DataWidth-1:0] data_in,

    // Read side. data_out is already valid; read_request advances past it.
    input                  read_request,
    output [DataWidth-1:0] data_out,

    output                       full,
    output [$clog2(Depth+1)-1:0] used_words
);

    // $clog2(Depth) addresses the memory; used_words has to represent Depth
    // itself as well as every count below it, so it needs the extra bit.
    localparam integer AddressBits = $clog2(Depth);
    localparam integer CountBits = $clog2(Depth + 1);

    // Verilog-2001 has no way to write a literal whose width follows a
    // parameter, so the sized forms of Depth are made by part-selecting a
    // 32-bit value. Assigning Depth directly to a localparam of the right
    // width is the same arithmetic, but it reads to the correctness linter as
    // a 32-bit value silently truncated on the way in - which is exactly the
    // mistake worth being told about everywhere else.
    localparam [31:0] LastAddressValue = Depth - 1;
    localparam [AddressBits-1:0] LastAddress = LastAddressValue[AddressBits-1:0];

    localparam [31:0] DepthValue = Depth;
    localparam [CountBits-1:0] DepthWords = DepthValue[CountBits-1:0];

    localparam [AddressBits-1:0] AddressZero = {AddressBits{1'b0}};
    localparam [AddressBits-1:0] AddressOne = {{(AddressBits - 1) {1'b0}}, 1'b1};
    localparam [CountBits-1:0] CountZero = {CountBits{1'b0}};
    localparam [CountBits-1:0] CountOne = {{(CountBits - 1) {1'b0}}, 1'b1};

    reg  [AddressBits-1:0] write_pointer;
    reg  [AddressBits-1:0] read_pointer;
    reg  [  CountBits-1:0] used;

    // Emptiness is not a port: used_words carries it, and no consumer in this
    // design wants a second way to ask the same question.
    wire                   empty = (used == CountZero);

    assign full       = (used == DepthWords);
    assign used_words = used;

    wire write_enable = write_request && !full;
    wire read_enable = read_request && !empty;

    // The address the head will be at after this edge. The memory read is
    // registered, so reading ahead by one is what makes the head word already
    // be on data_out when the next cycle starts.
    wire [AddressBits-1:0] read_pointer_next =
        read_enable ? ((read_pointer == LastAddress) ? AddressZero : read_pointer + AddressOne)
                    : read_pointer;

    // Storage ---------------------------------------------------------------
    //
    // No reset in this block, deliberately. An M9K has no reset on its
    // contents, so a reset here would deny Quartus the inference and put
    // Depth x DataWidth bits into logic elements instead - 262144 of them at
    // the depth this design uses, against the 22320 the whole device has.
    //
    // memory_data_out is left out of the reset for the same reason: it is the
    // memory's own output register, and resetting it would cost that too.

    reg [DataWidth-1:0] memory[0:Depth-1];
    reg [DataWidth-1:0] memory_data_out;

    always @(posedge clock) begin
        if (write_enable) begin
            memory[write_pointer] <= data_in;
        end

        memory_data_out <= memory[read_pointer_next];
    end

    // Write-to-read bypass --------------------------------------------------
    //
    // When the word being written is the same word being read ahead to, the
    // registered read returns what was in the memory before the write, so the
    // value has to come from the write port instead. This is the case where a
    // word is written into an empty queue and read on the very next cycle,
    // which is what happens whenever the FX3 keeps up completely.
    //
    // Quartus does not promise read-during-write behaviour for an inferred
    // simple dual-port memory, so this is not an optimisation - without it the
    // word read out in that case is undefined.

    reg                 bypass_valid;
    reg [DataWidth-1:0] bypass_data;

    assign data_out = bypass_valid ? bypass_data : memory_data_out;

    // Pointers and occupancy ------------------------------------------------

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            write_pointer <= AddressZero;
            read_pointer  <= AddressZero;
            used          <= CountZero;
            bypass_valid  <= 1'b0;
            bypass_data   <= {DataWidth{1'b0}};
        end else begin
            if (write_enable) begin
                if (write_pointer == LastAddress) begin
                    write_pointer <= AddressZero;
                end else begin
                    write_pointer <= write_pointer + AddressOne;
                end
            end

            read_pointer <= read_pointer_next;

            // A simultaneous read and write leaves the count alone
            if (write_enable && !read_enable) begin
                used <= used + CountOne;
            end else if (read_enable && !write_enable) begin
                used <= used - CountOne;
            end

            bypass_valid <= write_enable && (write_pointer == read_pointer_next);
            bypass_data  <= data_in;
        end
    end

endmodule
