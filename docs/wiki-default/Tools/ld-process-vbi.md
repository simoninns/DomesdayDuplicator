## ld-process-vbi

This application detects and extracts metadata from the vertical blanking interval for each field in an input TBC file, and adds the metadata into the TBC's JSON metadata file (or, with `--output-json`, to a new JSON file).

It currently supports:

* LaserDisc biphase code
* LaserDisc FM code and white flag (NTSC only)
* Closed Captions (NTSC only)
* Vertical Interval Timecode

While ld-decode and vhs-decode extract some of this information during decoding, ld-process-vbi supports more formats and does more thorough sanity-checking, so it's a good idea to run it after decoding and before further processing. If you are stacking multiple captures, you should run it again on the stacked TBC file to update the metadata.

Syntax:

ld-process-vbi \<options> \<input TBC file name>

```
Options:
  -h, --help                Displays this help.
  -v, --version             Displays version information.
  -d, --debug               Show debug
  --input-json <filename>   Specify the input JSON file (default input.json)
  --output-json <filename>  Specify the output JSON file (default same as
                            input)
  -n, --nobackup            Do not create a backup of the input JSON metadata
  -t, --threads <number>    Specify the number of concurrent threads (default
                            is the number of logical CPUs)

Arguments:
  input                     Specify input TBC file
```
