/************************************************************************

    crc32.v

    Byte-at-a-time CRC-32
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The standard CRC-32 - polynomial 0xEDB88320 reflected, initial value
    all ones, result inverted - which is the one zlib, PNG and every
    checksum tool on a developer's machine compute. That choice matters
    more than the polynomial does: the boot block this checks is written by
    a host, and a host must be able to produce the number with a library
    call rather than by porting this file.

    Eight bits per clock, unrolled into combinational logic. A serial
    implementation would be smaller, but the boot logic reads a whole
    application image through it - a few hundred kilobytes at one byte per
    flash byte time - and a bit-serial version would multiply that by eight
    for a saving of a few dozen logic elements in an image that has the
    whole device to itself.

    This is the one place in the update chain where a check is not SHA-256,
    and it is deliberate: the factory image is frozen and must stay small,
    and by the time these bytes are on the flash their authenticity has
    already been settled by the signature on the bundle and by the digest
    the FX3 recomputed from the medium. What remains for this to catch is
    corruption, which is exactly what a CRC is for. The reasoning is on the
    EPCS layout and boot flow page of the documentation site.

************************************************************************/

module crc32 (
    input reset_n,
    input clock,

    // A byte to fold in, valid while data_valid is high
    input       data_valid,
    input [7:0] data,

    // Start again from the initial value. Takes effect on the same clock,
    // so a caller can restart and present a byte together.
    input restart,

    // The CRC of everything folded in so far, in the form a host's library
    // reports it
    output [31:0] crc
);

    localparam [31:0] Polynomial = 32'hEDB88320;
    localparam [31:0] InitialValue = 32'hFFFFFFFF;

    reg [31:0] state;

    // One byte, eight shifts, as a function so the unrolling is written
    // once rather than eight times
    function [31:0] fold_byte;
        input [31:0] current;
        input [7:0] value;
        reg     [31:0] working;
        integer        bit_index;
        begin
            working = current ^ {24'd0, value};

            for (bit_index = 0; bit_index < 8; bit_index = bit_index + 1) begin
                if (working[0]) begin
                    working = (working >> 1) ^ Polynomial;
                end else begin
                    working = working >> 1;
                end
            end

            fold_byte = working;
        end
    endfunction

    // Inverted on the way out, which is the half of the specification that
    // is easy to leave out and impossible to notice until a host disagrees
    assign crc = ~state;

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            state <= InitialValue;
        end else if (restart) begin
            if (data_valid) begin
                state <= fold_byte(InitialValue, data);
            end else begin
                state <= InitialValue;
            end
        end else if (data_valid) begin
            state <= fold_byte(state, data);
        end
    end

endmodule
