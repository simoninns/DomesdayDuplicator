#!/usr/bin/env bash
#
# Return a bench board to the state it left the factory in.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Erases the FX3's I2C EEPROM and the FPGA's EPCS64 configuration flash, so that the pair
# comes up exactly as an unprogrammed one does: the FX3 falling back to its boot ROM at
# 04b4:00f3, the FPGA unconfigured with its pins high-Z.
#
#   ./tools/blank-board.sh                 both halves
#   ./tools/blank-board.sh --fx3           the EEPROM only
#   ./tools/blank-board.sh --fpga          the configuration flash only
#
# **This exists to make a test repeatable.** TESTING.md §6 has four items that need a board
# which has never been programmed — U6, B0, B1 and B2 — and B2 in particular has to be run
# against every binary package the release produces, on a machine each one has never been
# installed on. Keeping a virgin board per package is not a plan; erasing one is. The
# alternative, and what this replaces, is a page of remembered commands in which the two
# genuinely subtle steps are easy to get wrong in a way that leaves the board looking blank
# without being blank.
#
# It builds nothing, in keeping with the rest of tools/: it finds what is already built and
# says what to build if it cannot. Bring your own `fx3-programmer` and, for the FPGA half,
# a Quartus shell.
#
# ## The two subtle steps
#
# **The EEPROM has no erase command.** It is written, not erased, so blanking it means
# writing 0xFF over it — which `fx3-programmer -p` will happily do, because it does no
# format validation on its input and treats the file as bytes. The FX3 boot ROM only looks
# at the `CY` signature in the first two bytes, so destroying the first 64-byte page is all
# that is *needed*; this writes the whole device anyway, because a half-erased EEPROM and a
# virgin one are not the same thing and the difference is exactly what U6 is testing.
#
# The first page is written on its own first, before the full wipe. One page fits in any
# EEPROM the kit has ever shipped with, so that write cannot fail for being off the end of
# the part — and once it lands the board is non-bootable, which means a later failure
# leaves a board that is definitely blank rather than one in an unknown state.
#
# **The JTAG erase is page-selective by default**, covering only the pages the .jic itself
# occupies. The boot block lives at 0x100000, in the gap between the factory image at 0 and
# the application image at 0x200000, so a plain erase steps straight over it — see
# docs/content/development/epcs-layout-and-boot-flow.md, which describes this as the reason
# a reprovisioned unit can boot into the application instead of recovery. A board blanked
# that way would be indistinguishable from a virgin one right up until it was provisioned,
# and would then behave differently at the one moment the test is watching. So `--erase_all`
# is passed, which erases the whole device.
#
# There is no way to read the boot block back and prove it is gone: examining the EPCS
# through the serial flash loader is not something quartus_pgm offers, and reading it over
# the register link needs firmware this script has just erased. What can be observed is the
# time — a whole-device erase takes tens of seconds where the page-selective one takes
# about four — and the blank-check that follows, which covers the two image regions. The
# script prints the erase duration for that reason. If a board provisioned after this comes
# up in the application image rather than recovery, the boot block survived.
#
# ## After it runs
#
# **Power cycle the board.** Both halves are left in a transient state that a virgin board
# is never in: the FPGA is running Altera's serial flash loader, put there by the programmer
# to reach the flash, and the FX3 is in the Cypress flash-programmer mode at 04b4:4720. Both
# clear on a power cycle and neither is what any of these tests should be run against.
#
# **Check J4 is removed.** A factory board reads its blank EEPROM, fails, and falls back to
# the boot ROM; a board with J4 fitted never reads the EEPROM at all. The two are identical
# over USB and are not the same test — U6 step 1 says "no jumper fitted" for this reason.
# Nothing here can tell them apart, so the script asks rather than assuming.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(dirname "$here")"

do_fx3=0
do_fpga=0
programmer=""
flashprog=""
jic=""
cable="1"
eeprom_bytes=131072
assume_yes=0

