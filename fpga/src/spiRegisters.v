/************************************************************************

	spiRegisters.v
	SPI register bank

	Domesday Duplicator - LaserDisc RF sampler
	SPDX-FileCopyrightText: 2026 Simon Inns
	SPDX-License-Identifier: GPL-3.0-or-later

	The FX3's window into the gateware. It replaces the five dedicated
	configuration lines that carried one bit each, of which only test mode
	was ever used, with a register bank the FX3 reads and writes over a
	private SPI link.

	SPI mode 0, most significant bit first, framed by chip select. A
	transaction is a command byte - direction in bit 7, register address in
	bits 6:0 - followed by data bytes, with the address incrementing after
	each one. The full contract, including why this is SPI rather than the
	I2C an earlier draft specified, is on the "FPGA register interface"
	page of the documentation site.

	Everything here is deliberately in one module. The shift register and
	the registers it reaches are too small to be worth an interface between
	them, and the simulation runner compiles a testbench against exactly one
	source file - so a split would cost the end-to-end test that is the only
	one worth having here.

************************************************************************/

module spiRegisters (
	input nReset,
	input clock,

	// SPI slave. All three inputs are driven by the FX3 and are
	// asynchronous to clock.
	input spiClock,
	input spiMosi,
	input spiChipSelectN,
	output spiMiso,

	// Register outputs
	output testMode,
	output [7:0] leds
);

// The build this gateware was compiled from, presented read-only at
// addresses 0x02 to 0x0A. The top level supplies them from version.vh,
// which the build generates; the defaults here describe a build that
// cannot name its own commit, which is what a lint or simulation run is.
//
// The commit is eight ASCII characters, first character in the most
// significant byte, padded with nulls. ASCII rather than a packed number
// because the commit is not always eight characters long - CMake asks git
// for eight, a Nix build passes seven - and eight bytes of text represent
// both without inventing or losing a digit.
parameter commitText = 64'h0000000000000000;
parameter buildFlags = 8'h00;

// Input synchronisers ---------------------------------------------------

reg [1:0] spiClockSync;
reg [1:0] spiMosiSync;
reg [1:0] spiChipSelectNSync;

// Filtered levels, and the previous clock level for edge detection.
//
// A new level is accepted only once two consecutive synchronised samples
// agree, which discards anything narrower than 33 ns. That is aimed at
// ringing on the two board-to-board headers this link crosses, not at
// metastability - the synchronisers above are what deal with that. The
// specification sets a minimum clock phase of 250 ns, so the filter costs
// at most an eighth of the shortest pulse it has to pass, and the master
// runs two orders of magnitude slower than that in practice.
reg spiClockSample;
reg spiChipSelectNSample;
reg spiClockLevel;
reg spiChipSelectNLevel;
reg spiClockLevelPrevious;

wire spiClockRising  =  spiClockLevel & ~spiClockLevelPrevious;
wire spiClockFalling = ~spiClockLevel &  spiClockLevelPrevious;

always @ (posedge clock, negedge nReset) begin
	if (!nReset) begin
		spiClockSync <= 2'b00;
		spiMosiSync <= 2'b00;
		spiChipSelectNSync <= 2'b11;
		spiClockSample <= 1'b0;
		spiChipSelectNSample <= 1'b1;
		spiClockLevel <= 1'b0;
		spiChipSelectNLevel <= 1'b1;
		spiClockLevelPrevious <= 1'b0;
	end else begin
		spiClockSync <= {spiClockSync[0], spiClock};
		spiMosiSync <= {spiMosiSync[0], spiMosi};
		spiChipSelectNSync <= {spiChipSelectNSync[0], spiChipSelectN};

		spiClockSample <= spiClockSync[1];
		spiChipSelectNSample <= spiChipSelectNSync[1];

		if (spiClockSync[1] == spiClockSample)
			spiClockLevel <= spiClockSync[1];

		if (spiChipSelectNSync[1] == spiChipSelectNSample)
			spiChipSelectNLevel <= spiChipSelectNSync[1];

		spiClockLevelPrevious <= spiClockLevel;
	end
end

// Register bank ---------------------------------------------------------

reg [7:0] testModeRegister;
reg [7:0] ledRegister;

