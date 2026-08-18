/************************************************************************

    halfBandDecimator.v

    Anti-aliased 2:1 decimation of the sample stream
    Domesday Duplicator - LaserDisc RF sampler
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

    Halves the sampling rate from 40 MHz to 20 MHz for tape capture, where a
    LaserDisc's bandwidth is not needed and half the samples are half the file.

    The filter is the point. Dropping every second sample on its own folds
    everything above 10 MHz back down on top of the signal - 15 MHz would land
    on 5 MHz, directly on a tape's luma FM carrier, and nothing downstream
    could tell the alias from the signal. So the stream is low-passed at
    10 MHz first, and the two halves are one operation rather than an option
    with a caveat.

    A 63-tap half-band FIR, which is the filter this decimation is shaped for:
    its cutoff is fixed at a quarter of the sampling rate by the form of the
    filter, every second coefficient either side of the centre is exactly
    zero, and the centre is exactly one half. Sixteen multiplies therefore
    cover thirty-two taps. The coefficients, the length, the window and the
    measured response are in fpga/make-halfband-coefficients.py, which
    generated the table below.

    Every coefficient is a constant, so Quartus builds those sixteen multiplies
    as shift-and-add networks in ordinary logic rather than taking any of the
    device's 132 embedded multipliers. Measured on the whole application image
    with this module in it: 2,999 of 22,320 logic elements, no embedded
    multipliers at all, and worst-case setup slack of +0.89 ns on the 80 MHz
    clock.

    What it cannot do is protect the band edge. The response passes -6 dB at
    exactly 10 MHz and is antisymmetric about it, so energy just above 10 MHz
    aliases to just below it at a comparable level. That is 2:1 decimation
    rather than this filter, and the remedy is not to decimate a signal with
    content up there.

    Passthrough is not a special case of the filter, it is a bypass: with
    decimate low the input reaches the output unaltered on the cycle it
    arrived, and the arithmetic below runs on and is ignored. An undecimated
    capture is therefore bit-for-bit what it was before this module existed,
    which is the property that matters for a LaserDisc capture and for the
    test-pattern integrity check.

************************************************************************/

