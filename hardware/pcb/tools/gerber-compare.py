#!/usr/bin/env python3
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
"""Compare two RS-274X Gerber files, or two directories of them, by geometry.

A plain text diff of two Gerbers is useless for answering "did the board change?".
Replotting the same board with a different KiCad version renumbers apertures, reorders
aperture definitions, and emits objects in a different order, so thousands of lines
differ while not one track has moved.

This tool canonicalises all three of those away. Apertures are identified by their shape
definition rather than their D-code, straight strokes are normalised so direction does not
matter, region outlines are rotated to a canonical starting vertex, and everything is
collected into a multiset. What remains after subtracting the two multisets is real:
copper, mask, silkscreen or outline geometry that genuinely differs.

What this is for, and what it is not for. Use it to check that a change to the *board
file* did not change the board: plot before, plot after, compare, using the same KiCad to
do both plots. That is the question a format migration raises and this tool answers it
exactly.

It cannot meaningfully compare plots made by two different KiCad versions. KiCad 4
expressed a zone fill as a filled region plus a large number of stroked outline segments;
KiCad 10 emits regions alone. The copper ends up in the same place but the object counts
are nowhere near each other, and no amount of canonicalising reconciles that. Deciding
whether two such plots cover the same area needs polygon boolean operations, which this
tool deliberately does not attempt. This is precisely why the as-fabricated Gerbers in
fab/rev1.0/ are kept frozen rather than regenerated: they cannot be reproduced from a
modern KiCad, so they have to be preserved.

Usage:
    gerber-compare.py OLD.gbr NEW.gbr
    gerber-compare.py OLD_DIR/ NEW_DIR/

In directory mode, files are paired by layer rather than by filename, because KiCad
renamed the layers (F.SilkS -> F_Silkscreen, Edge.Cuts -> Edge_Cuts) and changed the
extensions between the versions this project spans.

Exit status is 0 when the geometry matches and 1 when it does not, so this can be used as
a check in a script.
"""

import os
import re
import sys
from collections import Counter

FS = re.compile(r"%FSLAX(\d)(\d)Y(\d)(\d)\*%")
AD = re.compile(r"%ADD(\d+)([^*]*)\*%")
AM = re.compile(r"%AM([^*]*)\*")
OP = re.compile(
    r"^(?:G0?([123]))?(?:X(-?\d+))?(?:Y(-?\d+))?"
    r"(?:I(-?\d+))?(?:J(-?\d+))?(?:D0?([123]))?\*$"
)

# Layer identity, matched against the filename with separators and case ignored. The
# first pattern that matches wins, so the more specific entries come first.
LAYERS = [
    ("F.Cu", ("fcu",)),
    ("B.Cu", ("bcu",)),
    ("F.Mask", ("fmask",)),
    ("B.Mask", ("bmask",)),
    ("F.Silkscreen", ("fsilks", "fsilkscreen")),
    ("B.Silkscreen", ("bsilks", "bsilkscreen")),
    ("F.Paste", ("fpaste",)),
    ("B.Paste", ("bpaste",)),
    ("F.Adhesive", ("fadhes", "fadhesive")),
    ("B.Adhesive", ("badhes", "badhesive")),
    ("F.Courtyard", ("fcrtyd", "fcourtyard")),
    ("B.Courtyard", ("bcrtyd", "bcourtyard")),
    ("F.Fab", ("ffab",)),
    ("B.Fab", ("bfab",)),
    ("Edge.Cuts", ("edgecuts",)),
    ("Margin", ("margin",)),
    ("User.Comments", ("cmtsuser", "usercomments")),
    ("User.Drawings", ("dwgsuser", "userdrawings")),
    ("User.Eco1", ("eco1user", "usereco1")),
    ("User.Eco2", ("eco2user", "usereco2")),
]


def layer_of(filename):
    """Return the canonical layer name for a plotted Gerber, or None."""
    stem = os.path.splitext(os.path.basename(filename))[0]
    key = re.sub(r"[^a-z0-9]", "", stem.lower())
    for name, aliases in LAYERS:
        if any(key.endswith(a) for a in aliases):
            return name
    return None


