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
	output [9:0] dataOut
);

// Register to store test and ADC data values
reg [9:0] adcData;
reg [9:0] testData;
wire [9:0] adc_databusRead;

// If we are in test-mode use test data,
// otherwise use the actual ADC data
assign dataOut = testModeFlag ? testData : adcData;

// Read the ADC data and generate the test data on the
// negative edge of the clock
always @ (posedge clock, negedge nReset) begin
	if (!nReset) begin
		adcData <= 10'd0;
		testData <= 10'd0;
	end else begin
		// Read the ADC data
		adcData <= adc_databus;
		
		// Test mode data generation
		testData <= testData + 10'd1;
	end
end

endmodule