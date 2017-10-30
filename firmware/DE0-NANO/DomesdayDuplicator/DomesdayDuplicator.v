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

// fx3_nReset			GPIO[27]		- input reset active low
// fx3_clk				GPIO[16]		- output clk
//
// fx3_nWrite 			GPIO[17]		- output not write flag
// fx3_nReady			GPIO[18]		- input not Ready flag
//
// fx3_th0Ready		GPIO[19]		- thread 0 ready flag
// fx3_th0Watermark	GPIO[20]		- thread 0 watermark flag
//
// fx3_addressbus		GPIO[29]		- address bus (1-bit)

// Wire definitions for FX3
wire fx3_nReset;
wire fx3_clock;

wire [15:0]fx3_databus;

wire fx3_nWrite;
wire fx3_nReady;

wire fx3_th0Ready;
wire fx3_th0Watermark;

// 16-bit Data bus to FX3
assign GPIO0[ 3] = fx3_databus[ 0];
assign GPIO0[ 5] = fx3_databus[ 1];
assign GPIO0[ 7] = fx3_databus[ 2];
assign GPIO0[ 9] = fx3_databus[ 3];
assign GPIO0[11] = fx3_databus[ 4];
assign GPIO0[13] = fx3_databus[ 5];
assign GPIO0[15] = fx3_databus[ 6];
assign GPIO0[17] = fx3_databus[ 7];
assign GPIO0[19] = fx3_databus[ 8];
assign GPIO0[21] = fx3_databus[ 9];
assign GPIO0[23] = fx3_databus[10];
assign GPIO0[25] = fx3_databus[11];
assign GPIO0[27] = fx3_databus[12];
assign GPIO0[29] = fx3_databus[13];
assign GPIO0[31] = fx3_databus[14];
assign GPIO0[33] = fx3_databus[15];

// Signal outputs to FX3
assign GPIO0[ 4] = fx3_clock;
assign GPIO0[ 6] = fx3_nWrite; // 0 = writing, 1 = not writing

// Signal inputs from FX3
assign fx3_nReady = GPIO0[ 8];

assign fx3_th0Ready     = GPIO0[10]; // 1 = not ready, 0 = ready
assign fx3_th0Watermark = GPIO0[12];

// nReset signal from FX3
assign fx3_nReset = GPIO0[26];

// FX3 Hardware mapping ends --------------------------------------------------


// ADC Hardware mapping begins ------------------------------------------------

wire [9:0]adcData;
wire adc_clock;

// 10-bit databus from ADC
assign adcData[0] = GPIO1[9];
assign adcData[1] = GPIO1[8];
assign adcData[2] = GPIO1[7];
assign adcData[3] = GPIO1[6];
assign adcData[4] = GPIO1[5];
assign adcData[5] = GPIO1[4];
assign adcData[6] = GPIO1[3];
assign adcData[7] = GPIO1[2];
assign adcData[8] = GPIO1[1];
assign adcData[9] = GPIO1[0];

// ADC clock output
assign GPIO1[10] = adc_clock;

// ADC Hardware mapping ends --------------------------------------------------

// Debug header hardware mapping begins ---------------------------------------

//wire [5:0] debugHeader;
//
//// 6-bit debug header
//assign GPIO1[24] = debugHeader[0];
//assign GPIO1[25] = debugHeader[1];
//assign GPIO1[26] = debugHeader[2];
//assign GPIO1[27] = debugHeader[3];
//assign GPIO1[28] = debugHeader[4];
//assign GPIO1[29] = debugHeader[5];

// Debug header hardware mapping end ------------------------------------------


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
	.fifoFull(fifoFull),
	
	// Outputs
	.fx3_nWrite(fx3_nWrite)
);

// Uncomment to output FIFO status to debug header
//assign debugHeader[0] = fifoHalfFull;
//assign debugHeader[1] = fifoAlmostEmpty;
//assign debugHeader[2] = fifoFull;

// Read the current ADC value
wire [15:0] adc_outputData;
readAdcData readAdcData0 (
	// Inputs
	.clock(adc_clock),
	.nReset(fx3_nReset),
	.runFlag(1'b1),
	.adcDatabus(adcData),
	
	// Outputs
	.adcData(adc_outputData)
);


// Dual-clock FIFO buffer from ADC to FX3
wire [15:0] fifo_outputData;
wire fifoEmpty;
wire fifoAlmostEmpty;
wire fifoHalfFull;
wire fifoFull;

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

// Copy the FIFO output data to the FX3 data bus
assign fx3_databus = fifo_outputData;

endmodule
