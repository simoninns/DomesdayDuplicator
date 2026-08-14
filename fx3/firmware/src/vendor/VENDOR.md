# Vendored SHA-256

One upstream file pair, copied here byte-for-byte and never edited. It is the only
third-party source in `fx3/firmware/` that is not the Cypress SDK.

| File | Upstream | Version | Licence |
| --- | --- | --- | --- |
| `sha-256.c`, `sha-256.h` | [amosnier/sha-2](https://github.com/amosnier/sha-2) | `master`, commit fetched 2026-08-14 | Unlicense **or** 0BSD, at your option |

Both licences are compatible with GPLv3 (AGENTS.md §10), and the files carry their own
licence text, so nothing here needs the project's SPDX header — they are exempt by name in
`tools/check-licence-headers.sh` for that reason (AGENTS.md §3).

## Provenance

```
sha-256.c   sha256 7a74437adc78576b8faff060f9573ba88a6da798914f45d263c75b149add3d27
sha-256.h   sha256 c2173d83813a0c29fcc3345ce489766efeadee6a52ca927ecd0f917a120df9fb
            from   https://raw.githubusercontent.com/amosnier/sha-2/master/
```

## Why this is a second copy

`ddd-gui/src/vendor/` holds the same two files, at the same pinned version and with the
same digests. That is duplication on purpose.

The device-update integrity chain is specified on **one digest, SHA-256, computed once at
build time and re-checked at every hand-off** — including the two hand-offs this firmware
owns: the chunk stream arriving over EP0 (link 5) and the readback from the EEPROM the
bytes were just written to (link 6). Both sides of that comparison have to be the same
number, so both sides have to be the same algorithm, which is why the pin is shared even
though the file is not.

The file itself cannot be: AGENTS.md §2 forbids cross-component source includes, and this
is a bare-metal ARM926EJ-S build against the Cypress SDK while the other is a hosted C++20
application. One upstream pin, two vendored copies, no shared source tree — the same
arrangement the USB wire protocol has, and for the same reason.

`sha-256.c` suits a freestanding build without modification: `stdint.h`, `stddef.h` and
`string.h`, no allocation, no I/O, and a streaming API so the firmware never needs the
whole image in RAM at once. Nothing here is compiled with `-Wall`, so a warning that is not
ours to fix cannot fail this build.

## Refreshing

Replace both files wholesale from upstream, update the digests above **and** the matching
table in `ddd-gui/src/vendor/VENDOR.md`, and keep the two copies identical — a refresh that
moved only one of them would leave the device and the host computing different numbers for
the same bytes, which the integrity chain would then report as corruption. `sha256sum` over
the two directories is the check.
