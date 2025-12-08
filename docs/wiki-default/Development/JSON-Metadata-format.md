# JSON Metadata format

Note: This format has been superceded by SQLite.  This documentation page needs updating.

The ld-decode JSON metadata format consists of a root object containing three main sections: `videoParameters`, `pcmAudioParameters` (optional), and `fields` (array). This format is used to store comprehensive metadata about decoded LaserDisc captures and other analog video sources.

Note: This analysis is based on examining the TBC library code (as the previous documentation (under 'Old' below) on the wiki has not been kept up to date).

## Root Object Structure

```
json
{
  "videoParameters": { ... },
  "pcmAudioParameters": { ... },
  "fields": [ ... ]
}
```

## videoParameters Object

Contains capture-level metadata and video system information:

| Field | Type | Description |
|-------|------|-------------|
| `numberOfSequentialFields` | Integer | Total number of fields decoded |
| `system` | String | Video system: "PAL", "NTSC", or "PAL_M" |
| `activeVideoStart` | Integer | Start position (pixels) of active video line |
| `activeVideoEnd` | Integer | End position (pixels) of active video line |
| `colourBurstStart` | Integer | Start position (pixels) of colour burst |
| `colourBurstEnd` | Integer | End position (pixels) of colour burst |
| `white16bIre` | Integer | White level IRE in 16-bit scale |
| `black16bIre` | Integer | Black level IRE in 16-bit scale |
| `fieldWidth` | Integer | Width of each field in pixels |
| `fieldHeight` | Integer | Height of each field in field-lines |
| `sampleRate` | Double | Sample rate in Hz |
| `isMapped` | Boolean | True if video mapped by ld-discmap |
| `isSubcarrierLocked` | Boolean | True if samples are subcarrier-locked |
| `isWidescreen` | Boolean | True if 16:9 anamorphic, false if 4:3 |
| `gitBranch` | String | Git branch of ld-decode used (optional) |
| `gitCommit` | String | Git commit of ld-decode used (optional) |
| `tapeFormat` | String | Tape format description (optional) |

## pcmAudioParameters Object (Optional)

PCM audio configuration when audio data is present:

| Field | Type | Description |
|-------|------|-------------|
| `sampleRate` | Double | Audio sample rate in Hz |
| `isLittleEndian` | Boolean | True for little endian, false for big endian |
| `isSigned` | Boolean | True for signed samples, false for unsigned |
| `bits` | Integer | Bits per sample (e.g., 16) |

## fields Array

Array of field objects, one per video field:

### Field Object

| Field | Type | Description |
|-------|------|-------------|
| `seqNo` | Integer | Unique sequential field number |
| `isFirstField` | Boolean | True for first field, false for second field |
| `syncConf` | Integer | Sync confidence (0=poor, 100=perfect) |
| `medianBurstIRE` | Double | Median colour burst level in IRE |
| `fieldPhaseID` | Integer | Position in 4-field (NTSC) or 8-field (PAL) sequence |
| `audioSamples` | Integer | Number of audio samples for this field (optional) |
| `diskLoc` | Double | Location in file (fields) (optional) |
| `fileLoc` | Integer | Sample number in file (optional) |
| `decodeFaults` | Integer | Decode fault flags (optional) |
| `efmTValues` | Integer | EFM T-Values in bytes (optional) |
| `pad` | Boolean | True if field is padded (no valid video) |

### Nested Objects in Fields

#### vitsMetrics Object (Optional)
Video Insert Test Signal metrics:

| Field | Type | Description |
|-------|------|-------------|
| `wSNR` | Double | White Signal-to-Noise ratio |
| `bPSNR` | Double | Black line PSNR |

#### vbi Object (Optional)
Vertical Blanking Interval data:

| Field | Type | Description |
|-------|------|-------------|
| `vbiData` | Integer Array | Raw VBI data from lines 16, 17, 18 (3 elements) |

#### ntsc Object (Optional)
NTSC-specific metadata:

| Field | Type | Description |
|-------|------|-------------|
| `isFmCodeDataValid` | Boolean | True if FM code data is valid |
| `fmCodeData` | Integer | 20-bit FM code data payload |
| `fieldFlag` | Boolean | True for first video field |
| `isVideoIdDataValid` | Boolean | True if VIDEO ID data is valid |
| `videoIdData` | Integer | 14-bit VIDEO ID code data payload |
| `whiteFlag` | Boolean | True if white flag present |

