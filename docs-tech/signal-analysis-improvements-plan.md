# Signal Analysis Improvements (ddd-gui Monitoring Panels) — Review and Plan

## Purpose

`ddd-gui` carries four live signal displays — waveform, spectrum, spectrogram and
amplitude history — and the analysis library under `ddd-gui/src/analysis/` that feeds
them. This document reviews that stack against the established practice for audio and RF
signal analysis (oscilloscopes, spectrum analysers, waterfall displays, STFT-based
spectrograms), and plans the changes that close the gaps it finds.

The review's headline: the measurement arithmetic is largely right — calibration,
windowing, decimation and threading all follow the norms — but the *presentation* is a
general-purpose one that has not been specialised for the signal this instrument exists
to capture. A LaserDisc RF signal lives between roughly 200 kHz and 13.2 MHz, with an FM
video carrier around 8 MHz; the waveform display cannot show a single cycle of that
carrier and has no trigger to hold it still, the frequency axes are linear where the norm
for this kind of monitoring is logarithmic, and the spectrum throws away seven eighths of
every snapshot it is handed.

This plan targets `ddd-gui` only. The legacy `gui/` application is being retired and
gains none of this.

## Authoritative references (in-tree)

- Conventions and gates: [AGENTS.md](../AGENTS.md) (Google style in `ddd-gui/`,
  clang-format and clang-tidy as errors, no git operations without an explicit request)
- The analysis library under review:
  [spectrum_analyser.h](../ddd-gui/src/analysis/spectrum_analyser.h) /
  [.cpp](../ddd-gui/src/analysis/spectrum_analyser.cpp),
  [fourier_transform.h](../ddd-gui/src/analysis/fourier_transform.h),
  [waveform_mapping.h](../ddd-gui/src/analysis/waveform_mapping.h),
  [spectrogram_history.h](../ddd-gui/src/analysis/spectrogram_history.h),
  [amplitude_history.h](../ddd-gui/src/analysis/amplitude_history.h)
- The panels that draw it:
  [waveform_panel.cpp](../ddd-gui/src/gui/waveform_panel.cpp),
  [spectrum_panel.cpp](../ddd-gui/src/gui/spectrum_panel.cpp),
  [amplitude_panel.cpp](../ddd-gui/src/gui/amplitude_panel.cpp),
  [theme_color_tokens.h](../ddd-gui/src/gui/theme_color_tokens.h) (spectrogram colormap)
- The data path that sets the cadence:
  [monitor_tap.h](../ddd-gui/src/capture/monitor_tap.h) (64 KiB snapshots, wait-free,
  ~9 published per second), [analysis_worker.h](../ddd-gui/src/gui/analysis_worker.h)
  (30 Hz poll on its own thread)
- Tests to extend: `ddd-gui/tests/analysis/` (one file per analysis module),
  `ddd-gui/tests/gui/widget/test_waveform_panel.cpp`, `test_spectrum_panel.cpp`
- User documentation to update when the panels change:
  [docs/content/capture-gui/signal-analysis.md](../docs/content/capture-gui/signal-analysis.md)

Out of scope: `analysis_dialog.*` / `test_data_analysis.*` (test-pattern verification of
recorded captures — a different instrument), and the statistics panel.

## The signal being displayed

Every choice below follows from what is actually in the band, so it is worth stating
once. At 40 Msps the displayable range is DC–20 MHz, and the board's anti-aliasing
filter corners at 13.2 MHz. Within that:

| Component | Frequency | Cycles per sample at 40 Msps |
| --- | --- | --- |
| EFM digital audio | ~0.2–1.75 MHz | — |
| PAL analogue audio carriers | 683.59 kHz, 1066.4 kHz | ~40 samples/cycle |
| NTSC analogue audio carriers | 2.3 MHz, 2.8 MHz | ~15 samples/cycle |
| Video FM carrier (PAL) | 6.76–7.9 MHz | ~5.5 samples/cycle |
| Video FM carrier (NTSC) | 7.6–9.3 MHz | ~4.6 samples/cycle |
| FM sidebands / filter corner | up to 13.2 MHz | ~3 samples/cycle |

Two consequences run through the whole plan. First, the interesting content spans about
1.8 decades — which is why a logarithmic frequency axis works here and a linear one
spends half its width on the nearly empty stretch above 8 MHz. Second, the video carrier
is only 4–6 samples per cycle, which is below the density where join-the-dots drawing
looks like the waveform: any cycle-level view needs reconstruction, exactly as a digital
oscilloscope provides.

