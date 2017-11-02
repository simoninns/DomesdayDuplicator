/************************************************************************
	
	adcDataConvert.v
	ADC data conversion module
	
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

module adcDataConvert (
	input clock,
	input nReset,
	input [9:0] inputData,
	
	output [15:0] outputData
);

// Convert the 10-bit unsigned input data to 16-bit signed data
// Note: 10-bit values are scaled to full 16-bit range
wire [14:0] wideInputData;
assign wideInputData[9:0] = inputData;
assign wideInputData[14:10] = 5'b0;

assign outputData = ($signed(wideInputData) - 16'sd512) * 16'sd64;

endmodule