module halfBandDecimator (
    input       reset_n,
    input       clock,
    input       sample_enable,  // 1 = a new sample arrives on this edge, at 40 MHz
    input [9:0] data_in,
    input       decimate,       // 1 = filter and halve the rate

    // Outputs
    output [9:0] data_out,
    output       output_enable  // 1 = data_out carries a sample worth keeping
);

    // Taps, and the width of the arithmetic they need.
    //
    // The accumulator has to hold sixteen products of a 12-bit signed pre-sum
    // and a 16-bit signed coefficient, plus the shifted centre tap. The worst
    // case is around 2^25, so 32 bits is six bits of headroom over it rather
    // than a guess.
    localparam integer TapCount = 63;
    localparam integer PairCount = 16;
    localparam integer CentreTap = 31;
    localparam integer CoefficientBits = 16;
    localparam integer ScaleBits = 15;
    localparam integer ProductBits = 28;
    localparam integer AccumulatorBits = 32;

    // The centre coefficient is exactly half of full scale.
    //
    // Written as a multiply by a constant rather than as a shift, because that
    // is the form whose width is the width of every other product and so needs
    // no sign extension to be spelled out. It costs no multiplier: 2^14 is a
    // power of two and every synthesiser turns the multiply into wiring. The
    // generator asserts the value comes out exactly 2^14 after the DC-gain
    // correction, and fpga/tests/test_halfband_coefficients.py fails if it
    // ever does not.
    localparam signed [CoefficientBits-1:0] CentreCoefficient = 16'sd16384;

    // Rounding, added before the final right shift so that the result is
    // rounded to nearest rather than truncated towards negative infinity. A
    // truncating filter would put a half-count offset on every decimated
    // capture, which is a black-level shift rather than a rounding detail.
    localparam signed [AccumulatorBits-1:0] RoundOffset = 32'sd16384;

    // The 10-bit sample value that represents zero. The converter is unsigned
    // and the filter is not, so samples are centred on the way in and put back
    // on the way out - at the pre-add's width going in and at the
    // accumulator's coming out, so that neither needs a sign extension
    // spelling out around it.
    localparam signed [11:0] SampleOffset = 12'sd512;
    localparam signed [AccumulatorBits-1:0] SampleOffsetWide = 32'sd512;

    // The ends of the converter's range, which is what a filtered sample is
    // clipped to.
    localparam signed [AccumulatorBits-1:0] MinimumSample = 32'sd0;
    localparam signed [AccumulatorBits-1:0] MaximumSample = 32'sd1023;

    // Coefficient table, generated by fpga/make-halfband-coefficients.py
    // and checked against it by fpga/tests/test_halfband_coefficients.py.
    // Do not hand-edit: run the generator.
    //
    // 63 taps, Kaiser window beta 7.0, scaled by 2^15.
    // Each entry multiplies the sum of the two samples it is symmetric
    // across, so 16 multipliers cover 32 taps; the centre tap is exactly
    // half of full scale and is applied as a shift rather than a multiply.
    //
    //   tap pair        coefficient
    //    0, 62              -2
    //    2, 60               7
    //    4, 58             -16
    //    6, 56              32
    //    8, 54             -56
    //   10, 52              92
    //   12, 50            -143
    //   14, 48             214
    //   16, 46            -311
    //   18, 44             443
    //   20, 42            -623
    //   22, 40             877
    //   24, 38           -1261
    //   26, 36            1917
    //   28, 34           -3373
    //   30, 32           10395
    //   31 (centre)      16384

    localparam [255:0] Coefficients = {
        16'sd10395, -16'sd3373, 16'sd1917, -16'sd1261,
        16'sd877, -16'sd623, 16'sd443, -16'sd311,
        16'sd214, -16'sd143, 16'sd92, -16'sd56,
        16'sd32, -16'sd16, 16'sd7, -16'sd2
    };

    // The sample history the taps read. delay[0] is the newest.
    reg [9:0] delay[0:TapCount-1];

    // Which of the two input samples this is. The filter produces one output
    // for every two inputs and this is what says which; the phase it starts in
    // is whichever one reset left, and it does not matter - a decimated stream
    // is a decimated stream whichever of the two grids it lands on.
    reg output_phase;

    // One flag per pipeline stage. Every stage below computes on every clock
    // and the flag says which cycle's result is the real one, which is cheaper
    // in logic than gating seven stages of enable and much easier to follow.
    // Two outputs are in flight at once - the pipeline is seven deep and an
    // output is due every four clocks - and this is what keeps them apart.
    reg [6:0] stage_valid;

    integer   i;

    // Stage 1: the symmetric pre-adds, and the centre sample -------------------

    reg signed [11:0] pre_sum[0:PairCount-1];
    reg signed [11:0] centre_sample;

    // Stage 2: the products ----------------------------------------------------
    //
    // Held at the accumulator's width rather than the product's, so that every
    // add in the tree below has operands of the width it produces. A product is
    // 28 bits and the tree needs 32; widening once here is one sign extension
    // rather than one at every level.

    reg signed [AccumulatorBits-1:0] product[0:PairCount-1];
    reg signed [AccumulatorBits-1:0] centre_product;

    // Stages 3 to 5: the adder tree --------------------------------------------
    //
    // Reduced two at a time with a register between each level rather than
    // summed in one expression. Sixteen 28-bit values is a four-deep carry
    // chain, which does not close at 80 MHz on this device; there are four
    // system clocks per output sample, so the pipelining is free.

    reg signed [AccumulatorBits-1:0] sum_eight[0:7];
    reg signed [AccumulatorBits-1:0] sum_four[0:3];
    reg signed [AccumulatorBits-1:0] sum_two[0:1];

    // Stage 6: the total, and stage 7: the rounded sample ----------------------

    reg signed [AccumulatorBits-1:0] total;
    reg        [             9 : 0] filtered;

    // The sample the filter produced, rounded, re-centred and clipped.
    //
    // Clipping is not belt and braces. A sharp filter overshoots on a step -
    // the passband ripple has to go somewhere - so a converter reading full
    // scale can produce a filtered value above it, and wrapping would turn the
    // brightest part of a picture black.
    wire signed [AccumulatorBits-1:0] scaled = (total + RoundOffset) >>> ScaleBits;
    wire signed [AccumulatorBits-1:0] recentred = scaled + SampleOffsetWide;

    // The centre tap, at the same width as the sixteen products it joins.
    wire signed [ProductBits-1:0] centre_scaled = centre_sample * CentreCoefficient;

    wire signed [AccumulatorBits-1:0] centre_scaled_wide = {
        {(AccumulatorBits - ProductBits) {centre_scaled[ProductBits-1]}}, centre_scaled
    };

    wire [9:0] clipped = (recentred < MinimumSample) ? 10'd0 :
        (recentred > MaximumSample) ? 10'd1023 : recentred[9:0];

    // The bypass, both halves of it: an undecimated capture takes the sample
    // that arrived, on the cycle it arrived.
    assign data_out      = decimate ? filtered : data_in;
    assign output_enable = decimate ? stage_valid[6] : sample_enable;

    genvar pair;

    generate
        for (pair = 0; pair < PairCount; pair = pair + 1) begin : gen_taps
            // Pair `pair` is taps 2*pair and TapCount-1-2*pair, which is the
            // order the generator packed the table in: index 0 is the outermost
            // pair and index 15 the pair either side of the centre.
            wire signed [CoefficientBits-1:0] coefficient =
                $signed(Coefficients[CoefficientBits*pair+:CoefficientBits]);

            // The multiply at its own width, then sign-extended once into the
            // accumulator's. Spelling the extension out is what keeps the lint
            // check able to report a width that is genuinely wrong.
            wire signed [ProductBits-1:0] tap_product = pre_sum[pair] * coefficient;

            wire signed [AccumulatorBits-1:0] tap_product_wide = {
                {(AccumulatorBits - ProductBits) {tap_product[ProductBits-1]}}, tap_product
            };

            always @(posedge clock, negedge reset_n) begin
                if (!reset_n) begin
                    product[pair] <= {AccumulatorBits{1'b0}};
                end else begin
                    product[pair] <= tap_product_wide;
                end
            end
        end
    endgenerate

    always @(posedge clock, negedge reset_n) begin
        if (!reset_n) begin
            for (i = 0; i < TapCount; i = i + 1) begin
                delay[i] <= 10'd0;
            end

            output_phase <= 1'b0;
            stage_valid  <= 7'd0;

            for (i = 0; i < PairCount; i = i + 1) begin
                pre_sum[i] <= 12'sd0;
            end
            centre_sample  <= 12'sd0;
            centre_product <= {AccumulatorBits{1'b0}};

            for (i = 0; i < 8; i = i + 1) begin
                sum_eight[i] <= {AccumulatorBits{1'b0}};
            end
            for (i = 0; i < 4; i = i + 1) begin
                sum_four[i] <= {AccumulatorBits{1'b0}};
            end
            for (i = 0; i < 2; i = i + 1) begin
                sum_two[i] <= {AccumulatorBits{1'b0}};
            end

            total    <= {AccumulatorBits{1'b0}};
            filtered <= 10'd0;
        end else begin
            // The sample history, advanced once per input sample.
            if (sample_enable) begin
                delay[0] <= data_in;
                for (i = 1; i < TapCount; i = i + 1) begin
                    delay[i] <= delay[i-1];
                end

                output_phase <= ~output_phase;
            end

            // Stage 1. Every tap reads the history as it stands before this
            // cycle's sample is shifted in - the non-blocking assignment above
            // sees to that - so all thirty-three of them read one consistent
            // snapshot. It is the snapshot from one sample ago, which is a
            // fixed extra delay through the filter and nothing more.
            stage_valid[0] <= sample_enable & output_phase;

            for (i = 0; i < PairCount; i = i + 1) begin
                pre_sum[i] <= ($signed({2'b00, delay[2*i]}) - SampleOffset) +
                    ($signed({2'b00, delay[TapCount-1-(2*i)]}) - SampleOffset);
            end
            centre_sample <= $signed({2'b00, delay[CentreTap]}) - SampleOffset;

            // Stage 2. The centre tap goes through the same one stage the
            // sixteen multipliers do, so it reaches its register in step with
            // them and needs no delay of its own to stay there.
            stage_valid[1] <= stage_valid[0];
            centre_product <= centre_scaled_wide;

            // Stages 3 to 5. The centre joins the tree here rather than at the
            // end: it is a seventeenth value and the first level of the tree is
            // where there is room for it without a stage of its own to keep it
            // in step with the other sixteen.
            stage_valid[2] <= stage_valid[1];
            sum_eight[0]   <= product[0] + product[1] + centre_product;
            for (i = 1; i < 8; i = i + 1) begin
                sum_eight[i] <= product[2*i] + product[(2*i)+1];
            end

            stage_valid[3] <= stage_valid[2];
            for (i = 0; i < 4; i = i + 1) begin
                sum_four[i] <= sum_eight[2*i] + sum_eight[(2*i)+1];
            end

            stage_valid[4] <= stage_valid[3];
            for (i = 0; i < 2; i = i + 1) begin
                sum_two[i] <= sum_four[2*i] + sum_four[(2*i)+1];
            end

            stage_valid[5] <= stage_valid[4];
            total          <= sum_two[0] + sum_two[1];

            stage_valid[6] <= stage_valid[5];
            filtered       <= clipped;
        end
    end

endmodule
