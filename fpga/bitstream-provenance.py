#!/usr/bin/env python3
"""Record what produced a bitstream, and digests that make it verifiable.

Domesday Duplicator - LaserDisc RF sampler
SPDX-FileCopyrightText: 2018-2026 Simon Inns
SPDX-License-Identifier: GPL-3.0-or-later

The FPGA bitstream is the one release artefact CI does not build. Quartus is
unfree, x86_64-linux only and marked non-redistributable, so it can never be
served from a binary cache and does not go on a runner; the maintainer builds
it locally and attaches it by hand. That makes provenance the artefact's own
responsibility, because nothing else records it.

Two kinds of digest, because they answer different questions:

  Release digest    over the file exactly as shipped
                    "is this the file that was released, intact?"

  Canonical digest  over the configuration content only
                    "does a rebuild of this commit agree with it?"

They are different for the .sof because Quartus stamps a compile timestamp and
a per-run design hash into its header. Measured on this project (P6-9): two
compiles of the same commit on the same toolchain produce a .sof differing in
34 of 704,015 bytes, all of it header metadata, and a .jic that is byte for
byte identical. So the .jic needs no canonicalisation — its release digest and
its canonical digest are the same number — and the .sof needs the four
maskings below.

Run it from the build directory after quartus_sh and quartus_cpf:

    fpga/bitstream-provenance.py --build-dir . --output bitstream-provenance.txt
"""

import argparse
import hashlib
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

# Fields Quartus varies between runs of an identical compile. Each is located
# by the structure around it rather than by a fixed offset, so the masking does
# not silently move if a future Quartus resizes anything earlier in the header.
#
# Each entry is (description, anchor bytes, bytes to skip after the *end* of the
# anchor, length to mask) — so an anchor that runs right up to the field it
# marks has a skip of zero. All four were identified by diffing two compiles of
# the same commit; see fpga/README.md "Reproducibility". tests/test_provenance.py
# checks that each one lands where it is meant to, which is not something the
# fail-loud anchor search can tell you: a mask at the wrong offset finds its
# anchor, zeroes the wrong bytes and yields a digest that looks fine and matches
# nothing.
SOF_VARIABLE_FIELDS = [
    # A 10-byte design hash, stored raw...
    ("design hash", b"design_hash.bin", 3, 10),
    # ...immediately followed by a 32-bit little-endian Unix compile timestamp.
    ("compile timestamp", b"design_hash.bin", 3 + 10 + 4, 4),
    # The same hash again, as ASCII hex inside the SLD project info XML.
    ("design hash (ASCII)", b'md5_digest_80b="', 0, 20),
    # A second copy of the compile timestamp, after the XML block.
    ("compile timestamp (second copy)", b"</sld_project_info>\n", 0, 4),
]

# The last two bytes are a checksum over the whole file, so they change
# whenever any of the fields above do.
SOF_TRAILING_CHECKSUM_BYTES = 2


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonicalise_sof(data):
    """Zero the fields that vary between identical compiles.

    Raises if an expected field is missing. A canonical digest computed over
    unmasked data would look like a valid answer and compare unequal against
    every rebuild, which is worse than refusing to produce one.
    """
    out = bytearray(data)

    for description, anchor, skip, length in SOF_VARIABLE_FIELDS:
        occurrences = [m.start() for m in re.finditer(re.escape(anchor), data)]
        if len(occurrences) != 1:
            raise ValueError(
                f"expected exactly one {anchor!r} in the .sof to locate the "
                f"{description}, found {len(occurrences)}. The file format has "
                f"changed — re-derive the masking before trusting a canonical "
                f"digest from this Quartus version."
            )
        start = occurrences[0] + len(anchor) + skip
        out[start : start + length] = b"\x00" * length

    out[-SOF_TRAILING_CHECKSUM_BYTES:] = b"\x00" * SOF_TRAILING_CHECKSUM_BYTES
    return bytes(out)


def canonical_digest(path):
    data = path.read_bytes()
    if path.suffix == ".sof":
        return hashlib.sha256(canonicalise_sof(data)).hexdigest()
    # The .jic was measured to be reproducible as-is, so canonicalising it
    # would be inventing a difference that is not there.
    return hashlib.sha256(data).hexdigest()


def qsf_assignments(qsf):
    """Every -name/value pair from set_global_assignment lines."""
    found = {}
    for line in qsf.read_text(errors="replace").splitlines():
        match = re.match(r"\s*set_global_assignment\s+-name\s+(\S+)\s+(.+?)\s*$", line)
        if match:
            found[match.group(1)] = match.group(2).strip('"')
    return found


def quartus_version():
    try:
        output = subprocess.run(
            ["quartus_sh", "--version"],
            capture_output=True,
            text=True,
            timeout=120,
            check=True,
        ).stdout
    except (OSError, subprocess.SubprocessError):
        return "unknown (quartus_sh not on PATH)"

    for line in output.splitlines():
        if line.startswith("Version "):
            return line.strip()
    return "unknown (unrecognised quartus_sh --version output)"


