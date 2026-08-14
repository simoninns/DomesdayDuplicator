/************************************************************************

    DomesdayDuplicator.v

    Top-level module
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2018-2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

`include "version.vh"

module DomesdayDuplicator (
    input         CLOCK_50,
    inout  [33:0] GPIO0,
    inout  [33:0] GPIO1,
    output [ 7:0] LED
);

    // FX3 Hardware mapping begins ------------------------------------------------

    // Generic pin-mapping for FX3 (DomDupBoard revisions 2_0 to 3_0)
    wire [15:0] fx3_databus;  // 32-bit databus (only 16-bits used)
    wire [12:0] fx3_control;  // 13-bit control bus
    wire        fx3_clock;  // FX3 GPIF Clock

    // 32-bit data bus physical mapping (output only)
    // Note: board supports 32-bits; software is limited to 16-bits
    assign GPIO1[32]       = fx3_databus[00];
    assign GPIO1[30]       = fx3_databus[01];
    assign GPIO1[28]       = fx3_databus[02];
    assign GPIO1[26]       = fx3_databus[03];
    assign GPIO1[24]       = fx3_databus[04];
    assign GPIO1[22]       = fx3_databus[05];
    assign GPIO1[20]       = fx3_databus[06];
    assign GPIO1[18]       = fx3_databus[07];
    assign GPIO1[16]       = fx3_databus[08];
    assign GPIO1[14]       = fx3_databus[09];
    assign GPIO1[12]       = fx3_databus[10];
    assign GPIO1[10]       = fx3_databus[11];
    assign GPIO1[08]       = fx3_databus[12];
    assign GPIO1[06]       = fx3_databus[13];
    assign GPIO1[04]       = fx3_databus[14];
    assign GPIO1[02]       = fx3_databus[15];

    // High-Z the unused FX3 databus pins
    assign GPIO0[02]       = 1'bZ;
    assign GPIO0[03]       = 1'bZ;
    assign GPIO0[04]       = 1'bZ;
    assign GPIO0[05]       = 1'bZ;
    assign GPIO0[06]       = 1'bZ;
    assign GPIO0[07]       = 1'bZ;
    assign GPIO0[12]       = 1'bZ;
    assign GPIO0[13]       = 1'bZ;
    assign GPIO0[14]       = 1'bZ;
    assign GPIO0[15]       = 1'bZ;
    assign GPIO0[16]       = 1'bZ;
    assign GPIO0[17]       = 1'bZ;
    assign GPIO0[18]       = 1'bZ;
    assign GPIO0[19]       = 1'bZ;
    assign GPIO0[20]       = 1'bZ;
    assign GPIO0[21]       = 1'bZ;

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
    assign GPIO1[31]       = fx3_clock;  // FX3 GPIO_16

    // 13-bit control bus physical mapping (outputs)
    assign GPIO1[27]       = fx3_control[00];  // FX3 CTL_00 GPIO_17 (output)
    assign GPIO1[21]       = fx3_control[03];  // FX3 CTL_03 GPIO_20 (output)
    assign GPIO1[19]       = fx3_control[04];  // FX3 CTL_04 GPIO_21 (output)
    assign GPIO1[13]       = fx3_control[07];  // FX3 CTL_07 GPIO_24 (output)
    assign GPIO1[05]       = fx3_control[11];  // FX3 CTL_11 GPIO_28 (output)
    assign GPIO1[03]       = fx3_control[12];  // FX3 CTL_12 GPIO_29 (output)

    // 13-bit control bus physical mapping (inputs)
    assign fx3_control[01] = GPIO1[25];  // FX3 CTL_01 GPIO_18
    assign fx3_control[02] = GPIO1[23];  // FX3 CTL_02 GPIO_19
    assign fx3_control[05] = GPIO1[17];  // FX3 CTL_05 GPIO_22
    assign fx3_control[06] = GPIO1[15];  // FX3 CTL_06 GPIO_23
    assign fx3_control[08] = GPIO1[11];  // FX3 CTL_08 GPIO_25
    assign fx3_control[09] = GPIO1[09];  // FX3 CTL_09 GPIO_26
    assign fx3_control[10] = GPIO1[07];  // FX3 CTL_10 GPIO_27

    // High-Z the unused GPIO0 pins
    assign GPIO0[0]        = 1'bZ;
    assign GPIO0[1]        = 1'bZ;
    assign GPIO0[8]        = 1'bZ;
    assign GPIO0[9]        = 1'bZ;
    assign GPIO0[10]       = 1'bZ;
    assign GPIO0[11]       = 1'bZ;
    assign GPIO0[22]       = 1'bZ;
    assign GPIO0[23]       = 1'bZ;
    assign GPIO0[24]       = 1'bZ;
    assign GPIO0[25]       = 1'bZ;
    assign GPIO0[26]       = 1'bZ;
    assign GPIO0[27]       = 1'bZ;
    assign GPIO0[28]       = 1'bZ;
    assign GPIO0[29]       = 1'bZ;
    assign GPIO0[30]       = 1'bZ;
    assign GPIO0[31]       = 1'bZ;
    assign GPIO0[32]       = 1'bZ;

    // High-Z the unused GPIO1 pins
    assign GPIO1[0]        = 1'bZ;
    assign GPIO1[1]        = 1'bZ;
    assign GPIO1[7]        = 1'bZ;
    assign GPIO1[9]        = 1'bZ;
    assign GPIO1[11]       = 1'bZ;
    assign GPIO1[15]       = 1'bZ;
    assign GPIO1[17]       = 1'bZ;
    assign GPIO1[23]       = 1'bZ;
    assign GPIO1[25]       = 1'bZ;
    assign GPIO1[29]       = 1'bZ;
    assign GPIO1[33]       = 1'bZ;

    // FX3 Signal mapping:
    //
    // Signal            GPIO     CTL     Direction Description
    //
    // CLK               GPIO16   PCLK    Output    - Data clock
    // Databus           GPIO0:15         Output    - Databus
    // data_available    GPIO_17  CTL_00  Output    - FPGA signals if data is available for reading
    // reset_n           GPIO_27  CTL_10  Input     - FX3 signals (not) reset condition
    // collect_data      GPIO_19  CTL_02  Input     - Unused
    // read_data         GPIO_18  CTL_01  Input     - FX3 signals it is reading from the databus
    //
    // input0            GPIO_20  CTL_03  Output    - Buffer error flag from FPGA
    // input1            GPIO_21  CTL_04  Output    - Unused
    // input2            GPIO_28  CTL_11  Output    - Unused
    // input3            GPIO_29  CTL_12  Output    - Unused
    //
    // spi_clock         GPIO_22  CTL_05  Input     - SPI clock from the FX3
    // spi_mosi          GPIO_23  CTL_06  Input     - SPI data from the FX3
    // spi_miso          GPIO_24  CTL_07  Output    - SPI data to the FX3
    // spi_chip_select_n GPIO_25  CTL_08  Input     - SPI chip select from the FX3 (active low)
    // (reserved)        GPIO_26  CTL_09  Input     - Unused; wired and held for a future signal

    // The four SPI lines replace what were five one-bit configuration signals, of
    // which only test mode was ever used. They reach the register bank in
    // spiRegisters, which is where test mode and the status LEDs now live; the
    // contract is the "FPGA register interface" page of the documentation site.

    // Wire definitions for FX3 GPIO mapping
    wire fx3_reset_n;
    wire fx3_data_available;
    wire fx3_read_data;
    wire fx3_buffer_error;
    wire fx3_spi_clock;
    wire fx3_spi_mosi;
    wire fx3_spi_miso;
    wire fx3_spi_chip_select_n;
    wire fx3_test_mode;

    // Signal outputs to FX3
    assign fx3_control[00]       = fx3_data_available;
    assign fx3_control[03]       = fx3_buffer_error;
    assign fx3_control[07]       = fx3_spi_miso;

    // These are currently unused, but must have a defined value
    assign fx3_control[04]       = 1'b0;
    assign fx3_control[11]       = 1'b0;
    assign fx3_control[12]       = 1'b0;

    // Signal inputs from FX3
    assign fx3_reset_n           = fx3_control[10];
    //assign fx3_unused = fx3_control[02];
    assign fx3_read_data         = fx3_control[01];

    // Signal inputs from FX3 (SPI register interface)
    assign fx3_spi_clock         = fx3_control[05];
    assign fx3_spi_mosi          = fx3_control[06];
    assign fx3_spi_chip_select_n = fx3_control[08];

    // FX3 Hardware mapping ends --------------------------------------------------


    // ADC Hardware mapping begins ------------------------------------------------

    wire [9:0] adc_databus;

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

    // ADC clock output - a divide-by-two of the system clock, generated below
    wire adc_clock;
    assign GPIO0[33] = adc_clock;

    // ADC Hardware mapping ends --------------------------------------------------


    // Application logic begins ---------------------------------------------------


    // PLL clock generation
    //
    // One 80 MHz system clock from the 50 MHz physical clock, and the whole
    // design runs from it. There is no second clock domain: the sampling rate
    // is set by decimating this clock rather than by a clock of its own, which
    // is what lets the FX3 drain faster than the ADC fills without the two
    // sides having to be synchronised to each other.
    //
    // 80 MHz because it is the lowest multiple of the 40 MSPS sampling rate
    // that leaves the FX3 room to catch up. The GPIF II interface is specified
    // to 100 MHz, so this is inside it; the ADC needs a uniform clock, so the
    // system clock has to be an exact multiple of the sampling rate, which
    // 60 MHz was not.
    wire system_clock;

    IPpllGenerator pll_generator_0 (
        // Inputs
        .inclk0(CLOCK_50),

        // Outputs
        .c0(system_clock)  // 80 MHz system clock
    );

    // The FX3's GPIF II is a synchronous slave and this pin is the clock it
    // runs from, so it is simply the system clock.
    assign fx3_clock = system_clock;

    // ADC sampling clock and the sampling instant
    //
    // A divide-by-two of the system clock, free-running and with no reset:
    // the ADC is a pipelined converter whose analogue behaviour was
    // characterised with a clock that is always present, and stopping it
    // whenever the host closes the device would be a change to the front end
    // rather than to the logic. The initial value is the power-up state, and
    // which phase it powers up in does not matter.
    //
    // sample_enable is high on the system clock edge that takes adc_clock
    // high, so the design captures the ADC bus at the same instant it did when
    // this module was clocked by the ADC clock directly - one full 40 MHz
    // period after the sample was launched.
    reg adc_clock_divider;

    initial begin
        adc_clock_divider = 1'b0;
    end

    always @(posedge system_clock) begin
        adc_clock_divider <= ~adc_clock_divider;
    end

    assign adc_clock = adc_clock_divider;

    wire       sample_enable = ~adc_clock_divider;

    // Reset synchroniser
    //
    // fx3_reset_n is driven by the FX3 and is asynchronous to the system
    // clock. Asserting asynchronously is what the design has always done and
    // is what makes a reset work with no clock; releasing it synchronously is
    // new, and is what stops two registers coming out of reset on different
    // cycles because they resolved the same asynchronous edge differently.
    reg  [1:0] reset_n_sync;

    always @(posedge system_clock, negedge fx3_reset_n) begin
        if (!fx3_reset_n) begin
            reset_n_sync <= 2'b00;
        end else begin
            reset_n_sync <= {reset_n_sync[0], 1'b1};
        end
    end

    wire        reset_n = reset_n_sync[1];

    wire        fx3_is_reading;
    wire [15:0] data_generator_out;

    // Generate 16-bit data either from the ADC or the test data generator
    dataGenerator data_generator_0 (
        // Inputs
        .reset_n       (reset_n),        // Not reset
        .clock         (system_clock),   // 80 MHz system clock
        .sample_enable (sample_enable),  // 1 = take a sample on this edge
        .adc_databus   (adc_databus),    // 10-bit ADC databus
        .test_mode_flag(fx3_test_mode),  // 1 = Test mode on

        // Outputs
        .data_out(data_generator_out)  // 16-bit data out
    );

    // FIFO buffer
    buffer buffer_0 (
        // Inputs
        .reset_n     (reset_n),             // Not reset
        .clock       (system_clock),        // 80 MHz system clock
        .write_enable(sample_enable),       // 1 = a sample is written this edge
        .data_in     (data_generator_out),  // 16-bit ADC data bus input
        .is_reading  (fx3_is_reading),      // 1 = FX3 is reading data

        // Outputs
        .data_out      (fx3_databus),         // 16-bit data output
        .data_available(fx3_data_available),  // Set if a whole packet is queued
        .buffer_error  (fx3_buffer_error)     // Set if a sample had to be dropped
    );

    // FX3 GPIF state-machine logic
    fx3StateMachine fx3_state_machine_0 (
        // Inputs
        .reset_n  (reset_n),       // Not reset
        .fx3_clock(system_clock),  // 80 MHz system clock
        .read_data(fx3_read_data), // FX3 is about to start sampling the databus

        // Output
        .fx3_is_reading(fx3_is_reading)  // Flag to indicate FX3 is sampling the databus
    );

    // SPI register bank
    //
    // The build stamp comes from version.vh, which fpga/generate-version.sh
    // writes into the build directory. The copy committed beside the sources
    // reports no commit, which is the honest answer for a lint or simulation run
    // and for anyone who compiles without running the generator first.
    wire        window_write;
    wire [ 1:0] window_address;
    wire [ 7:0] window_write_data;
    wire [31:0] window_read_data;
    wire        transaction_decoded;

    spiRegisters #(
        .CommitText(`GATEWARE_COMMIT_TEXT),
        .BuildFlags(`GATEWARE_BUILD_FLAGS),

        // This is the capture gateware, which is what a host reads out of
        // IMAGE_ROLE to know it is not looking at a unit in recovery
        .ImageRole(8'h01)
    ) spi_registers_0 (
        // Inputs
        .reset_n          (reset_n),
        .clock            (system_clock),
        .spi_clock        (fx3_spi_clock),
        .spi_mosi         (fx3_spi_mosi),
        .spi_chip_select_n(fx3_spi_chip_select_n),
        .window_read_data (window_read_data),

        // Outputs
        .spi_miso           (fx3_spi_miso),
        .test_mode          (fx3_test_mode),       // 1 = test data generator selected
        .leds               (LED),                 // Driven by the FX3, for status
        .window_write       (window_write),
        .window_address     (window_address),
        .window_write_data  (window_write_data),
        .transaction_decoded(transaction_decoded)
    );

    // Flash bridge and reconfiguration control
    //
    // The capture gateware carries these so that a gateware update is done from
    // the running application image rather than from the recovery one: the
    // factory image is for when something has gone wrong, and an update is not
    // that. Everything here is inert until the FX3 unlocks it.
    wire [7:0] bridge_unlock_read;
    wire [7:0] bridge_control_read;
    wire [7:0] bridge_data_read;
    wire [7:0] reconfiguration_read;

    wire       flash_clock;
    wire       flash_chip_select_n;
    wire       flash_data_out;
    wire       flash_data_in;
    wire       flash_drive;

    assign window_read_data = {
        reconfiguration_read, bridge_data_read, bridge_control_read, bridge_unlock_read
    };

    flashBridge flash_bridge_0 (
        // Inputs
        .reset_n          (reset_n),
        .clock            (system_clock),
        .window_write     (window_write),
        .window_address   (window_address),
        .window_write_data(window_write_data),
        .flash_data_in    (flash_data_in),

        // Outputs
        .unlock_read        (bridge_unlock_read),
        .control_read       (bridge_control_read),
        .data_read          (bridge_data_read),
        .flash_clock        (flash_clock),
        .flash_chip_select_n(flash_chip_select_n),
        .flash_data_out     (flash_data_out),
        .flash_drive        (flash_drive)
    );

    asmiBlock asmi_block_0 (
        // Inputs
        .dclk           (flash_clock),
        .chip_select_n  (flash_chip_select_n),
        .serial_data_out(flash_data_out),
        .output_enable  (flash_drive),

        // Output
        .serial_data_in(flash_data_in)
    );

    // The watchdog the factory image armed before it handed over is tickled by
    // transaction_decoded, so this image proves its fabric is alive rather than
    // merely proving it configured - which the configuration CRC already did.
    // A host reconfiguration request through this block returns the device to
    // the factory image, which then makes the boot decision again.
    remoteUpdate remote_update_0 (
        // Inputs
        .reset_n            (reset_n),
        .clock              (system_clock),
        .window_write       (window_write && (window_address == 2'd3)),
        .window_write_data  (window_write_data),
        .transaction_decoded(transaction_decoded),
        .arm_request        (1'b0),
        .boot_address       (24'd0),
        .reconfigure_request(1'b0),

        // Output
        .control_read(reconfiguration_read)
    );

endmodule
