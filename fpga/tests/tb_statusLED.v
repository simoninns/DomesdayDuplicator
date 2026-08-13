/************************************************************************

	tb_statusLED.v
	Testbench for the status LED control module (T3)

	Domesday Duplicator - LaserDisc RF sampler
	SPDX-FileCopyrightText: 2018-2025 Simon Inns
	SPDX-License-Identifier: GPL-3.0-or-later

	The LEDs are the only thing the board shows without a host attached, so
	"is the gateware loaded and clocked?" is answered by whether this pattern
	is running. That makes the pattern itself a diagnostic, and worth pinning.

	timerLimit is overridden to 4 here. At the design value of 4,000,000 one
	full cycle of the pattern is 56 million clock edges, which is minutes of
	simulation; the logic under test is the counter and the direction reversal,
	neither of which depends on the limit.

************************************************************************/

`timescale 1ns / 1ps

module tb_statusLED;

	reg nReset;
	reg clock;
	wire [7:0] leds;

	integer errors;
	integer i;
	integer step;

	localparam TIMER_LIMIT = 4;

	// A step happens on the edge after the timer reaches the limit, so the
	// period is one cycle longer than the limit itself.
	localparam CYCLES_PER_STEP = TIMER_LIMIT + 1;

	// The pattern bounces between the end LEDs without repeating either, so
	// one full period is 14 steps rather than 16.
	localparam PATTERN_STEPS = 14;

	reg [7:0] expected [0:PATTERN_STEPS-1];
	reg [7:0] held;

	statusLED #(.timerLimit(TIMER_LIMIT)) dut (
		.nReset(nReset),
		.clock(clock),
		.leds(leds)
	);

	// 60 MHz FX3 system clock — 16.667 ns period
	initial clock = 1'b0;
	always #8.333 clock = ~clock;

	task check;
		input [31:0] got;
		input [31:0] want;
		input [255:0] what;
		begin
			if (got !== want) begin
				$display("FAIL: %0s: got %b, expected %b (t=%0t)", what, got, want, $time);
				errors = errors + 1;
			end
		end
	endtask

	initial begin
		errors = 0;

		// Up the bus one LED at a time, then back down, turning round without
		// showing either end twice.
		expected[0]  = 8'b00000001;
		expected[1]  = 8'b00000010;
		expected[2]  = 8'b00000100;
		expected[3]  = 8'b00001000;
		expected[4]  = 8'b00010000;
		expected[5]  = 8'b00100000;
		expected[6]  = 8'b01000000;
		expected[7]  = 8'b10000000;
		expected[8]  = 8'b01000000;
		expected[9]  = 8'b00100000;
		expected[10] = 8'b00010000;
		expected[11] = 8'b00001000;
		expected[12] = 8'b00000100;
		expected[13] = 8'b00000010;

		nReset = 1'b0;
		@(posedge clock);
		#1;

		// --- Reset --------------------------------------------------------
		// A single lit LED, not all-on and not all-off: both of those are what
		// an unconfigured or unclocked board looks like, so the reset state
		// has to be distinguishable from them.
		check(leds, 8'b00000001, "leds while held in reset");
		nReset = 1'b1;

		// --- Two full periods ----------------------------------------------
		// Two, not one: the direction flag has to reverse at both ends, and a
		// single period only exercises the second reversal once.
		// The value the LEDs must hold while the timer counts up to the next
		// step. Before the first step that is the reset value, which happens to
		// be what step 0 sets as well.
		held = 8'b00000001;

		for (step = 0; step < PATTERN_STEPS * 2; step = step + 1) begin
			// The LEDs must hold their value between steps — a pattern that
			// updated every clock would be a blur on the board and would still
			// pass a test that only sampled at the step boundaries.
			for (i = 0; i < CYCLES_PER_STEP - 1; i = i + 1) begin
				@(posedge clock);
				#1 check(leds, held, "leds held between steps");
			end

			@(posedge clock);
			#1 check(leds, expected[step % PATTERN_STEPS], "leds at step");
			held = expected[step % PATTERN_STEPS];
		end

		// --- Reset mid-pattern ---------------------------------------------
		// Whatever the pattern was doing, a reset puts it back to a known LED
		// and a known direction, so a reset board always starts by walking up.
		nReset = 1'b0;
		#1 check(leds, 8'b00000001, "leds are cleared asynchronously by reset");

		@(posedge clock);
		#1 nReset = 1'b1;

		for (i = 0; i < CYCLES_PER_STEP - 1; i = i + 1) @(posedge clock);
		@(posedge clock);
		#1 check(leds, expected[0], "first step after reset restarts the pattern");

		for (i = 0; i < CYCLES_PER_STEP; i = i + 1) @(posedge clock);
		#1 check(leds, expected[1], "second step after reset walks up, not down");

		if (errors == 0) $display("tb_statusLED: PASS");
		else $display("tb_statusLED: FAIL (%0d errors)", errors);

		if (errors != 0) $fatal(1, "tb_statusLED failed");
		$finish;
	end

endmodule
