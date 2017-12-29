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
	input [9:0] adcData,
	
	output bufferError,
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
//
// Note: FIFO is 32767 10-bit words; therefore we can
// buffer 4 packets (of 8192 words) before overflow.
// Since the USB transfer is 16-bits, this is equivalent
// to 64Kbytes of buffering.
//
// Right now there is no error condition checking for a
// full FIFO.
wire [15:0] fifoUsedWords;
wire [9:0] fifoDataOut;

IPfifo IPfifo0 (
	.data(adcDataRead),		// [9:0] data in
	.rdclk(fx3Clk),			// FX3 clock
	.rdreq(readData),			// Read request
	.wrclk(adcClk),			// ADC clock
	.wrreq(collectData),		// Write request
	
	.q(fifoDataOut),			// [9:0] Data output
	.rdempty(rdEmpty),
	.rdfull(rdFull),
	.rdusedw(fifoUsedWords)	// [15:0] (read) used words
);

// Generate the data available flag
assign dataAvailable = (fifoUsedWords > 16'd8191) ? 1'b1 : 1'b0;

// Register to store test data value
reg [9:0] testData;
reg [9:0] adcDataRead;

// Collect data on the negative edge of the ADC clock
always @ (negedge adcClk, negedge nReset) begin
	if (!nReset) begin
		testData = 10'd0;
	end else begin
		if (collectData) begin
			// Test mode data generation
			testData = testData + 10'd1;
			
			// Select the data source
			if (testMode) begin
				// We are in test mode, use the test data
				adcDataRead = testData;
			end else begin
				// We are in normal mode, use the ADC data bus
				// Note: Here we correct the DC offset (see project notes for details)
				adcDataRead = adcData - 10'd65;;
			end
		end
	end
end

// Generate buffer error flag
//
// Note: This flag is only set if we are collecting data
//       The flag is not cleared until data collection stops
wire rdEmpty;
wire rdFull;
reg bufferError_flag;
assign bufferError = bufferError_flag;

always @ (posedge fx3Clk, negedge nReset) begin
	if (!nReset) begin
		bufferError_flag = 1'b0;
	end else begin
		if (collectData) begin
			// Collecting data, check for errors
			if ((rdFull) || (fifoUsedWords > 16'd32700)) begin
				bufferError_flag = 1'b1;
			end else begin
				bufferError_flag = 1'b0;
			end
		end else begin
			// Only flag errors if we are collecting data
			bufferError_flag = 1'b0;
		end
	end
end

endmodule