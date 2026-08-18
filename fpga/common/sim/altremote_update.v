/************************************************************************

    altremote_update.v

    Simulation model of the Cyclone IV remote update block
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    A test fixture, never compiled into a bitstream: Quartus resolves the
    real megafunction from its own libraries, and this file is what lets
    the free tools elaborate remoteUpdate.v without them.

    The port list is the megafunction's, from altremote_update.inc in the
    Quartus installation, and must not be changed to suit a caller - a
    model whose interface has drifted from the part is worse than no model,
    because it makes a design that cannot be synthesised simulate happily.

    What it models is what the gateware asks of the block: parameter writes
    are recorded, a write takes a few clocks during which the block is
    busy, and reconfiguration and watchdog requests are counted. What it
    deliberately does not model is the effect - a simulated device cannot
    reconfigure - so a testbench checks that the right thing was asked for
    at the right moment, and the bench checks that asking for it works.

    The recorded values are read through the hierarchy by the testbenches
    that care about them:

        dut.remote_update_0.remote_update_0.written_boot_address

************************************************************************/

module altremote_update #(
    // The names are the megafunction's, so they are neither this project's
    // parameter convention nor renameable
    parameter         operation_mode           = "remote",
    parameter         check_app_pof            = "false",
    parameter         is_epcq                  = "false",
    parameter integer config_device_addr_width = 24,
    parameter integer in_data_width            = 24,
    parameter integer out_data_width           = 29
) (
    input                     clock,
    input                     reset,
    input                     reset_timer,
    input                     reconfig,
    input                     ctl_nupdt,
    input                     write_param,
    input                     read_param,
    input [              2:0] param,
    input [              1:0] read_source,
    input [in_data_width-1:0] data_in,
    input                     asmi_busy,
    input                     asmi_data_valid,
    input [              7:0] asmi_dataout,

    output [          out_data_width-1:0] data_out,
    output [config_device_addr_width-1:0] asmi_addr,
    output                                asmi_rden,
    output                                asmi_read,
    output [                         2:0] pgmout,
    output                                pof_error,
    output                                busy
);

    // How many clocks a parameter write takes. Any small number will do:
    // what the gateware has to get right is waiting for busy to fall, not
    // the number it falls after.
    localparam integer WriteClocks = 4;

    // What the fabric asked for, for a testbench to check
    reg     [              23:0] written_boot_address;
    reg     [              11:0] written_watchdog_value;
    reg                          watchdog_enabled;
    reg                          early_confdone;
    reg                          osc_int_enabled;
    integer                      illegal_parameter_writes;
    integer                      reconfigure_count;
    integer                      timer_reset_count;

    reg                          busy_reg;
    integer                      busy_clocks;

    // What a read returns. Table 18 pairs a read_source with a param, and
    // the model answers only the pairings this design issues - anything
    // else reads as zero, so a design that asks for something it has not
    // thought about gets an obviously wrong answer rather than a plausible
    // one.
    reg     [out_data_width-1:0] data_out_reg;

    // The mode the model reports at param 000. Parameters are writable in
    // factory mode only, so a testbench that wants to prove a design
    // checks the mode can force it.
    reg     [               1:0] msm_mode;
    reg     [               4:0] trigger_condition;

    // The stored 22-bit boot address field, which is what an
    // input-register read presents in its low bits - measured on the
    // bench: writing 0x200000 reads back 0x080000.
    reg     [              21:0] boot_address_field;

    // What Past Status 1 reports: the 24-bit byte address the previous
    // configuration attempt used, and set by a testbench rather than by
    // anything here, because a simulated device cannot configure.
    reg     [              23:0] past_boot_address;

    assign busy      = busy_reg;
    assign data_out  = data_out_reg;
    assign asmi_addr = {config_device_addr_width{1'b0}};
    assign asmi_rden = 1'b0;
    assign asmi_read = 1'b0;
    assign pgmout    = 3'b000;
    assign pof_error = 1'b0;

    initial begin
        written_boot_address     = 24'd0;
        written_watchdog_value   = 12'd0;
        watchdog_enabled         = 1'b0;
        early_confdone           = 1'b0;
        osc_int_enabled          = 1'b0;
        illegal_parameter_writes = 0;
        reconfigure_count        = 0;
        timer_reset_count        = 0;
        busy_reg                 = 1'b0;
        busy_clocks              = 0;
        data_out_reg             = {out_data_width{1'b0}};
        msm_mode                 = 2'b00;  // factory
        trigger_condition        = 5'b00000;
        boot_address_field       = 22'd0;
        past_boot_address        = 24'd0;
    end

    always @(posedge clock) begin
        if (reset) begin
            busy_reg    <= 1'b0;
            busy_clocks <= 0;
        end else begin
            if (write_param && !busy_reg) begin
                busy_reg    <= 1'b1;
                busy_clocks <= WriteClocks;

                // Table 17 of the Remote Update IP User Guide (683695) for
                // Cyclone IV and Cyclone 10 LP. This is the model's own
                // statement of the part's contract rather than an echo of
                // what remoteUpdate.v happens to drive, which is the only
                // way round that makes the check worth running: a model
                // written to agree with the caller passes whatever the
                // caller does, and this project has already paid for that.
                // Both said parameter 000 for the boot address, both
                // agreed, and the board cycled between images for a day.
                case (param)
                    // cd_early. Recorded because writing it is what the
                    // first bring-up did by accident, and a testbench
                    // should be able to fail on it happening again.
                    3'b001: early_confdone <= data_in[0];
                    3'b010: written_watchdog_value <= data_in[11:0];
                    3'b011: watchdog_enabled <= data_in[0];
                    // The write presents the full 24-bit byte address;
                    // the block stores data_in[23:2] as the upper 22 bits
                    // of it and appends 2'b00 at boot (Table 17, Quartus
                    // 13.1 and later). written_boot_address records the
                    // byte address the device would boot from; the stored
                    // field is what an input-register read returns.
                    3'b100: begin
                        written_boot_address <= {data_in[23:2], 2'b00};
                        boot_address_field   <= data_in[23:2];
                    end
                    3'b101: illegal_parameter_writes <= illegal_parameter_writes + 1;
                    3'b110: osc_int_enabled <= data_in[0];
                    default: begin
                        // 000 and 111 are read only. Ignored rather than
                        // recorded: a testbench that expected a write to
                        // land fails on the value it reads back.
                    end
                endcase
            end else if (read_param && !busy_reg) begin
                busy_reg     <= 1'b1;
                busy_clocks  <= WriteClocks;

                // Zeroed first, so each pairing below assigns only the bits it
                // answers with and an unlisted one is left reading as zero.
                // Writing the zero extension into every arm instead is the same
                // model and does not fit the project's line length.
                data_out_reg <= {out_data_width{1'b0}};

                case ({
                    read_source, param
                })
                    {2'b00, 3'b000} : data_out_reg[1:0] <= msm_mode;
                    {2'b01, 3'b111} : data_out_reg[4:0] <= trigger_condition;
                    {2'b10, 3'b111} : data_out_reg[4:0] <= trigger_condition;
                    // Past Status 1: the byte address the previous
                    // configuration attempt used, 24 bits (Table 18).
                    {2'b01, 3'b100} : data_out_reg[23:0] <= past_boot_address;
                    // The input register presents the stored 22-bit field
                    // in its low bits, not the byte address.
                    {2'b11, 3'b100} : data_out_reg[21:0] <= boot_address_field;
                    {2'b11, 3'b010} : data_out_reg[11:0] <= written_watchdog_value;
                    {2'b11, 3'b011} : data_out_reg[0] <= watchdog_enabled;
                    {2'b11, 3'b110} : data_out_reg[0] <= osc_int_enabled;
                    {2'b11, 3'b001} : data_out_reg[0] <= early_confdone;
                    default: begin
                        // The zero above stands.
                    end
                endcase
            end else if (busy_reg) begin
                if (busy_clocks <= 1) begin
                    busy_reg <= 1'b0;
                end

                busy_clocks <= busy_clocks - 1;
            end

            if (reconfig) begin
                reconfigure_count <= reconfigure_count + 1;
            end

            if (reset_timer) begin
                timer_reset_count <= timer_reset_count + 1;
            end
        end
    end

endmodule
