# Obtaining a LaserDisc RF sample


Before you can decode anything with ld-decode, you will need something to decode.  There are a few choices; find someone awesome with a RF capture you desire and get a copy of it or use the [Domesday Duplicator](../Hardware/Domesday-Duplicator.md) or [CX Card](https://github.com/oyvindln/vhs-decode/wiki/CX-Cards) (along with a calibrated LaserDisc player) and capture your own. Alternatively, there are also plenty of samples of the DD86 Discs on [The Internet Archive](https://github.com/happycube/ld-decode/wiki/Disc-images-to-download), and [The Community Shared Drive](https://drive.google.com/drive/u/1/folders/1lzQWdFFfVclEQUDbuwngro0MCusOgPM6) has samples of LaserDiscs and many other analogue formats.

The resulting files can be the following:

`.lds` - 10-bit Packed DdD Capture (compressible with ld-compress linux/windows scripts inside the github repo)

`.ldf` - FLAC compressed 16-bit signed DdD capture

`.raw` - 16-bit signed RAW DdD capture

`.u8`  - 8-bit unsigned CX Card (or other RF device) capture

`.u16` - 16-bit unsigned CX Card (or other RF device) capture


## Inspecting a 10-bit packed Capture


The `.lds` is a 10-bit packed RF sample file, if you want to view the raw RF in a audio tool like audacity or audition you have 2 options.

FLAC compress it to a 16-bit flac file.

    ld-compress capture.lds

Or you can unpack it to a 16-bit singed file.

    ld-lds-converter --unpack --input capture.lds --output unpacked_capture.s16

If you have a new capture environment it can be very useful to make a small 10 second .lds at the start, middle and end of a disc, then view in audacity to make sure the capture environment is good.


## LaserDisc Support


| Format         | Format Type         | Line System | TV System                | Level of Decoding Support | RF Capture Support |
| -------------- | ------------------- | ----------- | ------------------------ | ------------------------- | ------------------ |
| LaserDisc CAV  | Composite Modulated | 525 & 625   | NTSC, NTSC-J, PAL, PAL-M | High                      | Yes, Standardised  |
| LaserDisc CLV  | Composite Modulated | 525 & 625   | NTSC, NTSC-J, PAL, PAL-M | High                      | Yes, Standardised  |
| LaserDisc MUSE | Composite Modulated | 1125        | HDTV                     | In Developent             | Work In Progress   |


# Making your first .tbc file


| TV System | Full-Frame 4fsc | Frequency      | Frame Rate | Field Rate | Data Rate CVBS      | Data Rate Y+C       | 
|-----------|-----------------|----------------|------------|------------|---------------------|---------------------| 
| PAL       | 1135x624        | 17727262 Hz    | 25i        | 50i        | 280mbps 2.1GB/min   | 560mbps 4.2GB/min   |
| NTSC      | 910x524         | 14318181 Hz    | 29.97i     | 59.94i     | 226.5mbps 1.7GB/min | 453mbps 3.4GB/min   |


The first step in the decoding process is the complicated (and time-consuming) process of converting the raw LaserDisc RF into multiple streams of:

`.tbc` (the 4fsc NTSC or PAL field stream)

`.pcm` (a raw 44.1Khz 16-bit stereo analogue sound track)

`.efm` (the data track & digital audio decodable to `.dts` & `.ac3`)

`.json` which is a metadata file (i.e. it describes the .tbc file to the rest of the wondrous tools in the ld-decode tool-chain)


The other complex things ld-decode does are 'time-base correction' and 'framing' - and, as confusing as it sounds, 'framing' is actually about processing fields: As the disc spins, jitters and flies around merrily in a LaserDisc player, it's not very accurate about it. 

To remove all of these timing issues (which make the video go wibbily-wobbily) ld-decode picks up on the timing signals in the RF and stretches and contracts things to make everything look right.  As well as this time-base correction, all of the NTSC/PAL information is arranged into fields which then go to make up the frames (every frame has a first and second field which are interlaced to make a complete 'picture').


# Setting your decode up 


In order to decode somthing, the decoder needs to know what and how you wish to decode an input RF capture. (as its not magic!)

TV System: `--pal` or `--ntsc` (NTSC is default)

If AC3 audio disc then `--AC3` is required.

Decoding has relative positon control of start & duration by frame.

`--start 60` for example starts you on the 60th frame of the file.

`--length` sets the maximum duration in frames although, if you don't specify a length or a start, things will start from the beginning of the RF file and continue until the end (or a lead-out frame).

`--threads` sets the CPU threads, the decoding code is bottlenecked by the TBC, so more then 6 threads wont normally help. (same applys to vhs-decode/cvbs-decode but not hifi-decode)


## Example


Example decoding command for a PAL disc:

    ld-decode laserdisc-test.ldf --threads 6 --pal --start 300 --length 1000 outputfile
    
If your captures are in another directory you will want to define the path of your input and output files:

    ld-decode --threads 6 --pal --start 300 --length 1000 /my/indisc/awesome.ldf /my/outdisc/outputfile 

Some captures include 'spin-up' - this is the bit where the player spins up the disc and settles onto the start of the first field of the first frame.  As the player spins up the RF is all over the place and can't be decoded.

ld-decode will attempt to skip this, but sometimes it needs some guidance with `--start`. 

The 'sequential frame' is the number of frames included from the start of the .lds/.ldf (it has nothing to do with the VBI frame numbering that might be shown by a real LD player).

Note that the `outputfile` shouldn't have a `.tbc` extension - ld-decode will add this itself (no one knows why this is... it will always be a mystery).

Once the decode is finished you will have a bunch of files ready for the next parts of the process.


# VBI data


[Visual VBI Data Guide](https://github.com/oyvindln/vhs-decode/wiki/Identifying-vbi-data)

Every disc (& videotape) within the PAL or NTSC, has a Vertical blanking interval - this is the gap (from the prehistoric days of CRT TVs) where the raster had to get from the bottom of the TV back to the top - nothing could be displayed whilst that happened, so some smart-cookie decided to put useful data in there so the LaserDisc player wouldn't get bored waiting.

The VBI data (often just called VBI as it sounds cooler) contains useful information about the video such as frame numbering and timecode or even fancy data like [Teletext](https://github.com/oyvindln/vhs-decode/wiki/Teletext)!


ld-decode will have already put some basic VBI data in the metadata (the .tbc.json file mentioned before) - but you can make it more complete by using the following command:

    ld-process-vbi /my/outdisc/outputfile.tbc


# Viewing the resulting decode


To view your NTSC/PAL decode while decoding or after `.tbc` use the following command:

Linux & MacOS:

    ld-analyse

Windows:

    ld-analyse.exe -style fusion

`-style fusion` is required for darkmode due to QT not automatically setting based off system setting on Windows and some Linux distros.

ld-analyse is so awesome and provides so much good information, it needs its [own wiki just to describe it](../Tools/ld-analyse.md).

<img src="assets/ld-analyse_Main_Window_DAR43.png" width="600" height="">


# Drop-out correction


Every now and again there is a loss of RF from a LaserDisc, the signal goes 'poof!' for a very short period.  This is called a drop-out - hence the terms DO (Drop Out), DOD (Drop Out Detection) and DOC (Drop Out Correction).  These appear as annoying black/white lines in the frames.  You can correct most of these dropouts using the following command:

    ld-dropout-correct /my/outdisc/outputfile.tbc /my/outdisc/outputfile.doc.tbc

Again, you can view the result of this correction in ld-analyse.

As a special note (for all you Unix wiz-kids) - ld-dropout-correct, ld-chroma-decoder (see below) and ffmpeg all support 'pipelining' so, if you know what you are doing, there is no need to create intermediate files as you go) and this was first automated with the vhs-decode scripts which today has ended up all rolled together in tbc-video-export, the first 100% cross platform tool in the projects.


# Dude, where's my colour?


To turn NTSC or PAL time base corrected files (GREY16) into a colour image it has to be processed with a chroma-decoder (because chroma is a posh word for colour and it has to be decoded); sometimes this is called comb-filtering, but it's got nothing to do with hairstyles really, so nobody knows why.

Running the NTSC/PAL through the [chroma-decoder](../Tools/ld-chroma-decoder.md) can produce an raw YUV or RGB uncompressed stream which you can then do anything with! 

Today this is all a hands off process with [tbc-video-export](https://github.com/oyvindln/vhs-decode/wiki/TBC-to-Video-Export-Guide) handling files generated by the whole family of decode projects.

Linux / MacOS / Windows:

    tbc-video-export Input-Media.tbc 

> [!NOTE]  
> The stock FFmpeg profile is lossless compressed FFV1 10-bit 4:2:2 in the .mkv container, `ld-dropout-correct` is automatically run on the tbc files used, but will not alter the original tbc files, `ld-process-vbi` can also be run automatically with the tool.


# Making a nice video file of the result


This is a tricky one as there is no one-size-fits-all solution.  To help with this you can look at the following wonderful wiki pages which cover audio and manual chroma decoding:

[NTSC decode guide](https://github.com/happycube/ld-decode/wiki/Creating-video-from-NTSC-decodes)

[PAL decode guide](https://github.com/happycube/ld-decode/wiki/Creating-video-from-PAL-decodes)

That's it, you are now an uber-user and can begin to impress your friends with your new found skills!  Once you are done with that, it's worth browsing the rest of the wiki for more information and some more advanced tricks.
