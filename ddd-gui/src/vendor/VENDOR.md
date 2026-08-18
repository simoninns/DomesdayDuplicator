# Vendored cryptography

Two upstream projects, copied here byte-for-byte and never edited. They are the only
third-party sources in `ddd-gui/`.

The update mechanism needs exactly two primitives: **SHA-256**, the one digest that covers
every hand-off from a CI build to the flash readback, and **Ed25519 verification**, which
is how a bundle proves it came from the project rather than from somebody else. Neither is
something this project should write, and neither is worth a system dependency: the
application ships as a Flatpak, an MSI and a DMG, and a library that has to be found on
three platforms in order to check a signature is a packaging problem for no benefit. Both
files below are small, public-domain-equivalent and self-contained.

| File | Upstream | Version | Licence |
| --- | --- | --- | --- |
| `sha-256.c`, `sha-256.h` | [amosnier/sha-2](https://github.com/amosnier/sha-2) | `master`, commit fetched 2026-08-14 | Unlicense **or** 0BSD, at your option |
| `monocypher.c`, `monocypher.h` | [Monocypher](https://monocypher.org/) | 4.0.2 | BSD-2-Clause **or** CC0-1.0, at your option |
| `monocypher-ed25519.c`, `monocypher-ed25519.h` | Monocypher `src/optional/` | 4.0.2 | BSD-2-Clause **or** CC0-1.0, at your option |

All three licences are compatible with GPLv3 (AGENTS.md §10), and each file carries its own
licence text or points at it, so nothing here needs the project's SPDX header — they are
exempt by name in `tools/check-licence-headers.sh` for that reason (AGENTS.md §3).

## Provenance

```
monocypher-4.0.2.tar.gz   sha256 38d07179738c0c90677dba3ceb7a7b8496bcfea758ba1a53e803fed30ae0879c
                          from   https://monocypher.org/download/monocypher-4.0.2.tar.gz

sha-256.c                 sha256 7a74437adc78576b8faff060f9573ba88a6da798914f45d263c75b149add3d27
sha-256.h                 sha256 c2173d83813a0c29fcc3345ce489766efeadee6a52ca927ecd0f917a120df9fb
                          from   https://raw.githubusercontent.com/amosnier/sha-2/master/
```

`monocypher.c` is taken whole rather than trimmed to the two functions the application
calls. Argon2, ChaCha20, Poly1305 and X25519 come along unused, which costs tens of
kilobytes in the binary and buys the thing that matters: this file is diffable against
upstream, so the next version is a copy rather than a merge. A trimmed copy is a fork.

## Why Monocypher rather than libsodium or OpenSSL

Monocypher is one C file with no build system, no configuration and no runtime
dependencies, and it implements RFC 8032 Ed25519 (`crypto_ed25519_check`, the SHA-512
variant that minisign uses) and BLAKE2b, which is what minisign's prehashed signature mode
hashes with. That is the complete list of what the verifier needs. libsodium and OpenSSL
would each be a platform-specific dependency for the same two calls.

Note that only the **verifier** is used here. Nothing in the application signs anything;
signing is `minisign` itself, invoked by `tools/make-update-bundle.sh` at build time.

## Why a separate SHA-256

Monocypher deliberately does not implement SHA-256 — it offers BLAKE2b and, for Ed25519's
sake, SHA-512. The integrity chain is specified on SHA-256 because that is the digest the
rest of the world's tooling agrees on: a maintainer can check a published payload with
`sha256sum` and get the number the manifest claims. That property is worth one more small
file.

`sha-256.c` is also freestanding-friendly — `stdint.h`, `stddef.h` and `string.h`, no
allocation, no I/O — which matters beyond this component. The device-update work
gives the FX3 firmware the same digest, hashing the incoming stream and the EEPROM
readback. The firmware cannot include from here (AGENTS.md §2 forbids cross-component
includes), so it takes its own copy of the *same pinned upstream version and digest*
recorded above. One upstream pin, two vendored copies, no shared source tree.

## Refreshing

Replace the files wholesale from upstream, update the version and digests in the table
above, and run the test suite: `ddd_update_tests` checks SHA-256 against the NIST vectors
and Ed25519 against RFC 8032 vector 3, so a refresh that broke either would fail rather
than pass quietly. Never reformat, never fix a warning, never re-indent (AGENTS.md §3) —
the build keeps these sources out of `-Wall -Wextra`, clang-format and clang-tidy precisely
so there is never a reason to touch them.
