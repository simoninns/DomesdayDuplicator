# File formats

## ld-decode.py

The ld-decode.py application accepts FM RF captures input in '10-bit packed' format. This is a bit-stream of 10-bit unsigned integers produced by the Domesday Duplicator's capture GUI (typically with the .lds file extension).  The input bit-stream is expected to be the raw LaserDisc RF captured at 40 Million Samples Per Second (MSPS) with each sample being 10-bits.

*The decoder also supports FLAC compressed captures, and lower sample rates if the input frequency is defined and bit-depths such as 8-bit & 16-bit.

The output from ld-decode.py is a stream of 16-bit unsigned values; each value representing a single grey-scale value.  The file extension used by ld-decode is .tbc for both NTSC and PAL decoded frames aptly named the Time Base Corrected format.

PAL output is 1135x625 16-bit values. (280mbps) (2.1GB/min) (126GB/hour)

NTSC output is 910x525 16-bit values. (226.5mbps) (1.7GB/min) (102GB/hour)

The 16-bit grey-scale values used by the output format are scaled representations of the standard 8-bit digital component values (i.e. an 8-bit right shift of the value will provide the standard 8-bit digital component intensity values).

The frequency values for .tbc to analogue CVBS playback via DAC are the following:

PAL - 17727262 Hz

NTSC - 14318181 Hz

## ld-chroma-decoder

The NTSC and PAL chroma-decoders (a.k.a. comb filters) accept .tbc files from the ld-decode.py application and produces a raw RGB bit-stream with 16 bits per color value in the order RGB16-16-16 giving 48-bits per pixel.  The file extension is .rgb (and can be used by applications such as ffmpeg by specifying the raw RGB format with a depth of 16).

Examples of pre-made export commands for FFV1/V210/V410 & ProRes-HQ/ProRes4444XQ codecs can be found [here](https://github.com/oyvindln/vhs-decode/wiki/Command-List#ld-chroma-decoder-export-commands)

## Example file sizes

The following file sizes show the typical disc usage consumed by an end-to-end capture and decode of a LaserDisc.

Individual decodes will vary from disc-to-disc:

* NPE - PAL CAV disc with 54348 frames
* NPE - LDS (RF Capture 40MSPS 10-bit packed) = 109.4GB
* NPE - LDF (RF Capture 40MSPS 16-bit FLAC compressed) = 22.6GB (Estimate*)
* NPE - TBC (Indexed16) = 77.1GB
* NPE - PCM (48K little Endian 16-bit signed) = 417.4MB
* NPE - RGB (RGB 16-16-16) = 175.6GB
* NPE - AVI (36min 13sec mp4) = 4.1GB

Raw 10-bit Packed DomesDayDuplicator Captures are 2.8GB/Min when compressed via `ld-compress` this becomes around 625MB/Min in 16-bit FLAC which is still decodable with a small processing speed penalty on solid state media.
