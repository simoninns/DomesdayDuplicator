/************************************************************************
	
	readAdcData.v
	Read ADC data module
	
	DomesdayDuplicator - LaserDisc RF sampler
	Copyright (C) 2017 Simon Inns
	
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

module readAdcData(
	input clock,
	input nReset,
	input [9:0] adcDatabus,
	input nTestmode,
	
	output reg [9:0] adcData
);

reg [9:0] adcUintValue;

// Register to store test data value
reg [9:0] testData;

// ADC data is valid on the negative edge of the clock
always @ (negedge clock, negedge nReset) begin
	if (!nReset) begin
		adcData <= 10'd0;
		testData <= 10'd0;
	end else begin
		if (nTestmode) begin
			// Read the ADC value
			adcData <= adcDatabus;
		end else begin
			// Test mode data generation
			adcData <= testData;
			testData = testData + 10'd1;
		end
	end
end

endmodule