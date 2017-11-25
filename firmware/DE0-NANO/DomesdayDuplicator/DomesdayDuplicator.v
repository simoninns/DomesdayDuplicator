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

// 13-bit control bus physical mapping
//assign fx3_control[00] = GPIO1[27];	// FX3 GPIO_17
assign GPIO1[27] = fx3_control[00]; // FX3 GPIO_17 (output)
assign fx3_control[01] = GPIO1[25];	// FX3 GPIO_18
assign fx3_control[02] = GPIO1[23];	// FX3 GPIO_19
assign fx3_control[03] = GPIO1[21];	// FX3 GPIO_20
assign GPIO1[19] = fx3_control[04];	// FX3 GPIO_21 (output)
assign fx3_control[05] = GPIO1[17];	// FX3 GPIO_22
assign GPIO1[15] = fx3_control[06];	// FX3 GPIO_23 (output)
assign fx3_control[07] = GPIO1[13];	// FX3 GPIO_24
assign fx3_control[08] = GPIO1[11];	// FX3 GPIO_25
assign fx3_control[09] = GPIO1[09];	// FX3 GPIO_26
assign fx3_control[10] = GPIO1[07];	// FX3 GPIO_27
assign fx3_control[11] = GPIO1[05];	// FX3 GPIO_28
assign fx3_control[12] = GPIO1[03];	// FX3 GPIO_29

// FX3 Clock physical mapping
assign GPIO1[31] = fx3_clock; // FX3 GPIO_16


// FX3 Application mapping

// fx3_nReset			CTL_10 (GPIO_27)		- input reset active low
//
// fx3_nWrite 			CTL_00 (GPIO_17)		- output not write flag
// fx3_nReady			CTL_01 (GPIO_18)		- input not Ready flag
//
// fx3_th0Ready		CTL_02 (GPIO_19)		- thread 0 ready flag
// fx3_th0Watermark	CTL_03 (GPIO_20)		- thread 0 watermark flag
//
// fx3_nError			CTL_04 (GPIO_21)		- output not error
// fx3_nTestmode		CTL_05 (GPIO_22)		- input not testmode
// fx3_nShort			CTL_06 (GPIO_23)		- output not short packet
//
// fx3_addressbus		CTL_12 (GPIO_29)		- address bus (1-bit)


// Wire definitions for FX3 GPIO mapping
wire fx3_nReset;
wire fx3_nWrite;
wire fx3_nReady;
wire fx3_th0Ready;
wire fx3_th0Watermark;
wire fx3_nError;
wire fx3_nTestmode;
wire fx3_nShort;

assign fx3_control[00] = fx3_nWrite; // 0 = writing, 1 = not writing
assign fx3_control[04] = fx3_nError; // 0 = error, 1 = not error
assign fx3_control[06] = fx3_nShort; // 1 = normal packet, 0 = short packet

// Signal inputs from FX3
assign fx3_nReady = fx3_control[01];

assign fx3_th0Ready     = fx3_control[02]; // 1 = not ready, 0 = ready
assign fx3_th0Watermark = fx3_control[03];

assign fx3_nTestmode = fx3_control[05];

// nReset signal from FX3
assign fx3_nReset = fx3_control[10];

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

// FX3 PCLK is DE0-Nano 50 MHz system clock
assign fx3_clock = CLOCK_50;

// ADC clock is 40 MHz and generated using a PLL
IPpllGenerator IPpllGenerator0 (
	// Inputs
	.inclk0(CLOCK_50),
	
	// Outputs
	.c0(adc_clock)
);


// FX3 State machine logic
fx3StateMachine fx3StateMachine0 (
	// Inputs
	.fx3_clock(fx3_clock),
	.fx3_nReset(fx3_nReset),
	.fx3_nReady(fx3_nReady),
	.fx3_th0Ready(fx3_th0Ready),
	.fx3_th0Watermark(fx3_th0Watermark),
	.fifoAlmostEmpty(fifoAlmostEmpty),
	.fifoHalfFull(fifoHalfFull),
	
	// Outputs
	.fx3_nWrite(fx3_nWrite),
	.fx3_nShort(fx3_nShort)
);

// Read the current ADC value
wire [9:0] adc_outputData;
readAdcData readAdcData0 (
	// Inputs
	.clock(adc_clock),
	.nReset(fx3_nReset),
	.adcDatabus(adcData),
	.nTestmode(fx3_nTestmode),
	
	// Outputs
	.adcData(adc_outputData)
);


// Dual-clock FIFO buffer from ADC to FX3
wire [9:0] fifo_outputData;
wire fifoEmpty;
wire fifoAlmostEmpty;
wire fifoHalfFull;
wire fifoFull;

// Flag error if FIFO becomes full (i.e. we are loosing data)
assign fx3_nError = !fifoFull;

fifo fifo0 (
	.inputData(adc_outputData),
	.inputClock(adc_clock),
	.nReset(fx3_nReset),
	.outputClock(fx3_clock),
	.outputAck(!fx3_nWrite),
	.nReady(fx3_nReady),
	
	.outputData(fifo_outputData),
	.empty_flag(fifoEmpty),
	.almostEmpty_flag(fifoAlmostEmpty),
	.halfFull_flag(fifoHalfFull),
	.full_flag(fifoFull)
);

// Convert 10-bit ADC data to 16-bit signed integer output
adcDataConvert adcDataConvert0 (
	.clock(fx3_clock),
	.nReset(fx3_nReset),
	.inputData(fifo_outputData),
	.outputData(fx3_databus)
);

endmodule
