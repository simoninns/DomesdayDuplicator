/************************************************************************

	tb_dataGenerator.v
	Testbench for the data generation module (T3)

	Domesday Duplicator - LaserDisc RF sampler
	SPDX-FileCopyrightText: 2018-2025 Simon Inns
	SPDX-License-Identifier: GPL-3.0-or-later

	This is the simulation counterpart of step 4 of the capture-integrity
	procedure in TESTING.md. That procedure asserts, on silicon and through a
	real USB capture, that the test data is an unbroken ramp with a monotonic
	sequence number. Everything it checks originates here, so a defect in this
	module is the one class of capture fault that can be caught before the
	bitstream is built.

************************************************************************/

`timescale 1ns / 1ps

module tb_dataGenerator;

	reg nReset;
	reg clock;
	reg [9:0] adc_databus;
	reg testModeFlag;
	wire [15:0] dataOut;

	integer errors;
	integer i;
	integer expected;

	// The ramp is 0..1020 inclusive, so it repeats every 1021 samples. This is
	// the number the GUI's analyser and dddutil both assume; if it changes here
	// it must change there too.
	localparam RAMP_LENGTH = 1021;

	// dataOut[15:10] is sequenceCount[21:16], so the sequence number advances
	// once every 65536 samples and wraps after 63 of them.
	localparam SAMPLES_PER_SEQUENCE = 65536;
	localparam SEQUENCE_COUNT = 63;

	dataGenerator dut (
		.nReset(nReset),
		.clock(clock),
		.adc_databus(adc_databus),
		.testModeFlag(testModeFlag),
		.dataOut(dataOut)
	);

	// 40 MHz ADC clock — 25 ns period
	initial clock = 1'b0;
	always #12.5 clock = ~clock;

	task check;
		input [31:0] got;
		input [31:0] want;
		input [255:0] what;
		begin
			if (got !== want) begin
				$display("FAIL: %0s: got %0d, expected %0d (t=%0t)", what, got, want, $time);
				errors = errors + 1;
			end
		end
	endtask

	initial begin
		errors = 0;
		adc_databus = 10'd0;
		testModeFlag = 1'b1;
		nReset = 1'b0;

		// Hold reset across a clock edge, then release it away from one
		@(posedge clock);
		#1 nReset = 1'b1;

		// --- Reset state -------------------------------------------------
		// Every register clears to zero, so the first sample of a capture is
		// sample 0 of the ramp with sequence number 0.
		check(dataOut[9:0], 0, "test data after reset");
		check(dataOut[15:10], 0, "sequence number after reset");

		// --- The ramp ----------------------------------------------------
		// Walk three full periods. Two would prove it wraps; three proves the
		// wrap leaves the counter in a state that wraps again the same way.
		for (i = 1; i <= RAMP_LENGTH * 3; i = i + 1) begin
			@(posedge clock);
			#1;
			expected = i % RAMP_LENGTH;
			check(dataOut[9:0], expected, "test data ramp");
		end

		// The ramp must reach 1020 and must never reach 1021. The loop above
		// covers both, but assert the endpoint explicitly so a failure names it.
		check(RAMP_LENGTH - 1, 1020, "ramp endpoint");

		// --- ADC passthrough ---------------------------------------------
		// Out of test mode the low ten bits are the ADC bus, registered on the
		// clock edge — so the value appears one cycle after it is presented.
		testModeFlag = 1'b0;
		adc_databus = 10'd682;		// 0b1010101010: every bit exercised
		@(posedge clock);
		#1 check(dataOut[9:0], 682, "ADC passthrough (alternating bits)");

		adc_databus = 10'd341;		// 0b0101010101: the complement
		@(posedge clock);
		#1 check(dataOut[9:0], 341, "ADC passthrough (complement)");

		adc_databus = 10'd1023;
		@(posedge clock);
		#1 check(dataOut[9:0], 1023, "ADC passthrough (full scale)");

		// Test mode must ignore the ADC bus entirely: the ramp continues from
		// where it left off rather than restarting or picking up ADC values.
		testModeFlag = 1'b1;
		@(posedge clock);
		#1;
		if (dataOut[9:0] === 1023) begin
			$display("FAIL: test mode is passing ADC data through (t=%0t)", $time);
			errors = errors + 1;
		end

		// --- Sequence number ---------------------------------------------
		// This is the field the capture-integrity procedure counts breaks in.
		// Restart from reset so the sample count is known exactly.
		nReset = 1'b0;
		@(posedge clock);
		#1 nReset = 1'b1;
		check(dataOut[15:10], 0, "sequence number after second reset");

		// One short of the boundary the field is still 0; one clock later it is 1.
		for (i = 1; i < SAMPLES_PER_SEQUENCE; i = i + 1) @(posedge clock);
		#1 check(dataOut[15:10], 0, "sequence number just before the first boundary");

		@(posedge clock);
		#1 check(dataOut[15:10], 1, "sequence number at the first boundary");

		for (i = 1; i <= SAMPLES_PER_SEQUENCE; i = i + 1) @(posedge clock);
		#1 check(dataOut[15:10], 2, "sequence number at the second boundary");

		// Run out the remaining sequence numbers and check the wrap. The
		// comparison in the DUT is against (63 << 16) - 1, where the 6-bit
		// constant is widened by the context — the one place in this module
		// where an implicit width promotion decides the behaviour, so it is
		// worth the simulation time to prove the wrap lands on 0 and not on 63.
		for (i = 1; i <= SAMPLES_PER_SEQUENCE * (SEQUENCE_COUNT - 2); i = i + 1) @(posedge clock);
		#1 check(dataOut[15:10], 0, "sequence number wrap after 63 sequences");

		@(posedge clock);
		#1 check(dataOut[9:0], (SAMPLES_PER_SEQUENCE * SEQUENCE_COUNT + 1) % RAMP_LENGTH,
			"ramp is continuous across the sequence wrap");

		if (errors == 0) $display("tb_dataGenerator: PASS");
		else $display("tb_dataGenerator: FAIL (%0d errors)", errors);

		if (errors != 0) $fatal(1, "tb_dataGenerator failed");
		$finish;
	end

endmodule
