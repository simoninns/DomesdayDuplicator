# NTSC decode guide


The following is a series of notes about how to use FFmpeg to convert the output from ld-decode (and ld-chroma-decoder) into usable video that can be watched using any compatible video player such as [VLC](https://www.videolan.org/) or [MPC](https://mpc-hc.org/).


# Identifying NTSC Material


| TV System | Lines | Full-Frame 4fsc | Frequency      | Frame Rate | Field Rate | 
|-----------|-------|-----------------|----------------|------------|------------|
| PAL       | 625   | 1135x624        | 17727262 Hz    | 25i        | 50         |
| NTSC      | 525   | 910x524         | 14318181 Hz    | 29.97i     | 59.94      |

NTSC - National TV Standards Committee (affectionately referred to as Never The Same Colour)

Used primariy in North Armerica but Japan there is the NTSC-J standard with 0 IRE black levels.

NTSC video is displayed at 29.97 frames per second, that is 59.94 fields per second (each frame being made of two fields). Except for progressive 29.97 material (commonly referred to as 30p), properly viewing it on modern displays will require some form of deinterlacing and the deinterlacing technique depends on the source.

The two most common formats are 59.94i material (commonly called 60i and usually recorded with TV cameras), and 23.976p film material (commonly called 24p) [pulled down in a 3:2 pattern](https://en.wikipedia.org/wiki/Three-two_pull_down) telecined into 29.97i. Note that some material can have a combination of both.

To identify what kind of material you have, look for an object in motion and examine the lines. If every frame has interlacing, then it is 59.94i.

![59.94i example](assets/60i.gif "59.94i")

If there are two frames with interlacing and three without, it is telecined.

![pulldown example](assets/pulldown.gif "Pulldown")

If you see no interlacing, it is 29.97p (this is not common).


# TBC to Video Export Tool


There is now the [TBC-Video-Export](https://github.com/oyvindln/vhs-decode/wiki/TBC-to-Video-Export-Guide) Python script for Linux/MacOS/Windows.

Which has pre-made easy to edit FFmpeg profiles and allows quick and simple exporting of CVBS & Y/C TBC files from ld-decode & vhs-decode and automatic encoding of the chroma-decoded output to ready to use interlaced or progressive video files.

This provies a very hands off inital export experiance sutiable for users new or ones just looking to save some time.

FFV1 10-bit 4:2:2 is the stock export profile.

Linux & MacOS

    python3 tbc-video-export.py Input-Media.tbc

Windows

    tbc-video-export.exe Input-Media.tbc


# Conversion TBC to Video


To convert the TBC output to playable video, you need to do the following:

1. Tell the chroma-decoder how to decode the colour from TBC
2. Tell the chroma-decoder the frame size of the video output
3. Tell FFmpeg if/how you want to process the video (deinterlace, 3:2 removal etc.)
4. Tell FFmpeg the required output format


## Examples 


Just edit the `input.doc.tbc` & `input.efm.pcm` with your input file names and `output.mkv` with your desired output name.


### Uncompressed Export 


V210 4:2:2 YUV Uncompressed - The universal standard for uncompressed capture and encoding and is a supported codec in broadcast, sutible for editing or playback in the QuickTime MOV container.

`ld-chroma-decoder input.doc.tbc -f ntsc2d -p y4m - | ffmpeg -i - -f s16le -r 44.1k -ac 2 -i input.efm.pcm -vcodec v210 -f mov -top 1 -vf setfield=tff -flags +ilme+ildct -pix_fmt yuv422p10le -acodec copy -color_primaries smpte170m -color_trc bt709 -colorspace smpte170m -color_range tv -pix_fmt yuv422p10 output.mov`


### Lossless Compressed Export


While FFmpeg's video filters are convenient, for better results one can compress the video losslessly and work on it with more powerful video software such as avisynth/vpaoursynth via Hybrid, StaxRip today.


`ld-chroma-decoder input.tbc -f ntsc2d -p y4m - | ffmpeg -i - -f s16le -r 44.1k -ac 2 -i input.efm.pcm -vcodec ffv1 -level 3 -threads 8 -slices 8 -coder 1 -context 1 -g 1 -slicecrc 1 -acodec flac -compression_level 11 -color_primaries smpte170m -color_trc bt709 -colorspace smpte170m -color_range tv -pix_fmt yuv422p10 output.mkv`

This example will convert the video to YUV422P10 (equivalent to 10-bit 4:2:2 SDI) and compress it losslessly with FFV1 in the `.mkv` mastroska container with `FLAC` 2:1 lossless compressed audio.

Smaller files are possible using `-g 30` and removing `-slices 8`, but decoding will require more CPU power. 

You can replace `ffv1` with `huffyuv` to use Huffyuv (another lossless intra codec) if FFV1 is not supported.


# Deinterlacing and Encoding with FFmpeg


59.94i material will look smoothest deinterlaced to 59.94p. Generally the best deinterlacer for this is [QTGMC](http://avisynth.nl/index.php/QTGMC), which is an AviSynth/VapourSynth script and is not available in FFmpeg, but used with ease in tools like [Hybrid](https://www.selur.de/downloads) which is cross platfrom & [StaxRip](https://github.com/staxrip/staxrip#readme) witch is windows based making automatic processing profiles possible to save time.

A fair substitute using FFmpeg-only filters is:

`ld-chroma-decoder input.tbc -f ntsc2d -p y4m - | ffmpeg -hide_banner -i - -f s16le -r 44.1k -ac 2 -i input.efm.pcm -vcodec libx264 -preset slow -crf 16 -acodec flac -strict -2 -compression_level 8 -vf dedot=m=rainbows,yadif=mode=send_field:parity=auto -color_primaries smpte170m -color_trc bt709 -colorspace smpte170m -color_range tv -pix_fmt yuv420p output.yadif.mcdeint.mp4 -y`

This will output a 59.94p AVC/H.264 file, in the MP4 container. To output 29.97p, change `yadif=mode=send_field` to `yadif=mode=send_frame`.

Pull-down material will look best restored to its original 23.976 framerate. This process is called inverse telecine (IVTC). FFmpeg has an IVTC filter ported from AviSynth and can do a fair job in most cases.

`ld-chroma-decoder input.tbc -f ntsc2d -p y4m - | ffmpeg -hide_banner -i - -f s16le -r 44.1k -ac 2 -i input.efm.pcm -vcodec libx264 -preset slow -crf 16 -acodec flac -strict -2 -compression_level 8 -vf dedot=m=rainbows,fieldmatch=order=auto:field=auto,decimate -color_primaries smpte170m -color_trc bt709 -colorspace smpte170m -color_range tv -pix_fmt yuv420p output.ivtc.mp4 -y`

This will output a 23.976p AVC/H.264 file, in the MP4 container.

If there are many messages stating that fields are still interlaced and you see interlaced lines in the output, try changing the order from `auto` to `tff` or `bff` and `field` from `auto` to `bottom` or `top`.  If interlacing still remains, you may be dealing with hybrid film/video content and more advanced processing will be required. Nevertheless, you can use yadif `fieldmatch=order=tff:combmatch=full,yadif=deint=interlaced,decimate` but you may need to remove `,decimate` if the output is jerky.

`dedot=m=rainbows` is a derainbowing filter and may cause artifacts.  It should be removed when ld-chroma-decoder has improved NTSC decoding.

Caveat: not all telecined sources are easy to work with. If there was mishandling in the mastering process, there can be blending of fields, which is more difficult to restore. Such sources can be identified by transparent-looking interlace lines. Either treat it as 59.94i material or see the VapourSynth examples below.

![field-blended example](assets/blended.gif "blended")


# Advanced Filtering with VapourSynth


Here are simple scripts for [VapourSynth](http://www.vapoursynth.com/), a powerful cross-platform video processing framework. Refer to the lossless export section above for getting your source ready for processing with VapourSynth.


### 59.94i Material


qtgmc.vpy:
```
import vapoursynth as vs
from vapoursynth import core
import havsfunc as haf
clip = core.ffms2.Source(source='input.mkv') # Specify input video here.
clip = haf.QTGMC(clip,Preset='slow',TFF=True)
#clip = clip[::2] # Un-comment for 29.97p output, otherwise leave commented for 59.94p
clip.set_output()
```

`vspipe qtgmc.vpy - -y | ffmpeg -i - -f s16le -r 44.1k -ac 2 -i input.efm.pcm -vcodec libx264 -preset slow -crf 16 -acodec flac -compression_level 8 -pix_fmt yuv420p -aspect 4:3 output.qtgmc.mp4 -y`

> [!CAUTION]  
> Requires [havsfunc](https://github.com/HomeOfVapourSynthEvolution/havsfunc) and its dependencies. 


### Pull-Down Material


ivtc.vpy:
```
import vapoursynth as vs
from vapoursynth import core
clip = core.ffms2.Source(source='input.mkv') # Specify input video here.
clip = core.vivtc.VFM(clip,order=1,field=0) # If results are poor, try combinations between this and order=0,field=1.
clip = core.vivtc.VDecimate(clip)
clip.set_output()
```

`vspipe ivtc.vpy - -y | ffmpeg -i - -f s16le -r 44.1k -ac 2 -i input.efm.pcm -vcodec libx264 -preset slow -crf 16 -acodec flac -compression_level 8 -pix_fmt yuv420p -aspect 4:3 output.ivtc.mp4 -y`


### Field-Blended Material


deblend.vpy:
```
import vapoursynth as vs
from vapoursynth import core
import havsfunc as haf
clip = core.ffms2.Source(source='input.mkv') # Specify input video here.
clip = haf.QTGMC(clip,Preset='slow',TFF=True)
clip = haf.srestore(clip,frate=24.000/1.001,speed=-1,thresh=8) # Try frate=25 if the material might have original come from a PAL source.
clip.set_output()
```

`vspipe deblend.vpy - -y | ffmpeg -i - -f s16le -r 44.1k -ac 2 -i input.efm.pcm -vcodec libx264 -preset slow -crf 16 -acodec flac -compression_level 8 -pix_fmt yuv420p -aspect 4:3 output.deblended.mp4 -y`

> [!CAUTION]  
> Requires [havsfunc](https://github.com/HomeOfVapourSynthEvolution/havsfunc) and its dependencies.

