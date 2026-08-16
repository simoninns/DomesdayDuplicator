/************************************************************************

    DomesdayDuplicatorFactory.v

    Top-level module of the factory image
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The resident image. It is written into the EPCS by JTAG when a unit is
    provisioned and is never written again in the field, so everything here
    is designed to be small, boring and permanent.

    What it does: brings up the clock, holds the FX3 interface in a safe
    idle, answers the register interface so a host can find out what it is
    talking to, gives the FX3 access to the flash through the bridge, and
    decides at power-on whether to hand over to the application image.

    What it deliberately does not do: capture. There is no ADC path, no
    FIFO, no GPIF state machine and no test generator in this image, and
    that absence is the design. An image that contained the capture logic
    would have to change whenever the capture logic changed, which is the
    opposite of resident - and a unit that fell back to it would silently
    run whatever the capture path looked like on the day it was
    provisioned, against a disc somebody is trying to preserve. Falling
    back to a gateware that plainly cannot capture is the better failure.

    Two resets, which is the one structural surprise in here. The register
    bank runs from the FX3's nReset, as the interface specification says it
    must. The boot logic, the flash bridge and the reconfiguration control
    run from a power-on reset of this image's own, because the boot
    decision must not be interruptible by the other chip's reset line: the
    FX3 is booting at the same moment, and a device that only booted its
    application image when the FX3 happened to release reset early enough
    would fail intermittently, in the field, for a reason nobody could see.

************************************************************************/

