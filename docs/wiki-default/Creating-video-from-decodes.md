# Creating Video Files 


This is a manual guide for people looking for a more "I want to tweak" or implement there own FFmpeg profiles or tools to do this process or just if you feel like learning how everything works behind the automated scripting today! 

> [!TIP]  
> If your lazy [tbc-video-export](https://github.com/oyvindln/vhs-decode/wiki/TBC-to-Video-Export-Guide) handles pretty much everything below automatically, with profiles for codecs & frame sizes and easier handling of metadata.


## The TBC Format


| TV System | Lines | Full-Frame 4fsc | Frequency      | Frame Rate | Field Rate |
|-----------|-------|-----------------|----------------|------------|------------|
| PAL       | 625   | 1135x624        | 17727262 Hz    | 25i        | 50         |
| NTSC      | 525   | 910x524         | 14318181 Hz    | 29.97i     | 59.94      |


## Conversion TBC to Video 


`ld-chroma-decoder` --> `pipe` --> `FFmpeg` --> `Video Files`

To convert the TBC output to playable video, you need to do the following:

1. Tell the chroma-decoder how to decode the colour from TBC
2. Tell the chroma-decoder the framing of the video output
3. Tell FFmpeg if/how you want to process the video (deinterlace, 3:2 removal etc.)
4. Tell FFmpeg the required output format


## Chroma-Decoder basic use 


> [!NOTE]  
> See the [ld-chroma-decoder wiki page](Tools/ld-chroma-decoder.md) for full possible options.

The majority of users today will just want to use a YUV stream via y4m output from the chroma-decoder and a pipe to FFmpeg allowing easy creation of standard video files.


## Full Breakdown Command Example


Lets breakdown a combined command that decodes the chroma and encodes a V210 uncompressed video file via FFmpeg and pipes ready for playback and editing with virtually any software.

`INPUT.tbc` & `OUTPUT.mov` just need to be changed to use this command right now on your decoded media! 

`ld-chroma-decoder --decoder transform3d -p y4m -q INPUT.tbc| ffmpeg -i - -c:v v210 -f mov -top 1 -vf setfield=tff -flags +ilme+ildct -pix_fmt yuv422p10le -color_primaries bt470bg -color_trc bt709 -colorspace bt470bg -color_range tv -vf setdar=4/3,setfield=tff OUTPUT.mov`

----

The first stage is the chroma-decoder arguments.

`ld-chroma-decoder --decoder transform3d --output-format y4m input.tbc| ffmpeg -i -`

`ld-chroma-decoder` - Calls the application a `./` might be needed at the start (ld-chroma-decoder.exe in windows)

`--decoder transform3d` - Tells the chroma-decoder to use the PAL Transform 3D decoder on the signal.

`--output-format y4m` - Tells the chroma-decoder to output uncompressed YUV (`-p` short hand command option)

----

The 2'nd stage is piping `|` video to FFmpeg.

`input.tbc| ffmpeg -i -` - Pipes the YUV stream from the y4m option to FFmpeg ready for encoding and muxing with audio or other data such as subtitles.

Just replace `input.tbc` with your TBC file and add standard FFmpeg commands after the `-i -` part like shown examples in this page.

----

The 3'rd stage is the actual FFmpeg command that makes the video file

`ffmpeg -i - -c:v v210 -f mov -top 1 -vf setfield=tff -flags +ilme+ildct -pix_fmt yuv422p10le -color_primaries bt470bg -color_trc bt709 -colorspace bt470bg -color_range tv -vf setdar=4/3,setfield=tff OUTPUT.mov`

Video Codec: `-c:v v210 -f mov`

Interlaced Flag: `-top 1 -vf setfield=tff -flags +ilme+ildct` - Interlaced Top Field first "tff" 

Pixel Format: `-pix_fmt yuv422p10le` - 10-bit 4:2:2

Colour Space & Transfer: 

`-color_primaries smpte170m -color_trc bt709 -colorspace smpte170m` - Sets origin as NTSC and transfer to HDTV.

`-color_primaries bt470bg -color_trc bt709 -colorspace bt470bg` - Sets origin as PAL and transfer to HDTV.

Colour Range: `-color_range tv` sets the black levels to 16-255 limited TV range. (ware as pc would do 0-255 full range)

Aspect Ratio: `-vf setdar=4/3,setfield=tff` - This sets it to 4:3 standard, also added is an redundant field flag to ensure field store is set to interlaced.

Container: is defined by the final item in the command `OUTPUT.mov` so .mov for QuickTime or .mkv for Mastroska.


### Decoder Options:


PAL options `pal2d`,`transform2d`, `transform3d`, `mono`

NTSC options `ntsc1d`, `ntsc2d`, `ntsc3d`, `ntsc3dnoadapt`, `mono`


### Advanced Options


> [!WARNING]  
> The noise reduction tools are emulating LD decoder hardware, modern software filters are far more powerful./

`--chroma-nr <number> `     NTSC: Chroma noise reduction level in dB (default 0.0)

`--luma-nr <number>`        Luma noise reduction level in dB (default 1.0)

`--chroma-gain <number>`    Gain factor applied to chroma components (default 1.0)

`--chroma-phase <number>`   Phase rotation applied to chroma components (degrees; default 0.0)


> [!TIP]  
> Phase/Gain can be tweaked visually in ld-analyse then when the json is saved the chroma-decoder use its adjusted values.


## Decide Frame Size 


If you wish to have just the active or vbi area included just use the following commands.

> [!NOTE]  
> Y4M Only.

You can use these 2 commands.


NTSC:

    ld-chroma-decoder --ffll 1 --lfll 259 --ffrl 2 --lfrl 525 --decoder ntsc3d -p y4m -q INPUT.tbc OUTPUT.mov

PAL:

    ld-chroma-decoder --ffll 2 --lfll 308 --ffrl 2 --lfrl 620 --decoder transform3d -p y4m -q INPUT.tbc OUTPUT.mov