#### vitc Object (Optional)
Vertical Interval Timecode:

| Field | Type | Description |
|-------|------|-------------|
| `vitcData` | Integer Array | 8 values of VITC raw data without framing bits or CRC |

#### cc Object (Optional)
Closed Caption data:

| Field | Type | Description |
|-------|------|-------------|
| `data0` | Integer | First closed caption byte (-1 if invalid) |
| `data1` | Integer | Second closed caption byte (-1 if invalid) |

#### dropOuts Object (Optional)
RF dropout detection data:

| Field | Type | Description |
|-------|------|-------------|
| `startx` | Integer Array | Start pixel positions of dropouts |
| `endx` | Integer Array | End pixel positions of dropouts |
| `fieldLine` | Integer Array | Field lines containing dropouts |


# Note: Old, out of date information below
# Purpose


The decode projects are made up of a series of decoders and tools which form a processing tool-chain for processing, analysing decoded RF samples.  In order for each tool in the chain to communicate in an efficient manner a JSON metadata file is used to store and communicate information about the decoded RF capture and any data that has been determined.

Tools should (as appropriate) be able to accept metadata as well as output metadata.

This document describes the metadata format to be used by all tools in the decode tool-chain.


# Conventions


## Supported JSON data types

| **Name**    | **Qt Equivalent** | **Description**                |
| ----------- | ----------------- | ------------------------------ |
| String      | QString           | String of UTF-8 characters     |
| Number      | qint32 or double  | Integer or real number         |
| JSON object | Object            | Object                         |
| Array       | Vector            | Vector of another type         |
| Boolean     | Qbool             | Boolean value of true or false |


##  File names


When producing a JSON metadata file the application/utility will use its normal file extension with `.json` added.  For example, ld-decode & cvbs-decode produces a `.tbc` file containing the composite signal, and a `.tbc.json` file containing the metadata. (For colour-under formats, vhs-decode additionally produces a file with the suffix `_chroma.tbc` containing the chroma signal; this can be processed using the same `.tbc.json` metadata.)

All metadata should be considered as optional.  Each tool will add metadata as appropriate but should be able to handle the case when metadata is not available.


## Invalid data fields


A field with a null or -1 value (for integers) should be considered as invalid.


## Object numbering

VBI frame numbers, sequential field numbers and field-lines are numbered starting from 1. A value of 0 should be considered as invalid.


# Objects


## metaData

| **Name**           | **Type**                  |
| ------------------ | ------------------------- |
| videoParameters    | Object videoParameters    |
| pcmAudioParameters | Object pcmAudioParameters |
| fields             | Array field               |


## videoParameters


| **Name**                 | **Type** | **Description**                                               |
| ------------------------ | -------- | ------------------------------------------------------------- |
| numberOfSequentialFields | Integer  | The total number of fields decoded                            |
| system                   | String   | The video system in use: `"PAL"`, `"NTSC"`, `"PAL-M"`         |
| colourBurstStart         | Integer  | Position (in pixels) of the colour-burst start                |
| colourBurstEnd           | Integer  | Position (in pixels) of the colour-burst end                  |
| blackLevelStart          | Integer  | Position (in pixels) of the black-level start                 |
| blackLevelEnd            | Integer  | Position (in pixels) of the black-level end                   |
| activeVideoStart         | Integer  | Position (in pixels) of the start of the active video line    |
| activeVideoEnd           | Integer  | Position (in pixels) of the end of the active video line      |
| white16bIre              | Integer  | The white level IRE in a 16-bit scale                         |
| black16bIre              | Integer  | The black level IRE in a 16-bit scale                         |
| fieldWidth               | Integer  | The width of each field in pixels                             |
| fieldHeight              | Integer  | The height of each field in field-lines                       |
| sampleRate               | Double   | The sample rate in Hz (usually 4 * fSC for ld-decode)         |
| tapeFormat               | String   | The type of tape media format i.g VHS, Betamax, Video8        |
| isMapped                 | Boolean  | true if the video has been mapped by ld-discmap               |
| isSubcarrierLocked       | Boolean  | true if samples are subcarrier-locked                         |
| isWidescreen             | Boolean  | true if the video is 16:9 anamorphic, rather than 4:3         |
| gitBranch                | String   | The git branch ID of ld-decode used to decode the TBC         |
| gitCommit                | String   | The git commit ID of ld-decode used to decode the TBC         |


