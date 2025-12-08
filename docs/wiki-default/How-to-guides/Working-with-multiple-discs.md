# Working with multiple disc sources

Working with multiple disc sources allows the use of some specific ld-decode features that can greatly enhance the final video produced from LaserDisc sources.  This wiki page aims to explain the available tools and how to use them.

Working with multiple sources is a relatively complicated task.  You will need a basic understanding of the structure of NTSC and PAL LaserDiscs with regards to VBI data, field/frame structure as well as 2:3 pulldown and what it's used for.  The multiple disc source tools are largely new and experimental and may not work for all types and formats of discs (especially NTSC CAV sources).

It is important that all disc sources are made from the same 'master plate' i.e. they have to be identical.  Some discs have multiple revisions and it can be hard to spot.  Typically mint-marks on the disc will show if physical discs really came from the same source.

# Preparing disc images

All required sources must first be decoded using ld-decode.  It's recommended that you clearly label source .tbc files to make it clear which disc is which.  In this example we will use 4 copies of the Dragon's Lair (NTSC CAV with pulldown) discs named as follows:

    dragonslair_ds1.tbc
    dragonslair_ds2.tbc
    dragonslair_ds3.tbc
    dragonslair_ds4.tbc

# Process the VBI

It is very important that the VBI metadata is as good as possible for any discs used with multi-source tools.  Ensure that ld-process-vbi is used with all source .tbc files:

    ld-process-vbi dragonslair_ds1.tbc
    ld-process-vbi dragonslair_ds2.tbc
    ld-process-vbi dragonslair_ds3.tbc
    ld-process-vbi dragonslair_ds4.tbc

Note that ld-process-vbi is more accurate and comprehensive that the on-the-fly VBI processing in ld-decode; so this extra step is required for the best possible results.

# Disc mapping

## Overview

Disc mapping is the process of mapping out a source disc to ensure that the VBI frame numbering is correct and there are no missing or repeating frames in the .tbc.  Basically all source copies must be 'aligned' so the tool chain knows which frames are identical across all source copies.

As there can be many capture issues (especially with damaged/rotten discs) combined with complexities such as pulldown frames (which are not numbered in the VBI) - disc mapping is not a fool-proof task.  The ld-discmap tool is designed to warn you if things don't look right, but it can't spot every possible issue.

Another important note is around VBI frame number rewriting.  If a disc contains pulldown frames (i.e. NTSC CAV discs) the disc mapper will rewrite all frame numbers to ensure that even the pulldown frames are numbered in sequence.  This will mean that the resulting VBI isn't the same as the original disc.

All sources must be successfully disc mapped before any other multi-source tool can be used:

    ld-discmap dragonslair_ds1.tbc dragonslair_ds1_mapped.tbc
    ld-discmap dragonslair_ds2.tbc dragonslair_ds2_mapped.tbc
    ld-discmap dragonslair_ds3.tbc dragonslair_ds3_mapped.tbc
    ld-discmap dragonslair_ds4.tbc dragonslair_ds4_mapped.tbc

As disc-mapping is **highly automated guess-work**; it is important to check and verify each mapped copy using ld-analyse to ensure that the resulting TBC looks correct before continuing with the process - especially if ld-discmap reports a suspected problem with a disc.

## Troublesome discs

### Odd pulldown sequences

If you are unlucky you will find a disc source that doesn't follow the normal 1-in-5 pulldown sequence.  The disc mapper hunts for pulldown frames by examining the phase of the NTSC field and ensuring that, for frames with no VBI frame number, the phases from the last and next frame fields match the unnumbered frame (this helps to distinguish between a pulldown frame with no VBI frame number and a out-of-sequence frame with a missing or corrupt VBI frame number caused by a skip or jump during capture).

Once a likely pulldown frame is identified, the disc mapper looks 5 frames back and 5 frames forward for another pulldown.  If either is present the current frame is also marked as a pulldown.  This second 'double-check' can fail for discs that do not follow the 1-in-5 pattern causing pulldown frames to be marked incorrectly as normal frames (and therefore the disc mapper does not correctly map the disc).

For sources where this occurs it may be possible to use the --nostrict option which disables the double-check.  If the source is poor (and has multiple skips/jumps) this can have the opposite effect of marking non-pulldown frames as pulldown...  If you use the --nostrict option be sure to manually check the disc map results in ld-analyse for errors.

### Un-mappable frames

In certain cases it may not be possible for the disc mapper to map certain frames; for example, if the player gets stuck on a pulldown frame the resulting .tbc will contain repeating frames with no VBI frame number, so the mapper cannot tell where the frames belong.

The normal action when there are un-mappable frames is to abort the disc mapping process and report the error.  It is possible to override this with the --delete-unmappable-frames which will tell the disc mapper to simply delete any frames that can't be successfully mapped.  If you use the --delete-unmappable-frames option be sure to manually check the disc map results in ld-analyse for errors.

# Stacking multiple discs

