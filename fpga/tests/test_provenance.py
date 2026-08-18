#!/usr/bin/env python3
"""Tests for the bitstream provenance record (T1, T2).

Domesday Duplicator - LaserDisc RF sampler
SPDX-FileCopyrightText: 2018-2026 Simon Inns
SPDX-License-Identifier: GPL-3.0-or-later

The canonical digest exists so that someone with the same pinned Quartus can
rebuild a release commit and confirm the bitstream agrees. That only works if
the masking lands on exactly the bytes Quartus varies between runs, and a
mask at the wrong offset fails silently: it still finds its anchor, it still
zeroes ten bytes, and it still produces a well-formed digest — one that will
never match anything. A wrong offset shipped once already, so these tests fix
each field's position rather than only checking that a digest comes out.

The fixture is a synthetic .sof header, not a real bitstream. A real one is
704 KB, cannot be committed to the repository, and needs Quartus to produce —
none of which is necessary to test byte offsets. The layout below is
transcribed from a real Quartus 25.1 .sof; the offsets it produces are the
ones measured there.
"""

import hashlib
import importlib.util
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location(
    "bitstream_provenance", HERE.parent / "bitstream-provenance.py"
)
provenance = importlib.util.module_from_spec(spec)
spec.loader.exec_module(provenance)


# The four fields, with the values a first build would write.
DESIGN_HASH_A = bytes.fromhex("69ac805f9f5a7ecf48ac")
DESIGN_HASH_B = bytes.fromhex("27a8fba5dc738edd657a")
TIMESTAMP_A = (0x6A7CC60E).to_bytes(4, "little")
TIMESTAMP_B = (0x6A7CC644).to_bytes(4, "little")


def make_sof(design_hash, timestamp, payload=b"\xa5" * 4096, checksum=b"\xc7\x4c"):
    """A synthetic .sof carrying the same field layout as a real one."""
    ascii_hash = design_hash.hex().encode("ascii")
    assert len(ascii_hash) == 20

    return b"".join(
        [
            b"SOF\x00\x00\x00\x00\x00",
            b"Quartus Prime Compiler Version 25.1std.0\x00",
            b"design_hash.bin",
            b"\x00\x00\x00",
            design_hash,
            b"\x00\x00\x00\x00",
            timestamp,
            b'<sld_project_info>\n    <hash md5_digest_80b="',
            ascii_hash,
            b'"/>\n  </project>\n</sld_project_info>\n',
            timestamp,
            payload,
            checksum,
        ]
    )


failures = []


def check(condition, description):
    if condition:
        print(f"  ok    {description}")
    else:
        print(f"  FAIL  {description}")
        failures.append(description)


print("canonicalise_sof")

first = make_sof(DESIGN_HASH_A, TIMESTAMP_A, checksum=b"\xc7\x4c")
second = make_sof(DESIGN_HASH_B, TIMESTAMP_B, checksum=b"\x6b\xdc")

# The whole point: two builds of identical logic, differing only in the fields
# Quartus stamps per run, must canonicalise to the same digest.
check(first != second, "the two fixtures differ before canonicalisation")
check(
    provenance.canonicalise_sof(first) == provenance.canonicalise_sof(second),
    "identical logic with different stamps canonicalises identically",
)

# Each field must be masked *where it is*, not merely somewhere. Change one
# field at a time and require the canonical form to be unaffected.
for description, mutated in [
    ("design hash", make_sof(DESIGN_HASH_B, TIMESTAMP_A, checksum=b"\xc7\x4c")),
    ("compile timestamp", make_sof(DESIGN_HASH_A, TIMESTAMP_B, checksum=b"\xc7\x4c")),
    ("trailing checksum", make_sof(DESIGN_HASH_A, TIMESTAMP_A, checksum=b"\x00\x01")),
]:
    check(
        provenance.canonicalise_sof(first) == provenance.canonicalise_sof(mutated),
        f"{description} is masked",
    )

# ...and the converse, which is what stops the masking from growing until it
# covers the configuration data itself and every bitstream looks identical.
different_logic = make_sof(DESIGN_HASH_A, TIMESTAMP_A, payload=b"\x5a" * 4096)
check(
    provenance.canonicalise_sof(first) != provenance.canonicalise_sof(different_logic),
    "a change to the configuration payload is NOT masked",
)

