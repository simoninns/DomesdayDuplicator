## ld-chroma-decoder


This application takes the output of ld-decode (a PAL or NTSC TBC file) and performs chroma decoding (i.e. colourises it).

The output is a sequence of RGB48, YUV444P16, or GRAY16 video frames suitable for piping to an external application such as FFmpeg.

Syntax:

`ld-chroma-decoder [options] <input TBC file name> [output file name]`

> [!NOTE]
> This is mostly automated with FFmpeg profiles today with [tbc-video-export](https://github.com/JuniorIsAJitterbug/tbc-video-export#readme).


## Direct Command Example:


`.tbc` --> Chroma-Decoder y4m --> Pipe | --> FFmpeg --> V210 4:2:2 Uncompressed in the QuickTime MOV container.

--------------

NTSC

    ld-chroma-decoder --decoder ntsc3d -p y4m -q INPUT.tbc| ffmpeg -i - -c:v v210 -f mov -top 1 -vf setfield=tff -flags +ilme+ildct -pix_fmt yuv422p10le -color_primaries smpte170m -color_trc bt709 -colorspace smpte170m -color_range tv -vf setdar=4/3,setfield=tff OUTPUT.mov

PAL

    ld-chroma-decoder --decoder transform3d -p y4m -q INPUT.tbc| ffmpeg -i - -c:v v210 -f mov -top 1 -vf setfield=tff -flags +ilme+ildct -pix_fmt yuv422p10le -color_primaries bt470bg -color_trc bt709 -colorspace bt470bg -color_range tv -vf setdar=4/3,setfield=tff OUTPUT.mov


## Example with full vertical VBI data:


--------------------

Y4M Only.

NTSC

    ld-chroma-decoder --ffll 1 --lfll 259 --ffrl 2 --lfrl 525 --decoder ntsc3d -p y4m -q INPUT.tbc OUTPUT.mov

PAL

    ld-chroma-decoder --ffll 2 --lfll 308 --ffrl 2 --lfrl 620 --decoder transform3d -p y4m -q INPUT.tbc OUTPUT.mov


## Notes & Command Argument List


Most options have a corresponding setting in ld-analyse's 'Chroma decoder configuration' window so you can experiment with them interactively there. For more information, see the [ld-analyse manual](https://github.com/happycube/ld-decode/wiki/ld-analyse#chroma-decoder-configuration).

The `mono` filter treats the whole signal as luma, so it's the best choice when you know that the input video doesn't contain any colour information (e.g. black-and-white films).


# Output Framing Control


### First Active Field Line


`--first-active-field-line int`, `--ffll int`

The first visible line of a field.

Ranges:

| Format | Range   | Default |
| ------ | ------- | ------- |
| NTSC   | 1 - 259 | 20      |
| PAL    | 2 - 308 | 22      |


### Last Active Field Line


`--last-active-field-line int`, `--lfll int`

The last visible line of a field.

Ranges:

| Format | Range   | Default |
| ------ | ------- | ------- |
| NTSC   | 1 - 259 | 259     |
| PAL    | 2 - 308 | 308     |


### First Active Frame Line


`--first-active-frame-line int`, `--ffrl int`

The first visible line of a field.

Ranges:

| Format | Range   | Default |
| ------ | ------- | ------- |
| NTSC   | 1 - 525 | 40      |
| PAL    | 2 - 620 | 44      |


### Last Active Frame Line


`--last-active-frame-line int`, `--lfrl int`

The last visible line of a field.


Ranges:

| Format | Range   | Default |
| ------ | ------- | ------- |
| NTSC   | 1 - 525 | 525     |
| PAL    | 2 - 620 | 620     |


# Chroma Decoding Control


### Luma Decoder


`--chroma-decoder-luma decoder`

Chroma decoder to use for the luma instance.


>[!CAUTION]
> It is unlikely you need to adjust this.

Available decoders:

| Format | Decoder         | Default |
| ------ | --------------- | ------- |
| B/W    | `MONO`          | LUMA    |
| PAL    | `PAL2D`         |         |
| PAL    | `TRANSFORM2D`   |         |
| PAL    | `TRANSFORM3D`   |         |
| NTSC   | `NTSC1D`        |         |
| NTSC   | `NTSC2D`        |         |
| NTSC   | `NTSC3D`        |         |
| NTSC   | `NTSC3DNOADAPT` |         |


### Chroma Decoder


`--chroma-decoder decoder`

Chroma decoder to use.

Available decoders:

| Format | Decoder         | Default     |
| ------ | --------------- | ----------- |
| B/W    | `MONO`          | LUMA        |
| PAL    | `PAL2D`         | PAL/PAL-M   |
| PAL    | `TRANSFORM2D`   |             |
| PAL    | `TRANSFORM3D`   | PAL CVBS/LD |
| NTSC   | `NTSC1D`        |             |
| NTSC   | `NTSC2D`        | NTSC        |
| NTSC   | `NTSC3D`        | NTSC CVBS   |
| NTSC   | `NTSC3DNOADAPT` |             |


### Chroma Gain


`--chroma-gain 0`

Gain factor applied to chroma components normally you will not adjust this over `1~3` in value.

### Chroma Phase


`--chroma-phase float`

Phase rotation applied to chroma components (degrees `0~360`).


### Luma NR


`--luma-nr float`

Luma noise reduction level in dB.

>[!CAUTION]
> It is unlikely you need to adjust this.
> We recommend applying noise reduction in post via avisynth. vapoursynth or tools like Resolve.


# Help Page


```
Options:
  -h, --help                                  Displays help on commandline
                                              options.
  --help-all                                  Displays help including Qt
                                              specific options.
  -v, --version                               Displays version information.
  -d, --debug                                 Show debug
  -q, --quiet                                 Suppress info and warning
                                              messages
  --input-json <filename>                     Specify the input JSON file
                                              (default input.json)
  -s, --start <number>                        Specify the start frame number
  -l, --length <number>                       Specify the length (number of
                                              frames to process)
  -r, --reverse                               Reverse the field order to
                                              second/first (default
                                              first/second)
  --chroma-gain <number>                      Gain factor applied to chroma
                                              components (default 1.0)
  --chroma-phase <number>                     Phase rotation applied to chroma
                                              components (degrees; default 0.0)

  -p, --output-format <output-format>         Output format (rgb, yuv, y4m;
                                              default rgb); RGB48, YUV444P16,
                                              GRAY16 pixel formats are supported
  -b, --blackandwhite                         Output in black and white

  --pad, --output-padding <number>            Pad the output frame to a
                                              multiple of this many pixels on
                                              both axes (1 means no padding,
                                              maximum is 32)
  -f, --decoder <decoder>                     Decoder to use (pal2d,
                                              transform2d, transform3d, ntsc1d,
                                              ntsc2d, ntsc3d, ntsc3dnoadapt,
                                              mono; default automatic)
  -t, --threads <number>                      Specify the number of concurrent
                                              threads (default number of logical
                                              CPUs)
  --ffll, --first_active_field_line <number>  The first visible line of a
                                              field. Range 1-259 for NTSC
                                              (default: 20), 2-308 for PAL
                                              (default: 22)
  --lfll, --last_active_field_line <number>   The last visible line of a field.
                                              Range 1-259 for NTSC (default:
                                              259), 2-308 for PAL (default: 308)
  --ffrl, --first_active_frame_line <number>  The first visible line of a
                                              frame. Range 1-525 for NTSC
                                              (default: 40), 1-620 for PAL
                                              (default: 44)
  --lfrl, --last_active_frame_line <number>   The last visible line of a frame.
                                              Range 1-525 for NTSC (default:
                                              525), 1-620 for PAL (default: 620)
  -o, --oftest                                NTSC: Overlay the adaptive filter
                                              map (only used for testing)
  --chroma-nr <number>                        NTSC: Chroma noise reduction
                                              level in dB (default 0.0)
  --luma-nr <number>                          Luma noise reduction level in dB
                                              (default 1.0)
  --ntsc-phase-comp                           NTSC: Adjust phase per-line using
                                              burst phase
  --simple-pal                                Transform: Use 1D UV filter
                                              (default 2D)
  --transform-threshold <number>              Transform: Uniform similarity
                                              threshold (default 0.4)
  --transform-thresholds <file>               Transform: File containing
                                              per-bin similarity thresholds
  --show-ffts                                 Transform: Overlay the input and
                                              output FFTs

Arguments:
  input                                       Specify input TBC file (- for piped input)
  output                                      Specify output file (omit or - for piped output)

```
