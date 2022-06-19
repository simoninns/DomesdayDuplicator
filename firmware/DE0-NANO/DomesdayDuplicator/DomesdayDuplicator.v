/************************************************************************
	
	Domesday Duplicator.v
	Top-level module
	
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

module DomesdayDuplicator(
	input        CLOCK_50,
	inout [33:0] GPIO0,
	inout [33:0] GPIO1,
	output [7:0] LED
);

// FX3 Hardware mapping begins ------------------------------------------------

// Generic pin-mapping for FX3 (DomDupBoard revisions 2_0 to 3_0)
wire [15:0] fx3_databus;	// 32-bit databus (only 16-bits used)
wire [12:0] fx3_control;	// 13-bit control bus
wire fx3_clock;				// FX3 GPIF Clock

// 32-bit data bus physical mapping (output only)
// Note: board supports 32-bits; software is limited to 16-bits
assign GPIO1[32] = fx3_databus[00];
assign GPIO1[30] = fx3_databus[01];
assign GPIO1[28] = fx3_databus[02];
assign GPIO1[26] = fx3_databus[03];
assign GPIO1[24] = fx3_databus[04];
assign GPIO1[22] = fx3_databus[05];
assign GPIO1[20] = fx3_databus[06];
assign GPIO1[18] = fx3_databus[07];
assign GPIO1[16] = fx3_databus[08];
assign GPIO1[14] = fx3_databus[09];
assign GPIO1[12] = fx3_databus[10];
assign GPIO1[10] = fx3_databus[11];
assign GPIO1[08] = fx3_databus[12];
assign GPIO1[06] = fx3_databus[13];
assign GPIO1[04] = fx3_databus[14];
assign GPIO1[02] = fx3_databus[15];

// High-Z the unused FX3 databus pins 
assign GPIO0[02] = 1'bZ;
assign GPIO0[03] = 1'bZ;
assign GPIO0[04] = 1'bZ;
assign GPIO0[05] = 1'bZ;
assign GPIO0[06] = 1'bZ;
assign GPIO0[07] = 1'bZ;
assign GPIO0[12] = 1'bZ;
assign GPIO0[13] = 1'bZ;
assign GPIO0[14] = 1'bZ;
assign GPIO0[15] = 1'bZ;
assign GPIO0[16] = 1'bZ;
assign GPIO0[17] = 1'bZ;
assign GPIO0[18] = 1'bZ;
assign GPIO0[19] = 1'bZ;
assign GPIO0[20] = 1'bZ;
assign GPIO0[21] = 1'bZ;

// Mappings for 32-bit databus
//assign GPIO0[02] = fx3_databus[16];
//assign GPIO0[03] = fx3_databus[17];
//assign GPIO0[04] = fx3_databus[18];
//assign GPIO0[05] = fx3_databus[19];
//assign GPIO0[06] = fx3_databus[20];
//assign GPIO0[07] = fx3_databus[21];
//assign GPIO0[12] = fx3_databus[22];
//assign GPIO0[13] = fx3_databus[23];
//assign GPIO0[14] = fx3_databus[24];
//assign GPIO0[15] = fx3_databus[25];
//assign GPIO0[16] = fx3_databus[26];
//assign GPIO0[17] = fx3_databus[27];
//assign GPIO0[18] = fx3_databus[28];
//assign GPIO0[19] = fx3_databus[29];
//assign GPIO0[20] = fx3_databus[30];
//assign GPIO0[21] = fx3_databus[31];

// FX3 Clock physical mapping
assign GPIO1[31] = fx3_clock; // FX3 GPIO_16

// 13-bit control bus physical mapping (outputs)
assign GPIO1[27] = fx3_control[00]; // FX3 CTL_00 GPIO_17 (output)
assign GPIO1[21] = fx3_control[03];	// FX3 CTL_03 GPIO_20 (output)
assign GPIO1[19] = fx3_control[04];	// FX3 CTL_04 GPIO_21 (output)
assign GPIO1[05] = fx3_control[11];	// FX3 CTL_11 GPIO_28 (output)
assign GPIO1[03] = fx3_control[12];	// FX3 CTL_12 GPIO_29 (output)

// 13-bit control bus physical mapping (inputs)
assign fx3_control[01] = GPIO1[25];	// FX3 CTL_01 GPIO_18
assign fx3_control[02] = GPIO1[23];	// FX3 CTL_02 GPIO_19
assign fx3_control[05] = GPIO1[17];	// FX3 CTL_05 GPIO_22
assign fx3_control[06] = GPIO1[15];	// FX3 CTL_06 GPIO_23
assign fx3_control[07] = GPIO1[13];	// FX3 CTL_07 GPIO_24
assign fx3_control[08] = GPIO1[11];	// FX3 CTL_08 GPIO_25
assign fx3_control[09] = GPIO1[09];	// FX3 CTL_09 GPIO_26
assign fx3_control[10] = GPIO1[07];	// FX3 CTL_10 GPIO_27

// High-Z the unused GPIO0 pins
assign GPIO0[0] = 1'bZ;
assign GPIO0[1] = 1'bZ;
assign GPIO0[8] = 1'bZ;
assign GPIO0[9] = 1'bZ;
assign GPIO0[10] = 1'bZ;
assign GPIO0[11] = 1'bZ;
assign GPIO0[22] = 1'bZ;
assign GPIO0[23] = 1'bZ;
assign GPIO0[24] = 1'bZ;
assign GPIO0[25] = 1'bZ;
assign GPIO0[26] = 1'bZ;
assign GPIO0[27] = 1'bZ;
assign GPIO0[28] = 1'bZ;
assign GPIO0[29] = 1'bZ;
assign GPIO0[30] = 1'bZ;
assign GPIO0[31] = 1'bZ;
assign GPIO0[32] = 1'bZ;

// High-Z the unused GPIO1 pins
assign GPIO1[0] = 1'bZ;
assign GPIO1[1] = 1'bZ;
assign GPIO1[7] = 1'bZ;
assign GPIO1[9] = 1'bZ;
assign GPIO1[11] = 1'bZ;
assign GPIO1[13] = 1'bZ;
assign GPIO1[15] = 1'bZ;
assign GPIO1[17] = 1'bZ;
assign GPIO1[23] = 1'bZ;
assign GPIO1[25] = 1'bZ;
assign GPIO1[29] = 1'bZ;
assign GPIO1[33] = 1'bZ;

// FX3 Signal mapping:
//
// CLK					GPIO16		PCLK		Output	- Data clock
// Databus				GPIO0:15					Output	- Databus
// dataAvailable		GPIO_17		CTL_00	Output	- FPGA signals if data is available for reading
// nReset				GPIO_27		CTL_10	Input		- FX3 signals (not) reset condition
// collectData			GPIO_19		CTL_02	Input		- Unused
// readData				GPIO_18		CTL_01	Input		- FX3 signals it is reading from the databus

// input0				GPIO_20		CTL_03	Output	- Buffer error flag from FPGA
// input1				GPIO_21		CTL_04	Output	- Unused
// input2				GPIO_28		CTL_11	Output	- Unused
// input3				GPIO_29		CTL_12	Output	- Unused

// outputE0				GPIO_22		CTL_05	Input		- FX3 Configuration bit 0 (Test mode off/on)
// outputD0				GPIO_23		CTL_06	Input		- FX3 Configuration bit 1 (Unused)
// outputD1				GPIO_24		CTL_07	Input		- FX3 Configuration bit 2 (Unused)
// outputD2				GPIO_25		CTL_08	Input		- FX3 Configuration bit 3 (Unused)
// outputD3				GPIO_26		CTL_09	Input		- FX3 Configuration bit 4 (Unused)

// Wire definitions for FX3 GPIO mapping
wire fx3_nReset;
wire fx3_dataAvailable;
wire fx3_readData;
wire fx3_bufferError;
wire fx3_testMode;

// Signal outputs to FX3
assign fx3_control[00] 		= fx3_dataAvailable;
assign fx3_control[03] 		= fx3_bufferError;

// These are currently unused, but must have a defined value
assign fx3_control[04]	= 1'b0;
assign fx3_control[11]	= 1'b0;
assign fx3_control[12]	= 1'b0;

// Signal inputs from FX3
assign fx3_nReset      = fx3_control[10];
//assign fx3_unused = fx3_control[02];
assign fx3_readData    = fx3_control[01];

// Signal inputs from FX3 (configuration bits)
assign fx3_testMode    		= fx3_control[05];
//assign fx3_configBit1    = fx3_control[06];
//assign fx3_configBit2 	= fx3_control[07];
//assign fx3_configBit3		= fx3_control[07];
//assign fx3_configBit4 	= fx3_control[07];

// FX3 Hardware mapping ends --------------------------------------------------


// ADC Hardware mapping begins ------------------------------------------------

wire [9:0]adc_databus;

// 10-bit databus from ADC
assign adc_databus[0] = GPIO0[32];
assign adc_databus[1] = GPIO0[31];
assign adc_databus[2] = GPIO0[30];
assign adc_databus[3] = GPIO0[29];
assign adc_databus[4] = GPIO0[28];
assign adc_databus[5] = GPIO0[27];
assign adc_databus[6] = GPIO0[26];
assign adc_databus[7] = GPIO0[25];
assign adc_databus[8] = GPIO0[24];
assign adc_databus[9] = GPIO0[23];

// ADC clock output
// Select the correct sampling clock based on the configuration
wire adc_clock;
assign GPIO0[33] = adc_clock;

// ADC Hardware mapping ends --------------------------------------------------


// Application logic begins ---------------------------------------------------


// PLL clock generation
// Generate 60 MHz FX3/FPGA system clock from the 50 MHz physical clock and
// 40 MHz sampling clock
IPpllGenerator IPpllGenerator0 (
	// Inputs
	.inclk0(CLOCK_50),
	
	// Outputs
	.c0(fx3_clock),	// 60 MHz system clock
	.c1(adc_clock)		// 40 MHz ADC clock
);

wire fx3_isReading;
wire [15:0] dataGeneratorOut;

// Generate 16-bit data either from the ADC or the test data generator
dataGenerator dataGenerator0 (
	// Inputs
	.nReset(fx3_nReset),				// Not reset
	.clock(adc_clock),				// ADC clock
	.adc_databus(adc_databus),		// 10-bit ADC databus
	.testModeFlag(fx3_testMode),	// 1 = Test mode on
	
	// Outputs
	.dataOut(dataGeneratorOut)		// 16-bit data out
);

// FIFO buffer
buffer buffer0 (
	// Inputs
	.nReset(fx3_nReset),						// Not reset
	.writeClock(adc_clock),					// ADC clock
	.readClock(fx3_clock),					// FX3 clock
	.isReading(fx3_isReading),				// 1 = FX3 is reading data
	.dataIn(dataGeneratorOut),				// 16-bit ADC data bus input
	
	// Outputs
	.bufferOverflow(fx3_bufferError),	// Set if a buffer overflow occurs
	.dataAvailable(fx3_dataAvailable),	// Set if buffer contains at least 8192 words of data
	.dataOut(fx3_databus)					// 16-bit data output
);

// FX3 GPIF state-machine logic
fx3StateMachine fx3StateMachine0 (
	// Inputs
	.nReset(fx3_nReset),						// Not reset
	.fx3_clock(fx3_clock),					// FX3 clock
	.readData(fx3_readData),				// FX3 is about to start sampling the databus
	
	// Output
	.fx3isReading(fx3_isReading)			// Flag to indicate FX3 is sampling the databus
);

// Status LED control
statusLED statusLED0 (
	// Inputs
	.nReset(fx3_nReset),
	.clock(fx3_clock),
	
	// Outputs
	.leds(LED)
);

endmodule
