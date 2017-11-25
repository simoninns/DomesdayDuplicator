/************************************************************************
	
	fx3StateMachine.v
	FX3 communication finite state-machine module
	
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

module fx3StateMachine(
	input fx3_clock,
	input fx3_nReset,
	input fx3_nReady,
	input fx3_th0Ready,
	input fx3_th0Watermark,
	input fifoAlmostEmpty,
	input fifoHalfFull,
	
	output fx3_nWrite,
	output fx3_nShort
);

// State machine state definitions (3-bit 0-7)
reg [2:0]sm_currentState;
reg [2:0]sm_nextState;

parameter [2:0] state_th0Wait				= 3'd1;
parameter [2:0] state_th0WaitWatermark	= 3'd2;
parameter [2:0] state_th0Send				= 3'd3;
parameter [2:0] state_th0Delay			= 3'd4;
parameter [2:0] state_shortPacket		= 3'd5;

// Control the nWrite flag signal to the FX3
reg fx3_nWrite_flag;
assign fx3_nWrite = fx3_nWrite_flag;
wire inSendingState;
assign inSendingState = (sm_currentState == state_th0Send || sm_currentState == state_shortPacket) ? 1'b0 : 1'b1; // 0 = Writing, 1 = not Writing

// Process the nWrite flag on the clock edge
always @(posedge fx3_clock, negedge fx3_nReset) begin
	if(!fx3_nReset)begin 
		fx3_nWrite_flag <= 1'b1;
	end else begin
		fx3_nWrite_flag <= inSendingState;
	end	
end

// Control the nShort flag signal to the FX3
reg fx3_nShort_flag;
assign fx3_nShort = fx3_nShort_flag;
wire inShortState;
assign inShortState = (sm_currentState == state_shortPacket) ? 1'b0 : 1'b1; // 0 = Short packet, 1 = not Short packet

// Process the nShort flag on the clock edge
always @(posedge fx3_clock, negedge fx3_nReset) begin
	if(!fx3_nReset)begin 
		fx3_nShort_flag <= 1'b1;
	end else begin
		fx3_nShort_flag <= inShortState;
	end	
end

// Set state machine to idle on reset condition
// or assign next state as current when running
always @(posedge fx3_clock, negedge fx3_nReset) begin
	if(!fx3_nReset)begin 
		sm_currentState <= state_th0Wait;
	end else begin
		sm_currentState <= sm_nextState;
	end	
end

// Ensure that the thread ready, watermark and nReady flags
// are only read on the clock edge
reg fx3_th0Ready_flag;
reg fx3_th0Watermark_flag;
reg fx3_nReady_flag;

always @(posedge fx3_clock, negedge fx3_nReset)begin
	if(!fx3_nReset)begin 
		fx3_th0Ready_flag <= 1'b0;
		fx3_th0Watermark_flag <= 1'b0;
		fx3_nReady_flag <= 1'b1;
	end else begin
		fx3_th0Ready_flag <= fx3_th0Ready;
		fx3_th0Watermark_flag <= fx3_th0Watermark;
		fx3_nReady_flag <= fx3_nReady;
	end	
end

// Note: The short packet support is included below, but it's very
// likely that the timing is incorrect and will result in lost data.
//
// Right now (using the Cypress test tools) there doesn't seem to be
// any support for short packets, so testing isn't possible until the
// linux application is up and running.
//
// In testing (with the present FIFO settings) the FIFO never gets to
// the 'nearly empty' watermark; so the functionality is not currently
// used (and data transfer is good).

// State machine transition control
always @(*)begin
	sm_nextState = sm_currentState;
	
	case(sm_currentState)
		
		// state_th0WaitReady - Wait for thread 0 ready flag,
		// FIFO to be not almost empty and nReady
		state_th0Wait:begin
																// Don't start transfer unless:
			if ((fx3_th0Ready_flag == 1'b1) &&		// Thread 0 is ready
				(fifoHalfFull == 1'b1) &&				// FIFO is at least half-full
				(fx3_nReady_flag == 1'b0)) begin		// Ready flag is set
				sm_nextState = state_th0WaitWatermark;
			end else begin
				sm_nextState = state_th0Wait;
			end
		end
		
		// state_th0WaitWatermark - Wait for thread 0 watermark flag to settle 
		state_th0WaitWatermark:begin
			if (fx3_th0Watermark_flag == 1'b1) begin
				sm_nextState = state_th0Send;
			end else begin
				sm_nextState = state_th0WaitWatermark;
			end
		end
		
		// state_th0Send - Stream data to thread 0, transistion on thread 0 watermark flag set
		state_th0Send:begin
			if (fx3_th0Watermark_flag == 1'b0) begin
				// Watermark flag set - transition to thread 0 wait for ready
				sm_nextState = state_th0Delay;
			end else begin
				// Check that FIFO still has sufficient data
				if (fifoAlmostEmpty == 1'b0) begin
					// FIFO has sufficient data, continue
					sm_nextState = state_th0Send; 
				end else begin
					// FIFO is almost empty... commit a short packet
					sm_nextState = state_shortPacket; 
				end
			end
		end
		
		// state_th0Delay - Clock delay before beginning the write cycle again 
		state_th0Delay:begin
			sm_nextState = state_th0Wait;
		end
		
		// state_shortPacket - Commit a short packet (FIFO level is too low)
		state_shortPacket:begin
			sm_nextState = state_th0Wait;
		end

	endcase
end

endmodule