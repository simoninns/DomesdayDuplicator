/************************************************************************
	
	convertTenToSixteenBits.v
	ADC data conversion module
	
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

module convertTenToSixteenBits (
	input nReset,
	input inclk,
	input [9:0] inputData,
	
	output [15:0] outputData
);

// This logic converts the 10-bit unsigned inputData
// into a 16-bit signed outputData
wire [14:0] conversionData;
assign conversionData[9:0] = inputData;
assign conversionData[14:10] = 5'b0;
assign outputData = ($signed(conversionData) - 16'sd512) * 16'sd64;

endmodule