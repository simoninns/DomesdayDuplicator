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
	
	output reg empty_flag,
	output reg almostEmpty_flag,
	output reg halfFull_flag,
	output reg full_flag
);

wire rdEmpty;
wire [13:0] rdLevel;
wire rdFull;

// Control Async clear flag
wire aclr;
assign aclr = (nReset == 1'b1 && nReady == 1'b0) ? 1'b0 : 1'b1;

// Map in the FCFIFO IP function (8192 word FIFO)
IPfifo IPfifo0 (
	.aclr(aclr),							// Clear FIFO when in reset or not ready
	.data(inputData),						// Input data to FIFO
	.rdclk(outputClock),					// Output clock
	.rdreq(outputAck),					// Output acknowledge (read-ahead)
	.wrclk(inputClock),					// Input clock
	.wrreq(1'b1),							// Input request
	.q(outputData),						// Output data
	.rdempty(rdEmpty),					// Output empty flag
	.rdfull(rdFull),						// Output full flag
	.rdusedw(rdLevel)						// Output number of used words
);

// Process the flags aligned with the output clock
always @(posedge outputClock, negedge nReset)begin
	if (!nReset) begin
		empty_flag <= 1'b0;
		almostEmpty_flag <= 1'b0;
		halfFull_flag <= 1'b0;
		full_flag <= 1'b0;
	end else begin 
		empty_flag <= rdEmpty;
		full_flag <= rdFull;
		
		// Half full flag logic
		if (rdFull) begin
			halfFull_flag <= 1'b1;
		end else begin
			if (rdEmpty) begin
				halfFull_flag <= 1'b0;
			end else begin
				if (rdLevel > 14'd4096) begin
					halfFull_flag <= 1'b1;
				end else begin
					halfFull_flag <= 1'b0;
				end
			end
		end
		
		// Almost empty logic
		if (rdFull) begin
			almostEmpty_flag <= 1'b0;
		end else begin
			if (rdEmpty) begin
				almostEmpty_flag <= 1'b1;
			end else begin
				if (rdLevel < 14'd8) begin
					almostEmpty_flag <= 1'b1;
				end else begin
					almostEmpty_flag <= 1'b0;
				end
			end
		end
	end
end

endmodule