# Kept because the argument loop below consumes them, and the hint printed when the
# Quartus shell is missing has to be a line the user can paste back.
invocation=("$@")

usage() {
    cat >&2 <<'EOF'
Usage: blank-board.sh [--fx3] [--fpga] [--yes]
                      [--programmer FILE] [--flash-prog FILE]
                      [--jic FILE] [--cable NAME]
                      [--eeprom-bytes N]

With neither --fx3 nor --fpga it does both.

  --fx3           erase the FX3 I2C EEPROM
  --fpga          erase the FPGA EPCS64 configuration flash
  --yes           do not ask for confirmation
  --programmer    use this fx3-programmer instead of searching
  --flash-prog    use this cyfxflashprog.img instead of the in-tree copy
  --jic           use this .jic to carry the serial flash loader instead of
                  searching; any EP4CE22 .jic will do, its contents are never
                  written
  --cable         USB-Blaster to use; defaults to 1, the first one found
  --eeprom-bytes  how much of the EEPROM to write; defaults to 131072, the size
                  of the part on a SuperSpeed Explorer Kit
EOF
    exit 2
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fx3) do_fx3=1; shift ;;
        --fpga) do_fpga=1; shift ;;
        --yes|-y) assume_yes=1; shift ;;
        --programmer) programmer="$2"; shift 2 ;;
        --flash-prog) flashprog="$2"; shift 2 ;;
        --jic) jic="$2"; shift 2 ;;
        --cable) cable="$2"; shift 2 ;;
        --eeprom-bytes) eeprom_bytes="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done

if (( do_fx3 == 0 && do_fpga == 0 )); then
    do_fx3=1
    do_fpga=1
fi

die() { echo "blank-board: $*" >&2; exit 1; }
say() { echo "blank-board: $*"; }

# First existing path, or nothing.
first_existing() {
    local candidate
    for candidate in "$@"; do
        if [[ -f "$candidate" ]]; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    return 0
}

# ---------------------------------------------------------------------------
# Preflight
#
# Everything is located and checked before anything is erased. Half a blanking is
# worse than none: it leaves a board whose state has to be worked out before it can
# be used for anything, which is the situation this script exists to avoid.
# ---------------------------------------------------------------------------

if (( do_fx3 )); then
    if [[ -z "$programmer" ]]; then
        programmer="$(first_existing \
            "$root/fx3/programmer/build/fx3-programmer" \
            "$root/build/fx3-programmer/fx3-programmer" \
            "$root/result-programmer/bin/fx3-programmer" \
            "$root/result/bin/fx3-programmer")"
    fi
    if [[ -z "$programmer" ]] && command -v fx3-programmer >/dev/null 2>&1; then
        programmer="$(command -v fx3-programmer)"
    fi
    [[ -n "$programmer" ]] || die "no fx3-programmer found.
Build it with:  nix build .#fx3-programmer -o result-programmer
or pass --programmer."
    [[ -x "$programmer" ]] || die "$programmer is not executable"

    # The secondary loader is tracked in this repository, so unlike the programmer it is
    # always here. $FX3_FLASH_PROG is honoured first because that is the order the
    # programmer itself resolves in, and disagreeing with it would be its own bug.
    if [[ -z "$flashprog" ]]; then
        flashprog="${FX3_FLASH_PROG:-}"
    fi
    if [[ -z "$flashprog" ]]; then
        flashprog="$(first_existing "$root/fx3/programmer/cyfxflashprog.img")"
    fi
    [[ -n "$flashprog" ]] || die "no cyfxflashprog.img found; pass --flash-prog"

    (( eeprom_bytes > 0 )) 2>/dev/null || die "--eeprom-bytes must be a positive number"
fi

if (( do_fpga )); then
    command -v quartus_pgm >/dev/null 2>&1 || die "quartus_pgm is not on PATH.
The FPGA half needs the Quartus shell:

  nix develop .#fpga-quartus --command ./tools/blank-board.sh ${invocation[*]:-}

