/************************************************************************
	
	dataGeneration.v
	Data generation module
	
	Domesday Duplicator - LaserDisc RF sampler
	Copyright (C) 2018 Simon Inns
	
	This file is part of Domesday Duplicator.
	
	Domesday Duplicator is free software: you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation, either version 3 of the
	License, or (at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	Email: simon.inns@gmail.com
	
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