The data source shapes everything too: the monitor tap publishes 32,768-sample (819.2 µs)
snapshots about nine times a second. The panels see under 1% of the sample stream, in
coherent 819 µs windows. Nothing in this plan changes the tap; the plan is about using
each snapshot fully instead of partially.

## What exists today, measured against the norms

### Where the code already follows established practice

These are correct and are kept, not revisited:

- **Periodic Hann window**, normalised by the window sum (coherent gain), so a
  full-scale sine reads 0 dBFS — the standard amplitude calibration.
- **Single-sided spectrum with mirror-energy doubling**, DC and Nyquist excepted.
- **Averaging in the power domain**, not in dB — the correct analogue of a spectrum
  analyser's video filter, and the exponential form is the norm for live displays.
- **Peak hold** as a separate trace, drawn under the live one.
- **Max-decimation from bins to pixels** in both the spectrum trace and the
  spectrogram, so a one-bin carrier can never fall between pixels. This is the
  correct reduction for a carrier-finding display (mean-decimation is the classic
  mistake).
- **Min/max envelope decimation** in the waveform plot — the standard DSO answer to
  more samples than pixels.
- **Display floor at −100 dB** against a 10-bit converter whose per-bin quantisation
  noise floor sits near −95 dBFS at this transform size: the scale ends where the
  physics does.
- **Spectrogram stored as levels, not pixels**, so theme and range changes re-render
  history — better than the norm, most waterfalls cannot do this.
- **A magma-like perceptual colormap** anchored to the window background, in both
  themes.
- **Analysis off the GUI thread**, fed by a wait-free triple buffer that can never
  make the capture wait. Frames dropped, never queued.

### Gap 1 — the waveform display is not usable at the carrier's timescale

This is the largest deviation from the norms, and the user-visible one.

1. **No trigger.** The plot draws each snapshot from its first sample, so every frame
   lands at an arbitrary carrier phase. Every oscilloscope since the 1940s holds a
   repetitive waveform still by triggering — aligning each sweep to a level crossing.
   Without one, any span short enough to show carrier cycles shows a different slice
   each frame: a 9 Hz shimmer, unreadable except as an envelope.
2. **Spans are wrong for the band.** The shortest offered span is 10 µs — **80 cycles**
   of an 8 MHz carrier. (The comment in `waveform_mapping.h` claiming 10 µs is "about
   two cycles of a LaserDisc FM carrier" is wrong by 40×: two cycles is 0.25 µs. The
   error matters because the span ladder was evidently designed to it.) There is no
   span at which individual carrier cycles are visible.
3. **No reconstruction.** At 4–6 samples per cycle, plotting samples joined by straight
   lines (or min/max bars) draws triangles, not the signal. The DSO norm below ~10
   samples per cycle is sin(x)/x (windowed-sinc) interpolation; anything shown at a
   sub-microsecond span needs it.
4. **No time axis.** The plot has unlabeled vertical gridlines; the norm is a labeled
   time axis (or time/div readout). The cursor readout partly compensates but requires
   pointing.
5. **Refresh is tied to the snapshot rate** (~9 Hz) with one sweep drawn per snapshot.
   The persistence option then builds an envelope over about a second. But each 819 µs
   snapshot *contains* hundreds of carrier-cycle sweeps; drawing one and discarding the
   rest is why the display feels slow. An analogue scope triggering on this signal
   would redraw tens of thousands of sweeps a second; the digital norm (DPO-style
   intensity-graded persistence) gets the same effect by rendering many triggered
   sweeps per acquisition. That is available here for free, from data already in hand —
   this is the correct reading of "choose the refresh rate from the expected RF
   frequency": the effective sweep rate must relate to the carrier, not to the USB
   snapshot cadence.

### Gap 2 — the spectrum uses one eighth of each snapshot

`SpectrumAnalyser::Analyse()` transforms the first 4,096 samples of a 32,768-sample
snapshot and ignores the other 28,672. The norm for estimating a power spectrum from a
block of samples is **Welch's method**: split the block into overlapping windowed
segments, transform each, average the power. A 32,768-sample snapshot yields 15
segments of 4,096 at 50% overlap; averaging them cuts the variance of the noise floor
by roughly 3.5× in amplitude terms per snapshot — the trace stops boiling — at a cost
of fourteen more small FFTs per snapshot (~15 ms/s of one core on the analysis thread,
which today idles).

