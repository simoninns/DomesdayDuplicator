/************************************************************************
	
	buffer.v
	Data buffer module
	
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

module buffer (
	input nReset,
	input inputClock,
	input outputClock,
	input collectData,
	input readData,
	input [9:0] dataIn,
	
	output bufferError,
	output dataAvailable,
	output [9:0] dataOut
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
	.data(dataIn),	// [9:0] data in
	.rdclk(outputClock),		// Output clock
	.rdreq(readData),			// Read request
	.wrclk(inputClock),		// Input clock
	.wrreq(collectData),		// Write request
	
	.q(dataOut),				// [9:0] Data output
	.rdempty(rdEmpty),
	.rdfull(rdFull),
	.rdusedw(fifoUsedWords)	// [15:0] (read) used words
);

// Generate the data available flag
assign dataAvailable = (fifoUsedWords > 16'd8191) ? 1'b1 : 1'b0;

// Generate buffer error flag
//
// Note: This flag is only set if we are collecting data
//       The flag is not cleared until data collection stops
wire rdEmpty;
wire rdFull;
reg bufferError_flag;
assign bufferError = bufferError_flag;

always @ (posedge outputClock, negedge nReset) begin
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