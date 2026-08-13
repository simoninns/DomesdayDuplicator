/************************************************************************

	tb_fx3StateMachine.v
	Testbench for the FX3 GPIF II state machine (T3)

	Domesday Duplicator - LaserDisc RF sampler
	SPDX-FileCopyrightText: 2018-2025 Simon Inns
	SPDX-License-Identifier: GPL-3.0-or-later

	The highest-risk module in the design. It has no visible failure mode: if
	it sends 8191 words instead of 8192, the FX3 still returns buffers, the
	capture still completes, and the samples are simply wrong from that point
	on. The packet length is the thing to pin, so that is what this checks.

************************************************************************/

`timescale 1ns / 1ps

module tb_fx3StateMachine;

	reg nReset;
	reg fx3_clock;
	reg readData;
	wire fx3isReading;

	integer errors;
	integer cycles;
	integer gap;
	integer i;

	// Must equal the FIFO depth in buffer.v and the FX3's DMA buffer size.
	// 8192 16-bit words is one 16 KB USB 3.0 bulk endpoint buffer.
	localparam PACKET_WORDS = 8192;

	fx3StateMachine dut (
		.nReset(nReset),
		.fx3_clock(fx3_clock),
		.readData(readData),
		.fx3isReading(fx3isReading)
	);

	// 60 MHz FX3 system clock — 16.667 ns period
	initial fx3_clock = 1'b0;
	always #8.333 fx3_clock = ~fx3_clock;

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

	// Count the clock edges for which fx3isReading is asserted. The count is
	// taken a delta after each edge, once the combinational next-state logic
	// has settled — wordCounter is written with blocking assignments inside a
	// clocked block, so mid-edge samples are not meaningful.
	task measurePacket;
		output integer count;
		begin
			count = 0;
			while (fx3isReading !== 1'b1) begin
				@(posedge fx3_clock);
				#1;
				count = count + 1;
				if (count > PACKET_WORDS * 4) begin
					$display("FAIL: fx3isReading never asserted (t=%0t)", $time);
					errors = errors + 1;
					count = 0;
					disable measurePacket;
				end
			end

			count = 0;
			while (fx3isReading === 1'b1) begin
				count = count + 1;
				@(posedge fx3_clock);
				#1;
			end
		end
	endtask

	initial begin
		errors = 0;
		readData = 1'b0;
		nReset = 1'b0;

		@(posedge fx3_clock);
		#1 nReset = 1'b1;

		// --- Idle ---------------------------------------------------------
		// Nothing is sent until the FX3 asks. Ten idle cycles, because a state
		// machine that self-starts would do so within one or two.
		for (i = 0; i < 10; i = i + 1) begin
			@(posedge fx3_clock);
			#1 check(fx3isReading, 1'b0, "fx3isReading while idle");
		end

		// --- One packet ---------------------------------------------------
		// readData is registered before it is acted on, so a single-cycle
		// assertion is enough to start a packet — the FX3 does not have to hold
		// it for the duration. That is deliberate, and it is asserted here so a
		// change to the handshake shows up as a test failure rather than as a
		// capture that is short by one buffer.
		readData = 1'b1;
		@(posedge fx3_clock);
		#1 readData = 1'b0;

		measurePacket(cycles);
		check(cycles, PACKET_WORDS, "packet length in clock cycles");

		// --- Back to idle -------------------------------------------------
		for (i = 0; i < 10; i = i + 1) begin
			@(posedge fx3_clock);
			#1 check(fx3isReading, 1'b0, "fx3isReading after a packet");
		end

		// --- Back-to-back packets -----------------------------------------
		// With readData held high the machine must still emit whole packets
		// with a gap between them, not run continuously. wordCounter is not
		// cleared until a cycle after the state leaves sendPacket, and the
		// wait state requires it to be zero, so the gap is what stops one
		// packet running into the next.
		readData = 1'b1;

		measurePacket(cycles);
		check(cycles, PACKET_WORDS, "first back-to-back packet length");

		gap = 0;
		while (fx3isReading !== 1'b1) begin
			gap = gap + 1;
			@(posedge fx3_clock);
			#1;
		end
		if (gap < 1) begin
			$display("FAIL: no gap between packets — wordCounter cannot have been cleared (t=%0t)", $time);
			errors = errors + 1;
		end

		cycles = 0;
		while (fx3isReading === 1'b1) begin
			cycles = cycles + 1;
			@(posedge fx3_clock);
			#1;
		end
		check(cycles, PACKET_WORDS, "second back-to-back packet length");

		// --- Reset during a packet ----------------------------------------
		// nReset is driven by the FX3 and drops when the host closes the
		// device. A part-sent packet must be abandoned, not resumed.
		readData = 1'b1;
		measurePacket(cycles);		// consumes a whole packet, leaving one in flight
		while (fx3isReading !== 1'b1) @(posedge fx3_clock);

		repeat (100) @(posedge fx3_clock);
		#1 check(fx3isReading, 1'b1, "mid-packet before reset");

		nReset = 1'b0;
		#1 check(fx3isReading, 1'b0, "fx3isReading is cleared asynchronously by reset");

		@(posedge fx3_clock);
		#1 nReset = 1'b1;
		@(posedge fx3_clock);
		#1 check(fx3isReading, 1'b0, "packet is not resumed after reset");

		// After a reset the next request must produce a full packet, not the
		// remainder of the abandoned one.
		measurePacket(cycles);
		check(cycles, PACKET_WORDS, "packet length after a mid-packet reset");

		if (errors == 0) $display("tb_fx3StateMachine: PASS");
		else $display("tb_fx3StateMachine: FAIL (%0d errors)", errors);

		if (errors != 0) $fatal(1, "tb_fx3StateMachine failed");
		$finish;
	end

endmodule