Related, smaller gaps against spectrum-analyser norms:

- **Fixed resolution bandwidth.** 4,096 points at 40 Msps is a 9.77 kHz bin
  (Hann ENBW ≈ 14.6 kHz). That resolves the video carrier but is coarse against the
  PAL audio carriers at 683/1066 kHz. Analysers let the user trade RBW against
  update stability; here that is one option (transform size 4,096 / 8,192 / 16,384,
  with 15 / 7 / 3 Welch segments respectively — the snapshot bounds the maximum).
- **No RBW / averaging readout.** An analyser always states its RBW; without it a
  reader cannot judge whether a narrow feature is real or one bin wide.
- **No peak marker.** Norm is at least one marker reporting the strongest peak's
  frequency and level without pointing at it.
- **Axis label says "dB"**, cursor says "dBFS". They are the same number; the axis
  should say dBFS too.

### Gap 3 — linear frequency axes

Both the spectrum and the spectrogram map frequency linearly. For audio work the
frequency axis is logarithmic essentially always; for RF monitoring of a band like this
one — content from 200 kHz to 13.2 MHz, nearly two decades — log is also the norm, and
the linear axis is measurably wasteful: everything below 2 MHz (EFM, PAL audio
carriers) is compressed into the leftmost seventh of the plot, while the octave of
near-nothing between 10 and 20 MHz gets more room than the entire digital audio band.

On a log axis from 100 kHz to 14 MHz the PAL audio carriers, the EFM band, the NTSC
audio carriers and the video carrier are each legible regions. The 9.77 kHz bin spacing
supports it: at 100 kHz adjacent bins are 10% of frequency apart, comfortably finer than
a pixel on a 2.15-decade axis. Linear remains the right view for examining the filter
corner and FM sideband symmetry, so this is a toggle, defaulting to log — matching
common analyser practice where span/axis is a display setting, not a measurement one.
The mel scale in the referenced spectrogram guidance is the perceptual-audio version of
the same idea; log frequency is its RF counterpart. (The guide's other prescriptions —
Hann window, overlapping segments, log-magnitude colour mapping — are covered by what
exists plus Gap 2; its MFCC/DCT material is speech-recognition specific and does not
apply.)

### Gap 4 — the spectrogram inherits the trace's video filter

`SpectrumPlot::SetSpectrum()` appends the *averaged* magnitudes to the history — the
same vector the trace draws. With "Heavy" averaging (α = 0.85 at ~9 Hz, a time constant
of ~0.7 s) every spectrogram row is smeared across several rows' worth of time. The
norm is the reverse: a spectrogram's rows are the raw STFT frames (here: the per-snapshot
Welch average, which is averaging *within* one 819 µs window, not across seconds), and
smoothing belongs to the trace view alone. A transient interferer should be one sharp
row, whatever the trace's averaging control says.

Smaller spectrogram gaps:

- **Fixed intensity range** (−100…0 dB). Waterfall norm is a reference level and range
  (contrast/brightness) control, because the interesting texture often lives in a
  30 dB slice of the 100 dB scale. Since history is stored as levels, re-rendering
  under new mapping is already cheap and lossless.
- **Full-image rebuild on every frame.** `spectrogram_valid_` is cleared on each
  append, so all ~300 rows are re-reduced and re-coloured ~9 times a second. Works,
  but the natural structure is an incremental append (render the new row, shift or
  ring-index the image) with full rebuilds reserved for resize/range/theme changes.
  Worth doing while touching the code for the log axis, which makes the vertical
  mapping more expensive per rebuild.

### Amplitude panel — no change planned

The envelope-plus-RMS band over minutes, per-interval clip ticks, and code-unit storage
with gain-declared relabeling are all sound; nothing here fights a norm. (A dB scale
option is the metering norm and could be added later; it earns nothing for the
capture-adjustment task this panel serves, so it is deliberately left out.)

## The plan

Five phases, ordered so that each is independently shippable and the shared
infrastructure lands before the panels that use it. Analysis-library changes come with
unit tests in `ddd-gui/tests/analysis/`; panel changes extend the widget tests. All
`ddd-gui` code is Google style behind clang-format/clang-tidy gates.

### Phase 1 — Welch averaging and selectable RBW (spectrum core) — **done**

