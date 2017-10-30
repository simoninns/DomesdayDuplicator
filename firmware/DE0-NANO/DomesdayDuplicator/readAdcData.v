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
	input runFlag,
	input [9:0] adcDatabus,
	
	output reg [15:0] adcData
);

// ADC data is valid on the negative edge of the clock
always @ (negedge clock, negedge nReset) begin
	if (!nReset) begin
		adcData <= 16'd0;
	end else begin
		// Uncomment to generate a test data counter
		// if (runFlag) adcData <= adcData + 16'd1; // Test data counter
		
		// Get the ADC data and shift 10-bit data to 16-bit (x256)
		if (runFlag) adcData <= adcDatabus << 6;
	end
end

endmodule