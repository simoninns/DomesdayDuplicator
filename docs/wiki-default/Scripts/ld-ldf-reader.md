# ld-ldf-reader

ld-ldf-reader is a streaming decompressor for .ldf files that can jump to a specific location in the file.  It is very lightly modified ffmpeg library sample code and is mostly used by ld-decode to support seamless seeking inside .ldf files, therefore it only outputs via standard output.

NOTE: This program only exists because of an ffmpeg CLI program bug preventing seeking deep into a file, so it may (or may not) go away after the move to Ubuntu 20.04.

usage: ld-ldf-reader infile [seek location] | (consumer)

