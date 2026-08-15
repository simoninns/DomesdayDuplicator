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
    The parameter encoding, and how it was got wrong
    ------------------------------------------------------------------

    ParamBootAddress, ParamWatchdogValue and ParamWatchdogEnable are now
    taken from Tables 17 and 18 of the Remote Update IP User Guide
    (683695), for Cyclone IV and Cyclone 10 LP. Before that they were
    guessed, and all three were wrong.

    The way they were wrong is worth keeping, because the symptom pointed
    somewhere else entirely. The boot address was written to parameter
    000, which is read only, and the watchdog timeout to parameter 001,
    which is the early CONF_DONE check - so every handover ran with
    cd_early set and no boot address at all. A forced early CONF_DONE
    check fails the application configuration whatever the image is, the
    device reverts to factory, and factory hands over again: an endless
    cycle that looks the same for any boot address, which is why three
    successive corrections to the address changed nothing.

    Parameters are writable in factory mode only, which is the only mode
    this ever writes them in.

    The claim that used to stand here - that getting them wrong fails
    loudly and safely, leaving the unit in the factory image - was wrong,
    and the first bench session is what showed it. A handover that fails
    reverts to this image, which hands over again, and the unit never
    settles: nothing survives a reconfiguration, so nothing counts the
    attempts. The failure mode of a bad handover is an endlessly cycling
    unit recoverable only with a cable, and the boot logic learning to
    decline a second attempt is still open.

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
    input        reconfigure_request,

    // What the block says about itself, latched once during arming and
    // held. Read-only, and read through the register bank rather than
    // acted on: every failure of this handover so far has looked identical
    // from outside, and these are the three numbers that tell them apart.
    output [63:0] diagnostics
);

    // BENCH DIAGNOSTIC, NOT A SETTING TO SHIP. With this set the block is
    // never asked to reconfigure, so the unit stays in the factory image
    // with the readings below sitting still to be read. A device that
    // hands over cannot be asked why it handed over.
    parameter DiagnosticHold = 1'b0;

    // BENCH DIAGNOSTIC, NOT A SETTING TO SHIP. Clocks to wait after the
    // readings are taken and before the handover is allowed.
    //
    // The readings are latched microseconds before the device
    // reconfigures, which is a window nothing can sample - so with a delay
    // here the cycle becomes long enough to read, and each pass reports
    // the trigger condition of the pass before it. That is the one number
    // that says whether the application image is being attempted and
    // rejected or never attempted at all. Zero disables the wait.
    parameter [27:0] HandOverDelayClocks = 28'd0;

    // Which parameter each write addresses. Table 17 of the Remote Update
    // IP User Guide, for Cyclone IV and Cyclone 10 LP:
    //
    //     000  master state machine current state mode   read only
    //     001  force early CONF_DONE check (cd_early)     1 bit
    //     010  watchdog timeout value                    12 bits
    //     011  watchdog enable                            1 bit
    //     100  boot address                              22 bits
    //     101  illegal
    //     110  force internal oscillator as startup clock 1 bit
    //     111  reconfiguration trigger condition         read only
    //
    // All three of these were wrong in the first bench session, and the
    // one that mattered was not the one anybody was looking at: the
    // address went to 000, which is read only, while the timeout value
    // went to 001 and set cd_early. A forced early CONF_DONE check fails
    // the application configuration however good the image is, so the
    // device reverted to factory, handed over again, and cycled - and it
    // cycled identically whatever the boot address was, which is what
    // made the address look guilty for so long.
    //
    // Osc_int and Cd_early are not optional, although this design once
    // treated them as if they were. Cyclone IV Device Handbook, Table
    // 8-23: "The Cd_early and Osc_int option bits for the application
    // configuration must be turned on by the factory configuration."
    // Osc_int gives the application configuration's startup state machine
    // a clock; without it the data loads and the startup hangs, the
    // device records an nSTATUS error and reverts - which presented as a
    // reconfiguration loop that survived every correction to the boot
    // address, because the address was never the problem. Cd_early makes
    // the block check that a valid image of the right size exists at the
    // boot address before it commits to loading it.
    localparam [2:0] ParamMsmMode = 3'b000;
    localparam [2:0] ParamCdEarly = 3'b001;
    localparam [2:0] ParamWatchdogValue = 3'b010;
    localparam [2:0] ParamWatchdogEnable = 3'b011;
    localparam [2:0] ParamBootAddress = 3'b100;
    localparam [2:0] ParamOscInt = 3'b110;
    localparam [2:0] ParamTriggerCondition = 3'b111;

    // The watchdog timeout, in the block's own units. The value here is the
    // largest the twelve-bit field holds, which is the deliberately
    // generous end of the range: the period has to sit comfortably above
    // the worst case of an FX3 boot plus its identity read, that figure is
    // a measurement nobody has taken yet, and a period set too short means
    // a device that drops into recovery whenever the FX3 boots slowly. It
    // is narrowed once measured, and it is measured before the factory
    // image is frozen.
    localparam [11:0] WatchdogTimeout = 12'hFFF;

    localparam WatchdogEnable = 1'b1;

    // How long a request is held. Not a synchroniser margin: the handbook
    // requires RU_nCONFIG - the block's reconfig input - to be asserted
    // for a minimum of 250 ns "to ensure the successful reconfiguration
    // between the pages", and gives reset_timer the same floor. The
    // previous value was sixteen system clocks, 200 ns, reasoned from the
    // clock ratio alone - under the minimum on every handover. Sixty-four
    // clocks is 800 ns: the constraint met three times over, and nothing
    // here waits on it.
    localparam [6:0] HoldClocks = 7'd64;

    // read_param is sampled by the block on its own clock, a quarter of
    // this one, so four system clocks is one of its cycles. Eight is that
    // with a cycle in hand and still short enough to be the single cycle
    // the guide asks for.
    localparam [3:0] ReadAssertClocks = 4'd8;

    // How long to wait before sampling data_out. The block's reads take a
    // handful of its own clocks; a thousand system clocks is 12.5 us,
    // which is hundreds of them, and the whole sequence still finishes in
    // well under a millisecond.
    localparam [9:0] ReadSettleClocks = 10'd1000;

    // The state machine that writes the three parameters and then reports
    // itself armed
    localparam [3:0] StateIdle = 4'd0;
    localparam [3:0] StateWriteTimeout = 4'd1;
    localparam [3:0] StateWriteEnable = 4'd2;
    localparam [3:0] StateWriteOscInt = 4'd3;
    localparam [3:0] StateWriteCdEarly = 4'd4;
    localparam [3:0] StateWriteAddress = 4'd5;
    localparam [3:0] StateReadMode = 4'd6;
    localparam [3:0] StateReadTrigger = 4'd7;
    localparam [3:0] StateReadBoot = 4'd8;
    localparam [3:0] StateReadTimer = 4'd9;
    localparam [3:0] StateReadWdEn = 4'd10;
    localparam [3:0] StateReadOscInt = 4'd11;
    localparam [3:0] StateReadCdEarly = 4'd12;
    localparam [3:0] StateHoldOff = 4'd13;
    localparam [3:0] StateArmed = 4'd14;

    // Which read each of those states issues. Table 18: read_source picks
    // current state, one of the two previous state registers, or the input
    // register that a write lands in.
    localparam [1:0] SourceCurrent = 2'b00;
    localparam [1:0] SourcePrevious1 = 2'b01;
    localparam [1:0] SourceInput = 2'b11;

    reg  [ 3:0] state;
    reg  [27:0] hold_off;
    reg  [ 2:0] param_select;
    reg  [23:0] param_data;
    reg         armed;

    // A parameter write is held from the clock it is issued until the block
    // acknowledges it by raising busy. That is the handshake rather than a
    // pulse and a delay, because it is the one that stays correct whatever
    // the ratio between the two clocks turns out to be.
    reg         write_pending;

    // Reads are timed rather than handshaken, and that is deliberate.
    //
    // The first version waited for busy the way the writes do, and the
    // block never raised it - so the read never finished, the state
    // machine never armed, and the boot logic sat waiting for a handover
    // that could not come. A diagnostic that can stop a device booting is
    // worse than no diagnostic, so this one cannot: read_param is asserted
    // for a bounded number of clocks, the answer is sampled a fixed time
    // later whether or not the block ever said it was busy, and the
    // sequence always ends. The guide asks for read_param to be asserted
    // for a single clock cycle, which is what ReadAssertClocks is for.
    reg  [ 1:0] read_source_select;
    reg  [ 3:0] read_assert;
    reg  [ 9:0] read_timer;

    reg  [ 1:0] diag_mode;
    reg  [ 4:0] diag_trigger;
    reg  [23:0] diag_boot_address;
    reg  [11:0] diag_timer;
    reg         diag_wd_en;
    reg         diag_osc_int;
    reg         diag_cd_early;
    reg         diag_complete;

    wire [28:0] block_data_out;

    reg  [ 6:0] reconfigure_hold;
    reg  [ 6:0] tickle_hold;

    // The block's clock, and its busy line brought back into this domain.
    // Two flops, because the two clocks share a source but not an edge.
    reg         update_clock_phase;
    reg         update_clock;
    reg  [ 1:0] block_busy_sync;

    wire        block_busy_raw;
    wire        block_busy = block_busy_sync[1];
    wire        block_idle = !block_busy && !write_pending;

    // The read has finished when the settling time has run out. Nothing
    // here waits on the block agreeing that it has.
    wire        read_done = (read_timer == 10'd0);

    // A signature in the top two bytes, so that a host reading zeroes here
    // knows it is looking at gateware without this instrument rather than
    // at a block that answered with nothing.
    // One byte of signature, one packed status byte, then the two
    // addresses that answer the only question left: the address the
    // previous configuration attempt actually used, and the address
    // staged in the input register for the next one.
    // Every field of the input register, read back after the writes: the
    // question this instrument answers now is not which address was
    // staged but whether each write landed at all.
    assign diagnostics = {8'hDD,
                          diag_complete, diag_mode, diag_trigger,
                          diag_boot_address, diag_timer,
                          diag_wd_en, diag_osc_int, diag_cd_early, 9'd0};

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
            read_source_select <= 2'b00;
            read_assert        <= 4'd0;
            read_timer         <= 10'd0;
            hold_off           <= 28'd0;
            diag_mode          <= 2'd0;
            diag_trigger       <= 5'd0;
            diag_boot_address  <= 24'd0;
            diag_timer         <= 12'd0;
            diag_wd_en         <= 1'b0;
            diag_osc_int       <= 1'b0;
            diag_cd_early      <= 1'b0;
            diag_complete      <= 1'b0;
            reconfigure_hold <= 7'd0;
            tickle_hold      <= 7'd0;
            block_busy_sync  <= 2'b00;
        end else begin
            block_busy_sync <= {block_busy_sync[0], block_busy_raw};

            if (write_pending && block_busy) begin
                write_pending <= 1'b0;
            end

            if (read_assert != 4'd0) begin
                read_assert <= read_assert - 4'd1;
            end

            if (read_timer != 10'd0) begin
                read_timer <= read_timer - 10'd1;
            end

            // Requests are stretched rather than passed straight through,
            // so that a one-clock event here is still visible to a block
            // running at a quarter of this clock.
            if (reconfigure_now) begin
                reconfigure_hold <= HoldClocks;
            end else if (reconfigure_hold != 7'd0) begin
                reconfigure_hold <= reconfigure_hold - 7'd1;
            end

            if (tickle) begin
                tickle_hold <= HoldClocks;
            end else if (tickle_hold != 7'd0) begin
                tickle_hold <= tickle_hold - 7'd1;
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
                        param_data    <= {23'd0, WatchdogEnable};
                        write_pending <= 1'b1;
                        state         <= StateWriteEnable;
                    end
                end

                StateWriteEnable: begin
                    if (block_idle) begin
                        param_select  <= ParamOscInt;
                        param_data    <= {23'd0, 1'b1};
                        write_pending <= 1'b1;
                        state         <= StateWriteOscInt;
                    end
                end

                StateWriteOscInt: begin
                    if (block_idle) begin
                        param_select  <= ParamCdEarly;
                        param_data    <= {23'd0, 1'b1};
                        write_pending <= 1'b1;
                        state         <= StateWriteCdEarly;
                    end
                end

                StateWriteCdEarly: begin
                    if (block_idle) begin
                        // The full 24-bit byte address. The block keeps 22
                        // bits of it - Table 18 gives the write width as 22
                        // - and the two it discards are the low ones, which
                        // are always zero here because images are sector
                        // aligned. Supplying the byte address rather than
                        // pre-shifting it is what Table 17 describes for
                        // Quartus 13.1 and later, where boot_address[23..2]
                        // are the bits the block takes.
                        param_select  <= ParamBootAddress;

                        // The full 24-bit byte address, unshifted. Table 17
                        // (Quartus 13.1 and later, 3-byte-addressing
                        // devices): data_in[23:2] is stored as the upper 22
                        // bits of the 24-bit boot address and 2'b00 is
                        // appended at boot. A read of the input register
                        // therefore shows address >> 2 - measured on the
                        // bench as 0x080000 for 0x200000, which is the
                        // stored field and not, as one debugging session
                        // concluded, the address the device would boot
                        // from. The pre-shift that conclusion produced put
                        // the boot address off the end of the flash.
                        param_data    <= boot_address;
                        write_pending <= 1'b1;
                        state         <= StateWriteAddress;
                    end
                end

                // The three reads. They happen before armed is asserted,
                // because armed is what tells the boot logic it may hand
                // over - and once it does, nothing here gets another
                // chance to ask.
                StateWriteAddress: begin
                    if (block_idle) begin
                        param_select       <= ParamMsmMode;
                        read_source_select <= SourceCurrent;
                        read_assert        <= ReadAssertClocks;
                        read_timer         <= ReadSettleClocks;
                        state              <= StateReadMode;
                    end
                end

                StateReadMode: begin
                    if (read_done) begin
                        diag_mode          <= block_data_out[1:0];
                        param_select       <= ParamTriggerCondition;
                        read_source_select <= SourcePrevious1;
                        read_assert        <= ReadAssertClocks;
                        read_timer         <= ReadSettleClocks;
                        state              <= StateReadTrigger;
                    end
                end

                StateReadTrigger: begin
                    if (read_done) begin
                        diag_trigger       <= block_data_out[4:0];
                        param_select       <= ParamBootAddress;
                        read_source_select <= SourceInput;
                        read_assert        <= ReadAssertClocks;
                        read_timer         <= ReadSettleClocks;
                        state              <= StateReadBoot;
                    end
                end

                StateReadBoot: begin
                    if (read_done) begin
                        diag_boot_address  <= block_data_out[23:0];
                        param_select       <= ParamWatchdogValue;
                        read_source_select <= SourceInput;
                        read_assert        <= ReadAssertClocks;
                        read_timer         <= ReadSettleClocks;
                        state              <= StateReadTimer;
                    end
                end

                StateReadTimer: begin
                    if (read_done) begin
                        diag_timer         <= block_data_out[11:0];
                        param_select       <= ParamWatchdogEnable;
                        read_source_select <= SourceInput;
                        read_assert        <= ReadAssertClocks;
                        read_timer         <= ReadSettleClocks;
                        state              <= StateReadWdEn;
                    end
                end

                StateReadWdEn: begin
                    if (read_done) begin
                        diag_wd_en         <= block_data_out[0];
                        param_select       <= ParamOscInt;
                        read_source_select <= SourceInput;
                        read_assert        <= ReadAssertClocks;
                        read_timer         <= ReadSettleClocks;
                        state              <= StateReadOscInt;
                    end
                end

                StateReadOscInt: begin
                    if (read_done) begin
                        diag_osc_int       <= block_data_out[0];
                        param_select       <= ParamCdEarly;
                        read_source_select <= SourceInput;
                        read_assert        <= ReadAssertClocks;
                        read_timer         <= ReadSettleClocks;
                        state              <= StateReadCdEarly;
                    end
                end

                StateReadCdEarly: begin
                    if (read_done) begin
                        diag_cd_early <= block_data_out[0];
                        diag_complete <= 1'b1;
                        hold_off      <= HandOverDelayClocks;
                        state         <= StateHoldOff;
                    end
                end

                StateHoldOff: begin
                    if (hold_off == 28'd0) begin
                        armed <= 1'b1;
                        state <= StateArmed;
                    end else begin
                        hold_off <= hold_off - 28'd1;
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
        .read_param (read_assert != 4'd0),
        .read_source(read_source_select),
        .ctl_nupdt  (1'b0),
        .reconfig   (!DiagnosticHold && (reconfigure_hold != 7'd0)),
        .reset_timer(tickle_hold != 7'd0),

        // The block can drive an ASMI interface of its own to check the
        // application image before booting it. This design does that check
        // itself, in bootLoader, over the same bridge the update path uses
        // - so the block's own flash access is left idle.
        .asmi_busy      (1'b0),
        .asmi_data_valid(1'b0),
        .asmi_dataout   (8'h00),

        // Outputs
        .busy    (block_busy_raw),
        .data_out(block_data_out)
    );

endmodule
