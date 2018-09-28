/************************************************************************
	
	statusLED.v
	Status LED control module
	
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

module statusLED (
	input nReset,
	input clock,
	
	// Outputs
	output reg [7:0] leds
);

// Control the status LEDs
reg [31:0] timer;
reg direction;
reg [3:0] position; // 4-bit value 0-15

always @ (posedge clock, negedge nReset) begin
	if (!nReset) begin
		leds <= 8'b00000001;
		timer <= 16'd0;
		direction <= 1'b1;
		position <= 4'd0;
	end else begin
		timer <= timer + 32'd1;
		// Wait for the timer to elapse before updating LEDs
		if (timer >= 32'd4000000) begin
			case(position)
				4'd0:leds <= 8'b00000001;
				4'd1:leds <= 8'b00000010;
				4'd2:leds <= 8'b00000100;
				4'd3:leds <= 8'b00001000;
				4'd4:leds <= 8'b00010000;
				4'd5:leds <= 8'b00100000;
				4'd6:leds <= 8'b01000000;
				4'd7:leds <= 8'b10000000;
			endcase
		
			if (direction) begin
				if (position == 4'd7) begin
					position <= 4'd6;
					direction <= 1'b0;
				end else begin
					position <= position + 4'd1;
				end
			end else begin
				if (position == 4'd0) begin
					position <= 4'd1;
					direction <= 1'b1;
				end else begin
					position <= position - 4'd1;
				end
			end
			
			// Reset timer
			timer <= 16'd0;
		end
	end
end

endmodule