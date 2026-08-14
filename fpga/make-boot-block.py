#!/usr/bin/env python3
"""Build the EPCS boot block for an application image.

Domesday Duplicator - LaserDisc RF sampler
SPDX-FileCopyrightText: 2026 Simon Inns
SPDX-License-Identifier: GPL-3.0-or-later

The boot block is the twenty-four bytes at 0x100000 that tell the factory
image where the application image is, how long it is, and what it should
checksum to. Writing it is the last step of a gateware update and the thing
that makes an application image count: until it is there and valid, a unit
boots the factory image and reports itself in recovery.

This is the encoder for that format, and it is deliberately small and
dependency-free. Two things will read what it writes - the factory image's
boot logic, in fabric, and the update path that rewrites it on a live device
- so the format is defined here, on the EPCS layout and boot flow
documentation page, and in fpga/factory/bootLoader.v, and those three have
to agree.

    fpga/make-boot-block.py --image DomesdayDuplicator_auto.rpd \\
                            --output boot-block.bin

The image argument is the raw programming data for the application image:
the bytes as they sit in the flash, which is what quartus_cpf writes when a
conversion has auto_create_rpd set. Verified against the .jic those same
bytes end up in - the application page of a provisioning image is the .rpd
byte for byte - so the CRC computed here is a CRC of what the device will
read back.
"""

import argparse
import struct
import sys
import zlib
from pathlib import Path

# "DDBB", most significant byte first, which is how it appears in a hex dump
MAGIC = 0x44444242

# The layout this encoder writes and fpga/factory/bootLoader.v accepts
LAYOUT_VERSION = 1

# Where the two halves live. Fixed for the life of the design: moving either
# means re-provisioning every fielded unit with a cable.
BOOT_BLOCK_ADDRESS = 0x100000
APPLICATION_ADDRESS = 0x200000

# The whole of an EPCS64
DEVICE_BYTES = 0x800000

BLOCK_BYTES = 24
HEADER_BYTES = 20


def build(image: bytes, address: int = APPLICATION_ADDRESS) -> bytes:
    """The twenty-four bytes, checksums included.

    Everything after the magic is little-endian, which is the byte order of
    both readers: the FX3 and the host that writes it.
    """
    if not image:
        raise ValueError("the application image is empty")

    if address + len(image) > DEVICE_BYTES:
        raise ValueError(
            f"an image of {len(image)} bytes at {address:#08x} runs past the "
            f"end of the device"
        )

    if address < BOOT_BLOCK_ADDRESS:
        raise ValueError(
            f"an application image at {address:#08x} would overlap the factory "
            f"image or the boot block"
        )

    header = struct.pack(
        "<IHHIII",
        # The magic is stored most significant byte first so that it reads as
        # "DDBB" in a dump of the flash, which is the only field anybody looks
        # at with their eyes.
        struct.unpack("<I", struct.pack(">I", MAGIC))[0],
        LAYOUT_VERSION,
        0,
        address,
        len(image),
        zlib.crc32(image) & 0xFFFFFFFF,
    )
    assert len(header) == HEADER_BYTES

    # The block carries its own checksum as well as the image's, so that a
    # block half written by an interrupted update is distinguishable from an
    # intact block describing a damaged image. Both mean "stay in the factory
    # image", and they mean different things to whoever is diagnosing it.
    block = header + struct.pack("<I", zlib.crc32(header) & 0xFFFFFFFF)
    assert len(block) == BLOCK_BYTES

    return block


def describe(block: bytes) -> str:
    magic, version, _, address, length, image_crc, block_crc = struct.unpack(
        "<IHHIIII", block
    )
    return "\n".join(
        [
            f"  magic                 {struct.pack('<I', magic).decode('ascii')}",
            f"  layout version        {version}",
            f"  application address   {address:#08x}",
            f"  application length    {length} bytes",
            f"  application CRC-32    {image_crc:08X}",
            f"  boot block CRC-32     {block_crc:08X}",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--image",
        type=Path,
        required=True,
        help="the application image as it sits in flash (.rpd)",
    )
    parser.add_argument(
        "--address",
        type=lambda value: int(value, 0),
        default=APPLICATION_ADDRESS,
        help=f"where that image starts (default {APPLICATION_ADDRESS:#08x})",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    try:
        block = build(args.image.read_bytes(), args.address)
    except (OSError, ValueError) as error:
        sys.exit(str(error))

    args.output.write_bytes(block)

    print(f"Boot block for {args.image.name}, to be written at {BOOT_BLOCK_ADDRESS:#08x}:")
    print(describe(block))

    return 0


if __name__ == "__main__":
    sys.exit(main())
