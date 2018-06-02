/************************************************************************
	
	fx3StateMachine.v
	FX3 State-Machine module
	
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

module fx3StateMachine (
	input nReset,
	input fx3_clock,
	input readData,
	
	output fx3isReading
);

// State machine logic ---------------------------------------------------

// State machine state definitions (4-bit 0-15)
reg [3:0]sm_currentState;
reg [3:0]sm_nextState;

parameter [3:0] state_waitForRequest	= 4'd01;
parameter [3:0] state_sendPacket			= 4'd02;

// Set state to state_idle on reset - or assign the next state
always @(posedge fx3_clock, negedge nReset) begin
	if(!nReset) begin 
		sm_currentState <= state_waitForRequest;
	end else begin
		sm_currentState <= sm_nextState;
	end	
end

// Ensure that the readData signal is only read
// on the FX3 clock edge
reg readData_flag;

always @(posedge fx3_clock, negedge nReset) begin
	if(!nReset) begin 
		readData_flag <= 1'b0;
	end else begin
		readData_flag <= readData;
	end	
end

// Counter for the sendPacket state
// Here we should send 8192 words to the FX3
reg [15:0] wordCounter;

always @(posedge fx3_clock, negedge nReset) begin
	if (!nReset) begin
		wordCounter = 16'd0;
	end else begin
		if (sm_currentState == state_sendPacket) begin
			wordCounter = wordCounter + 16'd1;
		end else begin
			wordCounter = 16'd0;
		end
	end
end

// Generate fx3isReading flag
assign fx3isReading = (sm_currentState == state_sendPacket) ? 1'b1 : 1'b0;

// State machine transition logic
always @(*)begin
	sm_nextState = sm_currentState;
	
	case(sm_currentState)
		
		// state_waitForRequest (waits for the FX3 to request a packet)
		state_waitForRequest:begin
			// Is the GPIF reading data?
			if (readData_flag == 1'b1 && wordCounter == 16'd0) begin
				sm_nextState = state_sendPacket;
			end else begin
				// GPIF not ready... wait
				sm_nextState = state_waitForRequest;
			end
		end
		
		// state_sendPacket (sends a packet of 8192 words to the FX3)
		state_sendPacket:begin
			if (wordCounter == 16'd8191) begin
				// Packet send, go back to waiting
				sm_nextState = state_waitForRequest;
			end else begin
				// Continue sending packet
				sm_nextState = state_sendPacket;
			end
		end
		
	endcase
end


endmodule