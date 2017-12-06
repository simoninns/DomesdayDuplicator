/************************************************************************
	
	dataGeneration.v
	Data generation module
	
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

module dataGenerator (
	input nReset,
	input inclk,
	input collectData,
	input readData,
	input testMode,
	
	output dataAvailable,
	output [15:0] dataOut
);

// Convert the 10-bit unsigned data to  16-bit signed data
convertTenToSixteenBits convertTenToSixteenBits0 (
	.nReset(nReset),
	.inclk(inclk),
	.inputData(testData),
	
	.outputData(dataOut)
);

// Register to store test data value
reg [9:0] testData;

// TEMP: Generate test data (always in test mode for now)
always @ (posedge inclk, negedge nReset) begin
	if (!nReset) begin
		testData <= 10'd0;
	end else begin
		if (collectData) begin
			if (readData) begin
				// Test mode data generation
				testData = testData + 10'd1;
			end
		end
	end
end

// Simulate dataAvailable
reg [31:0] simCounter;
reg dataAvailable_flag;
assign dataAvailable = dataAvailable_flag;

always @ (posedge inclk, negedge nReset) begin
	if (!nReset) begin
		simCounter <= 10'd0;
		dataAvailable_flag = 1'b0;
	end else begin
		simCounter = simCounter + 32'd1;
		
		if (simCounter > 1000000 && simCounter < 1000010) begin
			dataAvailable_flag = 1'b1;
		end else begin
			dataAvailable_flag = 1'b0;
		end
		
		if (simCounter > 2000000) simCounter = 32'd0;
	end
end

endmodule