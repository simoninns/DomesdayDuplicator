# ld-decode Documentation

Welcome to the ld-decode Wiki.

ld-decode is an open-source effort to provide a "software defined LaserDisc player".  

The project aims to take RAW FM RF Archival captures of LaserDiscs, ideally captured by the [Domesday Duplicator](Hardware/Domesday-Duplicator.md) hardware and software (as its desgined for LaserDisc RF in mind), but can also take captures from other RF capture devices that make PCM style samples such as the [CX Cards](https://github.com/oyvindln/vhs-decode/wiki/CX-Cards), [MISRC](https://github.com/Stefan-Olt/MISRC), [Hsdaoh](https://github.com/oyvindln/vhs-decode/wiki/RF-Capture-Hardware#hsdaoh-method) and decode the RF back into usable component parts such as composite video, analogue audio and digital data and audio too.

The decoding process (like a real LaserDisc player) is a multi-stage process.  The raw RF must be demodulated (from the original FM signal) and filtered into video, audio and EFM data. This data is then framed and passed through a digital time-base correction (TBC) process which attempts to remove errors caused by the mechanical nature of a LaserDisc player during capture.

The resulting lossless 4fsc sampled TBC output is then run through a chroma-decoder (comb-filter in NTSC speak) which recovers the original color and can encode it as a digital RGB or YUV stream.  

This raw stream can be directly output to a Y4M file via the `ld-chroma-decoder` for example, but typically will be exported as lossless FFV1 or uncompressed v210 in 10-bit 4:2:2 YUV via [tbc-video-export](https://github.com/JuniorIsAJitterbug/tbc-video-export). This automates 90% of the commands to interact with the chroma-decoder and FFmpeg to encode and wrap your audio/video streams into a container like MKV or MOV, ready for viewing using media players such as [VLC](https://www.videolan.org/) or [MPC](https://github.com/clsid2/mpc-hc) or for further post-processing such as de-interlacing and upscaling for modern display use.

Please see the [Installation guide](Installation/Installation.md) for details of how to download and install ld-decode and the [basic usage guide](How-to-guides/Basic-usage-of-ld-decode.md) for instructions on how to use ld-decode.

An overview of how a LaserDisc player functions (which can help you to understand the component parts of ld-decode) is available from [this link](https://www.domesday86.com/?page_id=1379).


# Current status


ld-decode revision 7 is the current release of the decoder and associated tools.  ld-decode is capable of decoding a wide-range of PAL and NTSC LaserDiscs with support for both analog and digital sound tracks (as well as EFM data tracks as used in Interactive Video systems such as the BBC Domesday system) 

The tools suite, decoders, and DomesDay Duplicators [capture app](Hardware/Domesday-Duplicator.md) now also have self contained builds for [Windows](https://github.com/oyvindln/vhs-decode/Windows-Build), [MacOS](https://github.com/oyvindln/vhs-decode/MacOS-Build) & [Linux](https://github.com/oyvindln/vhs-decode/Linux-Build) bundled alongside with [vhs-decode](https://github.com/oyvindln/vhs-decode/wiki/) (*supports a wide range of tape formats), [cvbs-decode](https://github.com/oyvindln/vhs-decode/wiki/CVBS-Composite-Decode) & [hifi-decode](https://github.com/oyvindln/vhs-decode/hifi-decode) projects.

![Release Highlights](Misc/assets/rev6_release.jpg)
