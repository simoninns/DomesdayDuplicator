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
	input writeClock,
	input readClock,
	input isWriting,
	input isReading,
	input [9:0] dataIn,
	
	output reg bufferOverflow,
	output reg dataAvailable,
	output [9:0] dataOut
);

// FIFO buffer size in words
localparam bufferSize = 14'd8191; // 0 - 8191 = 8192 words

// "Ping-pong" buffer storing 8192 10-bit words per buffer
reg currentWriteBuffer; // 0 = write to ping buffer read from pong,
								// 1 = write to pong buffer read from ping

// Define various buffer signals and values

// Buffer signals (write clock sync'd)
wire pingAsyncClear_wr;
wire pongAsyncClear_wr;
wire pingEmptyFlag_wr;
wire pongEmptyFlag_wr;
wire [13:0] pingUsedWords_wr;
wire [13:0] pongUsedWords_wr;

// Buffer signals (read clock sync'd)
wire pingEmptyFlag_rd;
wire pongEmptyFlag_rd;
wire [13:0] pingUsedWords_rd;
wire [13:0] pongUsedWords_rd;

// Data out buses
wire [9:0] pingDataOut;
wire [9:0] pongDataOut;

// Define the ping buffer (0) - 8192 10-bit words
IPfifo pingBuffer (
	.aclr(pingAsyncClear_wr),
	.data(pingDataIn),				// 10-bit [9:0]
	.rdclk(readClock),
	.rdreq(pingReadRequest),
	.wrclk(writeClock),
	.wrreq(pingWriteRequest),
	.q(pingDataOut),					// 10-bit [9:0]
	.rdempty(pingEmptyFlag_rd),
	.rdusedw(pingUsedWords_rd),	// 14-bit [13:0]
	.wrempty(pingEmptyFlag_wr),
	.wrusedw(pingUsedWords_wr)		// 14-bit [13:0]
);

// Define the pong buffer (1) - 8192 10-bit words
IPfifo pongBuffer (
	.aclr(pongAsyncClear_wr),
	.data(pongDataIn),				// 10-bit [9:0]
	.rdclk(readClock),
	.rdreq(pongReadRequest),
	.wrclk(writeClock),
	.wrreq(pongWriteRequest),
	.q(pongDataOut),					// 10-bit [9:0]
	.rdempty(pongEmptyFlag_rd),
	.rdusedw(pongUsedWords_rd),	// 14-bit [13:0]
	.wrempty(pongEmptyFlag_wr),
	.wrusedw(pongUsedWords_wr)		// 14-bit [13:0]
);


// Route the control signals according to the currently selected write buffer
wire [9:0] pingDataIn;
wire [9:0] pongDataIn;
wire pingReadRequest;
wire pongReadRequest;
wire pingWriteRequest;
wire pongWriteRequest;

// if current write buffer = ping then send data to ping buffer
// else send data to pong buffer
assign pingDataIn = currentWriteBuffer ? 10'b0 : dataIn;
assign pongDataIn = currentWriteBuffer ? dataIn : 10'b0; 

// if current write buffer = ping the dataOut = pong buffer else dataOut = pingBuffer
assign dataOut = currentWriteBuffer ? pingDataOut : pongDataOut; 

// If current write buffer = ping then read from pong else read from ping
assign pingReadRequest = currentWriteBuffer ? isReading : 1'b0;
assign pongReadRequest = currentWriteBuffer ? 1'b0 : isReading;

// if current write buffer = ping then write to ping else write to pong
assign pingWriteRequest = currentWriteBuffer ? 1'b0 : isWriting;
assign pongWriteRequest = currentWriteBuffer ? isWriting : 1'b0;

// Define registers for the async clear flags and map to registers
reg pingAsyncClear_reg;
reg pongAsyncClear_reg;
assign pingAsyncClear_wr = pingAsyncClear_reg;
assign pongAsyncClear_wr = pongAsyncClear_reg;

// FIFO Write-side logic (controls switching between ping and pong buffers)
always @ (posedge writeClock, negedge nReset) begin
	if (!nReset) begin
		// On reset default to the ping buffer and clear the buffers
		currentWriteBuffer <= 1'b0;
		bufferOverflow <= 1'b0;
	end else begin
		// If we're not writing, clear the buffer overflow flag
		if (!isWriting) begin
			// Reset the flags
			bufferOverflow <= 1'b0;
		end
	
		// Which buffer is being written to?
		if (currentWriteBuffer) begin
			// Current write buffer is pong
			
			// Is the pong buffer nearly full?
			if (pongUsedWords_wr == bufferSize - 2) begin
				// Check that the ping buffer has been emptied...
				if (!pingEmptyFlag_wr) begin
					// Flag an overflow error
					bufferOverflow <= 1'b1;
					
					// Set the ping buffer async clear (empty the ping buffer)
					pingAsyncClear_reg <= 1'b1;
				end
			end
			
			// Is the pong buffer 1 word from full?
			if (pongUsedWords_wr == bufferSize - 1) begin
				// Reset the ping buffer async clear
				pingAsyncClear_reg <= 1'b0;
				
				// Switch to the ping buffer
				currentWriteBuffer <= 1'b0;
			end
		end else begin
			// Current write buffer is ping
			
			// Is the ping buffer nearly full?
			if (pingUsedWords_wr == bufferSize - 2) begin
				// Check that the pong buffer has been emptied...
				if (!pongEmptyFlag_wr) begin
					// Flag an overflow error
					bufferOverflow <= 1'b1;
					
					// Set the pong buffer async clear (empty the pong buffer)
					pongAsyncClear_reg <= 1'b1;
				end
			end
			
			// Is the ping buffer 1 word from full?
			if (pingUsedWords_wr == bufferSize - 1) begin
				// Reset the pong buffer async clear
				pongAsyncClear_reg <= 1'b0;

				// Switch to the pong buffer
				currentWriteBuffer <= 1'b1;
			end
		end
	end
end

// FIFO read-side logic
// Control the data available flag (on the read side)
// Note: This is responsible for setting the flag when
// data is available and clearing the flag once all
// the available data has been read.
always @ (posedge readClock, negedge nReset) begin
	if (!nReset) begin
		// On reset default to data unavailable
		dataAvailable <= 1'b0;
	end else begin
		// Which buffer is being read from to?
		if (currentWriteBuffer) begin
			// Reading from ping buffer
			
			// Is the ping buffer empty?
			if (pingEmptyFlag_rd) begin
				dataAvailable <= 1'b0;
			end
			
			// Is the ping buffer full?
			if (pingUsedWords_rd == bufferSize) begin
				dataAvailable <= 1'b1;
			end
		end else begin
			// Reading from pong buffer
			
			// Is the pong buffer empty?
			if (pongEmptyFlag_rd) begin
				dataAvailable <= 1'b0;
			end
			
			// Is the pong buffer full?
			if (pongUsedWords_rd == bufferSize) begin
				dataAvailable <= 1'b1;
			end
		end
	end
end

endmodule