`include "version.vh"

module DomesdayDuplicatorFactory (
    input         CLOCK_50,
    inout  [33:0] GPIO0,
    inout  [33:0] GPIO1,
    output [ 7:0] LED
);

    // FX3 hardware mapping ------------------------------------------------------
    //
    // The same board, so the same pins as the application image. The
    // signal map is in application/DomesdayDuplicator.v; only the pins this
    // image has an opinion about appear here, and everything else is left
    // high-Z.

    wire        system_clock;
    wire        fx3_reset_n;
    wire        fx3_spi_clock;
    wire        fx3_spi_mosi;
    wire        fx3_spi_chip_select_n;
    wire        fx3_spi_miso;

    // The GPIF databus, held at a defined level rather than floating. The
    // FX3 is powered and running on the other side of these pins whatever
    // this image is doing, and inputs that float are inputs that dissipate
    // and pick up noise.
    wire [15:0] fx3_databus = 16'h0000;

    assign GPIO1[32]             = fx3_databus[00];
    assign GPIO1[30]             = fx3_databus[01];
    assign GPIO1[28]             = fx3_databus[02];
    assign GPIO1[26]             = fx3_databus[03];
    assign GPIO1[24]             = fx3_databus[04];
    assign GPIO1[22]             = fx3_databus[05];
    assign GPIO1[20]             = fx3_databus[06];
    assign GPIO1[18]             = fx3_databus[07];
    assign GPIO1[16]             = fx3_databus[08];
    assign GPIO1[14]             = fx3_databus[09];
    assign GPIO1[12]             = fx3_databus[10];
    assign GPIO1[10]             = fx3_databus[11];
    assign GPIO1[08]             = fx3_databus[12];
    assign GPIO1[06]             = fx3_databus[13];
    assign GPIO1[04]             = fx3_databus[14];
    assign GPIO1[02]             = fx3_databus[15];

    // PCLK keeps running. The FX3's GPIF II is a synchronous slave clocked
    // from this pin, and a slave whose clock stopped is a slave that cannot
    // be told anything - including that there is nothing to collect.
    assign GPIO1[31]             = system_clock;

    // dataAvailable low, for as long as this image is running, is the whole
    // of the capture-side contract: the FX3 never reads a databus that has
    // nothing behind it.
    assign GPIO1[27]             = 1'b0;

    // bufferError, and the three control lines the application drives to a
    // constant. Held at their inactive levels rather than left floating.
    assign GPIO1[21]             = 1'b0;
    assign GPIO1[19]             = 1'b0;
    assign GPIO1[05]             = 1'b0;
    assign GPIO1[03]             = 1'b0;

    // The register link
    assign GPIO1[13]             = fx3_spi_miso;
    assign fx3_spi_clock         = GPIO1[17];
    assign fx3_spi_mosi          = GPIO1[15];
    assign fx3_spi_chip_select_n = GPIO1[11];
    assign fx3_reset_n           = GPIO1[7];

    // Everything else on GPIO1: the FX3 outputs this image does not read,
    // and the pins the board does not use
    assign GPIO1[0]              = 1'bZ;
    assign GPIO1[1]              = 1'bZ;
    assign GPIO1[9]              = 1'bZ;
    assign GPIO1[23]             = 1'bZ;
    assign GPIO1[25]             = 1'bZ;
    assign GPIO1[29]             = 1'bZ;
    assign GPIO1[33]             = 1'bZ;

    // The whole of GPIO0 is the ADC, which this image has no use for. The
    // sampling clock included: an ADC that is not being read does not need
    // to be clocked, and this image is running precisely when nothing is
    // being read.
    assign GPIO0                 = {34{1'bZ}};

    // Clock ---------------------------------------------------------------------

    IPpllGenerator pll_generator_0 (
        // Inputs
        .inclk0(CLOCK_50),

        // Outputs
        .c0(system_clock)  // 80 MHz system clock
    );

    // Resets --------------------------------------------------------------------

    // The FX3's reset, synchronised on release exactly as the application
    // image does it, and used for the register bank alone.
    reg [1:0] reset_n_sync;

    always @(posedge system_clock, negedge fx3_reset_n) begin
        if (!fx3_reset_n) begin
            reset_n_sync <= 2'b00;
        end else begin
            reset_n_sync <= {reset_n_sync[0], 1'b1};
        end
    end

    wire       register_reset_n = reset_n_sync[1];

    // This image's own reset, released a fixed time after configuration
    // and never asserted again. Registers come up at their initial values
    // when the device is configured, so the counter starts at zero without
    // anything having to reset it - the same idiom the application image
    // uses for its ADC clock divider.
    reg  [7:0] power_on_count;

    initial begin
        power_on_count = 8'd0;
    end

    always @(posedge system_clock) begin
        if (power_on_count != 8'hFF) begin
            power_on_count <= power_on_count + 8'd1;
        end
    end

    wire        boot_reset_n = (power_on_count == 8'hFF);

    // Register interface --------------------------------------------------------

    wire        window_write_registers;
    wire [ 1:0] window_address_registers;
    wire [ 7:0] window_write_data_registers;
    wire [31:0] window_read_data;
    wire        transaction_decoded;
    wire [ 7:0] leds;

    // The register bank is shared with the application image, so it has a
    // test-mode output. There is no capture path here for it to select, and
    // there is not meant to be.
    wire        test_mode_unused;

    // For the same reason it has a capture buffer instrument, and this image
    // has no buffer. TelemetryPresent is left off, which folds the window away
    // entirely: 0x40 upwards reads zero here, exactly as it did before the
    // window was defined, and a host reading this image sees an unmapped
    // address rather than an instrument reporting a capture that cannot happen.
    wire        telemetry_latch_unused;

    // And for the same reason again it has a decimation register. There is no
    // sample stream here to decimate. DecimationPresent is left off, which
    // holds the register at its reset value and folds it away with the read
    // arm: 0x12 reads zero here, which is what an unmapped address reads and
    // is how a host tells this image's register bank from the application
    // image's without either of them having to bump the map version.
    wire [ 7:0] decimation_unused;

    spiRegisters #(
        .CommitText(`GATEWARE_COMMIT_TEXT),
        .BuildFlags(`GATEWARE_BUILD_FLAGS),

        // The one line in this file that decides what a host calls this
        // unit's state
        .ImageRole(8'h00),

        .TelemetryPresent (1'b0),
        .DecimationPresent(1'b0)
    ) spi_registers_0 (
        // Inputs
        .reset_n           (register_reset_n),
        .clock             (system_clock),
        .spi_clock         (fx3_spi_clock),
        .spi_mosi          (fx3_spi_mosi),
        .spi_chip_select_n (fx3_spi_chip_select_n),
        .window_read_data  (window_read_data),
        .diagnostics       (remote_update_diagnostics),
        .telemetry         (128'd0),
        .telemetry_geometry(48'd0),

        // Outputs
        .spi_miso           (fx3_spi_miso),
        .test_mode          (test_mode_unused),
        .decimation         (decimation_unused),
        .leds               (leds),
        .window_write       (window_write_registers),
        .window_address     (window_address_registers),
        .window_write_data  (window_write_data_registers),
        .transaction_decoded(transaction_decoded),
        .telemetry_latch    (telemetry_latch_unused)
    );

    assign LED = leds;

    // Flash bridge, and who is driving it ---------------------------------------

    wire window_write_boot;
    wire [1:0] window_address_boot;
    wire [7:0] window_write_data_boot;
    wire boot_active;

    wire [7:0] bridge_unlock_read;
    wire [7:0] bridge_control_read;
    wire [7:0] bridge_data_read;
    wire [7:0] reconfiguration_read;

    // The boot logic owns the bridge until it has made its decision, and
    // the host owns it afterwards. There is no arbitration beyond this
    // because there is no case where both want it: the boot decision
    // happens before the FX3 has finished booting, and once it is made
    // this image is either about to be replaced or is the recovery state a
    // host is repairing.
    wire window_write = boot_active ? window_write_boot : window_write_registers;
    wire [1:0] window_address = boot_active ? window_address_boot : window_address_registers;
    wire [ 7:0] window_write_data = boot_active ?
        window_write_data_boot : window_write_data_registers;

    wire flash_clock;
    wire flash_chip_select_n;
    wire flash_data_out;
    wire flash_data_in;
    wire flash_drive;

    assign window_read_data = {
        reconfiguration_read, bridge_data_read, bridge_control_read, bridge_unlock_read
    };

    flashBridge flash_bridge_0 (
        // Inputs
        .reset_n          (boot_reset_n),
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

    // Reconfiguration and the boot decision -------------------------------------

    wire        arm_request;
    wire [23:0] boot_address;
    wire        reconfigure_request;

    // BENCH DIAGNOSTIC. The remote update block's account of itself,
    // carried to the register bank and presented read-only at 0x30.
    wire [63:0] remote_update_diagnostics;

    // BENCH DIAGNOSTIC, NOT A SETTING TO SHIP. Holds the handover so the
    // block's own account of it can be read: the readings are latched in
    // the last instant before the device reconfigures, which is a window
    // no host can sample. With this set the unit stays in this image and
    // the numbers sit still at registers 0x30 to 0x37.
    remoteUpdate #(
        .DiagnosticHold(1'b0)
    ) remote_update_0 (
        // Inputs
        .reset_n            (boot_reset_n),
        .clock              (system_clock),
        .window_write       (window_write && (window_address == 2'd3)),
        .window_write_data  (window_write_data),
        .transaction_decoded(transaction_decoded),
        .arm_request        (arm_request),
        .boot_address       (boot_address),
        .reconfigure_request(reconfigure_request),

        // Outputs
        .control_read(reconfiguration_read),
        .diagnostics (remote_update_diagnostics)
    );

    bootLoader boot_loader_0 (
        // Inputs
        .reset_n              (boot_reset_n),
        .clock                (system_clock),
        .bridge_data_read     (bridge_data_read),
        .bridge_busy          (bridge_control_read[1]),
        .reconfiguration_armed(reconfiguration_read[2]),

        // Outputs
        .window_write       (window_write_boot),
        .window_address     (window_address_boot),
        .window_write_data  (window_write_data_boot),
        .arm_request        (arm_request),
        .boot_address       (boot_address),
        .reconfigure_request(reconfigure_request),
        .boot_active        (boot_active)
    );

endmodule
