## ld-export-metadata
This application reads an ld-decode JSON metadata file, typically as produced by ld-process-vbi, and exports information in standard formats that other tools can read. At present, it can export:

- Per-frame signal quality information from the VITS test signals, as CSV
- Per-frame LaserDisc VBI control signals, as CSV
- LaserDisc navigation information, as Audacity labels
- LaserDisc navigation information, as FFMETADATA1 (which FFmpeg can use to add chapter navigation to a video file)
- Closed Captions, as SCC format (which tools like [ttconv](https://github.com/sandflow/ttconv) can read)

Syntax:

ld-export-metadata \<options> \<input>

```
Options:
  -h, --help                Displays help on commandline options.
  --help-all                Displays help including Qt specific options.
  -v, --version             Displays version information.
  -d, --debug               Show debug
  -q, --quiet               Suppress info and warning messages
  --vits-csv <file>         Write VITS information as CSV
  --vbi-csv <file>          Write VBI information as CSV
  --audacity-labels <file>  Write navigation information as Audacity labels
  --ffmetadata <file>       Write navigation information as FFMETADATA1
  --closed-captions <file>  Write closed captions as Scenarist SCC V1.0 format

Arguments:
  input                     Specify input JSON file
```
