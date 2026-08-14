/************************************************************************

	tb_spiRegisters.v
	Testbench for the SPI register bank (T3)

	Domesday Duplicator - LaserDisc RF sampler
	SPDX-FileCopyrightText: 2026 Simon Inns
	SPDX-License-Identifier: GPL-3.0-or-later

	This is the only test the register interface gets that does not need
	hardware, and it is the reason the bank is one module rather than a
	slave and a register file with an interface between them: what is worth
	testing here is the whole path, from an edge on a pin to a register that
	changed.

	The bus is driven at 2 MHz, the fastest the specification allows, so the
	input synchronisers and the edge filter are exercised at their tightest.
	The master the FX3 will run is bit-banged and two orders of magnitude
	slower, so anything that passes here has a very large margin in the
	thing that will actually talk to it.

************************************************************************/

`timescale 1ns / 1ps

module tb_spiRegisters;

	// 2 MHz SPI: a 250 ns half period, which is the specification's minimum
	// clock phase. Chip select gets the 1 us of setup and hold the
	// specification asks for.
	parameter HALF_BIT = 250;
	parameter CS_TIME = 1000;

	// "7713495d" as eight ASCII bytes, and a dirty build of a known commit
	parameter COMMIT_TEXT = 64'h3737313334393564;
	parameter BUILD_FLAGS = 8'h03;

	reg nReset;
	reg clock;

	reg spiClock;
	reg spiMosi;
	reg spiChipSelectN;
	wire spiMiso;

	wire testMode;
	wire [7:0] leds;

	integer errors;
	integer bitIndex;
	integer readIndex;

	reg [7:0] spiReceived;
	reg [7:0] readData [0:15];

	spiRegisters #(
		.commitText(COMMIT_TEXT),
		.buildFlags(BUILD_FLAGS)
	) dut (
		.nReset(nReset),
		.clock(clock),
		.spiClock(spiClock),
		.spiMosi(spiMosi),
		.spiChipSelectN(spiChipSelectN),
		.spiMiso(spiMiso),
		.testMode(testMode),
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
				$display("FAIL: %0s: got %h, expected %h (t=%0t)", what, got, want, $time);
				errors = errors + 1;
			end
		end
	endtask

	// SPI mode 0 master: the clock idles low, data is presented while it is
	// low and sampled on the rising edge.
	task spiByte;
		input [7:0] send;
		begin
			spiReceived = 8'h00;
			for (bitIndex = 7; bitIndex >= 0; bitIndex = bitIndex - 1) begin
				spiMosi = send[bitIndex];
				#HALF_BIT;
				spiClock = 1'b1;
				spiReceived[bitIndex] = spiMiso;
				#HALF_BIT;
				spiClock = 1'b0;
			end
		end
	endtask

	// Clock only the top four bits, leaving a byte half delivered
	task spiHalfByte;
		input [7:0] send;
		begin
			for (bitIndex = 7; bitIndex >= 4; bitIndex = bitIndex - 1) begin
				spiMosi = send[bitIndex];
				#HALF_BIT;
				spiClock = 1'b1;
				#HALF_BIT;
				spiClock = 1'b0;
			end
		end
	endtask

	task spiSelect;
		begin
			spiChipSelectN = 1'b0;
			#CS_TIME;
		end
	endtask

	task spiDeselect;
		begin
			#CS_TIME;
			spiChipSelectN = 1'b1;
			#CS_TIME;
		end
	endtask

	// Read count bytes from startAddress into readData
	task spiRead;
		input [6:0] startAddress;
		input [7:0] count;
		begin
			spiSelect;
			spiByte({1'b1, startAddress});
			for (readIndex = 0; readIndex < count; readIndex = readIndex + 1) begin
				spiByte(8'h00);
				readData[readIndex] = spiReceived;
			end
			spiDeselect;
		end
	endtask

	task spiWriteOne;
		input [6:0] startAddress;
		input [7:0] value;
		begin
			spiSelect;
			spiByte({1'b0, startAddress});
			spiByte(value);
			spiDeselect;
		end
	endtask

	task spiWriteTwo;
		input [6:0] startAddress;
		input [7:0] first;
		input [7:0] second;
		begin
			spiSelect;
			spiByte({1'b0, startAddress});
			spiByte(first);
			spiByte(second);
			spiDeselect;
		end
	endtask

	initial begin
		errors = 0;

		nReset = 1'b0;
		spiClock = 1'b0;
		spiMosi = 1'b0;
		spiChipSelectN = 1'b1;

		repeat (4) @(posedge clock);
		#1 nReset = 1'b1;
		repeat (4) @(posedge clock);

		// --- Reset values ---
		//
		// One LED lit says "gateware running, FX3 has not written here yet",
		// which is the state that distinguishes a live but unconfigured board
		// from an unconfigured FPGA. Test mode off means a board that has just
		// come out of reset captures real samples.
		check(leds, 8'h01, "LED register reset value");
		check(testMode, 1'b0, "test mode is off after reset");

		// --- Identity block ---
		//
		// Eleven bytes in one transaction: the signature, the map version, the
		// build flags and eight ASCII characters of commit.
		spiRead(7'h00, 8'd11);
		check(readData[0], 8'h44, "ID register");
		check(readData[1], 8'h01, "map version");
		check(readData[2], BUILD_FLAGS, "build flags");
		check(readData[3], 8'h37, "commit character 0");
		check(readData[4], 8'h37, "commit character 1");
		check(readData[5], 8'h31, "commit character 2");
		check(readData[6], 8'h33, "commit character 3");
		check(readData[7], 8'h34, "commit character 4");
		check(readData[8], 8'h39, "commit character 5");
		check(readData[9], 8'h35, "commit character 6");
		check(readData[10], 8'h64, "commit character 7");

		// --- Test mode ---
		spiWriteOne(7'h10, 8'h01);
		check(testMode, 1'b1, "test mode on after writing 1");
		spiRead(7'h10, 8'd1);
		check(readData[0], 8'h01, "test mode reads back");

		// Any non-zero value means on, so that a host writing 1 and a host
		// writing 0xFF agree about what they asked for
		spiWriteOne(7'h10, 8'hFF);
		check(testMode, 1'b1, "test mode on after writing 0xFF");

		spiWriteOne(7'h10, 8'h00);
		check(testMode, 1'b0, "test mode off after writing 0");

		// --- LEDs ---
		spiWriteOne(7'h11, 8'hA5);
		check(leds, 8'hA5, "LED register drives the LEDs");
		spiRead(7'h11, 8'd1);
		check(readData[0], 8'hA5, "LED register reads back");

		// --- Address auto-increment on a write ---
		//
		// One transaction setting both control registers, which is what makes
		// the identity block a single read rather than seven.
		spiWriteTwo(7'h10, 8'h01, 8'h3C);
		check(testMode, 1'b1, "auto-increment wrote test mode");
		check(leds, 8'h3C, "auto-increment wrote the LED register");
		spiWriteOne(7'h10, 8'h00);

		// --- Unmapped addresses read as zero ---
		//
		// This is what lets the map grow: a host that reads an address this
		// gateware does not implement gets zero rather than nonsense, and
		// learns what is implemented from the map version instead.
		spiRead(7'h20, 8'd1);
		check(readData[0], 8'h00, "unmapped address reads zero");

		// --- Writes to read-only registers are discarded ---
		//
		// SPI cannot refuse a byte, so the write is accepted off the wire and
		// dropped. What must not happen is the identity block changing.
		spiWriteOne(7'h00, 8'hFF);
		spiRead(7'h00, 8'd1);
		check(readData[0], 8'h44, "ID register survives a write");

		// --- The address wraps rather than saturating ---
		spiRead(7'h7F, 8'd2);
		check(readData[0], 8'h00, "unmapped 0x7F reads zero");
		check(readData[1], 8'h44, "address wrapped to the ID register");

		// --- A byte cut short by chip select is discarded ---
		//
		// The whole recovery mechanism is that deasserting chip select returns
		// the slave to idle from any state. An FX3 that resets mid-transfer,
		// or a board powering up while the lines float, must not be able to
		// leave half a byte in a register.
		spiWriteOne(7'h11, 8'h81);
		spiSelect;
		spiByte(8'h10);				// write to test mode
		spiHalfByte(8'hFF);			// four bits of a data byte, then nothing
		spiDeselect;
		check(testMode, 1'b0, "a half-delivered byte did not reach test mode");

		// --- And the interface still works afterwards ---
		//
		// An abandoned transfer that left the slave mid-byte would show up
		// here as a command byte read four bits out of step.
		spiRead(7'h11, 8'd1);
		check(readData[0], 8'h81, "the LED register is intact after an abandoned transfer");
		spiWriteOne(7'h10, 8'h01);
		check(testMode, 1'b1, "a write still works after an abandoned transfer");

		// --- Reset returns the registers to their defaults ---
		#1 nReset = 1'b0;
		repeat (4) @(posedge clock);
		#1 nReset = 1'b1;
		repeat (4) @(posedge clock);
		check(leds, 8'h01, "reset restores the LED register");
		check(testMode, 1'b0, "reset clears test mode");

		if (errors == 0) $display("tb_spiRegisters: PASS");
		else $display("tb_spiRegisters: FAIL (%0d errors)", errors);

		if (errors != 0) $fatal(1, "tb_spiRegisters failed");
		$finish;
	end

endmodule
