## ld-chroma-encoder
This application does the opposite of ld-chroma-**de**coder.  It reads a stream of RGB48 or YUV444P16 frames, encodes them into PAL or NTSC composite video, and writes a TBC file as output.  It's mostly useful for testing and development purposes.  For example, you can use it to generate TBC files from standard test videos and then look at the effects of decoding them with different options in ld-analyse or ld-chroma-decoder.

The input is assumed to be either 928x576 for PAL (top field first) or 758x486 for NTSC (bottom field first).


Syntax:

ld-chroma-encoder \<options> \<input file name> \<output TBC file name> [\<output chroma TBC file name>]

Specify the input filename as `-` to read from standard input.

If you specify a chroma TBC filename, then output is split in the same way vhs-decode does.  Syncs and luma are written to the main TBC and chroma is written to the chroma TBC.  This is particularly useful for calibrating chroma filters (e.g. Transform PAL) since it gives you the "ideal" output from the filter to compare against.


## Video to TBC Example


To use a standard video file as an input source to create an TBC file

NTSC

CVBS (Single .TBC)

`ffmpeg -i INPUT.mkv -an -vf "scale=758:480,pad=758:486:0:3" -vcodec rawvideo -pix_fmt yuv444p16 -f rawvideo - | ld-chroma-encoder -p yuv -f NTSC - OUTPUT.tbc`

S-Video (Two .TBC)

`ffmpeg -i INPUT.mkv -an -vf "scale=758:480,pad=758:486:0:3" -vcodec rawvideo -pix_fmt yuv444p16 -f rawvideo - | ld-chroma-encoder -p yuv -f NTSC - OUTPUT.tbc OUTPUT_chroma.tbc`

PAL

CVBS (Single .TBC)

`ffmpeg -i INPUT.mkv -an -vf "scale=922:576,pad=922:576:0:3" -vcodec rawvideo -pix_fmt yuv444p16 -f rawvideo - | ld-chroma-encoder -p yuv -f PAL - OUTPUT.tbc`

S-Video (Two .TBC)

`ffmpeg -i INPUT.mkv -an -vf "scale=922:576,pad=922:576:0:3" -vcodec rawvideo -pix_fmt yuv444p16 -f rawvideo - | ld-chroma-encoder -p yuv -f PAL - OUTPUT.tbc OUPUT_chroma.tbc`


# Help Commands


```
Options:
  -h, --help                         Displays help on commandline options.
  --help-all                         Displays help including Qt specific
                                     options.
  -v, --version                      Displays version information.
  -d, --debug                        Show debug
  -q, --quiet                        Suppress info and warning messages
  -f, --system <system>              Video system (PAL, NTSC; default PAL)
  -p, --input-format <input-format>  Input format (rgb, yuv; default rgb);
                                     RGB48, YUV444P16 formats are supported
  --field-offset <offset>            Offset of the first output field within
                                     the field sequence (0, 2 for NTSC; 0, 2, 4,
                                     6 for PAL; default: 0)
  --chroma-mode <chroma-mode>        NTSC: Chroma encoder mode to use
                                     (wideband-yuv, wideband-yiq, narrowband-q;
                                     default: wideband-yuv)
  --no-setup                         NTSC: Output NTSC-J, without 7.5 IRE setup
  -c, --sc-locked                    PAL: Output samples are subcarrier-locked
                                     (default: line-locked)

Arguments:
  input                              Specify input RGB/YCbCr file (- for piped
                                     input)
  output                             Specify output TBC file
  chroma                             Specify chroma output TBC file (optional)
```
