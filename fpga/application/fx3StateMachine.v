/************************************************************************

    fx3StateMachine.v

    FX3 State-Machine module
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2018-2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

module fx3StateMachine (
    input reset_n,
    input fx3_clock,
    input read_data,

    output fx3_is_reading
);

    // State machine logic ---------------------------------------------------

    // State machine state definitions (4-bit 0-15)
    reg [3:0] sm_current_state;
    reg [3:0] sm_next_state;

    // localparam rather than parameter: these are state encodings, not knobs an
    // instantiation should be able to override.
    localparam [3:0] StateWaitForRequest = 4'd01;
    localparam [3:0] StateSendPacket = 4'd02;

    // Set state to StateWaitForRequest on reset - or assign the next state
    always @(posedge fx3_clock, negedge reset_n) begin
        if (!reset_n) begin
            sm_current_state <= StateWaitForRequest;
        end else begin
            sm_current_state <= sm_next_state;
        end
    end

    // Ensure that the read_data signal is only read
    // on the FX3 clock edge
    reg read_data_flag;

    always @(posedge fx3_clock, negedge reset_n) begin
        if (!reset_n) begin
            read_data_flag <= 1'b0;
        end else begin
            read_data_flag <= read_data;
        end
    end

    // Counter for the StateSendPacket state
    // Here we should send 8192 words to the FX3
    reg [15:0] word_counter;

    always @(posedge fx3_clock, negedge reset_n) begin
        if (!reset_n) begin
            word_counter = 16'd0;
        end else begin
            if (sm_current_state == StateSendPacket) begin
                word_counter = word_counter + 16'd1;
            end else begin
                word_counter = 16'd0;
            end
        end
    end

    // Generate fx3_is_reading flag
    assign fx3_is_reading = (sm_current_state == StateSendPacket) ? 1'b1 : 1'b0;

    // State machine transition logic
    always @(*) begin
        sm_next_state = sm_current_state;

        case (sm_current_state)

            // StateWaitForRequest (waits for the FX3 to request a packet)
            StateWaitForRequest: begin
                // Is the GPIF reading data?
                if (read_data_flag == 1'b1 && word_counter == 16'd0) begin
                    sm_next_state = StateSendPacket;
                end else begin
                    // GPIF not ready... wait
                    sm_next_state = StateWaitForRequest;
                end
            end

            // StateSendPacket (sends a packet of 8192 words to the FX3)
            StateSendPacket: begin
                if (word_counter == 16'd8191) begin
                    // Packet send, go back to waiting
                    sm_next_state = StateWaitForRequest;
                end else begin
                    // Continue sending packet
                    sm_next_state = StateSendPacket;
                end
            end

        endcase
    end


endmodule