def quartus_word_size():
    """32- or 64-bit, from where the real binaries live.

    A fit only reproduces on the same Quartus *build*, and the word size is the
    half of that claim the version string does not carry. Quartus installs its
    executables under linux64/ (or linux/ for the 32-bit builds that existed up
    to 16.x), so the directory name is the answer.
    """
    root = os.environ.get("QUARTUS_ROOTDIR")
    if root:
        if (Path(root) / "linux64").is_dir():
            return "64-bit"
        if (Path(root) / "linux").is_dir():
            return "32-bit"

    resolved = shutil.which("quartus_sh")
    if resolved:
        parts = Path(resolved).resolve().parts
        if "linux64" in parts:
            return "64-bit"
        if "linux" in parts:
            return "32-bit"

    return "64-bit (assumed — Quartus Prime 17.0 and later are 64-bit only)"


def git_commit(source_dir):
    try:
        commit = subprocess.run(
            ["git", "-C", str(source_dir), "rev-parse", "--short=8", "HEAD"],
            capture_output=True,
            text=True,
            timeout=60,
            check=True,
        ).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return "unknown"

    try:
        dirty = subprocess.run(
            ["git", "-C", str(source_dir), "status", "--porcelain"],
            capture_output=True,
            text=True,
            timeout=60,
            check=True,
        ).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return commit

    # Untracked files count. A bitstream built from a tree with uncommitted work
    # in it does not come from the commit it names, and saying so is the whole
    # point of this file.
    return f"{commit}-dirty" if dirty else commit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("."))
    parser.add_argument("--source-dir", type=Path, default=Path("."))
    parser.add_argument(
        "--commit",
        default=None,
        help="source commit; read from git in --source-dir when omitted, which "
        "a Nix build cannot do because it has no .git",
    )
    parser.add_argument("--quartus-version", default=None)
    parser.add_argument("--output", type=Path, default=None, help="default: stdout")
    args = parser.parse_args()

    qsf = args.build_dir / "DomesdayDuplicator.qsf"
    if not qsf.is_file():
        sys.exit(f"no DomesdayDuplicator.qsf in {args.build_dir}")
    assignments = qsf_assignments(qsf)

    artefacts = sorted(
        p
        for p in args.build_dir.iterdir()
        if p.suffix in (".sof", ".jic") and p.is_file()
    )
    if not artefacts:
        sys.exit(f"no .sof or .jic in {args.build_dir} — nothing to record")

    commit = args.commit if args.commit is not None else git_commit(args.source_dir)
    version = args.quartus_version or quartus_version()

    lines = [
        "Domesday Duplicator FPGA bitstream",
        "==================================",
        "",
        "Built outside CI. See fpga/README.md, 'Why this is not built by CI', for",
        "why, and 'Reproducibility' in the same file for how to reproduce it.",
        "",
        "Source",
        "------",
        f"  commit                    {commit}",
        f"  device                    {assignments.get('DEVICE', 'unknown')}",
        f"  family                    {assignments.get('FAMILY', 'unknown')}",
        f"  top level                 {assignments.get('TOP_LEVEL_ENTITY', 'unknown')}",
        "",
        "Toolchain",
        "---------",
        f"  quartus                   {version}",
        f"  quartus word size         {quartus_word_size()}",
        f"  host architecture         {platform.machine()}",
        f"  host system               {platform.system()}",
        "",
        "Fitter settings that affect reproducibility",
        "-------------------------------------------",
        f"  seed                      {assignments.get('SEED', 'not set (Quartus default)')}",
        f"  parallel processors       {assignments.get('NUM_PARALLEL_PROCESSORS', 'not set (Quartus default)')}",
        "",
        "Digests",
        "-------",
        "  Release digest: the file as shipped. Use it to check a download.",
        "  Canonical digest: the configuration content, with the compile",
        "  timestamp and per-run design hash masked out. Use it to check a",
        "  rebuild of the same commit on the same Quartus version. For the .jic",
        "  the two are identical, because the .jic carries no such fields.",
        "",
    ]

    for artefact in artefacts:
        release = sha256_file(artefact)
        try:
            canonical = canonical_digest(artefact)
        except ValueError as error:
            sys.exit(f"{artefact.name}: {error}")

        lines.append(f"  {artefact.name}  ({artefact.stat().st_size} bytes)")
        lines.append(f"    release        sha256:{release}")
        lines.append(f"    canonical      sha256:{canonical}")
        lines.append("")

    text = "\n".join(lines)

    if args.output:
        args.output.write_text(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    # Quartus writes into $HOME, and a build sandbox may not have one.
    os.environ.setdefault("HOME", os.environ.get("TMPDIR", "/tmp"))
    main()
