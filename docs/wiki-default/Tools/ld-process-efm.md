## ld-process-efm

Note: This tool is depreciated - use the [efm-decoder](efm-decoder.md) tools instead


| Format       | Info           | Sample Rate | Bit-Depth    | Extention |
|--------------|----------------|-------------|--------------|-----------|
| Analog Track | Stereo or Mono | 44.1KHz     | 16-bit       | .pcm      |
| Dolby AC-3   | Stereo PCM     | 44.1KHz     | 16-bit       | .ac3      |
| Dolby DTS    | Surround 5.1   | 44.1KHz     | 16/24-bit    | .dts      |


This application processes the raw digital `.efm` output from ld-decode into either digital audio or data.

ld-process-efm implements both audio and data error detection and correction.


## Dolby AC-3 Audio


Requires `--AC3` flag in ld-decode to enable extraction of signal data.

This can be 2.0 Stereo or 4.0, 5.1 Surround

5.1 Surround

<img src="assets/ac3_5.1_surround.png" width="130" height="">

4.0 Surround

<img src="assets/surround_4.1.png" width="130" height="">


AC-3 discs take the Right channel analog track area, so Left channel may contain an mono analog audio track.


LaserDisc digital audio is based on the standards used for CDs (stereo 44.1KHz 16-bit PCM), which require the digital audio stream to contain periodic timestamps so the player can tell where it is on the disc.

Since LaserDisc players get this information from the video instead, quite a lot of LaserDiscs don't bother to include audio timestamps.

You can tell if you're decoding a disc like this by looking at the statistics printed by ld-process-efm: if no data is coming out of the <q>F3 frame synchronisation</q> phase, try using the `--time` option to not require timestamps. (The downside of this is that it makes error padding less effective, so only use it if you need to.)


## Dolby DTS Audio


<img src="assets/dts_5.1.png" width="130" height="">


If you have a DTS disc, use the `--dts` option: this disables audio error concealment, and makes ld-process-efm accept a variation of the CD audio standard (a different F3 Sync 0 sequence) used by some DTS discs. Many DTS discs need `--time` too.

The output will be a `.dts` file.

The channel map is as follows for surround 5.1

1. Left Front
2. Right Front
3. Center Channel
4. Left Rear
5. Right Rear
6. LFE (Subwoofer)


## Note

Note: you can convert the `.pcm` analog audio to standard `.wav` with:

`ffmpeg -f s16le -ar 44.1k -ac 2 -i input.pcm output_file.wav`


## Help

Syntax:

ld-process-efm \<options> \<input EFM file name> \<output data file name>

```
Options:
  -h, --help             Displays help on commandline options.
  --help-all             Displays help including Qt specific options.
  -v, --version          Displays version information.
  -d, --debug            Show debug
  -q, --quiet            Suppress info and warning messages
  -c, --conceal          Conceal corrupt audio data (default)
  -s, --silence          Silence corrupt audio data
  -g, --pass-through     Pass-through corrupt audio data
  -p, --pad              Pad start of audio from 00:00 to match initial disc
                         time
  -b, --data             Decode F1 frames as data instead of audio
  -D, --dts              Audio is DTS rather than PCM (allow non-standard F3
                         syncs)
  -t, --time             Non-standard audio decode (no time-stamp information)
  --debug-efmtof3frames  Show EFM To F3 frame decode detailed debug
  --debug-syncf3frames   Show F3 frame synchronisation detailed debug
  --debug-f3tof2frames   Show F3 To F2 frame decode detailed debug
  --debug-f2tof1frame    Show F2 to F1 frame detailed debug
  --debug-f1toaudio      Show F1 to audio detailed debug
  --debug-f1todata       Show F1 to data detailed debug

Arguments:
  input                  Specify input EFM file
  output                 Specify output file

```

CLI options:

* Audio
  * Conceal corrupt audio data - Attempts to conceal corrupt audio data using the concealment type selected in the following section
  * Silence corrupt audio data - Silences any corrupt samples
  * Pass-through corrupt audio data - Passes any corrupt samples through to the output as-is

* Concealment type
  * Linear interpolation - Interpolates between last-good and next-good samples to conceal errors

* Options
  * Pad start of audio from 00:00 to match initial disc time - Pads the start of the input sample to ensure the output starts from the 00:00 time of the disc.  Useful for assembling partial captures.
  * Decode F1 frames as audio - Decode F1 frames as audio data
  * Decode F1 frames as data - Decode F1 frames as data
  * Non-standard audio decode (no time-stamp information in input EFM)

* Detailed debugging for development
  * EFM to F3 Frame debug
  * F3 and section debug
  * F3 to F2 Frame decoding debug
  * F2 to F1 Frame decoding debug
  * F1 Frame to audio samples debug
  * F1 Frame to data sector debug