The ld-disc-stacker application performs 'stacking' of multiple versions of the same disc (ideally different copies of discs containing the same mastered contents). Disc stacking requires a minimum of two input sources in order to work (although 3 or more are strongly recommended).

Stacking is performed by taking all available input sources and, pixel by pixel, determining the number of available sources for the pixel (by removing outliers marked as dropouts in the source files). If more than 3 sources are available for the pixel the tool will use the median value of the available sources as the output (when there are an even number available of pixel sources, the two centre values of the median are averaged to the output). If only two sources are available an average is used. If only one source is available it is passed directly as the output.

If no sources are available for a pixel the tool will attempt to use differential dropout detection (diffDOD) to recover a value (in the case where ld-decode marked the pixel as a false-negative) unless the --no-diffdod option is specified. See below for details.

Stacking of the example files in this wiki would be performed with a command such as:

    ld-disc-stacker dragonslair_ds1_mapped.tbc \
    dragonslair_ds2_mapped.tbc \
    dragonslair_ds3_mapped.tbc \
    dragonslair_ds4_mapped.tbc \
    dragonslair_stacked.tbc

## Dropout pass-through

If the --passthrough option is specified the tool will, when all pixel sources are marked as a dropout, mark the resulting output pixel as a dropout (regardless of the diffDOD result). The --passthrough is useful in non-preservation cases where it is desirable for master plate errors (which cause dropouts in the same place on all resulting LaserDisc copies) to be marked for dropout concealment (where-as diffDOD would correctly identify the plate error as present on all discs (and therefore not an error)).

## Differential Dropout Detection in ld-disc-stacker

Differential dropout detection (diffDOD) works by looking at the difference between multiple copies of the same disc.  Each matching field in the source .tbc files is compared to all other matching fields to produce a difference map.  The differences between the fields is caused only by errors - and these errors are marked as dropouts in the metadata.

The process becomes more accurate as the number of sources increase.  With only 3 sources, 2 frames should match with 1 in error, with 4 sources 3 should match with 1 in error.  If multiple sources have dropouts in the same place (or dropouts that overlap in the same place) this will produce false-positives.

Since all disc sources must be compared against all other disc sources for the differential comparison to work, ld-diffdod will rewrite the dropout metadata for all sources in a single process.

If you do not require diffDOD use the --no-diffdod option to turn it off.

# Analysing the resulting SNR (Signal to Noise Ratio) of the stacked TBC

The tool ld-process-vits is provided to update the white and black SNR metadata of a TBC file.  Each single-source TBC is provided (by the initial ld-decode processing) with the SNR values from the decoding process.  Once multiple TBCs are combined the SNR metadata will be incorrect.  Running ld-process-vits against a stacked TBC will update the SNR metadata and allow analysis of the stacking result in ld-analyse.

A sample command for ld-process-vits is as follows:

    ld-process-vits dragonslair_stacked.tbc

# Multi-Source Dropout Correction

Note: Dropout correction is generally not required when stacking discs unless the --passthrough option was used.  Dropouts will be corrected by the Stacker by substituting data from another source which does not have the dropout.  If, for some reason, you do not wish to use the disc stacker, then multi-source dropout correction is an alternative which only corrects missing pixels (rather than stacking them for greater accuracy).

The ld-dropout-correct tool supports both single source concealment and multi-source correction.  The advantage of multi-source correction is that dropouts can be corrected by copying in good data from another source so, unlike single disc concealment, the data is actually corrected (rather than being substituted using a similar field line).

Note that single-source concealment is still possible even with multiple sources.  In the unlikely event that a good replacement can't be found from the other sources (such as errors detected by the luma clip detection mentioned above) the corrector will use the normal inter-field concealment within the same source.

The dropout correction tool corrects only one source at a time; so the first source provided is the 'master' and the following sources are used to correct it (the last specified file is the output .tbc filename).  For example, to dropout correct the ds1 source, you would use the following command:

    ld-dropout-correct dragonslair_ds1_mapped.tbc dragonslair_ds2_mapped.tbc \
    dragonslair_ds3_mapped.tbc dragonslair_ds4_mapped.tbc \
    dragonslair_ds1_mapped_doc.tbc

Once you have the disc images decoded it is possible to use ld-analyse (specifically the SNR and DO graphs) to work out how good each copy is.  Rank them in order of best to worse.  This ranking is important for the dropout-correction tool as it will use all the available sources to 'repair' the initial source.  So the first specified source should always be the best available.

The simplest way to range discs is based on completeness and SNR - firstly use ld-decode to scan the disc looking for obvious errors or skipped frames then using the black SNR graph function, make a note of the best and worst SNR averages across the disc.

Lastly, use the dropout graph to see how much dropout loss the disc is estimated to have. Note: The 'Visible Dropout Loss Analysis' is the most important as that shows the number of dropouts in the actual picture area of the video (which is the most important part).

If you are not comfortable with making notes of the general disc condition, simply screen grab the SNR and dropout graph from each disc; then you can look at the results side-by-side making comparison easy.
