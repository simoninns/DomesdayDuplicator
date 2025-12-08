# ld-cut

ld-cut is a utility for cutting samples from raw RF LaserDisc captures (useful to create samples of trouble-areas when issue reporting), and can now also be used to compress .lds files.  The utility allows you to seek and specify start and end frames similar to the main ld-decode application.

```
usage: ld-cut [-h] [-s start] [-l length] [-S seek] [-E end] [-p] [-n]
              infile outfile

Extract a sample area from raw RF laserdisc captures. (Similar to ld-decode,
except it outputs samples)

positional arguments:
  infile                source file
  outfile               destination file (recommended to use .lds or .ldf suffixes)

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

Using ld-cut, you can do parallel .ldf encodings (optionally targeting different directories) using shell scripting pretty easily:

```
for i in f1.lds f2.lds f3.lds f4.lds; do (ld-cut $i /someotherdirectory/`basename -s .lds $i`.ldf &); done
```