or run with --fx3 to blank the EEPROM alone."

    # Only the serial flash loader inside the .jic is used — it is configured into the
    # FPGA so that the erase has a path to the flash, and nothing from the file is ever
    # written. So any .jic built for this device will do, and the provisioning one is
    # named first only because it is the one a checkout is likely to have.
    if [[ -z "$jic" ]]; then
        jic="$(first_existing \
            "$root/fpga/build/provisioning/DomesdayDuplicatorProvisioning.jic" \
            "$root/result-bitstream/provisioning/DomesdayDuplicatorProvisioning.jic" \
            "$root/result/provisioning/DomesdayDuplicatorProvisioning.jic")"
    fi
    [[ -n "$jic" ]] || die "no .jic found to carry the serial flash loader.
Build one with:  ./fpga/build-local.sh
or:              nix build .#bitstream -o result-bitstream
or pass --jic; any EP4CE22 .jic will do, its contents are never written."
    [[ -f "$jic" ]] || die "$jic does not exist"
fi

# ---------------------------------------------------------------------------
# Confirm
# ---------------------------------------------------------------------------

echo
echo "About to erase:"
if (( do_fx3 )); then
    echo "  FX3 EEPROM     $eeprom_bytes bytes to 0xFF, via $programmer"
fi
if (( do_fpga )); then
    echo "  EPCS64 flash   whole device, via cable $cable and $jic"
fi
echo
if (( do_fx3 && do_fpga )); then
    echo "Both are permanent. Reprogramming needs the same cables and, for the FPGA,"
    echo "Quartus — see docs/content/development/hardware-programming/."
else
    echo "That is permanent. Reprogramming is in"
    echo "docs/content/development/hardware-programming/."
fi
echo

if (( assume_yes == 0 )); then
    # `|| true` because end-of-input is an answer here rather than an error: a run with
    # no terminal — from a CI step or a pipeline — must reach the refusal below and say
    # what to do, not exit silently on `read`'s status under `set -e`.
    reply=""
    read -r -p "Type 'blank' to continue: " reply || true
    [[ "$reply" == "blank" ]] || die "not confirmed; nothing was erased.
Pass --yes to skip the prompt when there is no terminal to ask at."
fi

# ---------------------------------------------------------------------------
# The FPGA's configuration flash
#
# Done first because it is the half with the most ways to fail — a missing cable, a
# board that is not powered, a chain that does not scan — and none of them are worth
# discovering after the EEPROM has already gone.
# ---------------------------------------------------------------------------

if (( do_fpga )); then
    echo
    say "erasing the EPCS64 configuration flash"

    # quartus_pgm resolves the file relative to the working directory in some paths, and
    # writes its lock and settings files beside it. A scratch copy keeps both away from
    # the build tree, and away from the Nix store if the .jic came from there.
    workdir="$(mktemp -d)"
    trap 'rm -rf "$workdir"' EXIT
    cp "$jic" "$workdir/sfl.jic"
    chmod u+w "$workdir/sfl.jic"

    # 'i' configures the serial flash loader into the FPGA, which is what gives the
    # programmer a path to the flash at all; 'r' is the erase. --erase_all is the whole
    # point of the pair, and the header above says why.
    started="$SECONDS"
    ( cd "$workdir" && quartus_pgm -c "$cable" -m jtag --erase_all -o 'ir;sfl.jic' ) ||
        die "the erase failed.
Check the USB-Blaster is connected and the board is powered:  quartus_pgm -c $cable -a"
    elapsed=$(( SECONDS - started ))

    say "erase took ${elapsed}s"
    if (( elapsed < 15 )); then
        echo
        echo "  WARNING: that is fast enough to have been the page-selective erase" >&2
        echo "  rather than a whole-device one, which would leave the boot block at" >&2
        echo "  0x100000 intact. A board provisioned after this may boot into the" >&2
        echo "  application image instead of recovery. See the header of this script." >&2
        echo
    fi

    # Covers the two image regions the .jic spans, which is everything except the boot
    # block. Not proof the device is blank, but it is the only readback available and it
    # catches an erase that reported success without doing anything.
    say "blank-checking"
    ( cd "$workdir" && quartus_pgm -c "$cable" -m jtag -o 'ib;sfl.jic' ) ||
        die "the blank check failed: the flash still holds data"

    rm -rf "$workdir"
    trap - EXIT
    say "EPCS64 erased and blank-checked"
