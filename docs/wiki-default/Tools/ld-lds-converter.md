## ld-lds-converter
This application can accept 10-bit packed format .lds files and convert them to 16-bit signed files or vice-versa (useful for expanding .lds files and piping the output to a compression utility such as FLAC).

Syntax:

ld-lds-converter \<options>

```
Options:
  -h, --help           Displays this help.
  -v, --version        Displays version information.
  -d, --debug          Show debug
  -i, --input <file>   Specify input laserdisc sample file (default is stdin)
  -o, --output <file>  Specify output laserdisc sample file (default is stdout)
  -u, --unpack         Unpack 10-bit data into 16-bit (default)
  -r, --riff           Unpack 10-bit data into 16-bit with RIFF WAV headers (use with -u)
  -p, --pack           Pack 16-bit data into 10-bit
```
