/************************************************************************

    buffer.v

    Data buffer module
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2018-2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

module buffer (
    input        reset_n,
    input        write_clock,
    input        read_clock,
    input        is_reading,
    input [15:0] data_in,

    output reg        buffer_overflow,
    output reg        data_available,
    output     [15:0] data_out
);

    // FIFO buffer size in words
    // Note: The size of this buffer must match the buffer size used
    // by the FX3 (which is set to 8192 16-bit words per buffer) as this
    // must match the size of the USB3 end-point which is 16Kbytes
    //
    localparam [13:0] BufferSize = 14'd8191;  // 0 - 8191 = 8192 words

    // "Ping-pong" buffer storing 8192 16-bit words per buffer
    reg         current_write_buffer;  // 0 = write to ping buffer read from pong,
    // 1 = write to pong buffer read from ping

    // Define various buffer signals and values

    // Buffer signals (write clock sync'd)
    wire        ping_async_clear_wr;
    wire        pong_async_clear_wr;
    wire        ping_empty_flag_wr;
    wire        pong_empty_flag_wr;
    wire [13:0] ping_used_words_wr;
    wire [13:0] pong_used_words_wr;

    // Buffer signals (read clock sync'd)
    wire        ping_empty_flag_rd;
    wire        pong_empty_flag_rd;
    wire [13:0] ping_used_words_rd;
    wire [13:0] pong_used_words_rd;

    // Data out buses
    wire [15:0] ping_data_out;
    wire [15:0] pong_data_out;

    // Define the ping buffer (0) - 8192 16-bit words
    IPfifo ping_buffer (
        .aclr   (ping_async_clear_wr),
        .data   (ping_data_in),         // 16-bit [15:0]
        .rdclk  (read_clock),
        .rdreq  (ping_read_request),
        .wrclk  (write_clock),
        .wrreq  (ping_write_request),
        .q      (ping_data_out),        // 16-bit [15:0]
        .rdempty(ping_empty_flag_rd),
        .rdusedw(ping_used_words_rd),   // 14-bit [13:0]
        .wrempty(ping_empty_flag_wr),
        .wrusedw(ping_used_words_wr)    // 14-bit [13:0]
    );

    // Define the pong buffer (1) - 8192 16-bit words
    IPfifo pong_buffer (
        .aclr   (pong_async_clear_wr),
        .data   (pong_data_in),         // 16-bit [15:0]
        .rdclk  (read_clock),
        .rdreq  (pong_read_request),
        .wrclk  (write_clock),
        .wrreq  (pong_write_request),
        .q      (pong_data_out),        // 16-bit [15:0]
        .rdempty(pong_empty_flag_rd),
        .rdusedw(pong_used_words_rd),   // 14-bit [13:0]
        .wrempty(pong_empty_flag_wr),
        .wrusedw(pong_used_words_wr)    // 14-bit [13:0]
    );


    // Route the control signals according to the currently selected write buffer
    wire [15:0] ping_data_in;
    wire [15:0] pong_data_in;
    wire        ping_read_request;
    wire        pong_read_request;
    wire        ping_write_request;
    wire        pong_write_request;

    // if current write buffer = ping then send data to ping buffer
    // else send data to pong buffer
    assign ping_data_in       = current_write_buffer ? 16'd0 : data_in;
    assign pong_data_in       = current_write_buffer ? data_in : 16'd0;

    // if current write buffer = ping then data_out = pong buffer else data_out = ping_buffer
    assign data_out           = current_write_buffer ? ping_data_out : pong_data_out;

    // If current write buffer = ping then read from pong else read from ping
    assign ping_read_request  = current_write_buffer ? is_reading : 1'b0;
    assign pong_read_request  = current_write_buffer ? 1'b0 : is_reading;

    // if current write buffer = ping then write to ping else write to pong
    assign ping_write_request = current_write_buffer ? 1'b0 : 1'b1;
    assign pong_write_request = current_write_buffer ? 1'b1 : 1'b0;

    // Define registers for the async clear flags and map to registers
    // Note: the async clear flag can cause the empty flag to glitch when
    // asserted, so we have to sync it with the reset_n signal to avoid
    // issues on reset.
    reg ping_async_clear_reg;
    reg pong_async_clear_reg;
    assign ping_async_clear_wr = ping_async_clear_reg | !reset_n;
    assign pong_async_clear_wr = pong_async_clear_reg | !reset_n;

    // Register to track activation of the overflow flag (0-1024 10-bit)
    reg [9:0] buffer_overflow_hold;

    // FIFO Write-side logic (controls switching between ping and pong buffers)
    always @(posedge write_clock, negedge reset_n) begin
        if (!reset_n) begin
            // Clear all registers on reset
            current_write_buffer <= 1'b0;
            buffer_overflow      <= 1'b0;
            buffer_overflow_hold <= 10'd0;
            ping_async_clear_reg <= 1'b0;
            pong_async_clear_reg <= 1'b0;
        end else begin
            // Which buffer is being written to?
            if (current_write_buffer) begin
                // Current write buffer is pong

                // Is the pong buffer nearly full?
                if (pong_used_words_wr == BufferSize - 2) begin
                    // Check that the ping buffer has been emptied...
                    if (!ping_empty_flag_wr) begin
                        // Flag an overflow error
                        buffer_overflow      <= 1'b1;

                        // Set the ping buffer async clear (empty the ping buffer)
                        ping_async_clear_reg <= 1'b1;
                    end
                end

                // Is the pong buffer 1 word from full?
                if (pong_used_words_wr == BufferSize - 1) begin
                    // Reset the ping buffer async clear
                    ping_async_clear_reg <= 1'b0;

                    // Switch to the ping buffer
                    current_write_buffer <= 1'b0;
                end
            end else begin
                // Current write buffer is ping

                // Is the ping buffer nearly full?
                if (ping_used_words_wr == BufferSize - 2) begin
                    // Check that the pong buffer has been emptied...
                    if (!pong_empty_flag_wr) begin
                        // Flag an overflow error
                        buffer_overflow      <= 1'b1;

                        // Set the pong buffer async clear (empty the pong buffer)
                        pong_async_clear_reg <= 1'b1;
                    end
                end

                // Is the ping buffer 1 word from full?
                if (ping_used_words_wr == BufferSize - 1) begin
                    // Reset the pong buffer async clear
                    pong_async_clear_reg <= 1'b0;

                    // Switch to the pong buffer
                    current_write_buffer <= 1'b1;
                end
            end

            // Track and clear the buffer overflow flag
            // Note: This holds the error signal high
            // for 1000 write clock cycles
            if (buffer_overflow == 1'b1) begin
                // Increment the hold counter
                buffer_overflow_hold <= buffer_overflow_hold + 10'd1;

                // If the hold clock-cycles is exceeded, clear the flag
                if (buffer_overflow_hold > 10'd1000) begin
                    buffer_overflow <= 1'b0;
                end
            end
        end
    end

    // FIFO read-side logic
    // Control the data available flag (on the read side)
    // Note: This is responsible for setting the flag when
    // data is available and clearing the flag once all
    // the available data has been read.
    always @(posedge read_clock, negedge reset_n) begin
        if (!reset_n) begin
            // On reset default to data unavailable
            data_available <= 1'b0;
        end else begin
            // Which buffer is being read from to?
            if (current_write_buffer) begin
                // Reading from ping buffer

                // Is the ping buffer full?
                if (ping_used_words_rd == BufferSize) begin
                    data_available <= 1'b1;
                end else begin
                    // Is the ping buffer empty?
                    if (ping_empty_flag_rd) begin
                        data_available <= 1'b0;
                    end
                end
            end else begin
                // Reading from pong buffer

                // Is the pong buffer full?
                if (pong_used_words_rd == BufferSize) begin
                    data_available <= 1'b1;
                end else begin
                    // Is the pong buffer empty?
                    if (pong_empty_flag_rd) begin
                        data_available <= 1'b0;
                    end
                end
            end
        end
    end

endmodule
