## ld-dropout-correct
This application uses the drop-out information in the JSON metadata file to perform dropout correction on the input TBC file and produces a new output file.  The current version of the corrector uses framing in order to provide inter-field correction.  Note that inter-field correction may not function correctly for NTSC pull-down sources.

Syntax:

ld-dropout-correct \<options> \<input TBC file name> \<output TBC file name>

```
Options:
  -h, --help                Displays this help.
  -v, --version             Displays version information.
  -d, --debug               Show debug
  --input-json <filename>   Specify the input JSON file (default input.json)
  --output-json <filename>  Specify the output JSON file (default output.json)
  -r, --reverse             Reverse the field order to second/first (default
                            first/second)
  -o, --overcorrect         Over correct mode (use on heavily damaged sources)
  -i, --intra               Force intrafield correction (default interfield)
  -t, --threads <number>    Specify the number of concurrent threads (default
                            is the number of logical CPUs)

Arguments:
  input                     Specify input TBC file (- for piped input)
  output                    Specify output TBC file (omit or - for piped
                            output)
```
