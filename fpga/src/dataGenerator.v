/************************************************************************

    dataGenerator.v

    Data generation module
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2018-2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

module dataGenerator (
    input       reset_n,
    input       clock,
    input [9:0] adc_databus,
    input       test_mode_flag,

    // Outputs
    output [15:0] data_out
);

    // Register to store ADC data values
    reg [ 9:0] adc_data;

    // Register to store test data values
    reg [ 9:0] test_data;

    // Register to store the sequence number counter
    reg [21:0] sequence_count;

    // The top 6 bits of the output are the sequence number
    assign data_out[15:10] = sequence_count[21:16];

    // If we are in test-mode use test data,
    // otherwise use the actual ADC data
    assign data_out[9:0]   = test_mode_flag ? test_data : adc_data;

    // Read the ADC data and increment the counters on the
    // negative edge of the clock
    //
    // Note: The test data is a repeating pattern of incrementing
    // values from 0 to 1020.
    //
    // The sequence number counts from 0 to 62 repeatedly, with each
    // number being attached to 65536 samples.
    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            adc_data       <= 10'd0;
            test_data      <= 10'd0;
            sequence_count <= 22'd0;
        end else begin
            // Read the ADC data
            adc_data <= adc_databus;

            // Test mode data generation
            if (test_data == 10'd1021 - 1) begin
                test_data <= 10'd0;
            end else begin
                test_data <= test_data + 10'd1;
            end

            // Sequence number generation
            if (sequence_count == (6'd63 << 16) - 1) begin
                sequence_count <= 22'd0;
            end else begin
                sequence_count <= sequence_count + 22'd1;
            end
        end
    end

endmodule