def parse(path):
    """Return a Counter of canonical geometry tokens for one Gerber file."""
    with open(path, encoding="utf-8", errors="replace") as handle:
        text = handle.read()

    # Comments and attribute blocks carry filenames, dates and generator versions.
    text = re.sub(r"^G04[^*]*\*", "", text, flags=re.M)
    text = re.sub(r"%T[FAOD][^%]*%", "", text)

    decimals = 6
    match = FS.search(text)
    if match:
        decimals = int(match.group(2))
    scale = 10 ** decimals

    macros = {}
    for macro in AM.finditer(text):
        body = re.sub(r"\s+", "", macro.group(0)[3:-1])
        macros[body.split("*", 1)[0]] = body

    # A D-code is just a slot number; the shape is the identity.
    apertures = {}
    for defn in AD.finditer(text):
        spec = defn.group(2)
        apertures[defn.group(1)] = macros.get(spec.split(",", 1)[0], spec)

    items = Counter()
    x = y = 0
    aperture = None
    mode = "1"          # 1 linear, 2 clockwise arc, 3 counter-clockwise arc
    polarity = "D"
    region = None       # vertex accumulator between G36 and G37

    def point(a, b):
        return (round(a / scale, 6), round(b / scale, 6))

    for raw in text.replace("\r", "").split("\n"):
        line = raw.strip()
        if not line or line.startswith("M02"):
            continue
        if line.startswith("%LP"):
            polarity = line[3]
            continue
        if line.startswith("%"):
            continue
        if line.startswith("G36"):
            region = [point(x, y)]
            continue
        if line.startswith("G37"):
            if region and len(region) > 2:
                ring = region[:-1] if region[0] == region[-1] else region
                # Rotate to a canonical start vertex, and take the lesser of the two
                # winding directions, so the same outline compares equal however it
                # happens to have been emitted.
                start = min(range(len(ring)), key=lambda i: ring[i])
                fwd = tuple(ring[start:] + ring[:start])
                rev = tuple([fwd[0]] + list(reversed(fwd[1:])))
                items[("region", polarity, min(fwd, rev))] += 1
            region = None
            continue

        select = re.fullmatch(r"D0*(\d+)\*", line)
        if select and int(select.group(1)) >= 10:
            aperture = apertures.get(select.group(1), "?" + select.group(1))
            continue

        op = OP.match(line)
        if not op:
            continue
        g, xs, ys, i, j, d = op.groups()
        if g:
            mode = g
        nx = int(xs) if xs is not None else x
        ny = int(ys) if ys is not None else y

        if d == "1":
            if region is not None:
                region.append(point(nx, ny))
            elif mode == "1":
                a, b = point(x, y), point(nx, ny)
                # A straight stroke is the same object drawn either way round.
                items[("draw", polarity, aperture, (a, b) if a <= b else (b, a))] += 1
            else:
                items[("arc", polarity, aperture, mode, point(x, y), point(nx, ny),
                       point(int(i or 0), int(j or 0)))] += 1
        elif d == "3":
            items[("flash", polarity, aperture, point(nx, ny))] += 1
        x, y = nx, ny

    return items


def compare_files(old, new, label):
    """Compare one pair of Gerbers. Returns True when the geometry matches."""
    a, b = parse(old), parse(new)
    only_a, only_b = a - b, b - a
    if not only_a and not only_b:
        print(f"  {label:<16} identical ({sum(a.values())} objects)")
        return True
    print(f"  {label:<16} DIFFERS: {sum(only_a.values())} only in old, "
          f"{sum(only_b.values())} only in new "
          f"(old {sum(a.values())}, new {sum(b.values())} objects)")
    for side, counter in (("old", only_a), ("new", only_b)):
        for item, count in list(counter.items())[:3]:
            print(f"      only in {side} (x{count}): {str(item)[:120]}")
    return False


def collect(directory):
    """Map canonical layer name -> path for every Gerber in a directory."""
    found = {}
    for entry in sorted(os.listdir(directory)):
        path = os.path.join(directory, entry)
        if not os.path.isfile(path) or entry.endswith(".gbrjob"):
            continue
        layer = layer_of(entry)
        if layer:
            found[layer] = path
    return found


def main(argv):
    if len(argv) != 3:
        print(__doc__.strip().split("Usage:")[1].strip(), file=sys.stderr)
        return 2
    old, new = argv[1], argv[2]

    if os.path.isfile(old) and os.path.isfile(new):
        print(f"Comparing {old} against {new}")
        return 0 if compare_files(old, new, layer_of(old) or "layer") else 1

    if not (os.path.isdir(old) and os.path.isdir(new)):
        print("Both arguments must be files, or both directories.", file=sys.stderr)
        return 2

    a, b = collect(old), collect(new)
    if not a or not b:
        print("No recognisable Gerber layers found.", file=sys.stderr)
        return 2

    print(f"Comparing {old} against {new}")
    ok = True
    for layer in sorted(set(a) | set(b)):
        if layer not in a:
            print(f"  {layer:<16} ONLY IN NEW ({os.path.basename(b[layer])})")
            ok = False
        elif layer not in b:
            print(f"  {layer:<16} ONLY IN OLD ({os.path.basename(a[layer])})")
            ok = False
        else:
            ok &= compare_files(a[layer], b[layer], layer)

    print("\nGeometry matches." if ok else "\nGeometry differs.")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
