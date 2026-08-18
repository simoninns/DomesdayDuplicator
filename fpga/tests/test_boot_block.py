#!/usr/bin/env python3
"""Check the boot block encoder against the format the gateware reads (T1).

Domesday Duplicator - LaserDisc RF sampler
SPDX-FileCopyrightText: 2026 Simon Inns
SPDX-License-Identifier: GPL-3.0-or-later

Three files describe the boot block: this encoder, the boot logic in
fpga/factory/bootLoader.v that reads it in fabric, and the EPCS layout and
boot flow documentation page. They cannot be generated from one another, so
what keeps them together is that each field's offset is asserted here by
number rather than by calling the encoder's own constants back at it.

The one case worth stating: the same twenty-four bytes appear in
tests/tb_bootLoader.v, where the gateware accepts them. If this test and
that testbench ever disagree, the format has been changed in one place.
"""

import importlib.util
import struct
import sys
import zlib
from pathlib import Path

MODULE = Path(__file__).resolve().parent.parent / "make-boot-block.py"

spec = importlib.util.spec_from_file_location("make_boot_block", MODULE)
make_boot_block = importlib.util.module_from_spec(spec)
spec.loader.exec_module(make_boot_block)


failures = []


def check(what, got, want):
    if got != want:
        failures.append(f"{what}: got {got!r}, expected {want!r}")


def check_raises(what, call):
    try:
        call()
    except ValueError:
        return
    failures.append(f"{what}: no error was raised")


# The image the gateware testbench uses, so that the two agree byte for byte
IMAGE = bytes(range(0xA0, 0xB0))
BLOCK = make_boot_block.build(IMAGE, 0x00100020)

# --- The layout, offset by offset --------------------------------------------
check("the block is 24 bytes", len(BLOCK), 24)
check("the magic reads DDBB in a dump", BLOCK[0:4], b"DDBB")
check("the layout version is 1, little-endian", BLOCK[4:6], b"\x01\x00")
check("bytes 6 and 7 are reserved and zero", BLOCK[6:8], b"\x00\x00")
check("the application address", struct.unpack("<I", BLOCK[8:12])[0], 0x00100020)
check("the application length", struct.unpack("<I", BLOCK[12:16])[0], len(IMAGE))
check(
    "the application CRC-32",
    struct.unpack("<I", BLOCK[16:20])[0],
    zlib.crc32(IMAGE) & 0xFFFFFFFF,
)
check(
    "the block CRC-32 covers bytes 0 to 19",
    struct.unpack("<I", BLOCK[20:24])[0],
    zlib.crc32(BLOCK[0:20]) & 0xFFFFFFFF,
)

# --- The exact bytes the gateware testbench is written against ---------------
#
# tb_bootLoader.v loads these into its flash model and expects the boot logic
# to accept them. Pinning them here as well is what makes a change to either
# side visible in the other.
check("the application CRC the testbench expects", struct.unpack("<I", BLOCK[16:20])[0], 0xB225246F)
check("the block CRC the testbench expects", struct.unpack("<I", BLOCK[20:24])[0], 0x9A522E5C)

# --- The default address is the documented one -------------------------------
DEFAULT = make_boot_block.build(IMAGE)
check("the default application address", struct.unpack("<I", DEFAULT[8:12])[0], 0x200000)
check("the boot block's own address", make_boot_block.BOOT_BLOCK_ADDRESS, 0x100000)

# --- Refusals ----------------------------------------------------------------
#
# Each of these would produce a boot block that validates and describes
# something the device cannot read, which is the one failure this format has no
# defence against: the CRC would be right about the wrong thing.
check_raises("an empty image", lambda: make_boot_block.build(b""))
check_raises(
    "an image that runs past the end of the device",
    lambda: make_boot_block.build(b"\xff" * 16, 0x7FFFF8),
)
check_raises(
    "an image that would overlap the boot block",
    lambda: make_boot_block.build(IMAGE, 0x0FFFF0),
)

# --- A real-sized image ------------------------------------------------------
#
# The bitstream is a couple of hundred kilobytes, and nothing about the
# encoding changes with size - but a length field that had been written as
# anything narrower than 32 bits would show up here rather than on a bench.
BIG = bytes((index * 7) & 0xFF for index in range(300000))
BIG_BLOCK = make_boot_block.build(BIG)
check("a real-sized image's length", struct.unpack("<I", BIG_BLOCK[12:16])[0], 300000)
check(
    "a real-sized image's CRC-32",
    struct.unpack("<I", BIG_BLOCK[16:20])[0],
    zlib.crc32(BIG) & 0xFFFFFFFF,
)

if failures:
    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    sys.exit(1)

print("Boot block encoder: all checks passed.")