// Unmapped addresses read as zero. That is what lets the map grow without
// the host having to probe for what exists: it reads MAP_VERSION, which is
// a positive statement of what this gateware implements.
function [7:0] readRegister;
	input [6:0] readAddress;
	begin
		case (readAddress)
			7'h00: readRegister = 8'h44;				// ID
			7'h01: readRegister = 8'h01;				// MAP_VERSION
			7'h02: readRegister = buildFlags;
			7'h03: readRegister = commitText[63:56];
			7'h04: readRegister = commitText[55:48];
			7'h05: readRegister = commitText[47:40];
			7'h06: readRegister = commitText[39:32];
			7'h07: readRegister = commitText[31:24];
			7'h08: readRegister = commitText[23:16];
			7'h09: readRegister = commitText[15:8];
			7'h0A: readRegister = commitText[7:0];
			7'h10: readRegister = testModeRegister;
			7'h11: readRegister = ledRegister;
			default: readRegister = 8'h00;
		endcase
	end
endfunction

// SPI transfer ----------------------------------------------------------

reg [6:0] shiftIn;			// bits of the arriving byte, less the one still on the wire
reg [7:0] shiftOut;			// the byte being clocked out, most significant bit first
reg [2:0] bitCount;			// bits of the current byte received so far
reg commandReceived;		// the command byte has arrived, so data bytes follow
reg readTransfer;			// the command asked to read
reg [6:0] address;			// register the next data byte reads or writes
reg misoOut;

// The byte as it stands once the bit currently on spiMosi is taken in
wire [7:0] shiftInNext = {shiftIn, spiMosiSync[1]};

// A read has to fetch the next byte as the current one completes, so that
// its first bit is on the wire before the master clocks it out
wire [6:0] addressNext = address + 7'd1;

assign spiMiso = misoOut;

// Any non-zero value means on, so that a host writing 1 and a host writing
// 0xFF agree about what they asked for
assign testMode = (testModeRegister != 8'h00);
assign leds = ledRegister;

always @ (posedge clock, negedge nReset) begin
	if (!nReset) begin
		shiftIn <= 7'd0;
		shiftOut <= 8'h00;
		bitCount <= 3'd0;
		commandReceived <= 1'b0;
		readTransfer <= 1'b0;
		address <= 7'h00;
		misoOut <= 1'b0;

		testModeRegister <= 8'h00;

		// One LED lit, which says "configured and running, but the FX3 has
		// not written here yet". An unconfigured FPGA shows none, because
		// its pins are high-Z, and the firmware overwrites this within a
		// second of enumerating - so the board distinguishes three states
		// on hardware whose only other diagnostic is a UART header.
		ledRegister <= 8'h01;
	end else if (spiChipSelectNLevel) begin
		// Chip select is deasserted, so no transfer is in progress.
		//
		// Clearing the transfer state here rather than when the next one
		// starts is what makes a transfer that is cut short - by an FX3
		// reset, or by a board powering up mid-byte - leave nothing behind.
		// A partly received data byte cannot reach a register, because a
		// register is only written on the eighth bit of a byte.
		shiftIn <= 7'd0;
		shiftOut <= 8'h00;
		bitCount <= 3'd0;
		commandReceived <= 1'b0;
		misoOut <= 1'b0;
	end else begin
		if (spiClockRising) begin
			shiftIn <= shiftInNext[6:0];
			bitCount <= bitCount + 3'd1;

			if (bitCount == 3'd7) begin
				if (!commandReceived) begin
					commandReceived <= 1'b1;
					readTransfer <= shiftInNext[7];
					address <= shiftInNext[6:0];

					// Nothing meaningful goes out during the command byte,
					// so a read's first returned byte is always zero and the
					// register contents start with the second
					shiftOut <= shiftInNext[7] ? readRegister(shiftInNext[6:0]) : 8'h00;
				end else begin
					if (!readTransfer) begin
						case (address)
							7'h10: testModeRegister <= shiftInNext;
							7'h11: ledRegister <= shiftInNext;
							default: begin
								// Read-only and unmapped addresses: the
								// write is discarded. SPI has no way to
								// refuse a byte and nothing here pretends
								// otherwise.
							end
						endcase
					end

					address <= addressNext;
					shiftOut <= readTransfer ? readRegister(addressNext) : 8'h00;
				end
			end
		end

		if (spiClockFalling) begin
			// Mode 0: the slave changes its output on the falling edge, so
			// the bit has a full half period to settle before the master
			// samples it on the rise.
			misoOut <= shiftOut[7];
			shiftOut <= {shiftOut[6:0], 1'b0};
		end
	end
end

endmodule
