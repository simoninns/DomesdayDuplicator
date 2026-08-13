# Hardware testing with an AWG

This page describes how to test a completed Domesday Duplicator using an arbitrary waveform
generator (AWG) rather than a LaserDisc player. Feeding the Duplicator a pure sine-wave of a
known amplitude and frequency tests the whole capture solution end-to-end — input stage,
amplifier gain, anti-alias filter, ADC, FPGA and USB — against a signal whose expected
result can be calculated in advance.

The test is worth running on any newly built board, after any repair, and whenever a capture
looks wrong in a way that could be either the player or the Duplicator.

For the theory behind the circuit under test, see the
[Hardware Guide](hardware-guide.md). For the gain switch itself, see
[Gain control and capture notes](../general/gain-control-and-capture-notes.md).


## What you need

* An **AWG** able to produce a clean sine-wave to at least 16 MHz from a 50 Ohm output, with
  the amplitude settable in millivolts peak-to-peak. Amplitude flatness matters — see the
  note under [Setting the AWG](#setting-the-awg).
* A short **50 Ohm BNC coaxial lead** to connect the AWG to the Duplicator's BNC input. Keep
  it as short as practical, as you would for a real capture.
* A **fully programmed Duplicator** — DE0-Nano bitstream and FX3 firmware both loaded — with
  the capture application running on the host.
* Optionally an **oscilloscope** and a BNC T-adaptor, to confirm what the AWG is actually
  delivering.


## The principle of the test

The ADS825 ADC accepts a **2 V peak-to-peak** input centred on a 2.5 V DC offset, so 2 V p-p
at the ADC is digital full-scale. The test targets **75% of full scale**, which is 1.5 V p-p
at the ADC. That is the same amplitude a correctly configured LaserDisc capture should
produce: high enough to use most of the ADC's range, with enough headroom left that signal
peaks do not clip.

Everything between the BNC input and the ADC is a linear amplifier whose gain is set by the
4-way DIP switch on the Duplicator PCB. So the input amplitude needed to reach 75% of full
scale is simply:

```
input amplitude (mV p-p) = 1500 / gain
```

The capture application's amplitude readout reports the peak amplitude of the captured
signal as a fraction of full scale, assuming a sine-like crest factor of √2. A pure sine
therefore reads **0.750** when the test is set up correctly — the readout is the measurement
instrument for this test, and no separate analysis of the captured file is needed.


## Preparing the capture application

In the capture application's preferences, enable:

* **Show RMS amplitude as number** — this is the 0.000 to 1.000 readout the test uses. The
  chart version can be enabled as well, but the number is what should be recorded.
* **Show advanced capture statistics** — this adds the `RecentMin:` and `RecentMax:` readouts
  and the clipped-sample counts, which give the same measurement in raw 10-bit ADC codes.

Use the **10-bit `.lds`** capture mode. The amplitude readout is derived from the same data
in every mode, but the raw code values quoted below are 10-bit values.

In 10-bit terms the ADC's range is 0 to 1023, centred on 512. A correct 75% test signal
therefore swings between approximately **code 128 and code 895**, and both clipped-sample
counts should stay at **zero** throughout.


## Setting the AWG

Set the generator to:

| Setting | Value |
| --- | --- |
| Waveform | Sine |
| Output load / termination | **50 Ohm** |
| DC offset | 0 V |
| Modulation, sweep, burst | Off |
| Amplitude | See the table below |

!!! warning "Check the output load setting first"

    An AWG displays its amplitude according to the load it has been told to expect. The
    Duplicator's input is approximately 50 Ohms, so the generator must be set for a **50 Ohm
    load**. If it is left on High-Z it will deliver **twice** the displayed amplitude, and
    every reading in this test will come out at 1.5 rather than 0.75 — with the signal
    clipping hard against both ends of the ADC range. The right-hand column of the table
    below gives the equivalent High-Z setting for generators that cannot be switched.

The amplitude required depends only on the gain configuration set on the DIP switch. Switch
positions are given in the order 1-2-3-4, where up is 1 and down is 0.

| Configuration | Switch position | Gain | AWG amplitude (50 Ohm) | AWG amplitude (High-Z) |
| --- | --- | --- | --- | --- |
| 15 | 1111 | 2.02 | 743 mV p-p | 1486 mV p-p |
| 7  | 0111 | 2.17 | 691 mV p-p | 1382 mV p-p |
| 11 | 1011 | 2.27 | 661 mV p-p | 1322 mV p-p |
| 13 | 1101 | 2.45 | 612 mV p-p | 1224 mV p-p |
| 3  | 0011 | 2.54 | 591 mV p-p | 1182 mV p-p |
| 14 | 1110 | 2.59 | 579 mV p-p | 1158 mV p-p |
| 5  | 0101 | 2.79 | 538 mV p-p | 1076 mV p-p |
| 6  | 0110 | 3.02 | 497 mV p-p | 994 mV p-p  |
| 9  | 1001 | 3.04 | 493 mV p-p | 986 mV p-p  |
| 10 | 1010 | 3.34 | 449 mV p-p | 898 mV p-p  |
| 1  | 0001 | 3.80 | 395 mV p-p | 790 mV p-p  |
| 12 | 1100 | 4.00 | 375 mV p-p | 750 mV p-p  |
| 2  | 0010 | 4.40 | 341 mV p-p | 682 mV p-p  |
| 4  | 0100 | 6.00 | 250 mV p-p | 500 mV p-p  |
| 8  | 1000 | 8.50 | 176 mV p-p | 352 mV p-p  |

Testing at a single gain setting is enough to verify the frequency response. Testing at the
two extremes — configuration 15 (2.02) and configuration 8 (8.5) — additionally confirms
that the gain switch and its resistor network are behaving, since both should produce the
same 0.750 reading from very different input amplitudes.

!!! note "AWG amplitude flatness"

    An AWG's own output amplitude falls off towards the top of its range, and a generator
    with ±0.5 dB of flatness error will produce ±6% of apparent error in this test that has
    nothing to do with the Duplicator. If the results are marginal, put an oscilloscope on a
    BNC T-adaptor at the Duplicator's input and confirm the delivered amplitude at each test
    frequency before blaming the board. Note that the ADC injects clock noise back onto the
    input, which will be visible on the oscilloscope while the DE0-Nano is fitted and
    running; measure the peak-to-peak of the underlying sine, not the noise.


## Test frequencies

The Duplicator's anti-alias filter is a 2-pole elliptic low-pass filter with a 13.2 MHz
stop-band, protecting the 20 MHz Nyquist limit of the 40 MSPS ADC. Four frequencies inside
the pass-band and one above the cut-off are enough to characterise it.

The expected readings below are relative to the measured filter response shown in the
[Hardware Guide](hardware-guide.md#filter-stage), converted to the linear amplitude readout.
Leave the AWG amplitude unchanged as the frequency is stepped.

| # | Frequency | Purpose | Expected reading | Pass |
| --- | --- | --- | --- | --- |
| 1 | 1 MHz  | Reference — flat part of the pass-band | 0.750 | 0.72 to 0.78 |
| 2 | 4 MHz  | Lower video band | 0.74 | 0.68 or above |
| 3 | 8 MHz  | Upper video band | 0.72 | 0.65 or above |
| 4 | 12 MHz | Just below the filter cut-off | 0.69 | 0.60 or above |
| 5 | 16 MHz | **Above** the cut-off — should be rejected | 0.05 to 0.10 | 0.15 or below |

Test 1 is the reference: the amplitude is set here, and every other frequency is judged
against it. Within the pass-band the filter has some designed-in ripple, so tests 2 to 4 are
expected to sit slightly below the reference — around 1 to 2 dB down at worst — but never
above it.

Test 5 is the one test that must **not** read 0.75. At 16 MHz the signal is well into the
filter's stop-band and should be attenuated by at least 15 dB, so the readout should collapse
to a small fraction of the reference. A 16 MHz signal that survives at anything like full
amplitude means the filter is not working, and any capture made with that board will contain
aliasing that cannot be removed afterwards.


## Procedure

1. Power the Duplicator, connect it to the host over USB 3.0 and start the capture
   application. Confirm the application reports the device as connected.
2. Note the DIP switch setting on the PCB and look up its gain and required AWG amplitude in
   the table above.
3. Set the AWG to sine, 50 Ohm output load, 0 V DC offset, the amplitude from the table and
   1 MHz. Leave the output **off** for now.
4. Connect the AWG to the Duplicator's BNC input with the coaxial lead, then enable the AWG
   output.
5. Start a capture and let the amplitude readout settle — it is a rolling average, so give it
   several seconds. Record the amplitude reading, `RecentMin`, `RecentMax` and both clipped
   counts.
6. If the reading is not 0.750 at 1 MHz, resolve that before going any further; see
   [Interpreting the results](#interpreting-the-results).
7. Step the AWG through 4 MHz, 8 MHz, 12 MHz and 16 MHz **without changing the amplitude**,
   recording the same readings at each frequency.
8. Stop the capture and turn the AWG output off before disconnecting anything.

Captures of a few seconds are sufficient. The files themselves can be discarded — the
readings are the result — though keeping the 1 MHz capture is useful if the data ever needs
to be examined in a decoder or spectrum tool.


### Optional: alias check

For a stricter test of the filter, set the AWG to **24 MHz** at the same amplitude. This is
above the 20 MHz Nyquist limit, so anything that reaches the ADC will be reflected back into
the capture as a 16 MHz alias. The amplitude readout should stay near zero. Any significant
reading means unfiltered energy is aliasing into the sample and the filter stage needs
investigating.


## Interpreting the results

| Observation | Likely cause |
| --- | --- |
| All readings roughly double the expected value, clipped counts rising | AWG set for a High-Z load rather than 50 Ohms |
| All readings low or high by the same proportion, at every frequency | Wrong gain assumed — check the DIP switch against the table; check the AWG amplitude with an oscilloscope |
| 1 MHz correct, high frequencies far too low | Filter component value or solder-joint fault in the LPF stage; or the AWG's own output rolling off |
| 16 MHz reading close to the reference | Filter not attenuating — check the LPF inductors and capacitors. Do not use the board for captures until this is resolved |
| `RecentMin` and `RecentMax` not symmetrical about 512 | DC offset fault — check the input stage divider (R402/R403) and the ADC reference divider (R301/R302) |
| Clipped counts rising with the reading near 0.75 | Excessive noise or distortion riding on the signal; check grounding, cable length and shielding |
| Reading unstable or drifting at all frequencies | Poor connection, unterminated cable, or interference reaching the input |

A board that reads within tolerance at all five frequencies has a verified analogue front
end, a verified anti-alias filter and a verified digital path from the ADC to the capture
file, and any remaining capture problem lies with the player, the RF cabling or the host.


## Recording the results

| Test | Frequency | AWG amplitude | Amplitude reading | RecentMin | RecentMax | Clipped | Pass |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 1 MHz  | | | | | | |
| 2 | 4 MHz  | | | | | | |
| 3 | 8 MHz  | | | | | | |
| 4 | 12 MHz | | | | | | |
| 5 | 16 MHz | | | | | | |

Record the gain configuration, the DIP switch position and the AWG model alongside the
readings — the expected values are meaningless without the gain setting, and the AWG's own
flatness is part of the measurement.
