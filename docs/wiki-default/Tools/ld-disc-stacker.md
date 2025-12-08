## ld-disc-stacker
This application performs 'stacking' of multiple versions of the same disc (ideally different copies of discs containing the same mastered contents).  Disc stacking requires a minimum of two input sources in order to work (although 3 or more are strongly recommended).

Stacking is performed by taking all available input sources and, pixel by pixel, determining the number of available sources for the pixel (by removing outliers marked as dropouts in the source files).  If more than 3 sources are available for the pixel the tool will use the median value of the available sources as the output (when there are an even number available of pixel sources, the two centre values of the median are averaged to the output).  If only two sources are available an average is used.  If only one source is available it is passed directly as the output.

If no sources are available for a pixel the tool will attempt to use differential dropout detection (diffDOD) to recover a value (in the case where ld-decode marked the pixel as a false-negative) unless the --no-diffdod option is specified.

If the --passthrough option is specified the tool will, when all pixel sources are marked as a dropout, mark the resulting output pixel as a dropout (regardless of the diffDOD result).  The --passthrough is useful in non-preservation cases where it is desirable for master plate errors (which cause dropouts in the same place on all resulting LaserDisc copies) to be marked for dropout concealment (where-as diffDOD would correctly identify the plate error as present on all discs (and therefore not an error)).

Use the tool by specifying the available input .tbc files followed by the desired .tbc output file.

```
Options:
  -v, --version                     Displays version information.
  -d, --debug                       Show debug
  -q, --quiet                       Suppress info and warning messages
  -?, -h, --help                    Displays help on commandline options.
  --help-mode                       Show info about stacking mode
  -V, --verbose                     Show more info during stacking
  --input-metadata <filename>       Specify the input metadata file for the
                                    first input file (default input.db)
  --output-metadata <filename>      Specify the output metadata file (default
                                    output.db)
  -r, --reverse                     Reverse the field order to second/first
                                    (default first/second)
  -t, --threads <number>            Specify the number of concurrent threads
                                    (default is the number of logical CPUs)
  -m, --mode <number>               Specify the stacking mode to use (default
                                    is Auto) -1 = auto / 0 = mean / 1 = median /
                                    2 = smart mean / 3 = smart neighbor / 4 =
                                    neighbor
  --st, --smart-threshold <number>  Specify the range of value in 8 bit (0~128)
                                    for selecting sample where the distance to
                                    the median didn't exceed the selected value
                                    for applying mean (default is 15)
  --no-diffdod                      Do not use differential dropout detection
                                    on low source pixels
  --no-map                          Disable mapping requirement
  --passthrough                     Pass-through dropouts present on every
                                    source
  --it, --integrity                 Check if frames contain skip or sample drop
                                    and discard bad source for specific frame

Arguments:
  inputs                            Specify input TBC files (- as first source
                                    for piped input)
  output                            Specify output TBC file (omit or - for
                                    piped output)


--help-mode:

    (-1) auto            : select mode depending on the number off frame available (2f: mean, 3~4f: smart mean, 5+f: smart-neighbor)

    (0) mean            : average all samples not marked as dropouts using mean

    (1) median          : find the median from samples not marked as dropout

    (2) smart mean      : find the median from samples not marked as dropout then average all value within (median + smartThreshold) or (median - smart Threshold) using mean

    (3) smart neighbor  : find the median for every surroundings pixel not marked as dropout then find the closest sample to the surrounding median value for each neighbor
                          then take the closest value to the median of the current sample from the different closest value found
                          then average all value within (selectedSample + smartThreshold) or (selectedSample - smart threshold) using mean
                          when only 2 sources are available, it take the closest sample to the neighbor

    (4) neighbor        : find the median for every surroundings pixel not marked as dropout then find the closest sample to the surrounding median value for each neighbor
                          then take the closest value to the median of the current sample from the different closest value found then average the selected sample with the median
                          when only 2 sources are available, it take the closest sample to the neighbor
```
