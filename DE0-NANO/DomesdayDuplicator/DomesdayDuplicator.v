/************************************************************************
	
	DomesdayDuplicator.v
	Top-level module
	
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

module DomesdayDuplicator(
	input        CLOCK_50,
	inout [33:0] GPIO0,
	input [01:0] GPIO0_IN,
	inout [33:0] GPIO1,
	input [01:0] GPIO1_IN
);

// FX3 Hardware mapping begins ------------------------------------------------

// Generic pin-mapping for FX3 (DomDupBoard revision 2_0)
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

// 13-bit control bus physical mapping
assign GPIO1[27] = fx3_control[00]; // FX3 CTL_00 GPIO_17 (output)
assign fx3_control[01] = GPIO1[25];	// FX3 CTL_01 GPIO_18
assign fx3_control[02] = GPIO1[23];	// FX3 CTL_02 GPIO_19
assign fx3_control[03] = GPIO1[21];	// FX3 CTL_03 GPIO_20
assign GPIO1[19] = fx3_control[04];	// FX3 CTL_04 GPIO_21 (output)
assign fx3_control[05] = GPIO1[17];	// FX3 CTL_05 GPIO_22
assign fx3_control[06] = GPIO1[15];	// FX3 CTL_06 GPIO_23
assign fx3_control[07] = GPIO1[13];	// FX3 CTL_07 GPIO_24
assign fx3_control[08] = GPIO1[11];	// FX3 CTL_08 GPIO_25
assign fx3_control[09] = GPIO1[09];	// FX3 CTL_09 GPIO_26
assign fx3_control[10] = GPIO1[07];	// FX3 CTL_10 GPIO_27
assign fx3_control[11] = GPIO1[05];	// FX3 CTL_11 GPIO_28
assign fx3_control[12] = GPIO1[03];	// FX3 CTL_12 GPIO_29

// FX3 Signal mapping:
//
// CLK					GPIO16		PCLK		Output	- Data clock
// dataAvailable		GPIO_17		CTL_00	Output	- FPGA signals if data is available for reading
// bufferError			GPIO_21		CTL_04	Output	- FPGA signals buffering error
// Databus				GPIO0:15					Output	- Databus
// nReset				GPIO_27		CTL_10	Input		- FX3 signals (not) reset condition
// collectData			GPIO_19		CTL_02	Input		- FX3 signals data collection on/off
// readData				GPIO_18		CTL_01	Input		- FX3 signals it is reading from the databus
// testMode				GPIO_20		CTL_03	Input		- FX3 signals test mode on/off


// Wire definitions for FX3 GPIO mapping
wire fx3_nReset;
wire fx3_dataAvailable;
wire fx3_collectData;
wire fx3_readData;
wire fx3_testMode;
wire fx3_bufferError;

// Signal outputs to FX3
assign fx3_control[00] = fx3_dataAvailable;
assign fx3_control[04] = fx3_bufferError;

// Signal inputs from FX3
assign fx3_nReset      = fx3_control[10];
assign fx3_collectData = fx3_control[02];
assign fx3_readData    = fx3_control[01];
assign fx3_testMode    = fx3_control[03];

// FX3 Hardware mapping ends --------------------------------------------------


// ADC Hardware mapping begins ------------------------------------------------

wire [9:0]adcData;
wire adc_clock;

// 10-bit databus from ADC
assign adcData[0] = GPIO0[32];
assign adcData[1] = GPIO0[31];
assign adcData[2] = GPIO0[30];
assign adcData[3] = GPIO0[29];
assign adcData[4] = GPIO0[28];
assign adcData[5] = GPIO0[27];
assign adcData[6] = GPIO0[26];
assign adcData[7] = GPIO0[25];
assign adcData[8] = GPIO0[24];
assign adcData[9] = GPIO0[23];

// ADC clock output
assign GPIO0[33] = adc_clock;

// ADC Hardware mapping ends --------------------------------------------------


// Application logic begins ---------------------------------------------------


// PLL Function (clock generation) --------------------------------------------
//
// fx3_clock is 64 MHz (also used as system clock)
// adc_clock is 32 MHz
IPpllGenerator IPpllGenerator0 (
	// Inputs
	.inclk0(CLOCK_50),
	
	// Outputs
	.c0(fx3_clock),	// 64 MHz clock
	.c1(adc_clock)		// 32 MHz clock
);

wire fx3isReading;

// Data generation logic -----------------------------------------------------
dataGenerator dataGenerator0 (
	// Inputs
	.nReset(fx3_nReset),						// Not reset
	.adcClk(adc_clock),						// Data collection clock (ADC)
	.fx3Clk(fx3_clock),						// Data output clock (FX3)
	.collectData(fx3_collectData),		// Collect data (ADC data is discarded if 0)
	.readData(fx3isReading),				// 1 = FX3 is reading data
	.testMode(fx3_testMode),				// 1 = Test mode on
	.adcData(adcData),						// ADC data bus input
	
	// Outputs
	.bufferError(fx3_bufferError),		// Set if a FIFO buffer error occurs
	.dataAvailable(fx3_dataAvailable),	// Set if FIFO buffer contains at least 8192 words of data
	.dataOut(fx3_databus)					// 16-bit data output
);

// FX3 GPIF state-machine logic ----------------------------------------------
fx3StateMachine fx3StateMachine0 (
	// Inputs
	.nReset(fx3_nReset),						// Not reset
	.inclk(fx3_clock),						// Input clock
	.readData(fx3_readData),				// FX3 is about to start sampling the databus
	
	// Output
	.fx3isReading(fx3isReading)			// Flag to indicate FX3 is sampling the databus
);

endmodule
