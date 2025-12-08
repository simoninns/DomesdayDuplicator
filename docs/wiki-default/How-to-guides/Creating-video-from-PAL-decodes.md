# Overview


The following is a series of notes about how to use FFmpeg to convert the output from ld-decode (and ld-chroma-decoder) into usable video that can be watched using any compatible video player such as [VLC](https://www.videolan.org/) or [MPC](https://mpc-hc.org/).

Information is compiled from a number of sources; however the project would like to thank Stephen Neal for his valuable advice on this subject.


# PAL video formats


| TV System | Lines | Full-Frame 4fsc | Frequency      | Frame Rate | Field Rate |
|-----------|-------|-----------------|----------------|------------|------------|
| PAL       | 625   | 1135x624        | 17727262 Hz    | 25i        | 50         |

PAL - Phase Alternating Line, used in UK, Europe primarily, with PAL-M being used in Brazil.

The following text uses the EBU standard descriptions for video formats wherever possible.  

So PAL SD interlaced video is written as 576/i25 - whereas (in earlier descriptions) it was typically described as 576/50i.

In the EBU notation, if the letter is first then the following number is always frames. If the letter is last then the number is either fields (for interlaced video) or frames (for progressive video).

So, for example, PAL SD progressive film source is 576p25 2:2-ed in a 576i25 frame - but that would have been described as 576/25p in a 576/50i signal in the old format.

PAL SD progressive deinterlaced is 576p50 (which isn't a broadcast standard outside of Australia...) however, 576p50 is a lot easier for computers to handle than 576i25 as it removes the need for them to deinterlace - which many PC based players often can't do, or don't do well.


# TBC to Video Export Tool


There is now the [TBC-Video-Export](https://github.com/oyvindln/vhs-decode/wiki/TBC-to-Video-Export-Guide) the universal export tool for Linux/MacOS/Windows systems.

Which has pre-made easy to edit FFmpeg profiles and allows quick and simple exporting of CVBS & Y/C TBC files from ld-decode & vhs-decode and automatic encoding of the chroma-decoded output to ready to use interlaced or progressive video files.

This provides a very hands off lazy initial export experience suitable for new users and ones wanting a tool with advanced scripting abbilitys.

FFV1 10-bit 4:2:2 is the stock export profile.

Linux & MacOS

    tbc-video-export Input-Media.tbc

Windows

    tbc-video-export.exe Input-Media.tbc


## Manual Conversion TBC to Video 


To convert the TBC output to playable video, you need to do the following:

1. Tell the chroma-decoder how to decode the colour from TBC
2. Tell the chroma-decoder the frame size of the video output
3. Tell FFmpeg if/how you want to process the video (deinterlace, 3:2 removal etc.)
4. Tell FFmpeg the required output format


## Chroma-Decoder basic use 


See the [ld-chroma-decoder wiki page](../Tools/ld-chroma-decoder.md) for full possible options.

The majorty of users today will just want to use a YUV stream via y4m output from the chroma-decoder and a pipe to FFmpeg allowing easy creation of standard video files.


## Broken Down Command Example


Lets breakdown a combined command that decodes the chroma and encodes a V210 uncompressed video file via FFmpeg and pipes ready for playback and editing with virtually any software.

`INPUT.tbc` & `OUTPUT.mov` just need to be changed to use this command right now on your decoded media! 

`ld-chroma-decoder --decoder transform3d -p y4m -q INPUT.tbc| ffmpeg -i - -c:v v210 -f mov -top 1 -vf setfield=tff -flags +ilme+ildct -pix_fmt yuv422p10le -color_primaries bt470bg -color_trc bt709 -colorspace bt470bg -color_range tv -vf setdar=4/3,setfield=tff OUTPUT.mov`

----

The first stage is the chroma-decoder arguments.

`ld-chroma-decoder --decoder transform3d --output-format y4m input.tbc| ffmpeg -i -`

`ld-chroma-decoder` - Calls the application a `./` might be needed at the start (ld-chroma-decoder.exe in windows)

`--decoder transform3d` - Tells the chroma-decoder to use the PAL Trasform 3D decoder on the signal.

`--output-format y4m` - Tells the chroma-decoder to output uncompressed YUV (`-p` short hand command option)

----

The 2'nd stage is piping `|` video to FFmpeg.

`input.tbc| ffmpeg -i -` - Pipes the YUV stream from the y4m option to FFmpeg ready for encoding and muxing with audio or outher data such as subtitles.

Just replace `input.tbc` with your TBC file and add standard FFmpeg commands after the `-i -` part like shown examples in this page.

----

The 3'rd stage is the actual FFmpeg command that makes the video file

`ffmpeg -i - -c:v v210 -f mov -top 1 -vf setfield=tff -flags +ilme+ildct -pix_fmt yuv422p10le -color_primaries bt470bg -color_trc bt709 -colorspace bt470bg -color_range tv -vf setdar=4/3,setfield=tff OUTPUT.mov`

Video Codec: `-c:v v210 -f mov`

Interlaced Flag: `-top 1 -vf setfield=tff -flags +ilme+ildct` - Interlaced Top Field first "tff" 

Pixel Format: `-pix_fmt yuv422p10le` - 10-bit 4:2:2

Colour Space & Transfer: `-color_primaries bt470bg -color_trc bt709 -colorspace bt470bg` - Sets origin as PAL and trasfer to HDTV.

Colour Range: `-color_range tv` sets the black levels to 16-255 limited TV range. (ware as pc would do 0-255 full range)

Aspect Ratio: `-vf setdar=4/3,setfield=tff` - This sets it to 4:3 standard, also added is an redundent field flag to ensure field store is set to interlaced.

Container: is defined by the final item in the command `OUTPUT.mov` so .mov for Quicktime or .mkv for Mastroska.


### Decoder Options:


PAL options `pal2d`,`transform2d`, `transform3d`, `mono`

NTSC options `ntsc1d`, `ntsc2d`, `ntsc3d`, `ntsc3dnoadapt`, `mono`


### Advanced Options


`--chroma-nr <number> `     NTSC: Chroma noise reduction level in dB (default 0.0)

`--luma-nr <number>`        Luma noise reduction level in dB (default 1.0)

`--chroma-gain <number>`    Gain factor applied to chroma components (default 1.0)

`--chroma-phase <number>`   Phase rotation applied to chroma components (degrees; default 0.0)


## Decide Frame Size 


If you wish to have just the active or vbi area included just use the following commands.

TBC Video Export:

    python3 tbc-video-export.py --vbi Input-Media.tbc

(Y4M Only*) you can use these 2 commands.

NTSC:

    ld-chroma-decoder --ffll 1 --lfll 259 --ffrl 2 --lfrl 525 --decoder ntsc3d -p y4m -q INPUT.tbc OUTPUT.mov

PAL:

    ld-chroma-decoder --ffll 2 --lfll 308 --ffrl 2 --lfrl 620 --decoder transform3d -p y4m -q INPUT.tbc OUTPUT.mov


# Complete FFmpeg examples


You want to edit `input.tbc` and `output.xxx` on each command for your respective input and output file name, and container.

Use the `.mkv` container if you dont need `.mov` or `.mp4` for compatability sake. 


## Encode Interlaced


To encode a video without deinterlacing (note that the `-top 1` switch tells FFmpeg that the video is first field first; this hint is required for players to correctly de-interlace the video during playback.  If the original LaserDisc is second field first, this switch should be removed) :

`ffmpeg -i "input.mkv" -pix_fmt yuv420p -top 1 -vcodec libx264 -crf 18 -flags +ildct+ilme -aspect 768:576 "output.576i25.mp4"`

To add PCM analogue audio to the encoding add the following parameters before the initial -f in the command above:

`-f s16le -ar 44.1k -ac 2 -i input.pcm`

To chroma decode the .tbc file and combine analogue sound (pcm) with the video, use the following command line:

`ld-chroma-decoder --decoder transform3d input.tbc -p y4m | ffmpeg -f s16le -ar 44.1k -ac 2 -i input.pcm -i - -pix_fmt yuv420p -vcodec libx264 -crf 18 -flags +ildct+ilme -aspect 768:576 output.576i25.mp4`

To export a more practical codec for editing or post production you can encode to ProRes HQ.

`ld-chroma-decoder --decoder transform3d -p y4m -q input.tbc | ffmpeg -i - -c:v prores -profile:v 3 -vendor apl0 -bits_per_mb 8000 -quant_mat hq -f mov -top 1 -vf setfield=tff -flags +ilme+ildct -pix_fmt yuv422p10 -color_primaries bt470bg -color_trc bt709 -colorspace bt470bg -color_range tv -vf setdar=4/3,setfield=tff OUTPUT.mov`


## Encode with deinterlacing


`ffmpeg -i "input.mkv" -vf "w3fdif=complex:all" -pix_fmt yuv420p -vcodec libx264 -crf 18 -aspect 768:576 "output.576p50.mp4"`

Decode the .tbc file, deinterlace with w3fdif, and combine analogue sound (pcm) with the video:

`ld-chroma-decoder -p y4m --decoder transform3d input.tbc | ffmpeg -f s16le -ar 44.1k -ac 2 -i input.pcm -i - -filter:v "w3fdif=complex:all" -pix_fmt yuv420p -c:v libx264 -crf 18 -aspect 768:576 "output.576p50.mp4"`

Decode the .tbc file, deinterlace with bwdif, and combine analogue sound (pcm) with the video:

`ld-chroma-decoder -p y4m --decoder transform3d input.tbc| ffmpeg -f s16le -ar 44.1k -ac 2 -i input.pcm -i - -filter:v "bwdif=1" -pix_fmt yuv420p -c:v libx264 -crf 16 -flags +ildct+ilme -aspect 768:576 output.576p50.mp4`


## Online Usage


For upload to YouTube (deinterlaced files only) it is recommended to scale the video to `2880x2176` up to `5760x4320` to prevent YouTube's SD-HD highly macroblock baised encoding from causing a massive drop in visual image quality, while this will create large files however the resulting playback quality after upload is far better then the alternative.

- FFV1 (If under size limit)
- HEVC (120mbps for 50/59.97p)

`ld-chroma-decoder --decoder transform3d -p y4m -q input.tbc | ffmpeg -i - -f s16le -ar 44.1k -ac 2 -c:v prores -profile:v 3 -vendor apl0 -bits_per_mb 8000 -quant_mat hq -f mov -top 1 -pix_fmt yuv422p10 -color_primaries bt470bg -color_trc bt709 -colorspace bt470bg -color_range tv -vf bwdif=1:0:0 -vf scale=2880x2176:flags=lanczos -aspect 768:576 output_ProRes_HQ_YT.mov`


The `.mov` container is used for compliance as the codec used is ProRes HQ which is natively supported by YouTube in either container so if you do not need it for complince on editing or playback hardware or are storing the file long term you are better off using the `.mkv` container which is harder to damage and easyer to stream/upload due to not being headder dependent. 

For platforms that don't re-encode their uploaded files like [Odysee](https://odysee.com/), you can use a lower bitrate native SD file with this re-encoding script:

`ld-chroma-decoder --decoder transform3d -p y4m -q input.tbc | ffmpeg -i - -f s16le -ar 44.1k -ac 2 -c:v libx264 -bufsize 16000k -crf 20 -maxrate 8000k -movflags +faststart -pix_fmt yuv420p -color_primaries bt470bg -color_trc bt709 -colorspace bt470bg -color_range tv -vf bwdif=1:0:0 -vf -aspect 768:576 output_web.mov`

The mov or mp4 container is recommended for web-browser support. 

AVC/H.264 & HEVC/H.265 8mbps 4:2:0 web profiles are included in the tbc-video-export tool, but for highest compatability AVC/H.264 is the best codec to use today sadly and not HEVC or the newer AV1 codecs. 


## De-interlacing Filters with FFmpeg


Deinterlacing is the process of taking the 576i25 (i.e. 50 x 288 line fields sampled 1/50th second apart) and create a 576p50 (i.e. 50 x 576 line frames) 

Today there is 3 widly used options.

[BWDIF](https://ffmpeg.org/ffmpeg-filters.html#bwdif-1) (Bob Weaver Deinterlacing Filter) `-vf bwdif=1:0:0` combining elements from yadif & w3fdif FFmpegs newest hybrid deinterlacing filter for PAL.


[W3FDIF](https://ffmpeg.org/ffmpeg-filters.html#w3fdif) (BBC R&D Weston 3-field deinterlacing) `-filter:v "w3fdif=complex:all"` based off the BBC filter.

Complex forces it to use the complex VT co-efficients, all forces it to deinterlace all frames (not just those flagged as interlaced - which the rawvideo won't be).  This is only required if you want to avoid your display solution deinterlacing.

[QTGMC](http://avisynth.nl/index.php/QTGMC), which is an modern [AviSynth](http://avisynth.nl/index.php/Main_Page)/[VapourSynth](https://www.vapoursynth.com/) scriptable filter is not available in FFmpeg, but used with ease in tools like [Hybrid](https://web.archive.org/web/20230601152934/https://www.selur.de/downloads) which is cross platfrom & [StaxRip](https://github.com/staxrip/staxrip#readme) witch is windows based.


## Scaling & Resizing


If you want to scale to a standard resolution from 928x576 PAL or 760x488 NTSC can do this with `-vf "scale=768:576"` or you can do a pad and scale to 720 x 576 standard SD video resolution (TBC as I need to work out exact options to this as 4:3 analogue video should be in the central 702x576 area within a 720x576 frame, but a quick and dirty would be to ignore the 9 samples of blanking each side.


`-vf "scale=720:480"` - Standard SD NTSC

`-vf "scale=720:576"` - Standard SD PAL

`-vf "scale=1440:1080"` - HDTV

`-vf "scale=5760:4320"` - YouTube 8k Bracket Use

If you keep things interlaced then you may need a `-vf "scale=interl=1"` in the path to ensure 4:2:0 interlaced-aware chroma scaling.

[AviSynth](http://avisynth.nl/index.php/Main_Page)/[VapourSynth](https://www.vapoursynth.com/) - Also have filters like Spline16 wich are widly used.


## Tell FFmpeg the required output format


Today you have AVC/H.264 used with your common MP4 files from the last 15 years, then you have the modern HEVC/H.265 used over the last decade on phones/UHD blu-ray using the mp4/mov container or mkv if recorded with desktop applicaitons.


## Codecs


Note for UHD Blu-ray players and modern devices, 4:2:0 10-bit H.265 encoding can be used with `-pix_fmt yuv420p10`.

`-c:v libx264` will signal to use the AVC/x264 encoder.

`-c:v libx265` will signal to use the HEVC/x265 encoder. (Nearly half the file size of AVC)

-crf = **c**onstant **r**ate **f**actor.  0 is mathematically lossless, 18 is deemed near-transparent and visually close-to-lossless.  Default is 23. (Order of magnitude - decreasing crf by 6 doubles file size, increasing crf by 6 halves file size approx)

`-pix_fmt yuv420p -vcodec libx264 -crf 0 -aspect 768:576 'colourbars.mp4'`

H.264/AVC using the x264 encoder is a good solution for playback on older consumer devices (it's the Blu-ray format and used for HDTV in most of Europe).  

For graphics card accelerated encoding use `-hwaccel` for AMD and Nvidia `-hwaccel cuda` at the start of your command just after `ffmpeg`. 

### Containers 

A container or wrapper as its commonly referred to as is what holds your audio video data for example "mp4" is not a format is a container.

`.mov` is QuickTime and `.mkv` is Mastroska, `.wav` is almost always uncompressed PCM and `.flac` is FLAC compressed audio.

Today `.mov` is ideal for consumer devices and `.mxf` for editors, and `.mkv` is suitable for archival and media server use but also works with resolve today so is what most people will stick to. 

-----
### Aspect ratio 


Standard 4:3 and Widescreen 16:9

`-aspect 768:576`  This flags the display aspect ratio as square pixel 'PAL' but leaves the video as 928x576 within the codec. It's then up to the player to handle the scaling.  I keep the vertical resolution the same to avoid a vertical scale.   Effectively the two different figures let the player calculate the pixel aspect ratio - as it is non-square for most SD video formats - 4fSC and Rec.601 4:3 or 16:9.

------
### Interlacing 


If you haven't deinterlaced to 50p with a deinterlacer and want native interlaced output:

`-flags +ildct+ilme` force FFmpeg to encode native interlaced (using interlaced DCT and motion estimation) rather than progressive.

If this is not defined the `field store` flag will most likely be set to progressive. 

Example Command 

`-pix_fmt yuv420p -vcodec libx264 -crf 0 -flags +ildct+ilme -aspect 768:576 'colourbars.576i.mp4'`

-------
### Video Levels


`-color_range tv` defines 16-255 Limited TV Display Levels (Black Range)

`-color_range pc` defines 0-255 Full Desktop Monitor Levels (Black Range)

--------
### Colour System 


`-color_primaries bt470bg -colorspace bt470bg` Defines PAL colour space.

`-color_primaries smpte170m -colorspace smpte170m`  Defines NTSC colour space. 

`-color_trc bt709` transfers the colour space for SD content being displayed on HDTVs

----------
### Chroma Sub-Sampling 


For compatibility with consumer video devices you need to go from 4:2:2 10-bit / 4:4:4 16-bit to 4:2:0 8-bit -
 which `-pix_fmt yuv420p` will do.

`-pix_fmt yuv444p10le` 10-bit 4:4:4 - Full Information 

`-pix_fmt yuv422p10le` 10-bit 4:2:2 - Covers SD Colour Entirely (Editing/Archive)

`-pix_fmt yuv420p`     8-bit 4:2:0 - Supported by any digital playback device (Smart TV, Smartphone, Blu-Ray Player etc)


# Audio

This segment deals with how to handle audio.


## Stereo Analog


LaserDiscs mostly contain standard 2 channel left/right stereo.

This is decoded to a standard `output.pcm` file ready for FFmpeg encoding/remuxing with a video file.

The analog track on some discs can be slightly slower than the video, digital, and AC3 tracks.

The best way to workaround this currently is to use a lower samplerate value when importing/encoding the PCM file, between `44085` and `44089` and then resampling to `44100` which effectivly "stretches" the audio to fit correctly.


## Bilingual or dual-mono sound


Some LaserDiscs provide bilingual/dual-mono sound. This is where the stereo audio is two independent mono tracks.  

To map this correctly through FFmpeg use a command similar to the following:

`ffmpeg -i stereo.wav -map_channel 0.0.0 left.wav -map_channel 0.0.1 right.wav`


## DTS & AC3 Sound 

4.0 Matrix - AC3

5.1 Surround - DTS

Digital audio was one of the main benfits of LD when it came out, as until this point only 35mm & 16mm film prints was common to have digital audio tracks optically off to the side, this also included 5.1 surround audio.


### Multiple Audio Tracks


Using `ffmpeg` you can embed multiple audio tracks into a video container.

For any PCM files you will need to tell `ffmpeg` the format of the
audio before you include it with `-f s16le -r 44.1k -ac 2`. 

Then you can use the normal `-i $filename` include it. 

You can then `-map` the included audio files using the ID numbers for the streams (which start at 0 and include all 
inputs). You will likely always be outputting to a single file as well. 

So to map the second included file's audio to the output video you would map 
`-map 1:a:0`. This can be done for any kind of audio inputs.

You may also want to adjust the channel of tracks, for AC3 discs you may want
to convert from AC3 encoded audio to PCM and specifying for example:

`-channel_layout:a:1 "5.1" -c:a:1 pcm_s16le -osr 44100` 

> [!NOTE]  
> Where `a:1` is the *output* audio track will correct the sample rate, re-encode it, and set it as
5.1 surround. 


You will then likely want to use 
`-filter_complex "[3:a]channelsplit=channel_layout=stereo[left][right]"` 
where `3:a` is the *input* ID for the analog audio.

Then you can `-map "[left]"` to only include the left channel analog audio channel in your output file.


--------------

    ld-chroma-decoder --decoder ntsc3d -p y4m -q "$input" | ffmpeg -y -i - \
    -i digital-audio.dts \
    -f s16le -ar $analog_rate -ac 2 -i output.pcm \
    -i ffdata \
    $srt \
    $vcodec $vfilter \
    -map 0:v:0 \
    -map 2:a:0 \
    -metadata:s:a:0 title="Analog Stereo" \
    -c:a:0 $stereo -ar 44100 \
    -map 1:a:0 \
    -metadata:s:a:1 title="PCM Surround" \
    -channel_layout:a:1 "5.1" \
    -c:a:1 pcm_s16le \
    -map_metadata 4 \
    output.mov 2>&1 | tee -a log/ffmpeg.log


### Chapters and Subtitles


`ld-export-metadata` chapters and subtitles (as SRT) can be embedded in the 
output file. 

Chapters from the `--ffmetadata` option and be included with
`-map_metadata 4` where `4` is the *input* file ID. 

Subtitles can be included with `-map 5:s:0 -c:s mov_text -metadata:s:s:0 language=eng` where `5:s` is 
the *input* ID. 

The `-c:s mov_text` is for an MOV file and will be different 
for other video containers.



# Notations


Direct RAW FFmpeg Usage 

`ffmpeg -f rawvideo -r 25 -pix_fmt rgb48 -s 928x576 -i "input.rgb"`

This will take a PAL `input.rgb` source - tell it the source is raw video, at 25 frames per second, in RGB48 format and 928x576 resolution.

`ffmpeg -i "input.y4m"`

This will take a `input.y4m` source and automatically determine the framerate, pixel format, and resolution.

