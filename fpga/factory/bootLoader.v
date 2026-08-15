/************************************************************************

    bootLoader.v

    The factory image's boot decision
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Everything the factory image exists to do. On power-up it reads the
    boot block from the EPCS, checks it, checks the application image it
    points at, and either hands the device over to that image or stays
    where it is.

    It reads the flash through the same bridge the FX3 uses, driving the
    same three registers from the inside rather than through SPI. That is
    not a saving of a few hundred logic elements; it is what makes the
    bridge the only path to the flash in this design, so the path an update
    depends on is exercised on every power-on of every unit.

    The decision is deliberately conservative. Three things have to be true
    before the application image is entered:

      - the boot block carries the magic and a layout version this image
        understands;
      - the boot block's own CRC-32 is right, which is what distinguishes a
        block that was half written from a block that describes a damaged
        image;
      - the CRC-32 of the application image itself matches what the boot
        block says it should be.

    Anything else and the unit stays here, in a gateware that cannot
    capture and says so through IMAGE_ROLE. That is the failure this whole
    two-image design is built to produce: recognisable, repairable over the
    one cable the user already has, and impossible to mistake for a working
    capture path.

    Verifying the whole application image costs about a fifth of a second
    at power-on - a few hundred kilobytes at one byte per eight flash clock
    cycles - which is inside the window the FX3 spends booting and probing
    for the register bank anyway.

************************************************************************/