Landed 2026-08-16. Measured against the acceptance criteria below:

- Noise-floor scatter over a fixed noise sequence fell from **5.60 dB** (one segment) to
  **1.12 dB** (fifteen), which is what chi-squared theory predicts for 1 and 15 averages
  and is the "stops boiling" criterion.
- A tone confined to the last eighth of a snapshot now reads **−16.8 dB**; under the old
  first-segment-only code it read **−120 dB**, the floor — it was not merely attenuated,
  it was absent.
- Cost is **~1.1% of one core** at every offered resolution, measured against an
  unoptimised build of the transform, so the shipped build is cheaper still.


**Files:** `spectrum_analyser.{h,cpp}`, `analysis_worker.{h,cpp}`,
`spectrum_panel.{h,cpp}`, `tests/analysis/test_spectrum_analyser.cpp`,
`tests/gui/widget/test_spectrum_panel.cpp`.

1. Extend `SpectrumAnalyser::Analyse()` to consume the whole buffer it is given:
   segments of `transform_size` at 50% overlap, periodic Hann per segment, power
   averaged across segments *before* the existing exponential average. The existing
   calibration (window-sum normalisation, mirror doubling) applies per segment and is
   unchanged by averaging, so 0 dBFS stays 0 dBFS.
2. Keep the existing refusal semantics: fewer samples than one transform → `false`,
   nothing changes. A partial trailing segment is dropped, not zero-padded.
3. Add transform size to `Options` as a user-visible choice: 4,096 (default, 9.77 kHz
   bins), 8,192 (4.88 kHz), 16,384 (2.44 kHz). The worker already rebuilds the
   analyser when options change (the averaging path); route size through the same
   mechanism. `SpectrogramHistory` columns stay at 1,024 — reduction already handles
   any bin count.
4. Expose the derived figures — bin spacing, ENBW, segment count — as accessors, for
   Phase 5's readout.

**Tests:** full-scale sine still reads 0 dB at every size; a sine placed in the last
eighth of the buffer now appears (it is invisible today — a regression test that fails
against the current code); noise-floor variance across repeated `Analyse()` calls falls
by ~√15 versus single-segment; segment boundaary correctness via a tone plus an impulse.

**Acceptance:** the live noise floor visibly stops boiling at "None" averaging; CPU on
the analysis thread stays under a few percent.

### Phase 2 — shared frequency-axis mapping, log by default — **done**

Landed 2026-08-16, in `analysis/frequency_axis.{h,cpp}` with a "Log frequency" toggle on
the panel, on by default.

**Phase 3's item 4 was pulled forward with it.** The axis applies to the spectrogram as
well as the trace, because the two are the same measurement in one panel: shipping a
decade-spaced trace beside an evenly-spaced waterfall would have placed the same carrier
in two different places with nothing to say which to believe. Phase 3 keeps its other
three items.

One thing found while doing it, for Phase 3 to weigh: on a log axis the bottom decade
(100 kHz–1 MHz) occupies ~47% of the height but is covered by only ~46 of the history's
1,024 columns, about 3 pixels per band. Not a correctness problem — the max-of reduction
still cannot lose a carrier, it only widens it a pixel or two — but raising
`SpectrogramHistory::kDefaultColumns` to 2,048 would halve it for 2.4 MB more memory.
Deliberately not done here.

Also folded in: the cursor and the trace now share one decimation function
(`SpectrumPlot::ColumnLevel`). They previously computed their bin ranges separately and
agreed only because the axis was linear; a widget test now points at a carrier and checks
the reported level is the drawn one, which fails against the old arithmetic by reading
bin 1,264 instead of bin 800.

### Phase 2 — as planned

**Files:** new `analysis/frequency_axis.{h,cpp}` + test, `spectrum_panel.{h,cpp}`.

1. A small Qt-free mapper owning the norm-critical arithmetic:
   `proportion ↔ frequency` for linear and log modes, with a configurable minimum
   frequency for the log form (default 100 kHz — below it there are fewer than ten
   bins at the default RBW, and log(0) does not exist). Plus tick generation: the
   1–2–5 decade ladder for log (100 k, 200 k, 500 k, 1 M, 2 M, 5 M, 10 M…), the
   existing 2 MHz grid for linear.
2. Spectrum panel: an axis-scale toggle (log default, per the norm argued above),
   persisted like the other view settings. Trace painting decimates per pixel column
   through the mapper — max-of-bins-in-column survives unchanged, only the
   column→bin-range mapping changes. Cursor readout goes through the same mapper, so
   the pointer and the trace cannot disagree.
