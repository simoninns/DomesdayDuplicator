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
	input fifo_DataReady,
	
	output fx3_nWrite
);

// State machine state definitions (3-bit 0-7)
reg [2:0]sm_currentState;
reg [2:0]sm_nextState;

parameter [2:0] state_th0Wait				= 3'd1;
parameter [2:0] state_th0WaitWatermark	= 3'd2;
parameter [2:0] state_th0Send				= 3'd3;
parameter [2:0] state_th0Delay			= 3'd4;

// Control the nWrite flag signal to the FX3
reg fx3_nWrite_flag;
assign fx3_nWrite = fx3_nWrite_flag;
wire inSendingState;
assign inSendingState = (sm_currentState == state_th0Send) ? 1'b0 : 1'b1; // 0 = Writing, 1 = not Writing

// Process the nWrite flag on the clock edge
always @(posedge fx3_clock, negedge fx3_nReset) begin
	if(!fx3_nReset)begin 
		fx3_nWrite_flag <= 1'b1;
	end else begin
		fx3_nWrite_flag <= inSendingState;
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

// State machine transition control
always @(*)begin
	sm_nextState = sm_currentState;
	
	case(sm_currentState)
		
		// state_th0WaitReady
		state_th0Wait:begin
																// Don't start transfer unless:
			if ((fx3_th0Ready_flag == 1'b1) &&		// FX3 Thread 0 is ready
				(fifo_DataReady == 1'b1) &&			// FIFO is indicating data ready
				(fx3_nReady_flag == 1'b0)) begin		// FX3 (not)Ready flag is set
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
				// Watermark flag set - transition to state_th0WaitReady
				sm_nextState = state_th0Delay;
			end else begin
				// Continue sending data
				sm_nextState = state_th0Send; 
			end
		end
		
		// state_th0Delay - Clock delay before entering state_th0WaitReady state
		state_th0Delay:begin
			sm_nextState = state_th0Wait;
		end

	endcase
end

endmodule