module bootLoader (
    input reset_n,
    input clock,

    // The flash bridge's register window, driven from in here while
    // boot_active is high. The top level muxes these with the window
    // coming from the SPI register bank.
    output       window_write,
    output [1:0] window_address,
    output [7:0] window_write_data,
    input  [7:0] bridge_data_read,
    input        bridge_busy,

    // Reconfiguration control
    output        arm_request,
    output [23:0] boot_address,
    output        reconfigure_request,
    input         reconfiguration_armed,

    // High until the decision has been made, both to hold the bridge and
    // to tell the top level not to let anything else near it
    output boot_active
);

    // Where the boot block lives. Sector aligned, with a whole 64 KiB
    // sector to itself, and fixed for the life of the design: moving it
    // would mean re-provisioning every unit with a cable. The layout is on
    // the EPCS layout and boot flow page.
    parameter [23:0] BootBlockAddress = 24'h100000;

    // The boot block, as laid out on that page
    localparam [31:0] BootBlockMagic = 32'h44444242;  // "DDBB"
    localparam [15:0] SupportedLayoutVersion = 16'd1;
    localparam [4:0] BootBlockBytes = 5'd24;
    localparam [4:0] HeaderCrcBytes = 5'd20;

    // Flash commands. Only one is issued: the boot logic reads, and
    // nothing in any gateware here ever writes to the flash.
    localparam [7:0] CommandRead = 8'h03;

    // The bridge's window addresses
    localparam [1:0] BridgeUnlock = 2'd0;
    localparam [1:0] BridgeControl = 2'd1;
    localparam [1:0] BridgeData = 2'd2;

    // The unlock sequence, which flashBridge defines and this repeats
    // because it is a caller like any other
    localparam [31:0] UnlockSequence = 32'h444455AA;

    // Long enough for the bridge to come out of reset and the flash to be
    // ready to be selected after configuration
    localparam [7:0] SettleClocks = 8'd200;

    localparam [3:0] StateSettle = 4'd0;
    localparam [3:0] StateUnlock = 4'd1;
    localparam [3:0] StateSelectHeader = 4'd2;
    localparam [3:0] StateHeaderCommand = 4'd3;
    localparam [3:0] StateHeaderData = 4'd4;
    localparam [3:0] StateHeaderSettle = 4'd5;
    localparam [3:0] StateHeaderCheck = 4'd6;
    localparam [3:0] StateSelectImage = 4'd7;
    localparam [3:0] StateImageCommand = 4'd8;
    localparam [3:0] StateImageData = 4'd9;
    localparam [3:0] StateImageSettle = 4'd10;
    localparam [3:0] StateImageCheck = 4'd11;
    localparam [3:0] StateRelock = 4'd12;
    localparam [3:0] StateArm = 4'd13;
    localparam [3:0] StateHandOver = 4'd14;
    localparam [3:0] StateRecovery = 4'd15;

    reg  [ 3:0] state;
    reg  [ 7:0] settle_count;
    reg  [ 4:0] byte_index;
    reg  [31:0] byte_count;

    reg         window_write_reg;
    reg  [ 1:0] window_address_reg;
    reg  [ 7:0] window_write_data_reg;

    // Set when a byte has been handed to the bridge and cleared when the
    // bridge reports it finished, so that the wait is for this transfer
    // rather than for a busy line that has not risen yet
    reg         shift_started;

    // The boot block's fields, filled as the bytes arrive
    reg  [31:0] magic;
    reg  [15:0] layout_version;
    reg  [31:0] application_address;
    reg  [31:0] application_length;
    reg  [31:0] application_crc;
    reg  [31:0] header_crc;

    reg         image_valid;

    reg         crc_restart;
    reg         crc_valid;
    reg  [ 7:0] crc_data;

    wire [31:0] crc_value;

    // The address bytes of a read command, most significant first
    reg  [23:0] read_address;

    wire        bridge_idle = !bridge_busy && !shift_started;

    assign window_write        = window_write_reg;
    assign window_address      = window_address_reg;
    assign window_write_data   = window_write_data_reg;

    assign arm_request         = (state == StateArm);
    assign reconfigure_request = (state == StateHandOver);

    // The remote update block takes a byte address in the configuration
    // device, which is what the boot block carries
    assign boot_address        = application_address[23:0];

    assign boot_active         = (state != StateRecovery) && (state != StateHandOver);

    crc32 crc32_0 (
        // Inputs
        .reset_n   (reset_n),
        .clock     (clock),
        .data_valid(crc_valid),
        .data      (crc_data),
        .restart   (crc_restart),

        // Output
        .crc(crc_value)
    );

    // The boot block is well formed and describes an image this image can
    // enter. The length is bounded as well as checksummed: a block that
    // survived its CRC but claims an image running off the end of the
    // device would otherwise be read for as long as the counter took to
    // wrap.
    wire header_valid = (magic == BootBlockMagic)
        && (layout_version == SupportedLayoutVersion)
        && (application_length != 32'd0)
        && (application_address[31:23] == 9'd0)
        && ((application_address + application_length) <= 32'h00800000)
        && (header_crc == crc_value);

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            state                 <= StateSettle;
            settle_count          <= 8'd0;
            byte_index            <= 5'd0;
            byte_count            <= 32'd0;

            window_write_reg      <= 1'b0;
            image_valid           <= 1'b0;
            window_address_reg    <= 2'd0;
            window_write_data_reg <= 8'h00;
            shift_started         <= 1'b0;

            magic                 <= 32'd0;
            layout_version        <= 16'd0;
            application_address   <= 32'd0;
            application_length    <= 32'd0;
            application_crc       <= 32'd0;
            header_crc            <= 32'd0;

            crc_restart           <= 1'b0;
            crc_valid             <= 1'b0;
            crc_data              <= 8'h00;

            read_address          <= 24'd0;
        end else begin
            // Every one of these is a pulse
            window_write_reg <= 1'b0;
            crc_restart      <= 1'b0;
            crc_valid        <= 1'b0;

            // A byte handed over on the previous clock is in flight until
            // the bridge says it is not
            if (shift_started && bridge_busy) begin
                shift_started <= 1'b0;
            end

            case (state)
                StateSettle: begin
                    if (settle_count == SettleClocks) begin
                        state <= StateUnlock;
                    end else begin
                        settle_count <= settle_count + 8'd1;
                    end
                end

                StateUnlock: begin
                    // Four writes to BRIDGE_UNLOCK, one byte each
                    window_write_reg   <= 1'b1;
                    window_address_reg <= BridgeUnlock;

                    case (byte_index)
                        5'd0:    window_write_data_reg <= UnlockSequence[31:24];
                        5'd1:    window_write_data_reg <= UnlockSequence[23:16];
                        5'd2:    window_write_data_reg <= UnlockSequence[15:8];
                        default: window_write_data_reg <= UnlockSequence[7:0];
                    endcase

                    if (byte_index == 5'd3) begin
                        byte_index <= 5'd0;
                        state      <= StateSelectHeader;
                    end else begin
                        byte_index <= byte_index + 5'd1;
                    end
                end

                StateSelectHeader: begin
                    window_write_reg      <= 1'b1;
                    window_address_reg    <= BridgeControl;
                    window_write_data_reg <= 8'h01;

                    read_address          <= BootBlockAddress;
                    crc_restart           <= 1'b1;
                    byte_index            <= 5'd0;
                    state                 <= StateHeaderCommand;
                end

                StateHeaderCommand: begin
                    // The read command and its three address bytes
                    if (bridge_idle) begin
                        window_write_reg   <= 1'b1;
                        window_address_reg <= BridgeData;
                        shift_started      <= 1'b1;

                        case (byte_index)
                            5'd0:    window_write_data_reg <= CommandRead;
                            5'd1:    window_write_data_reg <= read_address[23:16];
                            5'd2:    window_write_data_reg <= read_address[15:8];
                            default: window_write_data_reg <= read_address[7:0];
                        endcase

                        if (byte_index == 5'd3) begin
                            byte_index <= 5'd0;
                            state      <= StateHeaderData;
                        end else begin
                            byte_index <= byte_index + 5'd1;
                        end
                    end
                end

                StateHeaderData: begin
                    // Every byte of the boot block is clocked out of the
                    // flash by shifting a dummy byte in its place
                    if (bridge_idle) begin
                        if (byte_index != 5'd0) begin
                            // The byte the previous shift brought back
                            if (byte_index <= HeaderCrcBytes) begin
                                crc_valid <= 1'b1;
                                crc_data  <= bridge_data_read;
                            end

                            case (byte_index - 5'd1)
                                5'd0:  magic[31:24] <= bridge_data_read;
                                5'd1:  magic[23:16] <= bridge_data_read;
                                5'd2:  magic[15:8] <= bridge_data_read;
                                5'd3:  magic[7:0] <= bridge_data_read;
                                5'd4:  layout_version[7:0] <= bridge_data_read;
                                5'd5:  layout_version[15:8] <= bridge_data_read;
                                5'd8:  application_address[7:0] <= bridge_data_read;
                                5'd9:  application_address[15:8] <= bridge_data_read;
                                5'd10: application_address[23:16] <= bridge_data_read;
                                5'd11: application_address[31:24] <= bridge_data_read;
                                5'd12: application_length[7:0] <= bridge_data_read;
                                5'd13: application_length[15:8] <= bridge_data_read;
                                5'd14: application_length[23:16] <= bridge_data_read;
                                5'd15: application_length[31:24] <= bridge_data_read;
                                5'd16: application_crc[7:0] <= bridge_data_read;
                                5'd17: application_crc[15:8] <= bridge_data_read;
                                5'd18: application_crc[23:16] <= bridge_data_read;
                                5'd19: application_crc[31:24] <= bridge_data_read;
                                5'd20: header_crc[7:0] <= bridge_data_read;
                                5'd21: header_crc[15:8] <= bridge_data_read;
                                5'd22: header_crc[23:16] <= bridge_data_read;
                                5'd23: header_crc[31:24] <= bridge_data_read;
                                default: begin
                                    // Bytes 6 and 7 are the reserved field
                                end
                            endcase
                        end

                        if (byte_index == BootBlockBytes) begin
                            // Deselect and decide
                            window_write_reg      <= 1'b1;
                            window_address_reg    <= BridgeControl;
                            window_write_data_reg <= 8'h00;
                            state                 <= StateHeaderSettle;
                        end else begin
                            window_write_reg      <= 1'b1;
                            window_address_reg    <= BridgeData;
                            window_write_data_reg <= 8'hFF;
                            shift_started         <= 1'b1;
                            byte_index            <= byte_index + 5'd1;
                        end
                    end
                end

                StateHeaderSettle: begin
                    // crc_valid is a registered pulse, so the byte it
                    // announces is folded at the end of the clock after
                    // the one that captured it. Both checks wait a clock
                    // rather than relying on how many shifts happen to
                    // separate the last CRC byte from the comparison.
                    state <= StateHeaderCheck;
                end

                StateHeaderCheck: begin
                    if (header_valid) begin
                        state <= StateSelectImage;
                    end else begin
                        state <= StateRecovery;
                    end
                end

                StateSelectImage: begin
                    window_write_reg      <= 1'b1;
                    window_address_reg    <= BridgeControl;
                    window_write_data_reg <= 8'h01;

                    read_address          <= application_address[23:0];
                    crc_restart           <= 1'b1;
                    byte_index            <= 5'd0;
                    byte_count            <= 32'd0;
                    state                 <= StateImageCommand;
                end

                StateImageCommand: begin
                    if (bridge_idle) begin
                        window_write_reg   <= 1'b1;
                        window_address_reg <= BridgeData;
                        shift_started      <= 1'b1;

                        case (byte_index)
                            5'd0:    window_write_data_reg <= CommandRead;
                            5'd1:    window_write_data_reg <= read_address[23:16];
                            5'd2:    window_write_data_reg <= read_address[15:8];
                            default: window_write_data_reg <= read_address[7:0];
                        endcase

                        if (byte_index == 5'd3) begin
                            byte_index <= 5'd0;
                            state      <= StateImageData;
                        end else begin
                            byte_index <= byte_index + 5'd1;
                        end
                    end
                end

                StateImageData: begin
                    if (bridge_idle) begin
                        if (byte_count != 32'd0) begin
                            crc_valid <= 1'b1;
                            crc_data  <= bridge_data_read;
                        end

                        if (byte_count == application_length) begin
                            window_write_reg      <= 1'b1;
                            window_address_reg    <= BridgeControl;
                            window_write_data_reg <= 8'h00;
                            state                 <= StateImageSettle;
                        end else begin
                            window_write_reg      <= 1'b1;
                            window_address_reg    <= BridgeData;
                            window_write_data_reg <= 8'hFF;
                            shift_started         <= 1'b1;
                            byte_count            <= byte_count + 32'd1;
                        end
                    end
                end

                StateImageSettle: begin
                    state <= StateImageCheck;
                end

                StateImageCheck: begin
                    image_valid <= (application_crc == crc_value);
                    state       <= StateRelock;
                end

                // Lock the bridge, which releases the active serial pins.
                //
                // This is the state whose absence cost the first bench
                // session a day. flash_drive follows the bridge's unlock,
                // so without an explicit relock the fabric is still
                // holding DCLK, nCSO and ASDO - with the flash deselected
                // - at the moment the reconfiguration it is about to
                // request needs those pins to read the application image.
                // The load then fails on a flash that can never answer,
                // the device records an nSTATUS error and reverts here,
                // and the unit cycles for ever. The firmware locks after
                // every flash operation for the same reason; this was the
                // one caller that did not.
                //
                // Locked on the recovery path too: a gateware that is not
                // deliberately talking to the flash should not be wired to
                // it, and the FX3 performs its own unlock when an update
                // starts.
                StateRelock: begin
                    window_write_reg      <= 1'b1;
                    window_address_reg    <= BridgeUnlock;
                    window_write_data_reg <= 8'h00;
                    state                 <= image_valid ? StateArm : StateRecovery;
                end

                StateArm: begin
                    // Hold until the watchdog and the boot address have
                    // been written. Only then is it safe to go: a
                    // reconfiguration with no watchdog behind it is a
                    // reconfiguration that cannot come back.
                    if (reconfiguration_armed) begin
                        state <= StateHandOver;
                    end
                end

                StateHandOver: begin
                    // The reconfiguration request is asserted by being in
                    // this state. Nothing follows it that this design will
                    // ever execute: the device reconfigures out from under
                    // the state machine.
                    state <= StateHandOver;
                end

                StateRecovery: begin
                    // Terminal, and the whole point of the image. The unit
                    // stays here, the register bank answers, IMAGE_ROLE
                    // reads 0x00, and the host offers to reinstall the
                    // gateware.
                    state <= StateRecovery;
                end

                default: begin
                    state <= StateRecovery;
                end
            endcase
        end
    end

endmodule
