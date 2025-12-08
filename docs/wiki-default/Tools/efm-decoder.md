# Overview
EFM-decoder is a collection of tools for handling EFM (Eight-to-Fourteen Modulation) data as used on Compact Discs and LaserDiscs.

Note: **The EFM decoder tools are mostly untested - please consider using and reporting any issues found.  If you need something more robust, use the ld-process-efm tool instead.**

The supported EFM data and structures are defined by the ECMA-130 (issue 2) specification "Data interchange on read-only 120 mm optical data disks (CD-ROM)" which was written to enhance the original (audio-only) specification IEC 60908 (second edition 1999-02) "Audio recording -
Compact disc digital audio system".

[ECMA-130 Specification](https://ecma-international.org/publications-and-standards/standards/ecma-130/)

The EFM decoder is split into a number of individual tools, each providing functionality at different layers of the decoding process.  In addition, the suite of tools includes an F2 Section stacker that is capable of performing error correction at the F2 Frame level based on multiple EFM input images.

# TL;DR - How do I get digital audio from Disney's Bambi?
Assuming that you have decoded the LaserDisc using ld-decode (and you specified the output as bambi_1) you will have a EFM file called bambi_1.efm (for side 1).

The following commands will decode into a 44.1KHz Stereo 16-bit wav file along with an Audacity label file (metadata) that can be used to understand the contents of the resulting audio:

```
efm-decoder-f2 ./bambi_1.efm ./bambi_1.f2s
efm-decoder-d24 ./bambi_1.f2s ./bambi_1.d24
efm-decoder-audio ./bambi_1.d24 ./bambi.wav --audacity-labels --zero-pad
```

This process will leave you with two files, bambi_1.wav (which contains the audio) and bambi_1.txt (which contains the labels).

# efm-decoder-f2
The efm-decoder-f2 tool takes T-values as input (supplied by a tool such as ld-decode) and decodes them into F2 sections.  Each section contains 98 F2 frames.

Note that the reason for 'sections' of 98 frames is due to the ECMA-130 requirements around metadata (which contains addressing information in the form of time-stamps).  The resolution of the time stamps is 1/75th of a second as the q-channel metadata repeats every 98 frames.  So each section contains 98 frames with the same Q and P channel metadata.  Correction of frame order at a higher resolution isn't possible due to the lack of addressing resolution.

The decoding sequence is:
* T-values
* Channel frames
* F3 Frames
* F2 Sections
* F2 Section Correction

## Command line
```
Usage: efm-decoder-f2 [options] input output
efm-decoder-f2 - EFM T-values to F2 Section decoder

(c)2025 Simon Inns
GPLv3 Open-Source - github: https://github.com/happycube/ld-decode

Options:
  -h, --help               Displays help on commandline options.
  --help-all               Displays help including Qt specific options.
  -v, --version            Displays version information.
  -d, --debug              Show debug
  -q, --quiet              Suppress info and warning messages
  --show-f3                Show F3 frame data
  --show-f2                Show F2 frame data
  --show-tvalues-debug     Show T-values to channel decoding debug
  --show-channel-debug     Show channel to F3 decoding debug
  --show-f3-debug          Show F3 to F2 section decoding debug
  --show-f2-correct-debug  Show F2 section correction debug
  --show-all-debug         Show all debug

Arguments:
  input                    Specify input EFM file
  output                   Specify output F2 section file
```

# efm-decoder-d24
The efm-decoder-d24 tool takes F2 sections as input (supplied by a tool such as efm-decoder-f2) and decodes them into Data24 sections.  Each section contains 98 frames.

Note that Data24 sections are almost identical to F1 Frames (as defined by ECMA-130) except the byte order of the F1 Frame has been corrected (see clause 16 of ECMA-130).  The reason for this extra data type is that Data24 frames can either be interpreted as audio (in accordance to IEC 60908-1999) or as sector-based data (in accordance to ECMA-130). So an intermediate type is required to support both possibilities.

The decoding sequence is:
* F2 Sections
* F1 Sections
* Data24 Sections

Note that the decoding from F2 to F1 includes the unscrambling and CIRC based error correction (known as C1 and C2 correction) according to ECMA-130 section 18.  Section metadata from the F2 sections is preserved by the F1 sections.

## Command line
```
Usage: efm-decoder-d24 [options] input output
efm-decoder-d24 - EFM F2 Section to Data24 Section decoder

(c)2025 Simon Inns
GPLv3 Open-Source - github: https://github.com/happycube/ld-decode

Options:
  -h, --help        Displays help on commandline options.
  --help-all        Displays help including Qt specific options.
  -v, --version     Displays version information.
  -d, --debug       Show debug
  -q, --quiet       Suppress info and warning messages
  --show-f1         Show F1 frame data
  --show-data24     Show Data24 frame data
  --show-f2-debug   Show F2 to F1 decoding debug
  --show-f1-debug   Show F1 to Data24 decoding debug
  --show-all-debug  Show all debug options

Arguments:
  input             Specify input F2 Section file
  output            Specify output Data24 Section file
```
# efm-decoder-audio
The efm-decoder-audio tool takes Data24 sections as input (supplied by a tool such as efm-decoder-d24) and decodes them into wav-format audio as well as metadata.

Note that wav files do not contain any detailed metadata, so the efm-decoder-audio tool produces a separate CSV file containing the information provided by the q-channel addressing information.  In addition, the CSV file contains the locations of any errors or erasures (missing samples) which have been silenced or concealed by the decoding process.

Note that, in accordance with IEC 60908-1999, the output wav format is 16-bit signed integer stereo at a sampling rate of 44.1KHz.  The tool includes a standard wav format header in the output to allow tools such as Audacity to correctly interpret the format automatically.

The decoding sequence is:
* Data24 Sections
* 16-bit WAV audio

Note that the silencing and concealment of corrupt samples is optional.  If a sample is corrupt the tool will look and see if the sample before and after is valid.  If valid the sample will be concealed with the average of the two valid samples.  If either the leading or trailing sample is corrupt, the tool will silence the sample with 0 amplitude.

## Command line
```
Usage: efm-decoder-audio [options] input output
efm-decoder-audio - EFM Data24 to Audio decoder

(c)2025 Simon Inns
GPLv3 Open-Source - github: https://github.com/happycube/ld-decode

Options:
  -h, --help                     Displays help on commandline options.
  --help-all                     Displays help including Qt specific options.
  -v, --version                  Displays version information.
  -d, --debug                    Show debug
  -q, --quiet                    Suppress info and warning messages
  --audacity-labels              Output WAV metadata as Audacity labels
  --no-audio-concealment         Do not conceal errors in the audio data
  --zero-pad                     Zero pad the audio data from 00:00:00
  --show-audio                   Show Audio frame data
  --show-audio-debug             Show Data24 to audio decoding debug
  --show-audio-correction-debug  Show Audio correction debug
  --show-all-debug               Show all decoding debug

Arguments:
  input                          Specify input Data24 Section file
  output                         Specify output wav file
```

## Metadata format
The metadata output is formatted as an Audacity label file and can be directly loaded into Audacity in order to annotate the WAV output.

Note that the timestamps are in Audacity's millisecond time (with enough resolution to identify individual stereo-sample pairs at 44.1 KHz).  Timestamps in the label text are CDDA format (to match the decoding process output).

Note: Track number metadata is only written if the track number changes

### Example metadata
The following shows an example of the metadata output:
```
2038.365964	2038.366100	Silenced: 33:58:27
2038.366281	2038.366281	Concealed: 33:58:27
2038.366327	2038.366327	Concealed: 33:58:27
2038.366372	2038.366372	Concealed: 33:58:27
2038.366531	2038.366531	Concealed: 33:58:27
2038.366576	2038.366576	Concealed: 33:58:27
2038.366621	2038.366621	Concealed: 33:58:27
2038.366825	2038.366825	Concealed: 33:58:27
2038.366871	2038.366871	Concealed: 33:58:27
2038.366916	2038.366916	Concealed: 33:58:27
0.000000	175.333333	Track: 01 [00:00:00-02:53:25]
175.346667	470.520000	Track: 02 [00:00:00-04:55:13]
470.533333	736.786667	Track: 03 [00:00:00-04:26:19]
```
# efm-decoder-data
The efm-decoder-data tool takes Data24 sections as input (supplied by a tool such as efm-decoder-d24) and decodes them into ECMA-130 compliant data sectors.

The output is a binary file that represents the original input data to the ECMA-130 encoding process (with the exception of unrecoverable errors).

Note that Data24 sections contain 24x98 = 2352 bytes.  The Data24 sections are stripped of parity and other error correction data when converted into sectors.  Each sector therefore contains 2048 bytes of data (see ECMA-130 for details).  Any sector metadata is preserved and output in the metadata output file.

The decoding sequence is:
* Data24 Sections
* Raw Sectors (2048 bytes per sector)
* Sectors (after unscramble and error correction)
* Binary data + metadata

Note that the decoding process from Data24 to binary data includes RSPC error correction using Q and P parity according to ECMA-130 section 14.

Note that the output of metadata is optional.

## Command line
```
Usage: efm-decoder-data [options] input output
efm-decoder-data - EFM Data24 to data decoder

(c)2025 Simon Inns
GPLv3 Open-Source - github: https://github.com/happycube/ld-decode

Options:
  -h, --help                      Displays help on commandline options.
  --help-all                      Displays help including Qt specific options.
  -v, --version                   Displays version information.
  -d, --debug                     Show debug
  -q, --quiet                     Suppress info and warning messages
  --output-metadata               Output bad sector map metadata
  --show-rawsector                Show Raw Sector frame data
  --show-rawsector-debug          Show Data24 to raw sector decoding debug
  --show-sector-debug             Show raw sector to sector decoding debug
  --show-sector-correction-debug  Show sector correction decoding debug
  --show-all-debug                Show all decoding debug

Arguments:
  input                           Specify input Data24 Section file
  output                          Specify output data file
```

## Metadata format
Format: Address

Each address represents a 2048 byte sector.  Since error correction is performed on the whole sector there are no individual error/erasure flags; the resulting sector is either completely valid or invalid.  Only invalid sectors are written to the metadata.

### Example metadata
```
28
29
30
31
32
33
34
```
# efm-stacker-f2
The efm-stacker-f2 tool takes F2 section files (produced by efm-decoder-f2) and stacks them together.  The stacking process examines differences between the available F2 section sources and corrects based on a "most likely correct" voting system.  The more available sources, the more error-free the output.

## Command line
```
Usage: efm-stacker-f2 [options] inputs output
efm-stacker-f2 - EFM F2 Section stacker

(c)2025 Simon Inns
GPLv3 Open-Source - github: https://github.com/happycube/ld-decode

Options:
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.
  -v, --version  Displays version information.
  -d, --debug    Show debug
  -q, --quiet    Suppress info and warning messages

Arguments:
  inputs         Specify input F2 section files
  output         Specify output F2 section file
```

## Example usage
`./efm-stacker-f2 --debug ../efm-decoder-f2/DS2_NatA_PP.f2s ../efm-decoder-f2/DS4_NatA_PP.f2s ../efm-decoder-f2/DS6_NatA_PP.f2s ../efm-decoder-f2/DS7_NatA_PP.f2s ../efm-decoder-f2/DS8_NatA_PP.f2s ../efm-decoder-f2/DS10_NatA_PP.f2s ./DSstacked_NatA_PP.f2s`

# vfs-verifier
The VFS verifier is a specific testing tool for BBC Domesday AIV data images produced by EFM decoding.  The VFS verifier takes the output from the efm-decoder-data tool and the valid sector metadata and attempts to read the underlying VFS file structure within the data.  By reading the VFS structure the tool can work out where the actual VFS file data is in the EFM image.  This is compared to the EFM valid sector metadata to verify that all required data for the VFS image is valid in the resulting data output file.

## Command line
```
Usage: vfs-verifier [options] input bad-sector-map
vfs-verifier - Acorn VFS (Domesday) image verifier

(c)2025 Simon Inns
GPLv3 Open-Source - github: https://github.com/happycube/ld-decode

Options:
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.
  -v, --version  Displays version information.
  -d, --debug    Show debug
  -q, --quiet    Suppress info and warning messages

Arguments:
  input          Specify input EFM file
  bad-sector-map Specify bad sector map metadata file
```

# EFM data structure
In order to understand the EFM decoding it is necessary to understand the underlying structure of the EFM data.  The various EFM data types are as follows:

* T-Values
* Channel Frames
* F3 Frames
* F2 Frame Sections
* F1 Frame Sections
* Data24 Sections
* ECMA-130 Sectors (data only)

## T-Values
The initial EFM data (supplied by a tool such as ld-decode or another extraction method) consists of a file containing unsigned bytes.  Each byte represents a 'T' value.  T-values range from T3 to T11 with T3 being the shortest 'event' in the EFM and T11 being the longest (these actually represent the period of the EFM from the original source).

The T-values are first converted into a bit-stream.  Each T value represents a section of the bit-stream as shown below:

```
 T3 = 100
 T4 = 1000
 T5 = 10000
 T6 = 100000
 T7 = 1000000
 T8 = 10000000
 T9 = 100000000
T10 = 1000000000
T11 = 10000000000
```

The initial bit-stream is formed by simply concatenating the T-value bit equivalents together.  For example, T3+T6+T9 would be `100100000100000000`.

## Channel frames
The bit-steam is then split into "channel frames" - each channel frame consists of 27 sync pattern bits and 33 EFM data symbols (of 17 bits per symbol) totalling 588 bits per channel frame.

A channel frame has the following structure:

- Sync Header : 24 Channel bits
- Merging bits : 3 Channel bits
- Control byte : 14 Channel bits
- Merging bits : 3 Channel bits

Bytes 1 to 32, each followed by Merging bits : 32 x (14+3) = 544 Channel bits

Thus, each Channel Frame representing a F3-Frame comprises 588 Channel bits.

These Channel bits are recorded on the (CD) disk along a Physical Track. A ONE Channel bit shall be represented by a change of pit to land or land to pit in the reflective layer. A ZERO Channel bit shall be represented by no change in the reflective layer.

## F3 Frames
The F3 Frame consists of 33 symbols each of 8-bits in length which result from the initial EFM data processing.  The total of 33 symbols of 8-bits in length gives a total of 264 bits per frame. 

Each F3 frame consists of (since each symbol is 8-bits, one symbol is equivalent to one byte):

- 1 Subcode byte
- 24 user-data bytes
- 8 parity bytes

## Recording of the F3 Frames on the disk
In order to record the F3-Frames on the disk each 8-bit byte shall be represented by 14 so-called Channel bits. Each F3-Frame is represented by a so-called Channel Frame comprising a Sync Header, Merging bits and 33 14-Channel bit bytes.

### 8-to-14 Encoding
All 33 bytes of the F3-Frames of each Section are 8-bit bytes. They shall be converted into 14-bit bytes according to the table of annex D. The bits of these 14-bit bytes are called Channel bits. These bytes of 14 Channel bits are characterized by the fact that between two ONEs there are at least two and at most ten ZEROs.

The first byte of the first two F3-Frames of each Section, i.e. the Control byte of these frames, is not converted according to this table but is given a specific synchronisation pattern of 14 Channel bits that is not included in the table of valid EFM codes. These two patterns shall be:

- 1st Frame, byte 0, called SYNC 0 : 00100000000001
- 2nd Frame, byte 0, called SYNC 1 : 00000000010010

The left-most Channel bit is sent first in the data stream.

### Sync Header
A Sync Header shall be the following sequence of 24 Channel bits:

100000000001000000000010

### Merging Channel bits
Merging Channel bits are sequences of three Channel bits set according to ECMA-130 annex E and inserted between the bytes of 14 Channel bits as well as between the Sync Header and the adjacent bytes of 14 Channel bits.

## F2 Frame Sections
The result of decoding 98 F3 Frames (along with the associated subcode data) produces an F2 Frame Section (containing 98 F2 Frames).  F2 Frames also contain CIRC parity data allowing basic error correction (and detection) to be performed.

## F1 Frame Sections
Once an F2 Frame has passed through error correction (as well as delay-lines and interleaving, etc) the parity data is stripped leaving an F1 Frame Section that contains 98 F1 Frame (of 24 bytes each) and the associated subcode metadata.  In accordance to the ECMA-130 spec, all F1 frames have the byte order reversed.

## Data24
When handing raw data (such as input data or input WAV data) the tools use data24 to represent chunks of data. The reason for this is that F1 Frames take 24 bytes as input, so the data24 is a convenient way of handling incoming data. Data24 is not part of the ECMA-130 definitions.  Data24 sections consist of 98 data24 frames.  A Data24 frame is basically an F1 Frame with the byte order of the data corrected.

Note: data24 * 98 frames represents 1/75th of a second 

## Sections of 98 frames
A section is a group of 98 F2 Frames representing 1/75th of a second of user-data (audio or other data) or a lead-in/lead-out section.

Each section has 98 bytes of subcode data (one byte per F2 Frame).

These requirements mean you need a minimum of 98 F2 Frames in order to produce an F3 Frame (or vice-versa, 98 F3 Frames in order to produce subcode data).

### Why 98 Frames per Section?
#### IEC Sample requirements:
1. **Sample Rate**: 44.1 kHz = 44,100 samples per second (per channel)
2. **Bit Depth**: 16 bits = 2 bytes (per sample)
3. **Channels**: Stereo = 2 channels

#### Calculation:
- Data per sample = 2 bytes
- Data per second per channel = 44,100 samples/second} * 2 bytes/sample = 88,200 bytes/second
- Total data for stereo = 88,200 bytes/second * 2 channels = 176,400 bytes/second

A 44.1 kHz, 16-bit, stereo sample requires **176,400 bytes per second**.

Since the original IEC specification is audio based and the supported sample type is 16-bit, 44.1 KHz every second of audio requires 176,400 bytes.  The minimum time window allowed is a "section" which is 98x24 bytes = 2352 bytes.  176,400 / 2,352 bytes = 75 (i.e. a section represents a time period of 1/75th of a second).

