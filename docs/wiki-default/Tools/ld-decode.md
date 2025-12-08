## ld-decode


This application processes raw LaserDisc capture files (`.lds` 10-bit packed or `.ldf` 16-bit signed compressed in FLAC) into Time Base Corrected files (.tbc) for use with the rest of the ld-decode tool-chain.


-----------

Syntax:

```
usage: ld-decode [-h] [--start file-location] [--length frames] [--seek frame] [--PAL] [--NTSC] [--NTSCJ] [-m mtf] [--MTF_offset mtf_offset] [--noAGC] [--noDOD]
                 [--noEFM] [--preEFM] [--disable_analog_audio] [--AC3] [--start_fileloc start_fileloc] [--ignoreleadout] [--verboseVITS] [--RF_TBC] [--lowband]
                 [--NTSC_color_notch_filter] [--V4300D_notch_filter] [-d deemp_adjust] [--deemp_low deemp_low] [--deemp_high deemp_high] [-t threads] [-f FREQ]
                 [--analog_audio_frequency AFREQ] [--video_bpf_high FREQ] [--video_lpf FREQ] [--video_lpf_order VLPF_ORDER] [--audio_filterwidth FREQ]
                 infile outfile

Extracts audio and video from raw RF laserdisc captures

positional arguments:
  infile                source file
  outfile               base name for destination files

options:
  -h, --help            show this help message and exit
  --start file-location, -s file-location
                        rough jump to frame n of capture (default is 0)
  --length frames, -l frames
                        limit length to n frames
  --seek frame, -S frame
                        seek to frame n of capture
  --PAL, -p, --pal      source is in PAL format
  --NTSC, -n, --ntsc    source is in NTSC format
  --NTSCJ, -j           source is in NTSC-J (IRE 0 black) format
  -m mtf, --MTF mtf     mtf compensation multiplier
  --MTF_offset mtf_offset
                        mtf compensation offset
  --noAGC               Disable AGC
  --noDOD               disable dropout detector
  --noEFM               Disable EFM front end
  --preEFM              Write filtered but otherwise pre-processed EFM data
  --disable_analog_audio, --disable_analogue_audio, --daa
                        Disable analog(ue) audio decoding
  --AC3                 Enable AC3 audio decoding (NTSC only)
  --start_fileloc start_fileloc
                        jump to precise sample # in the file
  --ignoreleadout       continue decoding after lead-out seen
  --verboseVITS         Enable additional JSON fields
  --RF_TBC              Create a .tbc.ldf file with TBC'd RF
  --lowband             Use more restricted RF settings for noisier disks
  --NTSC_color_notch_filter, -N
                        Mitigate interference from analog audio in reds in NTSC captures
  --V4300D_notch_filter, -V
                        LD-V4300D PAL/digital audio captures: remove spurious ~8.5mhz signal
  -d deemp_adjust, --deemp_adjust deemp_adjust
                        Deemphasis level multiplier
  --deemp_low deemp_low
                        Deemphasis low coefficient
  --deemp_high deemp_high
                        Deemphasis high coefficient
  -t threads, --threads threads
                        number of CPU threads to use
  -f FREQ, --frequency FREQ
                        RF sampling frequency in source file (default is 40MHz)
  --analog_audio_frequency AFREQ
                        RF sampling frequency in source file (default is 44100hz)
  --video_bpf_high FREQ
                        Video BPF high end frequency
  --video_lpf FREQ      Video low-pass filter frequency
  --video_lpf_order VLPF_ORDER
                        Video low-pass filter order
  --audio_filterwidth FREQ
                        Analog audio filter width

FREQ can be a bare number in MHz, or a number with one of the case-insensitive suffixes Hz, kHz, MHz, GHz, fSC (meaning NTSC) or
fSCPAL.
```
