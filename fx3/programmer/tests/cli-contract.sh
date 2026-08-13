#!/usr/bin/env bash
#
# T2 contract test for the fx3-programmer command line.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This exists because of D24 and D25, and it guards a specific failure mode: help text that
# describes something the tool does not do. Both defects survived for a long time precisely
# because nothing could fail when the words and the code disagreed --
#
#   * -p was documented as programming "SPI flash" in four places. It programs the I2C
#     EEPROM, and there is no SPI code path at all. The supported hardware -- a SuperSpeed
#     Explorer Kit -- has no SPI flash. Wrong documentation on a destructive, permanent
#     operation is the worst place for it.
#   * -r was documented as "Reset device". It printed a message, slept and returned 0.
#
# The assertions below are about *promises*, not implementation. If the tool ever does grow
# real SPI support, this test should be updated deliberately, in the same change that adds
# the code and the hardware verification -- not quietly deleted to make it pass.
#
# Usage: cli-contract.sh <path-to-fx3-programmer>

set -uo pipefail

prog="$1"
status=0

fail() {
    echo "FAIL: $1" >&2
    status=1
}

ok() {
    echo "ok: $1"
}

help_text="$("$prog" -h 2>&1)"
help_rc=$?

# --- the help must be obtainable at all
if [ "$help_rc" -ne 0 ]; then
    fail "-h exited $help_rc, expected 0"
else
    ok "-h exits 0"
fi

# --- D24: the tool must not claim SPI flash support it does not have
if grep -qi "spi" <<<"$help_text"; then
    if grep -qiE "no SPI flash support|has no SPI" <<<"$help_text"; then
        ok "the only mention of SPI is the disclaimer"
    else
        fail "help text mentions SPI without disclaiming support (D24)"
        grep -in "spi" <<<"$help_text" >&2
    fi
else
    ok "help text makes no SPI claim"
fi

# --- D24: it must name the memory it actually writes
if grep -qi "EEPROM" <<<"$help_text"; then
    ok "help text names the I2C EEPROM"
else
    fail "help text does not mention the EEPROM, which is what -p programs (D24)"
fi

# --- D25: a removed option must not be advertised
if grep -qE '^\s+-r\b' <<<"$help_text"; then
    fail "help text still advertises -r, which does not exist (D25)"
else
    ok "-r is not advertised"
fi

# --- D25: and using it must explain itself rather than just failing
r_out="$("$prog" -r 2>&1)"
r_rc=$?
if [ "$r_rc" -eq 0 ]; then
    fail "-r exited 0; a removed option must fail"
elif grep -qi "power cycle" <<<"$r_out"; then
    ok "-r fails and explains the power-cycle requirement"
else
    fail "-r fails without explaining what to do instead"
fi

# --- -v alone cannot verify anything; it is a modifier for -p
v_out="$("$prog" -v 2>&1)"
v_rc=$?
if [ "$v_rc" -eq 0 ]; then
    fail "-v alone exited 0, but it has no file to verify against"
elif grep -q -- "-p" <<<"$v_out"; then
    ok "-v alone fails and points at -p"
else
    fail "-v alone fails without saying how to use it"
fi

# --- a bad device index must be rejected, not silently treated as device 0
for bad in abc -1 99; do
    out="$("$prog" -d "$bad" -l 2>&1)"
    rc=$?
    if [ "$rc" -eq 0 ]; then
        fail "-d '$bad' was accepted; atoi() used to make this device 0"
    else
        ok "-d '$bad' rejected"
    fi
done

# --- an unknown option must name itself, not print "-?"
z_out="$("$prog" -z 2>&1)"
if grep -q -- "-z" <<<"$z_out"; then
    ok "unknown options are named in the error"
else
    fail "unknown option error does not name the option (optopt vs opt)"
fi

exit "$status"
