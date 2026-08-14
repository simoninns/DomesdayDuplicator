/************************************************************************

    remoteUpdate.v

    Reconfiguration control and the configuration watchdog
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Register 0x23 of the register map, and the boot control the factory
    image's boot logic drives. Both images instantiate this; they use
    different halves of it.

    The factory image writes the application's start address and the
    watchdog timeout into the remote update block, then triggers
    reconfiguration - that is the whole of the handover. The application
    image never writes a parameter: it tickles the watchdog when the
    register interface decodes a transaction, and it triggers a plain
    reconfiguration when the host asks for one, which returns the device to
    the factory image so that the factory image can make the boot decision
    again with whatever the flash now holds.

    That asymmetry is deliberate. It means a gateware update ends with the
    device booting the way it boots after a power cycle, rather than the
    way a special case in the application image thought it should - so the
    path taken after an update is the path that has been exercised at every
    power-on since provisioning.

    The block gets a clock of its own, at a quarter of the system clock.
    That is not a preference: on this device its clock is specified for a
    25 ns minimum period and 10.1 ns minimum high and low times, which
    Quartus checks and which the 80 MHz system clock violates by a factor
    of two. 20 MHz is inside both with a wide margin, and nothing here is
    in a hurry - the parameter writes happen once at power-on and a
    watchdog tickle is a request rather than a deadline.

    Because the block runs slower than the logic driving it, every signal
    into it is a level rather than a pulse: a parameter write is held until
    the block acknowledges it by raising busy, and reconfiguration and
    tickle requests are stretched over enough system clocks that the block
    cannot miss one between its own edges.

    ------------------------------------------------------------------
    PROVISIONAL: the parameter encoding below is not yet confirmed
    ------------------------------------------------------------------

    ParamBootAddress, ParamWatchdogValue and ParamWatchdogEnable describe
    how the Cyclone IV remote update block expects its configuration
    parameters to be presented. They are written here as named constants,
    in one place and nowhere else, because they are the one part of the
    factory image that has not been verified against the device: they need
    the Cyclone IV handbook's remote system upgrade tables and a bench boot
    to confirm, and that confirmation is the first task of the factory
    image's bring-up.

    Getting them wrong fails loudly and safely - the application image is
    never entered and the unit stays in the factory image, which is the
    same state it is in before it has ever been provisioned - but they must
    be confirmed before the factory image is frozen into fielded hardware,
    because after that they cannot be changed without a cable.

************************************************************************/

