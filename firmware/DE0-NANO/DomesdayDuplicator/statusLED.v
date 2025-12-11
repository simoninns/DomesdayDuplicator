/************************************************************************
	
	statusLED.v
	Status LED control module
	
	Domesday Duplicator - LaserDisc RF sampler
	SPDX-FileCopyrightText: 2018-2025 Simon Inns
	SPDX-License-Identifier: GPL-3.0-or-later
	
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