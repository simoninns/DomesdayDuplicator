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
	input adcClk,
	input fx3Clk,
	input collectData,
	input readData,
	input testMode,
	
	output dataAvailable,
	output [15:0] dataOut
);

// Convert the 10-bit unsigned data from the FIFO
// to 16-bit signed data ready for the FX3 data bus
convertTenToSixteenBits convertTenToSixteenBits0 (
	.nReset(nReset),
	.inclk(fx3Clk),
	.inputData(fifoDataOut),
	
	.outputData(dataOut)
);

// Dual-clock FIFO IP
wire [15:0] fifoUsedWords;
wire [9:0] fifoDataOut;

IPfifo IPfifo0 (
	.data(adcData),			// [9:0] data in
	.rdclk(fx3Clk),			// FX3 clock
	.rdreq(readData),			// Read request
	.wrclk(adcClk),			// ADC clock
	.wrreq(collectData),		// Write request
	
	.q(fifoDataOut),			// [9:0] Data output
	.rdusedw(fifoUsedWords)	// [15:0] (read) used words
);

// Generate the data available flag
assign dataAvailable = (fifoUsedWords > 16'd8191) ? 1'b1 : 1'b0;

// Register to store test data value
reg [9:0] adcData;

// TEMP: Generate test data (always in test mode for now)
always @ (posedge adcClk, negedge nReset) begin
	if (!nReset) begin
		adcData <= 10'd0;
	end else begin
		if (collectData) begin
			// Test mode data generation
			adcData = adcData + 10'd1;
		end
	end
end

endmodule