module remoteUpdate (
    input reset_n,
    input clock,

    // The 0x23 part of the register window. Write bit 0 tickles the
    // watchdog and write bit 1 triggers reconfiguration; the read reports
    // what the block is doing.
    input       window_write,
    input [7:0] window_write_data,

    output [7:0] control_read,

    // From the register interface: a data byte of a framed transaction
    // completed, so the fabric is alive and talking
    input transaction_decoded,

    // Boot control, driven by the factory image's boot logic and left
    // idle by the application image. boot_address is a byte address in the
    // configuration device, which is the width the block's data path is:
    // Quartus rejects any other data_in width for this family, naming the
    // configuration device's own address width as the legal value.
    input        arm_request,
    input [23:0] boot_address,
    input        reconfigure_request
);

    // Which parameter each write addresses. See the provisional note in
    // the header.
    localparam [2:0] ParamBootAddress = 3'b000;
    localparam [2:0] ParamWatchdogValue = 3'b001;
    localparam [2:0] ParamWatchdogEnable = 3'b010;

    // The watchdog timeout, in the block's own units. The value here is the
    // largest the twelve-bit field holds, which is the deliberately
    // generous end of the range: the period has to sit comfortably above
    // the worst case of an FX3 boot plus its identity read, that figure is
    // a measurement nobody has taken yet, and a period set too short means
    // a device that drops into recovery whenever the FX3 boots slowly. It
    // is narrowed once measured, and it is measured before the factory
    // image is frozen.
    localparam [11:0] WatchdogTimeout = 12'hFFF;

    // How long a request is held for the block to see it. Two of the
    // block's clock periods is eight system clocks; sixteen is that with
    // the same margin again, and costs nothing that anything here waits on.
    localparam [4:0] HoldClocks = 5'd16;

    // The state machine that writes the three parameters and then reports
    // itself armed
    localparam [2:0] StateIdle = 3'd0;
    localparam [2:0] StateWriteTimeout = 3'd1;
    localparam [2:0] StateWriteEnable = 3'd2;
    localparam [2:0] StateWriteAddress = 3'd3;
    localparam [2:0] StateArmed = 3'd4;

    reg  [ 2:0] state;
    reg  [ 2:0] param_select;
    reg  [23:0] param_data;
    reg         armed;

    // A parameter write is held from the clock it is issued until the block
    // acknowledges it by raising busy. That is the handshake rather than a
    // pulse and a delay, because it is the one that stays correct whatever
    // the ratio between the two clocks turns out to be.
    reg         write_pending;

    reg  [ 4:0] reconfigure_hold;
    reg  [ 4:0] tickle_hold;

    // The block's clock, and its busy line brought back into this domain.
    // Two flops, because the two clocks share a source but not an edge.
    reg         update_clock_phase;
    reg         update_clock;
    reg  [ 1:0] block_busy_sync;

    wire        block_busy_raw;
    wire        block_busy = block_busy_sync[1];
    wire        block_idle = !block_busy && !write_pending;

    // A tickle from either source. The register write is what a host uses
    // deliberately; the decoded transaction is what happens on a healthy
    // device with nothing plugged into it, because the FX3 reads the
    // identity block during its own start-up.
    wire        tickle = transaction_decoded || (window_write && window_write_data[0]);
    wire        reconfigure_now = reconfigure_request || (window_write && window_write_data[1]);

    // Bit 2 is what the boot logic waits on before it hands over, and bit
    // 1 is what a host polls while a parameter write is in flight. Bit 0
    // reads back as zero: writing it tickles the watchdog, and a tickle is
    // an event rather than a state.
    assign control_read = {5'd0, armed, block_busy, 1'b0};

    // Divide by four. The phase register toggles every clock and the clock
    // itself every second one, so the result is symmetric - which matters
    // here, because the block specifies a minimum high time as well as a
    // minimum period.
    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            update_clock_phase <= 1'b0;
            update_clock       <= 1'b0;
        end else begin
            update_clock_phase <= ~update_clock_phase;

            if (update_clock_phase) begin
                update_clock <= ~update_clock;
            end
        end
    end

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            state            <= StateIdle;
            param_select     <= 3'd0;
            param_data       <= 24'd0;
            armed            <= 1'b0;
            write_pending    <= 1'b0;
            reconfigure_hold <= 5'd0;
            tickle_hold      <= 5'd0;
            block_busy_sync  <= 2'b00;
        end else begin
            block_busy_sync <= {block_busy_sync[0], block_busy_raw};

            if (write_pending && block_busy) begin
                write_pending <= 1'b0;
            end

            // Requests are stretched rather than passed straight through,
            // so that a one-clock event here is still visible to a block
            // running at a quarter of this clock.
            if (reconfigure_now) begin
                reconfigure_hold <= HoldClocks;
            end else if (reconfigure_hold != 5'd0) begin
                reconfigure_hold <= reconfigure_hold - 5'd1;
            end

            if (tickle) begin
                tickle_hold <= HoldClocks;
            end else if (tickle_hold != 5'd0) begin
                tickle_hold <= tickle_hold - 5'd1;
            end

            case (state)
                StateIdle: begin
                    if (arm_request) begin
                        param_select  <= ParamWatchdogValue;
                        param_data    <= {12'd0, WatchdogTimeout};
                        write_pending <= 1'b1;
                        state         <= StateWriteTimeout;
                    end
                end

                StateWriteTimeout: begin
                    if (block_idle) begin
                        param_select  <= ParamWatchdogEnable;
                        param_data    <= {23'd0, 1'b1};
                        write_pending <= 1'b1;
                        state         <= StateWriteEnable;
                    end
                end

                StateWriteEnable: begin
                    if (block_idle) begin
                        param_select  <= ParamBootAddress;
                        param_data    <= boot_address;
                        write_pending <= 1'b1;
                        state         <= StateWriteAddress;
                    end
                end

                StateWriteAddress: begin
                    if (block_idle) begin
                        armed <= 1'b1;
                        state <= StateArmed;
                    end
                end

                StateArmed: begin
                    // Nothing further: the boot logic decides when to go,
                    // and the application image never reaches this state at
                    // all because nothing there asks to be armed.
                    state <= StateArmed;
                end

                default: begin
                    state <= StateIdle;
                end
            endcase
        end
    end

    // The Cyclone IV remote update block. Instantiated as a megafunction
    // rather than as generated wrapper output, for the same reason
    // IPpllGenerator.v is committed as plain Verilog: the project builds
    // without any wizard having to be run.
    //
    // The parameter widths are the device's, not a choice. Quartus rejects
    // anything else for this family, by name and with the legal value: the
    // data going in is as wide as a configuration device address, and the
    // data coming back is 29 bits of status this design does not read.
    altremote_update #(
        .operation_mode          ("remote"),
        .config_device_addr_width(24),
        .in_data_width           (24),
        .out_data_width          (29),
        .check_app_pof           ("false"),
        .is_epcq                 ("false")
    ) remote_update_0 (
        // Inputs
        .clock      (update_clock),
        .reset      (~reset_n),
        .param      (param_select),
        .data_in    (param_data),
        .write_param(write_pending),
        .read_param (1'b0),
        .read_source(2'b00),
        .ctl_nupdt  (1'b0),
        .reconfig   (reconfigure_hold != 5'd0),
        .reset_timer(tickle_hold != 5'd0),

        // The block can drive an ASMI interface of its own to check the
        // application image before booting it. This design does that check
        // itself, in bootLoader, over the same bridge the update path uses
        // - so the block's own flash access is left idle.
        .asmi_busy      (1'b0),
        .asmi_data_valid(1'b0),
        .asmi_dataout   (8'h00),

        // Outputs
        .busy(block_busy_raw)
    );

endmodule
