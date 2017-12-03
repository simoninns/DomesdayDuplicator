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
	input fx3_th1Ready,
	input fx3_th1Watermark,
	input fifo_DataReady,
	
	output fx3_nWrite,
	output fx3_addressBus
);

// State machine state definitions (4-bit 0-15)
reg [3:0]sm_currentState;
reg [3:0]sm_nextState;

parameter [3:0] state_idle					= 4'd1;
parameter [3:0] state_th0Wait				= 4'd2;
parameter [3:0] state_th0WaitWatermark	= 4'd3;
parameter [3:0] state_th0Send				= 4'd4;
parameter [3:0] state_th0Delay			= 4'd5;
parameter [3:0] state_th1Wait				= 4'd6;
parameter [3:0] state_th1WaitWatermark	= 4'd7;
parameter [3:0] state_th1Send				= 4'd8;
parameter [3:0] state_th1Delay			= 4'd9;

// Control the nWrite flag signal to the FX3
reg fx3_nWrite_flag;
assign fx3_nWrite = fx3_nWrite_flag;
wire inSendingState;
assign inSendingState = (sm_currentState == state_idle) ? 1'b1 : 1'b0; // 0 = Writing, 1 = not Writing

// Process the nWrite flag on the clock edge
always @(posedge fx3_clock, negedge fx3_nReset) begin
	if(!fx3_nReset)begin 
		fx3_nWrite_flag <= 1'b1;
	end else begin
		fx3_nWrite_flag <= inSendingState;
	end	
end

// Control the address bus signal to the FX3
// Select the current address using the current state
assign fx3_addressBus = ((sm_currentState == state_idle) ||
								(sm_currentState == state_th0Wait) ||
								(sm_currentState == state_th0WaitWatermark) ||
								(sm_currentState == state_th0Send) ||
								(sm_currentState == state_th0Delay)) ? 1'b0 : 1'b1;

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
reg fx3_th1Ready_flag;
reg fx3_th1Watermark_flag;
reg fx3_nReady_flag;

always @(posedge fx3_clock, negedge fx3_nReset)begin
	if(!fx3_nReset)begin 
		fx3_th0Ready_flag <= 1'b0;
		fx3_th0Watermark_flag <= 1'b0;
		fx3_th1Ready_flag <= 1'b0;
		fx3_th1Watermark_flag <= 1'b0;
		fx3_nReady_flag <= 1'b1;
	end else begin
		fx3_th0Ready_flag <= fx3_th0Ready;
		fx3_th0Watermark_flag <= fx3_th0Watermark;
		fx3_th1Ready_flag <= fx3_th1Ready;
		fx3_th1Watermark_flag <= fx3_th1Watermark;
		fx3_nReady_flag <= fx3_nReady;
	end	
end

// State machine transition control
always @(*)begin
	sm_nextState = sm_currentState;
	
	case(sm_currentState)
		
		// state_idle
		state_idle:begin
																// Don't start new transfer unless:
			if ((fx3_th0Ready_flag == 1'b1) &&		// FX3 Thread 0 is ready
				(fifo_DataReady == 1'b1) &&			// FIFO is indicating data ready
				(fx3_nReady_flag == 1'b0)) begin		// FX3 (not)Ready flag is set
				sm_nextState = state_th0WaitWatermark;
			end else begin
				sm_nextState = state_idle;
			end
		end
		
		// state_th0Wait
		state_th0Wait:begin
																// Don't continue transfer unless:
			if ((fx3_th0Ready_flag == 1'b1) &&		// FX3 Thread 0 is ready
				//(fifo_DataReady == 1'b1) &&			// FIFO is indicating data ready
				(fx3_nReady_flag == 1'b0)) begin		// FX3 (not)Ready flag is set
				sm_nextState = state_th0WaitWatermark;
			end else begin
				sm_nextState = state_idle;				// Can't proceed - go to idle
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
				// Watermark flag set - transition to state_th0Delay
				sm_nextState = state_th0Delay;
			end else begin
				// Continue sending data
				sm_nextState = state_th0Send; 
			end
		end
		
		// state_th0Delay - Clock delay before entering state_th1Wait state
		state_th0Delay:begin
			sm_nextState = state_th1Wait;
		end
		
		// state_th1Wait
		state_th1Wait:begin
																// Don't continue transfer unless:
			if ((fx3_th1Ready_flag == 1'b1) &&		// FX3 Thread 1 is ready
				//(fifo_DataReady == 1'b1) &&			// FIFO is indicating data ready
				(fx3_nReady_flag == 1'b0)) begin		// FX3 (not)Ready flag is set
				sm_nextState = state_th1WaitWatermark;
			end else begin
				sm_nextState = state_idle;				// Can't proceed - go to idle
			end
		end
		
		// state_th1WaitWatermark - Wait for thread 1 watermark flag to settle 
		state_th1WaitWatermark:begin
			if (fx3_th1Watermark_flag == 1'b1) begin
				sm_nextState = state_th1Send;
			end else begin
				sm_nextState = state_th1WaitWatermark;
			end
		end
		
		// state_th1Send - Stream data to thread 1, transistion on thread 1 watermark flag set
		state_th1Send:begin
			if (fx3_th1Watermark_flag == 1'b0) begin
				// Watermark flag set - transition to state_th1Delay
				sm_nextState = state_th1Delay;
			end else begin
				// Continue sending data
				sm_nextState = state_th1Send; 
			end
		end
		
		// state_th1Delay - Clock delay before entering state_th0Wait state
		state_th1Delay:begin
			sm_nextState = state_th0Wait;
		end

	endcase
end

endmodule