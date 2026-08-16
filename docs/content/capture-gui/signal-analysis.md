# Signal analysis

Three displays, all fed from the same live stream, all available while merely monitoring —
so a player can be set up and a disc judged without writing anything to disk.

They answer different questions:

| Panel | The question it answers |
| --- | --- |
| [Waveform](#waveform) | What is the signal doing *right now*, microsecond by microsecond |
| [Spectrum](#spectrum) | What is the signal *made of* — and, as a spectrogram, how that changes |
| [Amplitude History](#amplitude-history) | Has the level been *steady*, over the last five minutes |

## How they are fed

The capture pipeline publishes a snapshot of the stream about nine times a second through a
tap that never makes the pipeline wait for anything. A worker thread picks those up at
about thirty times a second, does the arithmetic, and hands the results to the panels.

Two consequences worth knowing:

- **Nothing here can slow a capture down.** A display that cannot keep up misses snapshots;
  it never delays the stream. Frames are dropped rather than queued, because an old picture
  of a live signal is of no interest and a backlog of them would be worse than useless.
- **These are snapshots, not the whole stream.** The displays show a representative slice
  measured continuously, not every one of the 40 million samples a second. The
  [Statistics](statistics.md) panel is where whole-stream figures live — every sample is
  measured for the extremes, the clip counts and the sequence check.

Levels read in **converter codes** — 0 to 1023 — until a
[front-end gain](settings.md#front-end-gain) is declared, and in millivolts at the BNC once
one is. Declaring it, or correcting it, re-scales every figure already on screen: nothing
was ever stored in the derived units.

## Waveform

The scope. The signal as it arrives, with time across and level up.

### Span

How much time is on screen: **10 µs**, **50 µs**, **100 µs** (the default), **200 µs** or
**500 µs**.

10 µs is about two cycles of a LaserDisc FM carrier — the shortest span with anything to
see in it. 500 µs is 20,000 samples, which is the longest span that still shows all of the
time it claims to.

### Persistence

Lets each trace fade rather than replacing it, so a repeating waveform builds up its
envelope over about a second. This is the way to see the *shape* of an FM carrier rather
than one arbitrary slice of it — and the way to notice a modulation that a single sweep
would show as a slightly different trace each time.

### The cursor

Point at the trace and the readout gives the position in microseconds and the level, in
codes or in millivolts.

## Spectrum

The same signal by frequency. Two views of it, chosen with the first control.

### Spectrum

A live trace: level against frequency, 0 dB being a full-scale sine wave. That reference is
the one you can act on — a carrier at −6 dB is using half the converter's range, and the
number says so without anybody having to know how the transform was normalised.

The transform is 4,096 points, which at 40 Msps is a bin about 9.8 kHz wide. There are far
more bins than pixels, so each column of the display draws the **highest** bin it covers
rather than the first or the average: a narrow carrier that fell between two sampled bins
would otherwise simply not be drawn, which on a display whose job is finding carriers is the
one failure that matters.

### Spectrogram

The same measurement over time: frequency up the side, time running left to right over a
fixed window labelled in seconds, level as colour.

This is what makes a **drifting carrier** or an **intermittent interferer** visible at all —
neither is distinguishable from ordinary noise on a live trace, because by the time you have
looked away and back the trace has moved on.

The spectrogram records whichever view is showing, so switching to it shows what has already
happened rather than starting again from the moment you asked for it.

### Range

The top of the displayed frequency range: **14 MHz** (the default), **16**, **18** or
**20 MHz**.

The converter reaches 20 MHz and the board's anti-aliasing filter rolls off at 13.2 MHz, so
everything above that is the filter's skirt and the noise under it. 14 MHz puts the filter's
corner just inside the right-hand edge, where it can be seen to be working without crowding
out the 8 MHz carrier that matters. The wider ranges are for when *"is the filter doing what
I think"* is the actual question.

Narrowing the range spreads a subset of the bins across the same width. Nothing is thrown
away and nothing is interpolated.

### Averaging

How much of the previous display each new transform replaces: **None**, **Light**,
**Medium** (the default) or **Heavy**.

More averaging makes a weak carrier readable against the noise. Less shows a transient that
would otherwise be averaged away. The averaging is done on power rather than on decibels, so
a peak that appears in one frame out of ten reads as a tenth of its power and not as a tenth
of its level.

### Peak hold

Draws the highest level each frequency has reached since the last reset, underneath the live
trace. The way to catch an interferer that appears for a moment while you are looking
somewhere else. **Reset peaks** starts it again.

Both apply to the live trace only — the spectrogram already shows every frame they would be
summarising, so the two controls are disabled rather than left to do nothing when pressed.

### The cursor

Point at the plot for a frequency and level. Over the spectrogram it also gives how long ago
that column was measured, so a feature can be located in time as well as in frequency.

## Amplitude History

A strip of the last **five minutes**: the min/max envelope, RMS drawn either side of 0 V,
and a tick wherever clipping happened. Ten points a second, aggregated from the statistics
stream.

This is the panel that catches the faults the other two cannot. A player whose RF output
sags for two seconds forty minutes into a side is invisible on a scope you are not looking
at and invisible on a spectrum that has averaged it away; here it is a notch in the
envelope, still on screen when you come back.

### The nominal bounds

The strip marks the recommended level on both sides: the signal should peak at no more than
**75 %** of the converter's range, which is codes 128 and 896.

It is a nominal level rather than a limit. The hard limit is the converter, which clips at 0
and 1023 and is counted separately; the headroom between the two is what absorbs the moments
a disc is worse than the moment the gain was set on. A capture that spends its time at 95 %
is not yet clipping and is one dropout away from it.

The bound is drawn on both sides because the signal is centred and swings both ways — a
bound on one side alone would say nothing about the half of the waveform that was already
closer to the rail.

### Span

**All** shows everything the history holds — five minutes, once it has filled.

**Match spectrogram** narrows it to the same span the Spectrum panel is showing, so the two
scroll at the same pace and a moment on one lines up with the same moment on the other. It
is the setting to use when you are trying to work out whether a level dip and a spectral
event are the same event.

### The summary

Under the plot: the range across everything still held, and the clip count.

Note that this is not the same as the whole-run extremes in
[Statistics](statistics.md#extremes). These figures fall off the back as the five minutes
wrap, and that is the point — they answer *how is it doing now*, not *what is the worst it
has ever been*.

### Clear history

Starts the history again from now, without affecting the run. This is for after a cable or a
gain setting has been changed, when what came before is no longer what is being measured.
