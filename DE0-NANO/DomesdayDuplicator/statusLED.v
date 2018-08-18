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
always @ (posedge clock, negedge nReset) begin
	if (!nReset) begin
		leds <= 8'b00000001;
		timer <= 16'd0;
	end else begin
		timer <= timer + 32'd1;
		if (timer >= 32'd6000000) begin
			leds <= {leds[6:0], leds[7]};
			timer <= 0;
		end
	end
end

endmodule