# The exact offsets, measured on a real Quartus 25.1 .sof. Stated as a
# self-contained assertion so a future Quartus that moves a field is caught
# here rather than by a release nobody can verify.
canonical = provenance.canonicalise_sof(first)
zeroed = {i for i, byte in enumerate(canonical) if byte == 0 and first[i] != 0}
expected_starts = {
    "design hash": first.index(b"design_hash.bin") + len(b"design_hash.bin") + 3,
    "ascii hash": first.index(b'md5_digest_80b="') + len(b'md5_digest_80b="'),
}
check(
    expected_starts["design hash"] in zeroed,
    "the design hash mask starts immediately after its anchor plus three bytes",
)
check(
    expected_starts["ascii hash"] in zeroed,
    "the ASCII hash mask starts immediately after the opening quote",
)
check(
    all(first[i] == canonical[i] for i in range(len(first) - 4096 - 2, len(first) - 2)),
    "the configuration payload is left untouched",
)

print("\nfail-loud behaviour")

# A .sof whose format has moved on must produce no digest at all. Half a
# canonicalisation is a digest that disagrees with every rebuild for a reason
# nobody will find.
truncated = first.replace(b"design_hash.bin", b"design_hash.BIN")
try:
    provenance.canonicalise_sof(truncated)
    check(False, "a missing anchor raises rather than digesting unmasked data")
except ValueError:
    check(True, "a missing anchor raises rather than digesting unmasked data")

duplicated = first + b'md5_digest_80b="' + b"0" * 20
try:
    provenance.canonicalise_sof(duplicated)
    check(False, "an ambiguous anchor raises rather than guessing")
except ValueError:
    check(True, "an ambiguous anchor raises rather than guessing")

print("\ncanonical_digest")

# The .jic was measured to be byte-identical across rebuilds, so its canonical
# digest is its plain digest — canonicalising it would invent a difference.
with tempfile.TemporaryDirectory() as scratch:
    jic = Path(scratch) / "fixture.jic"
    jic.write_bytes(b"jic contents, whatever they are")
    check(
        provenance.canonical_digest(jic)
        == hashlib.sha256(jic.read_bytes()).hexdigest(),
        "a .jic canonical digest is the plain digest of the file",
    )

print("\ncanonicalise_svf")

# The converter names its input file and that file's modification time in a
# header comment, so a rebuild whose .jic is byte for byte identical still
# produces a .svf that is not. Everything below that line is a function of
# the .jic alone.
SVF_HEADER = (
    b"!Quartus Prime SVF converter 25.1\n"
    b"!\n"
    b"!Device #1: EP4CE22 - ./DomesdayDuplicatorProvisioning.jic %s\n"
    b"!\n"
    b"FREQUENCY 4.50E+06 HZ;\n"
    b"SIR 10 TDI (006);\n"
)

first_svf = SVF_HEADER % b"Mon Aug 17 17:43:42 2026"
second_svf = SVF_HEADER % b"Mon Aug 17 18:11:41 2026"

check(first_svf != second_svf, "the two fixtures differ before canonicalisation")
check(
    provenance.canonicalise_svf(first_svf)
    == provenance.canonicalise_svf(second_svf),
    "the same vectors converted at different times canonicalise identically",
)
check(
    b"SIR 10 TDI (006);" in provenance.canonicalise_svf(first_svf),
    "canonicalising a .svf leaves its vectors alone",
)
check(
    provenance.canonicalise_svf(first_svf)
    != provenance.canonicalise_svf(first_svf.replace(b"(006)", b"(00E)")),
    "different vectors canonicalise differently",
)

# A day of the month the C library space-pads, which is the form that would
# quietly stop matching and leave every rebuild disagreeing with the record.
check(
    provenance.canonicalise_svf(SVF_HEADER % b"Sun Aug  3 09:01:02 2025")
    == provenance.canonicalise_svf(first_svf),
    "a space-padded day of the month is matched too",
)

# Fail loudly rather than digesting a file whose header has moved on.
try:
    provenance.canonicalise_svf(b"!Quartus Prime SVF converter 26.1\nSIR 10 TDI (0);\n")
    check(False, "a .svf with no device line raises")
except ValueError:
    check(True, "a .svf with no device line raises rather than guessing")

with tempfile.TemporaryDirectory() as scratch:
    svf = Path(scratch) / "fixture.svf"
    svf.write_bytes(first_svf)
    check(
        provenance.canonical_digest(svf)
        == hashlib.sha256(provenance.canonicalise_svf(first_svf)).hexdigest(),
        "a .svf canonical digest is the digest of its canonical form",
    )
    check(
        provenance.canonical_digest(svf)
        != hashlib.sha256(first_svf).hexdigest(),
        "a .svf canonical digest is not simply the digest of the file",
    )

print()
if failures:
    print(f"test_provenance: FAIL ({len(failures)} of the checks above)")
    sys.exit(1)

print("test_provenance: PASS")