fi

# ---------------------------------------------------------------------------
# The FX3's EEPROM
# ---------------------------------------------------------------------------

if (( do_fx3 )); then
    echo
    say "erasing the FX3 EEPROM"

    export FX3_FLASH_PROG="$flashprog"

    listing="$("$programmer" -l 2>&1)" || die "could not list FX3 devices:
$listing"

    # Application mode is the one case with a real answer for the user, and the boot ROM
    # is not reachable from it: the firmware is in control and neither -u nor -p works.
    # The transient flash-programmer mode is fine — that is where -p ends up anyway.
    if grep -q 'Mode=Application' <<<"$listing"; then
        die "the FX3 is running firmware, so its boot ROM cannot be reached.
Fit J4 (PMODE), power cycle, and run this again. It must show 04b4:00f3.

$listing"
    fi
    grep -qE 'Mode=(Bootloader|FlashProgrammer)' <<<"$listing" || die "no FX3 found.

$listing"

    blank="$(mktemp)"
    trap 'rm -f "$blank"' EXIT

    # The first page alone, so that the board is non-bootable before anything that could
    # fail for being off the end of the part is attempted. See the header.
    head -c 64 /dev/zero | tr '\0' '\377' >"$blank"
    say "clearing the boot signature (first 64-byte page)"
    "$programmer" -d 0 -p "$blank" -v ||
        die "could not write the EEPROM; the board is unchanged"

    # Then the whole device. A failure here is worth reporting precisely, because what it
    # leaves behind is still a blank-booting board — just not a fully erased one.
    head -c "$eeprom_bytes" /dev/zero | tr '\0' '\377' >"$blank"
    say "writing 0xFF over $eeprom_bytes bytes"
    if ! "$programmer" -d 0 -p "$blank" -v; then
        echo >&2
        echo "blank-board: the full erase failed, but the boot signature is already" >&2
        echo "  gone: the board will come up in the boot ROM at 04b4:00f3, which is" >&2
        echo "  what the bring-up tests need. Only the bytes past the first page are" >&2
        echo "  still whatever they were." >&2
        echo >&2
        echo "  If this part is smaller than $eeprom_bytes bytes, retry with" >&2
        echo "  --eeprom-bytes 65536." >&2
        exit 1
    fi

    rm -f "$blank"
    trap - EXIT
    say "EEPROM erased and verified"
fi

# ---------------------------------------------------------------------------

# Both halves are left in a mode a virgin board is never in, and only one of them is
# obvious from the outside — so whichever ran gets named rather than described in
# general terms.

echo
echo "Done. Before testing against this board:"
echo
echo "  1. Power cycle it."
if (( do_fpga )); then
    echo "     The FPGA is running Altera's serial flash loader, put there to reach"
    echo "     the flash. It answers nothing on the register link."
fi
if (( do_fx3 )); then
    echo "     The FX3 is in the Cypress flash-programmer mode at 04b4:4720."
fi
if (( do_fx3 )); then
    echo
    echo "  2. Check J4 (PMODE) is removed. With it fitted the FX3 never reads the"
    echo "     EEPROM, which looks identical over USB and is not the same test."
fi
echo
if (( do_fx3 && do_fpga )); then
    echo "It should then show 04b4:00f3 on USB, with no LED lit — an unconfigured FPGA."
elif (( do_fx3 )); then
    echo "It should then show 04b4:00f3 on USB."
else
    echo "The FPGA should then come up unconfigured, with no LED lit."
fi
