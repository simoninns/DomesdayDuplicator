# ld-compress

ld-compress is a script to simplify the compression of .lds (raw LaserDisc RF files) into .ldf images.

ld-decode fully supports FLAC compressed files as input.  Files can be suffixed with .ldf as shown here, or .raw.oga.  ld-decode will automatically uncompress the input file during processing.

To compress a .lds file simply use:

```
ld-compress <filename>.lds
```

This script will write a .ldf compressed version of the .lds file to the directory it's called from.

### Enabling GPU acceleration for ld-compress

ld-compress also supports GPU acceleration via [FlaLDF](https://github.com/TokugawaHeavyIndustries/FlaLDF).  This requires an OpenCL compatible GPU and installation of FlaLDF.

[Download FlaLDF](https://github.com/TokugawaHeavyIndustries/FlaLDF/releases/latest).  Linux users, install using the .deb.  If you're on Mac, add FlaLDF to your PATH.

To compress an .lds file with GPU acceleration, use:

```
ld-compress -a <filename>.lds
```
Flaccl does not presently support ogg file containers, so the the output will be with the file extension of .flac.ldf to distinguish from traditionally compressed captures.

## Windows Bash Scripts

Requires FLAC [Installed in path](https://github.com/oyvindln/vhs-decode/wiki/Windows-Build#install-ffmpegsoxflac-inside-windows) / [FLACLDF](https://github.com/TokugawaHeavyIndustries/FlaLDF) Installed in the directory of tools.

Save files to `.bat` to make them drag and drop scripts. 

CPU (Native)

```````
@echo off
title Compressing : %~n1
C:\ld-tools-suite-windows\ld-lds-converter.exe -u -i "%~1" | ffmpeg -f s16le -ar 40k -ac 1 -i - -acodec flac -compression_level 11 -f ogg "%~dp1%~n1.ldf"

pause
```````

GPU (Nvida CUDA)

```````
@echo off
title Compressing : %~n1
C:\ld-tools-suite-windows\ld-lds-converter.exe -i "%~1" -u -r | C:\ld-tools-suite-windows\CUETools.FLACCL.cmd.exe -11 -o "%~dp1%~n1.ldf" --lax --ignore-chunk-sizes --task-size 16 --fast-gpu -

pause
````````


## Command List

The full list of command line options is as follows:

```
Usage: /usr/local/bin/ld-compress [-c] [-a] [-u] [-v] [-p] [-h] [-l <1-12>] [-g] file(s)

Modes:
-c Compress (default): Takes one or more .lds files and compresses them to .ldf files in the current directory.
-u Uncompress: Takes one or more .ldf/.raw.oga files and uncompresses them to .lds files in the current directory.
-a GPU Acceleration.  Uses OpenCL or CUDA to accelerate encoding. See https://github.com/happycube/ld-decode/wiki/ld-decode-utilities
-v Verify: Returns md5 checksums of the given .ldf/.raw.oga files and their contained .lds files for verification purposes.
Options
-p Progress: displays progress bars - requires pv to be installed.
-h Help: This dialog.
-l Compression level 1 - 12 (1 - 11 for GPU encoding). Default is 11 (10 for GPU). 6 is recommended for faster but fair compression.
-g Use .raw.oga extension instead of .ldf when compressing.
```
