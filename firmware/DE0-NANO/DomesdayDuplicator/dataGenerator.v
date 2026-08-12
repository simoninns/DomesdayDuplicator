/************************************************************************
	
	dataGeneration.v
	Data generation module
	
	Domesday Duplicator - LaserDisc RF sampler
	SPDX-FileCopyrightText: 2018-2025 Simon Inns
	SPDX-License-Identifier: GPL-3.0-or-later
	
************************************************************************/

module dataGenerator (
	input nReset,
	input clock,
	input [9:0] adc_databus,
	input testModeFlag,
	
	// Outputs
	output [15:0] dataOut
);

// Register to store ADC data values
reg [9:0] adcData;

// Register to store test data values
reg [9:0] testData;

// Register to store the sequence number counter
reg [21:0] sequenceCount;

// The top 6 bits of the output are the sequence number
assign dataOut[15:10] = sequenceCount[21:16];

// If we are in test-mode use test data,
// otherwise use the actual ADC data
assign dataOut[9:0] = testModeFlag ? testData : adcData;

// Read the ADC data and increment the counters on the
// negative edge of the clock
//
// Note: The test data is a repeating pattern of incrementing
// values from 0 to 1020.
//
// The sequence number counts from 0 to 62 repeatedly, with each
// number being attached to 65536 samples.
always @ (posedge clock, negedge nReset) begin
	if (!nReset) begin
		adcData <= 10'd0;
		testData <= 10'd0;
		sequenceCount <= 22'd0;
	end else begin
		// Read the ADC data
		adcData <= adc_databus;
		
		// Test mode data generation
		if (testData == 10'd1021 - 1)
			testData <= 10'd0;
		else
			testData <= testData + 10'd1;
		
		// Sequence number generation
		if (sequenceCount == (6'd63 << 16) - 1)
			sequenceCount <= 22'd0;
		else
			sequenceCount <= sequenceCount + 22'd1;
	end
end

endmodule