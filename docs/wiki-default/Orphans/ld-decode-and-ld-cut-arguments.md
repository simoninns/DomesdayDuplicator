# ld-decode and ld-cut arguments

```
usage: ld-decode [-h] [-s start] [--start_fileloc start_fileloc] [-S seek]
                 [-l length] [-p] [-n] [-m mtf] [--MTF_offset mtf_offset] [-j]
                 [--noDOD] [--noEFM] [--daa] [--ignoreleadout] [--verboseVITS]
                 [--lowband] [--WibbleRemover] [-t threads] [-f FREQ]
                 [--video_bpf_high FREQ] [--video_lpf FREQ]
                 infile outfile

Extracts audio and video from raw RF laserdisc captures

positional arguments:
  infile                source file
  outfile               base name for destination files

optional arguments:
  -h, --help            show this help message and exit
  -s start, --start start
                        rough jump to frame n of capture (default is 0)
  --start_fileloc start_fileloc
                        jump to precise sample # in the file
  -S seek, --seek seek  seek to frame n of capture
  -l length, --length length
                        limit length to n frames
  -p, --pal             source is in PAL format
  -n, --ntsc            source is in NTSC format
  -m mtf, --MTF mtf     mtf compensation multiplier
  --MTF_offset mtf_offset
                        mtf compensation offset
  -j, --NTSCJ           source is in NTSC-J (IRE 0 black) format
  --noDOD               disable dropout detector
  --noEFM               Disable EFM front end
  --daa                 Disable analog audio decoding
  --ignoreleadout       continue decoding after lead-out seen
  --verboseVITS         Enable additional JSON fields
  --lowband             Use more restricted RF settings for noisier disks
  --WibbleRemover       PAL/digital sound: (try to) remove spurious ~8.5mhz
                        signal. Mitigate interference from analog audio in
                        reds on NTSC
  -t threads, --threads threads
                        number of CPU threads to use
  -f FREQ, --frequency FREQ
                        RF sampling frequency in source file (default is
                        40MHz)
  --video_bpf_high FREQ
                        Video BPF high end frequency
  --video_lpf FREQ      Video low-pass filter frequency

FREQ can be a bare number in MHz, or a number with one of the case-insensitive
suffixes Hz, kHz, MHz, GHz, fSC (meaning NTSC) or fSCPAL.
```

ld-cut is a subset, with outfile being a raw 16-bit file or lds:

```
usage: ld-cut [-h] [-s start] [-l length] [-S seek] [-E end] [-p] [-n]
              infile outfile

Extract a sample area from raw RF laserdisc captures. (Similar to ld-decode,
except it outputs samples)

positional arguments:
  infile                source file
  outfile               destination file (recommended to use .lds suffix)

optional arguments:
  -h, --help            show this help message and exit
  -s start, --start start
                        rough jump to frame n of capture (default is 0)
  -l length, --length length
                        limit length to n frames
  -S seek, --seek seek  seek to frame n of capture
  -E end, --end end     cutting: last frame
  -p, --pal             source is in PAL format
  -n, --ntsc            source is in NTSC format
```