3. The maximum-frequency selector keeps its meaning in both modes (top of axis);
   `kLowPassCornerHz` annotation (a marker at 13.2 MHz) is drawn by Phase 5.

**Superseded 2026-08-16: the selector was removed and the top fixed at Nyquist.** The
control existed because on a linear axis the stretch above the filter's corner was a
third of the width spent on a band the hardware has deliberately removed. On a decade
axis that stretch is a fifth of a decade — under a tenth of the width — so showing
everything the converter can represent costs almost nothing and there is no longer a
trade to offer. `kMaximumFrequencyChoicesHz` and `SpectrumPlot::SetMaximumFrequency` are
gone; `kLowPassCornerHz` stays for Phase 5's marker. The widget test that varied the
range is replaced by a stronger one: the carrier must be drawn within six pixels of where
the axis puts its frequency.

**Tests:** round-trip proportion↔Hz in both modes; tick ladders at several ranges; the
regression the mapper exists to prevent — cursor frequency equals the frequency of the
bin drawn at that pixel, in log mode, at the edges.

**Acceptance:** at the default view the PAL/NTSC audio carriers, EFM band and video
carrier are each separately legible; switching scale never moves a carrier's labeled
frequency.

### Phase 3 — spectrogram correctness and controls — **done**

Landed 2026-08-16. Item 4 (the log axis) had already gone in with Phase 2; the other
three are here.

- **Unaveraged rows.** `SpectrumAnalyser` now reports `snapshot_db()` beside
  `magnitudes_db()`, and `SpectrumReady` carries all three vectors. The trace keeps its
  averaging; the history records the snapshot. A widget test feeds a smoothed trace and a
  loud snapshot together and checks which one the row holds.
- **Reference and range**, offered in the spectrogram view only. Each view now shows just
  the controls that act on it — peak hold and reset in the trace, the two colour-scale
  combos in the waterfall — which keeps the control row from growing. Both stay
  `setEnabled(false)` as well as hidden, so "can this be pressed" still answers honestly.
- **Incremental rendering.** The picture is held at the history's full capacity and
  scrolled; a full rebuild happens only on resize, scale, contrast or theme change.
  Measured at **474 µs per frame against 5,375 µs for a rebuild — 11× —** and that 474 µs
  is the whole panel repaint, not just the waterfall.

Two things worth recording:

- **A theme change never re-coloured the picture.** There was no `changeEvent` handler, so
  the waterfall kept its old palette until the next frame invalidated the cache — a tenth
  of a second during a run, and for ever after one had stopped, which is exactly when
  somebody is looking at it. Fixed here, with a test that swaps the palette and asserts the
  picture changed.
- The scroll/rebuild equivalence test was initially weaker than it looked: painting after
  every frame means the scroll is always one column, so an off-by-one in the multi-column
  path would have gone unseen. It now also feeds in batches of five without painting.
  Verified by deliberately breaking the column order and confirming it fails.

Still not done, and still a Phase 3-shaped question: `SpectrogramHistory::kDefaultColumns`
remains 1,024, so the bottom decade of a log axis gets ~3 pixels per band. See the note
under Phase 2.

### Phase 3 — as planned

**Files:** `spectrum_panel.{h,cpp}`, `analysis_worker.{h,cpp}`,
`spectrogram_history.{h,cpp}` (minor), widget tests.

1. **Unaveraged rows.** Emit the per-snapshot Welch spectrum (Phase 1's output before
   the exponential stage) alongside the averaged one; the history appends that. The
   trace keeps its averaging control; the spectrogram stops answering to it.
2. **Reference level and range.** Two controls (ref: 0/−10/−20/−30 dBFS; range:
   100/60/40/20 dB) mapping level→colour proportion. History is already stored as dB,
   so changing them re-renders everything held.
3. **Incremental rendering.** Append rows to a ring-indexed image; full rebuild only
   on resize, scale change, range change, theme change.
4. **Log frequency axis** via Phase 2's mapper, following the spectrum's toggle. The
   vertical reduction (bands→pixel rows, max-of) goes through the mapper exactly as
   the trace does; stored 1,024-column resolution comfortably exceeds pixel
   resolution across the log range down to 100 kHz.

