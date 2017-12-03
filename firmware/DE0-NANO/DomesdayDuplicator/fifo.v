/************************************************************************
	
	fifo.v
	Dual-clock FIFO wrapper module
	
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

module fifo (
	input [9:0] inputData,
	input inputClock,
	input nReset,
	
	input outputClock,
	input outputAck,
	
	input nReady,
	
	output [9:0] outputData,
	
	output reg dataReadyFlag,
	output reg errorFlag
);

wire rdEmpty;
wire rdFull;
wire [15:0] rdLevel;
wire [9:0] fifoOutputData;

// (not) Collect data flag is 0 when we should be collecting data
// nCollectData is 0 when out of reset and ready
wire nCollectData;
assign nCollectData = (nReset == 1'b1 && nReady == 1'b0) ? 1'b0 : 1'b1;

// Data output is only valid when FIFO is not in reset
assign outputData = (nCollectData) ? 10'b0 : fifoOutputData;

// Map in the DCFIFO IP function
IPfifo IPfifo0 (
	.aclr(nCollectData),					// Clear FIFO when in reset or not ready
	.data(inputData),						// Input data to FIFO
	.rdclk(outputClock),					// Output clock
	.rdreq(outputAck),					// Output acknowledge (read-ahead)
	.wrclk(inputClock),					// Input clock
	.wrreq(!nCollectData),				// Input request (only input when collecting data)
	.q(fifoOutputData),					// Output data
	.rdempty(rdEmpty),					// Output empty flag
	.rdfull(rdFull),						// Output full flag
	.rdusedw(rdLevel)						// Output number of used words
);

// Note:
//
// FX3 DMA buffer size per thread is 16Kbytes
// The FX3 is supporting 2 threads, therefore the complete DMA data buffer
// is 32Kbytes.
//
// ADC data is 10-bits, this is scaled to 16-bits before sending to the FX3
// This means that the FX3 DMA buffer is 8K words
//
// Data transmission will only start when the FIFO buffer contains
// enough data to fulfil a complete transfer (i.e. the size of the FX3 DMA
// buffer).
//
// Data transmission to the FX3 is at twice the data rate of ADC data
// collection.  To ensure the FIFO never gets full, the FIFO size is set
// to 32K words (32767 10-bit words) - equivalent to 64Kbytes

// Note:
//
// rdLevel (rdusedw) is 16 bits (15 bits + 1)
// rdLevel should not be used as 'full flag' - refer to DCFIFO
// documentation for details.

// Process the flags aligned with the output clock
always @(posedge outputClock, negedge nReset)begin
	if (!nReset) begin
		dataReadyFlag <= 1'b0;
		errorFlag <= 1'b0;
	end else begin
		// Error flag is set when FIFO is full (data is being lost)
		errorFlag <= rdFull;
		
		// Flag data ready when 8K words of data are available
		if (rdLevel > 16'd8191) begin
			// Data is ready
			dataReadyFlag <= 1'b1;
		end else begin
			// Data is not ready
			dataReadyFlag <= 1'b0;
		end
	end
end

endmodule