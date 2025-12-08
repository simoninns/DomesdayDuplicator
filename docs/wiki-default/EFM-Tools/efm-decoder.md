# efm-decoder

EFM-decoder is a collection of tools for handling EFM (Eight-to-Fourteen Modulation) data as used on Compact Discs and LaserDiscs.

The supported EFM data and structures are defined by the ECMA-130 (issue 2) specification "Data interchange on read-only 120 mm optical data disks (CD-ROM)" which was written to enhance the original (audio-only) specification IEC 60908 (second edition 1999-02) "Audio recording -
Compact disc digital audio system".

[ECMA-130 Specification](https://ecma-international.org/publications-and-standards/standards/ecma-130/)

The EFM decoder is split into a number of individual tools, each providing functionality at different layers of the decoding process.  In addition, the suite of tools includes an F2 Section stacker that is capable of performing error correction at the F2 Frame level based on multiple EFM input images.

Please see the individual EFM-Tools pages for details of the various tools.

# TL;DR - How do I get digital audio from Disney's Bambi?
Assuming that you have decoded the LaserDisc using ld-decode (and you specified the output as bambi_1) you will have a EFM file called bambi_1.efm (for side 1).

The following commands will decode into a 44.1KHz Stereo 16-bit wav file along with an Audacity label file (metadata) that can be used to understand the contents of the resulting audio:

```
efm-decoder-f2 ./bambi_1.efm ./bambi_1.f2s
efm-decoder-d24 ./bambi_1.f2s ./bambi_1.d24
efm-decoder-audio ./bambi_1.d24 ./bambi.wav --audacity-labels --zero-pad
```

This process will leave you with two files, bambi_1.wav (which contains the audio) and bambi_1.txt (which contains the labels).

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