**Tests:** a one-frame transient occupies exactly one row regardless of the averaging
setting (fails today); ref/range mapping; cursor frequency agreement in log mode.

**Acceptance:** a brief interferer is a sharp horizontal-axis event; a drifting carrier
remains a clean sloped line; contrast controls make a −60 dB feature inspectable.

### Phase 4 — a triggered, carrier-scale waveform display — **done**

Landed 2026-08-16, in `analysis/waveform_trigger.{h,cpp}`,
`analysis/sinc_interpolation.{h,cpp}` and the panel. All five items are in: trigger,
span ladder, multi-sweep rendering, reconstruction, time axis.

- **Trigger**, on by default, with the crossing located between samples. Sub-sample
  interpolation was not in the original sketch and turned out to be the difference
  between working and nearly working: at five samples a cycle, rounding the crossing to a
  sample leaves a fifth of a cycle of jitter, which is most of the shimmer the trigger
  exists to remove. `WaveformMapping` gained a `sub_sample_offset` to carry it.
- **Span ladder** now starts at 0.5 µs and defaults to 1 µs. The old comment claiming
  10 µs was "about two cycles" is corrected in place — it was eighty.
- **Reconstruction** via a precomputed windowed-sinc kernel (16 taps, 256 phases). The
  table is not premature: evaluating the window and sinc per point would be three
  transcendental calls per tap, and at 32 sweeps × 600 columns × 16 taps × 9 snapshots a
  second that is millions of them on the GUI thread. Measured cost of the full 32-sweep
  reconstructed persistence view: **2.9 ms per frame, about 2.6% of one core**.
- **Multi-sweep**: up to 32 sweeps per snapshot, spread across the whole 819 µs rather
  than clustered at its start, giving ~300 sweeps/s from a 9 Hz device.

Two things learned from the tests rather than from the plan:

- **Two existing tests encoded the old behaviour and had to change**, and both were
  right to fail. `PersistenceLeavesEarlierSweepsOnScreen` fed phase-shifted sines and
  expected them to spread across the screen; the trigger now aligns them, which is the
  feature working. It feeds varying amplitudes instead. `SpansAreLabelledInTheTimeTheyCover`
  repeated the wrong two-cycles claim.
- **Image comparison is the wrong tool for "did the trace move".** The reconstructed
  trace is a one-pixel antialiased curve, so shifting it by a hundredth of a pixel
  changes the coverage of every pixel along it: a diff of two visually identical frames
  lit up the whole curve. The test measures mean vertical displacement of the trace per
  column instead — under 1.5 px triggered against tens of pixels untriggered.

Also fixed while there: the time-axis end labels were clipped at the plot edges (a last
mark reading "1 µ"), now pulled inside.

**Revised 2026-08-16: persistence became a slider, off by default.** It was a checkbox
with one fixed fade. The control is calibrated in seconds of tail (0 to 2, quarter-second
steps, ticked every half second) rather than in the per-frame alpha, because the relation between them is
exponential — the alphas worth having are 0.85 to 0.99, so a slider over them would spend
four fifths of its travel doing nothing. The fade is derived from the time actually
elapsed since the last one, measured the same way the spectrogram measures its own frame
rate, so a tail is the length it claims whatever rate the pipeline is publishing at.

That surfaced a test that was passing for the wrong reason: paints in a widget test arrive
microseconds apart, so nothing decays and every setting draws the same picture. The
picture-level test now compares persistence against *off* — genuinely different code paths
— and `WaveformPlot::RetainedAlpha` is public so the exponential itself can be pinned
directly (37% after one time constant, 5% after three).

### Phase 4 — as planned

The waveform panel becomes a real single-channel DSO view. This is the phase that
answers "specialise for 1–13.2 MHz".

**Files:** new `analysis/waveform_trigger.{h,cpp}` + test, possibly
`analysis/sinc_interpolation.{h,cpp}` + test, `waveform_mapping.h` (span ladder and
the incorrect comment), `waveform_panel.{h,cpp}`, widget tests.