## Notes:

A video system is a combination of a line standard and a colour standard - see [World TV Standards](http://web.archive.org/web/20190506044136/http://www.radios-tv.co.uk/Pembers/World-TV-Standards/index.html). `"PAL"` is standard 625-line PAL, and `"NTSC"` is standard 525-line NTSC; these are the only two systems used on LaserDisc. `"PAL-M"` is the 525-line PAL system used in Brazil, and is supported by vhs-decode. Other systems may be supported in the future.

The 'blackLevelStart' and 'blackLevelEnd' parameters point to a recommended section of the scan-line from which the average black level IRE can be determined.  This is typically from the end of the colour-burst (with some safety margin) to the beginning of the active video (again with some safety margin).

`fieldHeight` represents the taller of the two video fields. In the `.tbc` file, the shorter field will be padded with an additional line at the end to make both fields the same size. The first field is always the taller of the two - that is, in 625-line systems, the whole of line 313 is considered to be part of the first field, not the second (which is not always the convention used elsewhere).

`isSubcarrierLocked` indicates, for video sampling rates where there are not an integer number of samples in each line of video, whether the samples are aligned to the start of lines (line-locked) or to the colour subcarrier (subcarrier-locked). For subcarrier-locked sampling, any additional samples at the end of the frame can be found at the start of the second field's padding line. PAL digital video is often subcarrier-locked with a sample rate of 4fSC, giving four extra samples after the end of the second field; for NTSC, there are an integer number of 4fSC samples in a line so it doesn't make a difference.


## pcmAudioParameters


| **Name**       | **Type** | **Description**                                               |
| -------------- | -------- | ------------------------------------------------------------- |
| sampleRate     | Double   | The sample rate in Hz                                         |
| isLittleEndian | Boolean  | true = sample is little endian, false = sample is big endian  |
| isSigned       | Boolean  | true = sample data is signed, false = sample data is unsigned |
| bits           | Integer  | The number of bits used per sample (i.e. 16)                  |


## field


| **Name**       | **Type**        | **Description**                                                                                                   |
| -------------- | --------------- | ----------------------------------------------------------------------------------------------------------------- |
| seqNo          | Integer         | The unique sequential field number for the field                                                                  |
| diskLoc        | Integer         | The location in the file (in fields) the field is located                                                         |
| fileLoc        | Integer         | The sample # in the file the field is located                                                                     |
| isFirstField   | Boolean         | true = first field, false = second field                                                                          |
| syncConf       | Integer         | 0 = poor, 100 = perfect - The percentage confidence of the sync point determination                               |
| vits           | Object vits     | The VITS information                                                                                              |
| vbi            | Object vbi      | The VBI information                                                                                               |
| ntsc           | Object ntsc     | NTSC specific information                                                                                         |
| vitc           | Object vitc     | Vertical Interval Timecode (if present)                                                                           |
| dropOuts       | Object dropOuts | The detected RF drop-outs in the field                                                                            |
| medianBurstIRE | Real number     | The median point of the colour burst (in IRE)                                                                     |
| fieldPhaseID   | Integer         | The position of this field in the 4 (NTSC) or 8 (PAL) field sequence                                              |
| audioSamples   | Integer         | The number of (stereo, signed 16-bit) audio samples corresponding to the video field                              |
| efmTValues     | Integer         | The number of .efm T-Values (in bytes) corresponding to the video field                                           |
| decodeFaults   | Integer         | bit 1: first-field detection failure, bit 2: field phase ID mismatch, bit 3: skipped field (likely a player skip) |
| pad            | Boolean         | true = field is padded (contains no valid video data), false = normal field                                       |


## vitsMetrics


The VITS object contains data obtained by analysing the VITS field lines.  Not all data is available in all fields (i.e. white* on NTSC)

Default:

| **Name** | **Type**    | **Description**                                                 |
| -------- | ----------- | --------------------------------------------------------------- |
| wSNR     | real number | The Signal to Noise ratio of a white (100IRE) area of the field |
| bPSNR    | real number | Black line PSNR (not conventional SNR)                          |

Additional fields With --verboseVITS:

| **Name**                | **Type**    | **Description**                                                                                                                                                |
| ----------------------- | ----------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| whiteIRE                | real number | The IRE level of the white sample, compared to the standard                                                                                                    |
| whiteRFLevel            | real number | The RF level of the white sample                                                                                                                               |
| greyPSNR                | real number | The Peak Signal to Noise ratio of a grey (50IRE) area of the field                                                                                             |
| greyIRE                 | real number | The IRE level of the grey sample, compared to the standard                                                                                                     |
| greyRFLevel             | real number | NTSC ONLY: The RF level of the grey sample                                                                                                                     |
| blackLinePreTBCIRE      | real number | Black line IRE, before TBC/wow-flutter correction                                                                                                              |
| blackLinePostTBCIRE     | real number | Black line IRE, after TBC/wow-flutter correction                                                                                                               |
| blackLineRFLevel        | real number | The RF level of the black line sample                                                                                                                          |
| syncLevelPSNR           | real number | Sync sample PSNR (not conventional SNR)                                                                                                                        |
| syncRFLevel             | real number | The RF level of the sync sample                                                                                                                                |
| syncToBlackRFRatio      | real number | Ratio between sync and black RF signal level                                                                                                                   |
| syncToWhiteRFRatio      | real number | Ratio between sync and white RF signal level                                                                                                                   |
| blackToWhiteRFRatio     | real number | Ratio between black and white RF signal level                                                                                                                  |
| ntscWhiteFlagSNR        | real number | The Signal to Noise ratio of the white flag, if it exists in this field                                                                                        |
| ntscWhiteFlagRFLevel    | real number | The RF level of the white flag, if it exists in this field                                                                                                     |
| ntscLine19Burst0IRE     | real number | NTSC Line 19 Color burst level after TBC/scaling                                                                                                               |
| ntscLine19Burst70IRE    | real number | NTSC Line 19 70IRE burst level                                                                                                                                 |
| ntscLine19ColorPhase    | real number | The field X color phase                                                                                                                                        |
| ntscLine19ColorRawSNR   | real number | The field X raw SNR.  This is unfiltered and should be lower than any post-processing chroma SNR, since it is ~3.59Mhz and on-disk chroma is never above 2Mhz. |
| ntscLine19Color3DPhase  | real number | The "3D comb filtered" color phase (second field only)                                                                                                         |
| ntscLine19Color3DRawSNR | real number | The "3D comb filtered" raw SNR (second field only).  This is higher than the per-field values, but still should be below real-world chroma SNR.                |
| palVITSBurst50Level     | real number | PAL VITS 50IRE color burst level                                                                                                                               |
| cavFrameNr              | Integer     | CAV Frame # in VITS (useful for determining pulldown)                                                                                                          |
| clvMinutes              | Integer     | CLV Minute # from VITS                                                                                                                                         |
| clvSeconds              | Integer     | CLV Seconds # from VITS (not on early disks)                                                                                                                   |
| clvFrameNr              | Integer     | Computed CLV Frame # from VITS (if clvSeconds is present)                                                                                                      |


> [!NOTE]
>  This object is preliminary, and is subject to change as features are implemented within the decoders or vbi-processing tool or newer tools.


## vbi


> [!NOTE]
>  See IEC 60857-1986 and IEC 60856-1986 for details.

> [!NOTE]
>  See IEC 60857-1986 Amendment 2 and IEC 60856-1986 Amendment 2 for details.

| **Name** | **Type**      | **Description**                                           |
| -------- | ------------- | --------------------------------------------------------- |
| vbiData  | Integer Array | The field line 16, 17 and 18 raw VBI data (as 3 elements) |
| vp       | Array         | Array containing the VBI parameters (see below            |

> [!NOTE]
>  For ld-decode rev6 'vp' is removed and only vbiData is contained in the JSON.

The vp array contains the following values:

| **Element** | **Type**                                       | **Description**                                        |
| ----------- | ---------------------------------------------- | ------------------------------------------------------ |
| 0           | Enum discTypes (integer)                       | The disc type (CAV or CLV)                             |
| 1           | String                                         | The reported user-code for the field                   |
| 2           | Integer                                        | The picture number                                     |
| 3           | Integer                                        | The chapter number                                     |
| 4           | Number (integer)                               | CLV programme time code hours                          |
| 5           | Number (integer)                               | CLV programme time code seconds                        |
| 6           | Number (integer)                               | CLV Current seconds value                              |
| 7           | Number (integer)                               | CLV Current frame number (1-25 for PAL, 1-30 for NTSC) |
| 8           | Enum – Programme status - soundModes (integer) | Current sound mode                                     |
| 9           | Enum – soundModes (integer)                    | Am2 Current sound mode                                 |
| 10          | Integer                                        | 12-bit word of boolean flags (see below)               |

Flags (bit 0 is LSB):

| **bit** | **Name**    | **Description**                                                                     |
| ------- | ----------- | ----------------------------------------------------------------------------------- |
| 0       | leadIn      | true = field is lead-in                                                             |
| 1       | leadOut     | true = field is lead-out                                                            |
| 2       | picStop     | true = field has stop-code set                                                      |
| 3       | cx          | Programme status - true = CX is on                                                  |
| 4       | size        | Programme status - true = disc is 12", false = disc is 8"                           |
| 5       | side        | Programme status - true = side 1, false = side 2                                    |
| 6       | teletext    | Programme status - true = teletext data is present on the disc                      |
| 7       | dump        | Programme status - true = programme dump - See IEC specification                    |
| 8       | fm          | Programme status - true = FM-FM Multiplex - See IEC specification                   |
| 9       | digital     | Programme status - true = Video contents are digital - See IEC specification        |
| 10      | parity      | Programme status - true = parity confirmed as correct, false = parity not confirmed |
| 11      | copyAm2     | true = copying is allowed - See IEC Am2 specification                               |
| 12      | standardAm2 | Am2 true = video signal is standard, false = future use                             |


## ntsc


Note: This object contains the field data that is specific to NTSC (and not present in the PAL IEC specifications)

| **Name**           | **Type**         | **Description**                                                 |
| ------------------ | ---------------- | --------------------------------------------------------------- |
| isFmCodeDataValid  | Boolean          | true = FM code data is valid, false = FM code data is invalid   |
| fmCodeData         | Number (integer) | The 20-bit FM code data payload (X5 to X1)                      |
| fieldFlag          | Boolean          | true = first video field, false = not first video field         |
| whiteFlag          | Boolean          | true = white flag, false = white flag not present               |
| isVideoIdDataValid | Boolean          | true = VIDEO ID data is valid, false = VIDEO ID data is invalid |
| videoIdData        | Number (integer) | The 14-bit VIDEO ID code data payload (IEC 61880)               |


## cc


This object represents Closed Caption data for a field.

| **Name** | **Type**         | **Description**                |
| -------- | ---------------- | ------------------------------ |
| ccData0  | Number (integer) | The first closed caption byte  |
| ccData1  | Number (integer) | The second closed caption byte |

For `ccData0` and `ccData1`, if the value is `-1` or not present then no valid CC was found. 0 is a valid value - it means that CC is present, but no data is being transferred on this line.

Note: See ANSI/CTA-608 for details.


## vitc


This object represents Vertical Interval Timecode data for a field.

| **Name** | **Type**      | **Description**           |
| -------- | ------------- | ------------------------- |
| vitcData | Integer Array | VITC raw data as 8 values |

Each of the values in `vitcData` represents 8 bits of the raw VITC data, without the framing bits or CRC. The LSB of `vitcData[0]` is VITC bit 2 (the LSB of the frame number), and the MSB of `vitcData[7]` is VITC bit 79.


## dropOuts

| **Name**  | **Type**      | **Description**                                      |
| --------- | ------------- | ---------------------------------------------------- |
| startx    | Integer Array | An array of start pixels for the detected drop-outs  |
| endx      | Integer Array | An array of end pixels for the detected drop-outs    |
| fieldLine | Integer Array | An array of field-lines on which the drop-outs occur |


# Enums


## VBI Disc types


This describes types of LaserDisc.

discTypes
number (32-bit integer)

| **Name**        | **Value** |
| --------------- | --------- |
| unknownDiscType | 0         |
| clv             | 1         |
| cav             | 2         |


## VBI Sound Modes


soundModes
number (32-bit integer)

> [!NOTE]
> See IEC 60857-1986 and IEC 60856-1986 for details.

| **Name**             | **Value** |
| -------------------- | --------- |
| stereo               | 0         |
| mono                 | 1         |
| audioSubCarriersOff  | 2         |
| bilingual            | 3         |
| stereo\_stereo       | 4         |
| stereo\_bilingual    | 5         |
| crossChannelStereo   | 6         |
| bilingual\_bilingual | 7         |
| mono\_dump           | 8         |
| stereo\_dump         | 9         |
| bilingual\_dump      | 10        |
| futureUse            | 11        |
