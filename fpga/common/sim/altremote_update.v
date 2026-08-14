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
    reg     [23:0] written_boot_address;
    reg     [11:0] written_watchdog_value;
    reg            watchdog_enabled;
    integer        reconfigure_count;
    integer        timer_reset_count;

    reg            busy_reg;
    integer        busy_clocks;

    assign busy      = busy_reg;
    assign data_out  = {out_data_width{1'b0}};
    assign asmi_addr = {config_device_addr_width{1'b0}};
    assign asmi_rden = 1'b0;
    assign asmi_read = 1'b0;
    assign pgmout    = 3'b000;
    assign pof_error = 1'b0;

    initial begin
        written_boot_address   = 24'd0;
        written_watchdog_value = 12'd0;
        watchdog_enabled       = 1'b0;
        reconfigure_count      = 0;
        timer_reset_count      = 0;
        busy_reg               = 1'b0;
        busy_clocks            = 0;
    end

    always @(posedge clock) begin
        if (reset) begin
            busy_reg    <= 1'b0;
            busy_clocks <= 0;
        end else begin
            if (write_param && !busy_reg) begin
                busy_reg    <= 1'b1;
                busy_clocks <= WriteClocks;

                case (param)
                    3'b000: written_boot_address <= data_in[23:0];
                    3'b001: written_watchdog_value <= data_in[11:0];
                    3'b010: watchdog_enabled <= data_in[0];
                    default: begin
                        // A parameter this model does not carry. Recorded
                        // by being ignored: a testbench that expected it to
                        // land somewhere fails on the value it reads back.
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
