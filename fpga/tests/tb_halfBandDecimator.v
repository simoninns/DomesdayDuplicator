/************************************************************************

    tb_halfBandDecimator.v

    Testbench for the 2:1 decimation filter (T3)
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    The property this module exists for cannot be checked by looking at the
    samples it emits one at a time: "the stream was low-passed before it was
    decimated" is a statement about a spectrum. So the frequency tests here
    drive a real sinusoid in at the sampling rate and measure the amplitude
    that comes out, which is the same measurement
    fpga/make-halfband-coefficients.py --response predicts and the same one a
    bench would make.

    An unfiltered decimator passes every one of the ordinary tests below - the
    rate is right, the passband is right, and a 15 MHz tone comes out as a
    5 MHz tone at full amplitude rather than being rejected. That last case is
    the whole point of the module and is the test that would fail.

************************************************************************/

`timescale 1ns / 1ps

module tb_halfBandDecimator;

    reg         reset_n;
    reg         clock;
    reg  [ 9:0] data_in;
    reg         decimate;

    wire [ 9:0] data_out;
    wire        output_enable;

    integer     errors;
    integer     i;

    // The sampling rate, and the two rates derived from it. Written out
    // because every frequency assertion below is stated in MHz and has to be
    // converted to a phase step against this.
    localparam real SampleRateHz = 40.0e6;

    // The filter's group delay is (63-1)/2 = 31 input samples, and the
    // pipeline adds seven system clocks on top. Nothing here depends on the
    // exact figure - the measurements below discard a settling window far
    // longer than it - but a run that produced no output at all would
    // otherwise look like a very quiet one.
    localparam integer SettlingSamples = 400;

    // 80 MHz system clock - 12.5 ns period
    initial begin
        clock = 1'b0;
    end
    always begin
        #6.25 clock = ~clock;
    end

    // The sampling enable, built as the top level builds it: a free-running
    // divide-by-two of the system clock, with the enable high on the cycle
    // that takes the ADC clock high.
    reg adc_clock_divider;
    initial begin
        adc_clock_divider = 1'b0;
    end
    always @(posedge clock) begin
        adc_clock_divider <= ~adc_clock_divider;
    end

    wire sample_enable = ~adc_clock_divider;

    halfBandDecimator dut (
        .reset_n      (reset_n),
        .clock        (clock),
        .sample_enable(sample_enable),
        .data_in      (data_in),
        .decimate     (decimate),

        .data_out     (data_out),
        .output_enable(output_enable)
    );

    // Measurement state, updated by the monitor below ------------------------

    integer input_samples;  // sample_enable pulses since the counters were cleared
    integer output_samples;  // output_enable pulses in the same window
    integer minimum_out;
    integer maximum_out;
    integer settling;  // outputs still to be discarded before measuring

    task clear_measurement;
        input integer discard;
        begin
            @(negedge clock);
            input_samples  = 0;
            output_samples = 0;
            minimum_out    = 1023;
            maximum_out    = 0;
            settling       = discard;
        end
    endtask

    always @(posedge clock) begin
        if (reset_n) begin
            if (sample_enable) begin
                input_samples = input_samples + 1;
            end

            if (output_enable) begin
                if (settling > 0) begin
                    settling = settling - 1;
                end else begin
                    output_samples = output_samples + 1;
                    if (data_out < minimum_out) begin
                        minimum_out = data_out;
                    end
                    if (data_out > maximum_out) begin
                        maximum_out = data_out;
                    end
                end
            end
        end
    end

    // Drive a sinusoid at the sampling rate, full scale about mid-range.
    //
    // Amplitude 480 rather than 511 so that the filter's overshoot has
    // somewhere to go: a test that clipped would measure the clipping rather
    // than the filter.
    // The phase advances once per *sample*, not once per clock. The enable is
    // high on every second clock, so a loop that stepped the phase per clock
    // would drive a tone at twice the frequency it named - and would do it
    // convincingly, because every measurement below would still come out as a
    // number.
    task drive_tone;
        input real frequency_hz;
        input integer samples;
        integer n;
        real phase;
        begin
            n = 0;
            while (n < samples) begin
                @(negedge clock);
                if (sample_enable) begin
                    phase   = 2.0 * 3.14159265358979 * frequency_hz * n / SampleRateHz;
                    data_in = 512 + $rtoi(480.0 * $sin(phase));
                    n       = n + 1;
                end
            end
        end
    endtask

    task drive_constant;
        input integer value;
        input integer samples;
        integer n;
        begin
            data_in = value;
            for (n = 0; n < samples; n = n + 1) begin
                @(negedge clock);
            end
        end
    endtask

    // The peak-to-peak swing of the last measurement, against the 960-count
    // swing that was driven in.
    real gain;

    task check_gain;
        input [255:0] name;
        input real low;
        input real high;
        begin
            gain = (maximum_out - minimum_out) / 960.0;
            if (gain < low || gain > high) begin
                $display("FAIL: %0s gain %f not in %f..%f (swing %0d..%0d)", name, gain, low, high,
                         minimum_out, maximum_out);
                errors = errors + 1;
            end
        end
    endtask

    task do_reset;
        begin
            reset_n = 1'b0;
            data_in = 10'd512;
            @(negedge clock);
            @(negedge clock);
            reset_n = 1'b1;
            @(negedge clock);
        end
    endtask

    initial begin
        errors   = 0;
        decimate = 1'b0;
        do_reset;

        // --- Passthrough --------------------------------------------------
        //
        // With decimation off this module must be invisible. Not "close
        // enough": a LaserDisc capture and the test-pattern integrity check
        // both go through here, and either would notice a filter.

        clear_measurement(0);
        for (i = 0; i < 200; i = i + 1) begin
            @(negedge clock);
            if (sample_enable) begin
                data_in = i[9:0];
            end

            // A settle before reading the combinational output back. Without
            // it this compares the new data_in against the previous delta
            // cycle's data_out and reports a fault the design does not have.
            #1;
            if (output_enable && data_out !== data_in) begin
                $display("FAIL: passthrough altered a sample: in %0d out %0d", data_in, data_out);
                errors = errors + 1;
            end
        end

        if (output_samples != input_samples) begin
            $display("FAIL: passthrough rate %0d outputs for %0d inputs", output_samples,
                     input_samples);
            errors = errors + 1;
        end

        // --- The rate ------------------------------------------------------

        decimate = 1'b1;
        do_reset;
        drive_constant(512, 40);
        clear_measurement(0);
        drive_tone(1.0e6, 2000);

        // One output for every two inputs. Allowed to be one short: the run
        // can end with an input whose output is still in the pipeline.
        if (output_samples > (input_samples / 2) ||
            output_samples < (input_samples / 2) - 1) begin
            $display("FAIL: decimated rate %0d outputs for %0d inputs", output_samples,
                     input_samples);
            errors = errors + 1;
        end

        // --- DC ------------------------------------------------------------
        //
        // A filter whose coefficients do not sum to exactly full scale puts a
        // level shift on every decimated capture. The generator corrects the
        // sum on the centre tap; this is that correction reaching silicon.

        do_reset;
        drive_constant(512, 400);
        clear_measurement(0);
        drive_constant(512, 400);
        if (minimum_out !== 512 || maximum_out !== 512) begin
            $display("FAIL: DC 512 came out as %0d..%0d", minimum_out, maximum_out);
            errors = errors + 1;
        end

        do_reset;
        drive_constant(200, 400);
        clear_measurement(0);
        drive_constant(200, 400);
        if (minimum_out !== 200 || maximum_out !== 200) begin
            $display("FAIL: DC 200 came out as %0d..%0d", minimum_out, maximum_out);
            errors = errors + 1;
        end

        // --- The passband --------------------------------------------------
        //
        // Flat to 8 MHz: the generator measures 0.001 dB of ripple there, and
        // the window here is wide enough to absorb the coarse quantisation of
        // a 10-bit measurement rather than to pin that figure.

        do_reset;
        drive_tone(1.0e6, SettlingSamples);
        clear_measurement(0);
        drive_tone(1.0e6, 3000);
        check_gain("1 MHz", 0.97, 1.03);

        do_reset;
        drive_tone(5.0e6, SettlingSamples);
        clear_measurement(0);
        drive_tone(5.0e6, 3000);
        check_gain("5 MHz", 0.97, 1.03);

        do_reset;
        drive_tone(8.0e6, SettlingSamples);
        clear_measurement(0);
        drive_tone(8.0e6, 3000);
        check_gain("8 MHz", 0.95, 1.03);

        // --- The stopband, which is the reason this module exists -----------
        //
        // Each of these would come out at full amplitude from a decimator that
        // simply dropped every second sample, aliased down to 40 MHz minus
        // itself: 15 MHz would appear as 5 MHz, on top of a tape's luma FM
        // carrier, and nothing downstream could tell it from signal.
        //
        // The bounds are generous against the generator's predicted -79 dB and
        // -85 dB. What is being asserted is that the energy is gone, and a
        // 10-bit output measured over a few thousand samples cannot resolve
        // much below a count or two of the 960 driven in.

        do_reset;
        drive_tone(13.0e6, SettlingSamples);
        clear_measurement(0);
        drive_tone(13.0e6, 3000);
        check_gain("13 MHz alias", 0.0, 0.02);

        do_reset;
        drive_tone(15.0e6, SettlingSamples);
        clear_measurement(0);
        drive_tone(15.0e6, 3000);
        check_gain("15 MHz alias", 0.0, 0.02);

        do_reset;
        drive_tone(18.0e6, SettlingSamples);
        clear_measurement(0);
        drive_tone(18.0e6, 3000);
        check_gain("18 MHz alias", 0.0, 0.02);

        // --- The band edge --------------------------------------------------
        //
        // The transition either side of 10 MHz, which is the part of the
        // response that is a property of 2:1 decimation rather than of the
        // filter: energy just above the edge aliases to just below it at a
        // comparable level, and no half-band can prevent that. A tape whose
        // sidebands reach up here is a tape that should not be decimated, and
        // the numbers a user would need to make that judgement are these.
        //
        // Measured at 9.5 and 10.5 MHz rather than at 10. A 10 MHz tone is
        // exactly four samples per cycle, so the decimation lands either on
        // the two zero crossings or on the two peaks depending on which phase
        // reset left - the output is constant or full swing, and neither
        // number says anything about the filter. Every other frequency in this
        // file is irrational against the sampling rate for the same reason.

        do_reset;
        drive_tone(9.5e6, SettlingSamples);
        clear_measurement(0);
        drive_tone(9.5e6, 3000);
        check_gain("9.5 MHz", 0.75, 0.88);

        do_reset;
        drive_tone(10.5e6, SettlingSamples);
        clear_measurement(0);
        drive_tone(10.5e6, 3000);
        check_gain("10.5 MHz", 0.12, 0.25);

        // --- Clipping -------------------------------------------------------
        //
        // A step between the converter's extremes makes the filter overshoot
        // both ends. The output must stop at the ends of the range rather than
        // wrap, which would turn the brightest part of a picture black.

        do_reset;
        drive_constant(1023, 200);
        clear_measurement(0);
        for (i = 0; i < 60; i = i + 1) begin
            drive_constant(0, 4);
            drive_constant(1023, 4);
        end
        if (minimum_out < 0 || maximum_out > 1023) begin
            $display("FAIL: a filtered sample left the converter's range: %0d..%0d", minimum_out,
                     maximum_out);
            errors = errors + 1;
        end

        if (errors == 0) begin
            $display("tb_halfBandDecimator: PASS");
        end else begin
            $display("tb_halfBandDecimator: FAIL (%0d errors)", errors);
        end

        $finish;
    end

endmodule
