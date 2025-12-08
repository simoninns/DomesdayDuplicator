# ld-decode-tools
ld-decode-tools is a suite of applications for processing the .tbc output from ld-decode.  The tools enable various processing of the LaserDisc including VBI, 40-bit FM code, White-Flag, dropout detection/correction and comb filtering.  The ld-decode-tools use a JSON based metadata file to store and communicate information about the .tbc.  Details of the JSON format can be found on the [JSON Metadata format](https://github.com/happycube/ld-decode/wiki/JSON-Metadata-format) wiki pages.


# Installation

From the tools directory (of the ld-decode repo) type 'qmake' followed by 'make all'.  Once all of the applications are compiled use 'sudo make install' to install and 'sudo make uninstall' to remove.

Note: The ld-decode-tools.pro qmake project file is designed only for command line compilation with Ubuntu.  The individual application .pro files can be used within the Qt Creator IDE (you should build ld-decode-shared first in order to make a local copy of the required shared libraries).

So if, for example, your .tbc output file is test_video.tbc (and is NTSC):

```
ld-process-vbi test_video.tbc (add in the VBI data and determine the field order)

ld-dropout-correct test_video_map.tbc test_video_mapdoc.tbc (correct the dropouts and create a new .tbc)

ld-analyse test_video_mapdoc.tbc & (view the corrected .tbc)

ld-chroma-decoder test_video_mapdoc.tbc test_video_mapdoc.rgb (convert the .tbc to RGB 16-16-16 frames)
```

For a PAL .tbc output file a typical sequence would be:
```
ld-process-vbi test_video.tbc (add in the VBI data and determine the field order)

ld-discmap test_video.tbc test_video_map.tbc (maps the disc and removes duplicate frames)

ld-dropout-correct test_video.tbc test_video_doc.tbc (correct the dropouts and create a new .tbc)

ld-analyse test_video_doc.tbc & (view the corrected .tbc)

ld-chroma-decoder test_video_doc.tbc test_video_doc.rgb (convert the .tbc to RGB 16-16-16 frames)
```

Note that the VBI processing, NTSC processing, disc-mapping, drop-out detection and correction are optional.

Note: If compilation fails after pulling a new version of the tools from github, try running the following commands in the tools/ directory before filing a github issue report:

```
make distclean
qmake
make all
sudo make install
```


## ld-process-vbi
This application examines the input TBC file and determines the available VBI data for each available frame.  The VBI data is stored as both the raw data value for the 3 VBI lines as well as a full decode of the VBI according to the IEC specifications.  The resulting information is written back into the JSON metadata file for the TBC output.

For NTSC sources this application also processes items from the IEC NTSC specification that are specific to NTSC LaserDiscs such as 40-bit FM codes, white flags, etc.

Syntax:

ld-process-vbi \<options> \<input TBC file name>
  
```
Options:
  -h, --help                Displays this help.
  -v, --version             Displays version information.
  -d, --debug               Show debug
  --input-json <filename>   Specify the input JSON file (default input.json)
  --output-json <filename>  Specify the output JSON file (default same as
                            input)
  -n, --nobackup            Do not create a backup of the input JSON metadata
  -t, --threads <number>    Specify the number of concurrent threads (default
                            is the number of logical CPUs)

Arguments:
  input                     Specify input TBC file
```
  

## ld-export-metadata
This application reads an ld-decode JSON metadata file, typically as produced by ld-process-vbi, and exports information in standard formats that other tools can read. At present, it can export:

- Per-frame signal quality information from the VITS test signals, as CSV
- Per-frame LaserDisc VBI control signals, as CSV
- LaserDisc navigation information, as FFMETADATA1 (which FFmpeg can use to add chapter navigation to a video file)

Syntax:

ld-export-metadata \<options> \<input>

```
Options:
  -h, --help           Displays this help.
  -v, --version        Displays version information.
  -d, --debug          Show debug
  -q, --quiet          Suppress info and warning messages
  --vits-csv <file>    Write VITS information as CSV
  --vbi-csv <file>     Write VBI information as CSV
  --ffmetadata <file>  Write navigation information as FFMETADATA1

Arguments:
  input                Specify input JSON file
```

## ld-dropout-correct
This application uses the drop-out information in the JSON metadata file to perform dropout correction on the input TBC file and produces a new output file.  The current version of the corrector uses framing in order to provide inter-field correction.  Note that inter-field correction may not function correctly for NTSC pull-down sources.

Syntax:

ld-dropout-correct \<options> \<input TBC file name> \<output TBC file name>
  
```
Options:
  -h, --help                Displays this help.
  -v, --version             Displays version information.
  -d, --debug               Show debug
  --input-json <filename>   Specify the input JSON file (default input.json)
  --output-json <filename>  Specify the output JSON file (default output.json)
  -r, --reverse             Reverse the field order to second/first (default
                            first/second)
  -o, --overcorrect         Over correct mode (use on heavily damaged sources)
  -i, --intra               Force intrafield correction (default interfield)
  -t, --threads <number>    Specify the number of concurrent threads (default
                            is the number of logical CPUs)

Arguments:
  input                     Specify input TBC file (- for piped input)
  output                    Specify output TBC file (omit or - for piped
                            output)
```


## ld-chroma-decoder
This application takes the PAL or NTSC TBC input video and performs chroma decoding (i.e colourises it).  Output is a sequence of RGB 16-16-16 video frames suitable for pipelining to an external application such as ffmpeg.

Syntax:

ld-chroma-decoder \<options> \<input TBC file name> (\<output RGB file name>)
  
The output RGB file name is optional.  If omitted the output will be sent to stdout (for pipe-lining).

Most of the options here correspond to settings in ld-analyse's 'Chroma decoder configuration' window, so you can experiment with them interactively using ld-analyse. For more information on what the settings mean, see the [ld-analyse User Guide](https://github.com/happycube/ld-decode/wiki/ld-analyse-User-Guide).

The `mono` filter isn't available in ld-analyse. It treats the whole signal as being luma, so it's the best choice when you know that the input video doesn't contain any colour information (e.g. black-and-white films).

```
Options:
  -h, --help                      Displays this help.
  -v, --version                   Displays version information.
  -d, --debug                     Show debug
  -q, --quiet                     Suppress info and warning messages
  --input-json <filename>         Specify the input JSON file (default
                                  input.json)
  -s, --start <number>            Specify the start frame number
  -l, --length <number>           Specify the length (number of frames to
                                  process)
  -r, --reverse                   Reverse the field order to second/first
                                  (default first/second)
  --chroma-gain <number>          Gain factor applied to chroma components
                                  (default 1.0)
  -b, --blackandwhite             Output in black and white
  -f, --decoder <decoder>         Decoder to use (pal2d, transform2d,
                                  transform3d, ntsc1d, ntsc2d, ntsc3d,
                                  ntsc3dnoadapt, mono; default automatic)
  -t, --threads <number>          Specify the number of concurrent threads
                                  (default number of logical CPUs)
  -o, --oftest                    NTSC: Overlay the adaptive filter map (only
                                  used for testing)
  -w, --white                     NTSC: Use 75% white-point (default 100%)
  --chroma-nr <number>            NTSC: Chroma noise reduction level in dB
                                  (default 0.0)
  --luma-nr <number>              NTSC: Luma noise reduction level in dB
                                  (default 1.0)
  --simple-pal                    Transform: Use 1D UV filter (default 2D)
  --transform-mode <mode>         Transform: Filter mode to use (level,
                                  threshold; default threshold)
  --transform-threshold <number>  Transform: Uniform similarity threshold in
                                  'threshold' mode (default 0.4)
  --transform-thresholds <file>   Transform: File containing per-bin similarity
                                  thresholds in 'threshold' mode
  --show-ffts                     Transform: Overlay the input and output FFTs

Arguments:
  input                           Specify input TBC file (- for piped input)
  output                          Specify output RGB file (omit or - for piped
                                  output)
```

## ld-chroma-encoder
This application does the opposite of ld-chroma-**de**coder: it reads a stream of RGB 16-16-16 frames, and encodes them into PAL or NTSC composite video, writing a TBC file as output. It's mostly useful for testing and development purposes -- for example, you can use it to generate TBC files from standard test videos, then look at the effects of decoding them with different options in ld-analyse or ld-chroma-decoder.

Syntax:

ld-chroma-encoder \<options> \<input RGB file name> \<output TBC file name>

Specify the input filename as `-` to make it read from standard input. 

```
Options:
  -h, --help                   Displays help on commandline options.
  --help-all                   Displays help including Qt specific options.
  -v, --version                Displays version information.
  -d, --debug                  Show debug
  -q, --quiet                  Suppress info and warning messages
  -c, --sc-locked              Output samples are subcarrier-locked. PAL only.
                               (default: line-locked)
  -f, --system <system>        Select color system, PAL or NTSC. (default PAL)
  --chroma-mode <chroma-mode>  NTSC only. Chroma encoder mode to use
                               (wideband-yuv, wideband-yiq; default:
                               wideband-yuv)
  --no-setup                   NTSC only. Do not add 7.5 IRE setup (as in
                               NTSC-J)

Arguments:
  input                        Specify input RGB file (- for piped input)
  output                       Specify output TBC file
```
  
## ld-analyse
This GUI application provides a range of features for examining TBC output including drop-out detection, video extent, line scope and VBI data.  The application works with NTSC and PAL TBC output files.

Note: The input file name is optional (you can either specify it from the command line or using the GUI once the application is running).

For more information about ld-analyse please see the [ld-analyse User Guide](https://github.com/happycube/ld-decode/wiki/ld-analyse-User-Guide)

Syntax:

ld-analyse \<options> \<input TBC file name>

```
Options:
  -h, --help     Displays this help.
  -v, --version  Displays version information.
  -d, --debug    Show debug

Arguments:
  input          Specify input TBC file
```


## ld-process-efm
This application processes the .efm output from ld-decode into either digital audio or data.  The application implements both audio and data error detection and correction.  Audio output is a stereo 44.1KHz 16-bit PCM file.

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


## ld-lds-converter
This application can accept 10-bit packed format .lds files and convert them to 16-bit signed files or vice-versa (useful for expanding .lds files and piping the output to a compression utility such as FLAC).

Syntax:

ld-lds-converter \<options>
  
```
Options:
  -h, --help           Displays this help.
  -v, --version        Displays version information.
  -d, --debug          Show debug
  -i, --input <file>   Specify input laserdisc sample file (default is stdin)
  -o, --output <file>  Specify output laserdisc sample file (default is stdout)
  -u, --unpack         Unpack 10-bit data into 16-bit (default)
  -r, --riff           Unpack 10-bit data into 16-bit with RIFF WAV headers (use with -u)
  -p, --pack           Pack 16-bit data into 10-bit
```

## ld-discmap
This application scans through the metadata for a .tbc file and attempts to correct metadata and video data to ensure the .tbc contains the correct amount of frames, in the right order, without duplication.  The application also pads the .tbc with dummy (black) frames when one or more frames are missing.

It is recommended to run ld-process-vbi on the input .tbc before running this tool.

```
Options:
  -h, --help     Displays this help.
  -v, --version  Displays version information.
  -d, --debug    Show debug
  -r, --reverse  Reverse the field order to second/first (default first/second)
  -m, --maponly  Only perform mapping, but do not save to target (for testing
                 purposes)

Arguments:
  input          Specify input TBC file
  output         Specify output TBC file
```

## ld-disc-stacker
This application performs 'stacking' of multiple versions of the same disc (ideally different copies of discs containing the same mastered contents).  Disc stacking requires a minimum of two input sources in order to work (although 3 or more are strongly recommended).

Stacking is performed by taking all available input sources and, pixel by pixel, determining the number of available sources for the pixel (by removing outliers marked as dropouts in the source files).  If more than 3 sources are available for the pixel the tool will use the median value of the available sources as the output (when there are an even number available of pixel sources, the two centre values of the median are averaged to the output).  If only two sources are available an average is used.  If only one source is available it is passed directly as the output.

If no sources are available for a pixel the tool will attempt to use differential dropout detection (diffDOD) to recover a value (in the case where ld-decode marked the pixel as a false-negative) unless the --no-diffdod option is specified.

If the --passthrough option is specified the tool will, when all pixel sources are marked as a dropout, mark the resulting output pixel as a dropout (regardless of the diffDOD result).  The --passthrough is useful in non-preservation cases where it is desirable for master plate errors (which cause dropouts in the same place on all resulting LaserDisc copies) to be marked for dropout concealment (where-as diffDOD would correctly identify the plate error as present on all discs (and therefore not an error)).

Use the tool by specifying the available input .tbc files followed by the desired .tbc output file.

```
Options:
  -h, --help                Displays this help.
  -v, --version             Displays version information.
  -d, --debug               Show debug
  -q, --quiet               Suppress info and warning messages
  --input-json <filename>   Specify the input JSON file for the first input
                            file (default input.json)
  --output-json <filename>  Specify the output JSON file (default output.json)
  -r, --reverse             Reverse the field order to second/first (default
                            first/second)
  -t, --threads <number>    Specify the number of concurrent threads (default
                            is the number of logical CPUs)
  --no-diffdod              Do not use differential dropout detection on low
                            source pixels
  --passthrough             Pass-through dropouts present on every source

Arguments:
  inputs                    Specify input TBC files (- as first source for
                            piped input)
  output                    Specify output TBC file (omit or - for piped
                            output)


```

## ld-process-vits
This application performs an analysis of the VITS (Vertical Interval Test Signals) and recalculates the white and black SNR values in the metadata.  Note that any TBC file will already have this metadata provided by ld-decode.  This tool is generally used along with dropout correction and stacking as the video content of the TBC is modified - running ld-process-vits therefore allows you update the SNR metadata in order to analyse the result (using ld-analyse).

Use the tool by specifying the required input .tbc file.  The tool will backup the previous JSON file (to .vbup) before proceeding.

```
Options:
  -h, --help                Displays this help.
  -v, --version             Displays version information.
  -d, --debug               Show debug
  -q, --quiet               Suppress info and warning messages
  --input-json <filename>   Specify the input JSON file (default input.json)
  --output-json <filename>  Specify the output JSON file (default same as
                            input)
  -n, --nobackup            Do not create a backup of the input JSON metadata
  -t, --threads <number>    Specify the number of concurrent threads (default
                            is the number of logical CPUs)

Arguments:
  input                     Specify input TBC file


```