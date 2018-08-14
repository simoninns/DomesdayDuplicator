// megafunction wizard: %FIFO%
// GENERATION: STANDARD
// VERSION: WM1.0
// MODULE: dcfifo 

// ============================================================
// File Name: IPfifo.v
// Megafunction Name(s):
// 			dcfifo
//
// Simulation Library Files(s):
// 			altera_mf
// ============================================================
// ************************************************************
// THIS IS A WIZARD-GENERATED FILE. DO NOT EDIT THIS FILE!
//
// 18.0.0 Build 614 04/24/2018 SJ Lite Edition
// ************************************************************


//Copyright (C) 2018  Intel Corporation. All rights reserved.
//Your use of Intel Corporation's design tools, logic functions 
//and other software and tools, and its AMPP partner logic 
//functions, and any output files from any of the foregoing 
//(including device programming or simulation files), and any 
//associated documentation or information are expressly subject 
//to the terms and conditions of the Intel Program License 
//Subscription Agreement, the Intel Quartus Prime License Agreement,
//the Intel FPGA IP License Agreement, or other applicable license
//agreement, including, without limitation, that your use is for
//the sole purpose of programming logic devices manufactured by
//Intel and sold by Intel or its authorized distributors.  Please
//refer to the applicable agreement for further details.


// synopsys translate_off
`timescale 1 ps / 1 ps
// synopsys translate_on
module IPfifo (
	aclr,
	data,
	rdclk,
	rdreq,
	wrclk,
	wrreq,
	q,
	rdempty,
	rdusedw,
	wrempty,
	wrusedw);

	input	  aclr;
	input	[9:0]  data;
	input	  rdclk;
	input	  rdreq;
	input	  wrclk;
	input	  wrreq;
	output	[9:0]  q;
	output	  rdempty;
	output	[13:0]  rdusedw;
	output	  wrempty;
	output	[13:0]  wrusedw;
`ifndef ALTERA_RESERVED_QIS
// synopsys translate_off
`endif
	tri0	  aclr;
`ifndef ALTERA_RESERVED_QIS
// synopsys translate_on
`endif

	wire [9:0] sub_wire0;
	wire  sub_wire1;
	wire [13:0] sub_wire2;
	wire  sub_wire3;
	wire [13:0] sub_wire4;
	wire [9:0] q = sub_wire0[9:0];
	wire  rdempty = sub_wire1;
	wire [13:0] rdusedw = sub_wire2[13:0];
	wire  wrempty = sub_wire3;
	wire [13:0] wrusedw = sub_wire4[13:0];

	dcfifo	dcfifo_component (
				.aclr (aclr),
				.data (data),
				.rdclk (rdclk),
				.rdreq (rdreq),
				.wrclk (wrclk),
				.wrreq (wrreq),
				.q (sub_wire0),
				.rdempty (sub_wire1),
				.rdusedw (sub_wire2),
				.wrempty (sub_wire3),
				.wrusedw (sub_wire4),
				.eccstatus (),
				.rdfull (),
				.wrfull ());
	defparam
		dcfifo_component.add_usedw_msb_bit = "ON",
		dcfifo_component.intended_device_family = "Cyclone IV E",
		dcfifo_component.lpm_numwords = 8192,
		dcfifo_component.lpm_showahead = "ON",
		dcfifo_component.lpm_type = "dcfifo",
		dcfifo_component.lpm_width = 10,
		dcfifo_component.lpm_widthu = 14,
		dcfifo_component.overflow_checking = "ON",
		dcfifo_component.rdsync_delaypipe = 3,
		dcfifo_component.read_aclr_synch = "OFF",
		dcfifo_component.underflow_checking = "ON",
		dcfifo_component.use_eab = "ON",
		dcfifo_component.write_aclr_synch = "ON",
		dcfifo_component.wrsync_delaypipe = 3;


endmodule

// ============================================================
// CNX file retrieval info
// ============================================================
// Retrieval info: PRIVATE: AlmostEmpty NUMERIC "0"
// Retrieval info: PRIVATE: AlmostEmptyThr NUMERIC "-1"
// Retrieval info: PRIVATE: AlmostFull NUMERIC "0"
// Retrieval info: PRIVATE: AlmostFullThr NUMERIC "-1"
// Retrieval info: PRIVATE: CLOCKS_ARE_SYNCHRONIZED NUMERIC "1"
// Retrieval info: PRIVATE: Clock NUMERIC "4"
// Retrieval info: PRIVATE: Depth NUMERIC "8192"
// Retrieval info: PRIVATE: Empty NUMERIC "1"
// Retrieval info: PRIVATE: Full NUMERIC "1"
// Retrieval info: PRIVATE: INTENDED_DEVICE_FAMILY STRING "Cyclone IV E"
// Retrieval info: PRIVATE: LE_BasedFIFO NUMERIC "0"
// Retrieval info: PRIVATE: LegacyRREQ NUMERIC "0"
// Retrieval info: PRIVATE: MAX_DEPTH_BY_9 NUMERIC "0"
// Retrieval info: PRIVATE: OVERFLOW_CHECKING NUMERIC "0"
// Retrieval info: PRIVATE: Optimize NUMERIC "2"
// Retrieval info: PRIVATE: RAM_BLOCK_TYPE NUMERIC "0"
// Retrieval info: PRIVATE: SYNTH_WRAPPER_GEN_POSTFIX STRING "0"
// Retrieval info: PRIVATE: UNDERFLOW_CHECKING NUMERIC "0"
// Retrieval info: PRIVATE: UsedW NUMERIC "1"
// Retrieval info: PRIVATE: Width NUMERIC "10"
// Retrieval info: PRIVATE: dc_aclr NUMERIC "1"
// Retrieval info: PRIVATE: diff_widths NUMERIC "0"
// Retrieval info: PRIVATE: msb_usedw NUMERIC "1"
// Retrieval info: PRIVATE: output_width NUMERIC "10"
// Retrieval info: PRIVATE: rsEmpty NUMERIC "1"
// Retrieval info: PRIVATE: rsFull NUMERIC "0"
// Retrieval info: PRIVATE: rsUsedW NUMERIC "1"
// Retrieval info: PRIVATE: sc_aclr NUMERIC "0"
// Retrieval info: PRIVATE: sc_sclr NUMERIC "1"
// Retrieval info: PRIVATE: wsEmpty NUMERIC "1"
// Retrieval info: PRIVATE: wsFull NUMERIC "0"
// Retrieval info: PRIVATE: wsUsedW NUMERIC "1"
// Retrieval info: LIBRARY: altera_mf altera_mf.altera_mf_components.all
// Retrieval info: CONSTANT: ADD_USEDW_MSB_BIT STRING "ON"
// Retrieval info: CONSTANT: INTENDED_DEVICE_FAMILY STRING "Cyclone IV E"
// Retrieval info: CONSTANT: LPM_NUMWORDS NUMERIC "8192"
// Retrieval info: CONSTANT: LPM_SHOWAHEAD STRING "ON"
// Retrieval info: CONSTANT: LPM_TYPE STRING "dcfifo"
// Retrieval info: CONSTANT: LPM_WIDTH NUMERIC "10"
// Retrieval info: CONSTANT: LPM_WIDTHU NUMERIC "14"
// Retrieval info: CONSTANT: OVERFLOW_CHECKING STRING "ON"
// Retrieval info: CONSTANT: RDSYNC_DELAYPIPE NUMERIC "3"
// Retrieval info: CONSTANT: READ_ACLR_SYNCH STRING "OFF"
// Retrieval info: CONSTANT: UNDERFLOW_CHECKING STRING "ON"
// Retrieval info: CONSTANT: USE_EAB STRING "ON"
// Retrieval info: CONSTANT: WRITE_ACLR_SYNCH STRING "ON"
// Retrieval info: CONSTANT: WRSYNC_DELAYPIPE NUMERIC "3"
// Retrieval info: USED_PORT: aclr 0 0 0 0 INPUT GND "aclr"
// Retrieval info: USED_PORT: data 0 0 10 0 INPUT NODEFVAL "data[9..0]"
// Retrieval info: USED_PORT: q 0 0 10 0 OUTPUT NODEFVAL "q[9..0]"
// Retrieval info: USED_PORT: rdclk 0 0 0 0 INPUT NODEFVAL "rdclk"
// Retrieval info: USED_PORT: rdempty 0 0 0 0 OUTPUT NODEFVAL "rdempty"
// Retrieval info: USED_PORT: rdreq 0 0 0 0 INPUT NODEFVAL "rdreq"
// Retrieval info: USED_PORT: rdusedw 0 0 14 0 OUTPUT NODEFVAL "rdusedw[13..0]"
// Retrieval info: USED_PORT: wrclk 0 0 0 0 INPUT NODEFVAL "wrclk"
// Retrieval info: USED_PORT: wrempty 0 0 0 0 OUTPUT NODEFVAL "wrempty"
// Retrieval info: USED_PORT: wrreq 0 0 0 0 INPUT NODEFVAL "wrreq"
// Retrieval info: USED_PORT: wrusedw 0 0 14 0 OUTPUT NODEFVAL "wrusedw[13..0]"
// Retrieval info: CONNECT: @aclr 0 0 0 0 aclr 0 0 0 0
// Retrieval info: CONNECT: @data 0 0 10 0 data 0 0 10 0
// Retrieval info: CONNECT: @rdclk 0 0 0 0 rdclk 0 0 0 0
// Retrieval info: CONNECT: @rdreq 0 0 0 0 rdreq 0 0 0 0
// Retrieval info: CONNECT: @wrclk 0 0 0 0 wrclk 0 0 0 0
// Retrieval info: CONNECT: @wrreq 0 0 0 0 wrreq 0 0 0 0
// Retrieval info: CONNECT: q 0 0 10 0 @q 0 0 10 0
// Retrieval info: CONNECT: rdempty 0 0 0 0 @rdempty 0 0 0 0
// Retrieval info: CONNECT: rdusedw 0 0 14 0 @rdusedw 0 0 14 0
// Retrieval info: CONNECT: wrempty 0 0 0 0 @wrempty 0 0 0 0
// Retrieval info: CONNECT: wrusedw 0 0 14 0 @wrusedw 0 0 14 0
// Retrieval info: GEN_FILE: TYPE_NORMAL IPfifo.v TRUE
// Retrieval info: GEN_FILE: TYPE_NORMAL IPfifo.inc FALSE
// Retrieval info: GEN_FILE: TYPE_NORMAL IPfifo.cmp FALSE
// Retrieval info: GEN_FILE: TYPE_NORMAL IPfifo.bsf FALSE
// Retrieval info: GEN_FILE: TYPE_NORMAL IPfifo_inst.v FALSE
// Retrieval info: GEN_FILE: TYPE_NORMAL IPfifo_bb.v TRUE
// Retrieval info: LIB_FILE: altera_mf