1. **Trigger.** Qt-free edge trigger over a snapshot: rising crossing of mid-scale
   (code 512) with hysteresis (±16 codes, above the converter's noise), returning all
   trigger positions. Display anchors each sweep at a fixed pre-trigger fraction
   (10% of span). Auto-mode norm: if no trigger is found (flat or DC input), free-run
   as today rather than freezing. A "Trigger" checkbox, on by default.
2. **Span ladder for the band.** Add 0.5 µs (20 samples), 1 µs (40), 2 µs (80) and
   5 µs (200) below the existing ladder; default to 1 µs — about eight video-carrier
   cycles, the classic "several cycles on screen" scope default. Correct the
   two-cycles comment in `waveform_mapping.h` alongside. Long spans keep today's
   envelope behaviour and remain the right view for EFM/audio-band structure.
3. **Multi-sweep rendering.** With triggering on and persistence on, render up to
   ~32 triggered sweeps from each snapshot into the persistence image (dim
   individually, full-strength composite), giving an effective sweep rate of ~300/s
   from the 9 Hz snapshot cadence — the DPO-style eye-of-the-carrier view, and where
   FM deviation becomes directly visible as horizontal blur at the zero crossings.
   Persistence decay moves to per-snapshot and `kPersistenceRetainedAlpha` is
   retuned for the new accumulation rate.
4. **Reconstruction.** When the span puts fewer than ~2 samples per pixel column,
   switch from min/max columns to windowed-sinc interpolation through the sample
   points (norm: sin(x)/x, 8–16 taps, Blackman-windowed), with sample dots drawn on
   top at the sparsest spans so measured points remain distinguishable from
   reconstruction. Between ~2 and ~1 sample/pixel, plain polyline; above, today's
   min/max envelope. Threshold arithmetic lives in `waveform_mapping` where the tests
   can reach it.
5. **Time axis.** Label the existing gridlines in µs through the established
   axis-label style; cursor readout unchanged.

**Tests:** trigger positions on synthetic sines (phase invariance: shifting the input
by any offset yields sweeps identical to within one sample); hysteresis rejects
noise-level chatter; sinc reconstruction of an 8 MHz sine at 40 Msps hits the true
peaks within 1% (the visible failure of linear interpolation); span/threshold
arithmetic; widget test that triggered mode with a synthetic carrier produces a stable
image across frames (hash two successive renders of phase-shifted input).

**Acceptance:** a synthetic or real ~8 MHz carrier at a 1 µs span is a stationary,
recognisable sine; persistence shows the FM deviation envelope within ~1 s of
switching on; untriggerable input degrades to today's behaviour.

### Phase 5 — instrument readouts and documentation

**Files:** `spectrum_panel.{h,cpp}`, `waveform_panel.cpp` (labels only),
[signal-analysis.md](../docs/content/capture-gui/signal-analysis.md).

1. **RBW readout** on the spectrum panel: "RBW 9.8 kHz · 15 avg" style, from Phase 1's
   accessors — the figure an analyser is never without.
2. **Peak marker**: annotate the strongest displayed peak with frequency and level;
   norm marker behaviour, no pointing required. (One marker; a marker table is out of
   scope.)
3. **Filter-corner marker** at 13.2 MHz on both frequency axes, labeled, so the roll
   off has a name on screen.
4. **dBFS everywhere** the axis or cursor states a level.
5. Rewrite the signal-analysis page in `docs/` for the new behaviour: what the
   trigger does, what the spans mean against the carriers, what RBW is, how to read
   the spectrogram contrast controls. The page's screenshots regenerate.

**Acceptance:** a user can state the instrument's RBW, the strongest carrier's
frequency and level, and the trigger state without pointing at anything.

## Order and dependencies

Phase 1 → Phase 3 (rows are per-snapshot Welch output); Phase 2 → Phase 3 (log axis).
Phase 4 is independent of 1–3 and can proceed in parallel. Phase 5 last. Each phase
leaves the tree releasable; nothing depends on landing the whole plan.

## What this plan deliberately does not do

- **No change to the monitor tap or capture path.** 9 snapshots/s of 32,768 samples is
  enough for everything above once each snapshot is fully used; touching the wait-free
  tap for the sake of the panels would invert the project's priorities.
- **No FFT library.** The in-tree radix-2 transform gains a heavier duty cycle
  (15 transforms per snapshot instead of 1, ~45 ms/s of one core at the default size)
  which is still invisible on the analysis thread; the licensing/tooling reasoning in
  `fourier_transform.h` stands.
- **No mel scale.** It encodes human pitch perception; nothing here is heard.
- **No amplitude-panel changes**, per the review.
- **No persistence of new view settings beyond the session** unless the panels already
  persist their peers (follow whatever `capture_settings` does for the existing
  controls — investigate at Phase 2, not decided here).
