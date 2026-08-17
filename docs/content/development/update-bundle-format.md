# Update bundle format

One file carries everything a Domesday Duplicator may write to itself. This page defines it: the archive, the manifest schema, the signature, and the order in which a reader must check them.

This one is a description rather than a promise — the format exists, the tooling that produces it is `tools/make-update-bundle.sh`, and the reader is in `ddd-gui/src/capture/update_bundle.cpp`. The device-side protocol it feeds is specified on the [device update mechanism](device-update-mechanism.md) page and is still being built.

## The file

```
domesday-duplicator-update-<version>.dddfw     uncompressed ustar archive
  manifest.json      always the first entry
  manifest.minisig   detached Ed25519 signature over manifest.json
  firmware.img       FX3 image, when the bundle carries firmware
  gateware-app.rpd   raw EPCS byte stream, when it carries gateware
  gateware-provisioning.svf   JTAG vectors, in a provisioning set
```

**Uncompressed tar**, because the two properties that matter are that stock `tar` can list and extract it and that reading it takes a couple of hundred lines of dependency-free code. Both payloads are already compressed by the toolchains that produced them, so a compressed container would trade both properties for nothing measurable.

**The manifest is signed, not the archive.** The manifest carries the SHA-256 of every payload, so signing that one small file authenticates the whole bundle — and nothing has to sign a file that would then have to contain its own signature. It also means the offline file-picker path and the online download path verify by identical means, because everything they check is inside the file.

**The `gateware` payload is the application image only.** The factory image and the combined provisioning `.jic` are release artefacts too, but they are not that component: it carries only what the device is permitted to write to itself. Provisioning has a component of its own, below, and it is written through a cable rather than by the device.

**And it is written to the flash verbatim** — no header stripped, no bytes reordered, nothing interpreted, by the application or the firmware or the gateware. So the `.rpd` in a bundle must already be in the bit orientation the FPGA's configuration engine reads, which is decided when Quartus emits it and is not observable anywhere downstream: an image in the wrong orientation passes the signature, every digest, and the device's own readback, and then configures nothing. The [EPCS layout and boot flow](epcs-layout-and-boot-flow.md#the-bytes-in-the-flash-are-bit-reversed) page has the detail; what belongs here is that this format carries a payload no check in it can validate the meaning of.

**Components are individually optional.** A firmware-only bundle is a complete bundle. That is what the developer loop produces, what per-commit CI produces, and what the early phases of this work shipped before the gateware path existed. A bundle with no components at all is refused.

**A component kind the reader does not know is refused**, rather than skipped. Every member of `components` names a payload that something has to write to somebody's hardware, and a reader that quietly ignored one would install a partial bundle while reporting a complete one.

### The provisioning set

A third component kind, `gateware-provisioning-svf`, carries the FPGA's gateware as **JTAG vectors** rather than as a flash image. A bundle carrying it and firmware is a *provisioning set*: what a board with no working gateware is brought up with.

The difference from the `gateware` component is not what it contains but what writes it. `gateware` goes to the device over USB, through the flash bridge that the running gateware provides — which is exactly what a board being brought up does not have. The vectors are played into the FPGA's JTAG port through the DE0-Nano's own USB-Blaster, by [the bring-up wizard](../capture-gui/bringing-up-a-board.md); see [USB-Blaster and SVF programming](usb-blaster-and-svf.md).

Three consequences worth stating:

- the ordinary update path **never installs it**. `UpdateOrchestrator` does not look at it, and the compatibility gate does not gate on it;
- a bundle carrying *only* provisioning vectors is refused by the update window with a sentence naming the window that does want it. It is a legal manifest and not an update;
- the schema version is deliberately **not** bumped for this. A build predating the component reads the firmware beside it and offers an ordinary firmware install, which is a true description of what that build can do with the file and exactly as safe as any other firmware bundle. Bumping the version would instead have every older build refuse every bundle, for a component that changes the meaning of no other field.

**It is published as its own file**, `domesday-duplicator-provisioning-<version>.dddfw`, beside the update bundle in every firmware release — rather than as a third component of the update bundle. The vectors are an order of magnitude larger than the images they stand for, and every user of the ordinary update path would carry them for nothing. One format, one key, one verifier; two files, because they are installed by different people at different times.

**Packaged builds of the application carry a copy.** A board being brought up cannot be updated over USB — that is what bring-up is for — so the machine beside it may be one that has just been built, with no network at all. The installers therefore fetch the set by digest at packaging time and install it beside the application, and the wizard preselects it. Being bundled buys the offline case and nothing else: the file is verified exactly as a downloaded one is, by the same reader with the same key policy, so it is data the signature covers and never a second trust anchor. Which set is bundled is pinned in `ddd-gui/packaging/bundled-provisioning.env`, and a build with nothing pinned bundles nothing and says so — see [Release pipeline](release-pipeline.md).

## Reading one by hand

Nothing about the format needs this project's tools:

```bash
tar tvf domesday-duplicator-update-1.4.0.dddfw
tar xOf domesday-duplicator-update-1.4.0.dddfw manifest.json

mkdir unpacked && tar xf domesday-duplicator-update-1.4.0.dddfw -C unpacked
minisign -Vm unpacked/manifest.json -p tools/keys/development.pub \
         -x unpacked/manifest.minisig
sha256sum unpacked/firmware.img
```

That the last three lines work with programs this project did not write is the point of choosing tar, minisign and SHA-256 rather than anything cleverer. A maintainer can check a published bundle without trusting the application, and the application can be checked against a bundle it did not produce.

## The manifest

```json
{
  "manifest_version": 1,
  "channel": "release",
  "version": "1.4.0",
  "commit": "0123abcd",
  "created": "2026-08-14T09:15:00Z",
  "release_notes": "Jumper-free firmware updates.",
  "components": {
    "firmware": {
      "file": "firmware.img",
      "length": 116432,
      "sha256": "c8422f817b69fa687531b26f4f190b2c4a97fe7b6791850ff76dc10a2d52b2d9",
      "identity": "0123abcd",
      "interface_version": 1
    },
    "gateware": {
      "file": "gateware-app.rpd",
      "length": 368640,
      "sha256": "73af12c40cb9f02c03d84162217a841544fb5cb7d2dda9fdd00fff8e9d117c2a",
      "identity": "0123abcd",
      "interface_version": 2
    }
  },
  "compatibility": {
    "minimum_application_version": "1.4.0",
    "minimum_register_map_version": 2,
    "epcs_layout_version": 1
  }
}
```

### Top level

| Field | Type | Meaning |
| --- | --- | --- |
| `manifest_version` | integer | The schema version. `1` is the only one defined. A reader refuses anything else outright |
| `channel` | string | `release` or `development`, and nothing else |
| `version` | string | The release version, as dotted decimal numbers |
| `commit` | string | The git commit every payload was built from |
| `created` | string | When the bundle was assembled, ISO 8601 in UTC |
| `release_notes` | string | One line, shown before the user confirms |
| `components` | object | At least one of `firmware`, `gateware` and `gateware-provisioning-svf`; no other member |
| `compatibility` | object | What a device and an application must already be |

Every field is required. There are no optional top-level fields and no defaults, because a default is a decision made by whoever wrote the reader on behalf of whoever wrote the bundle, and the two are separated here by a release process and possibly by years.

`created` is recorded for the human reading a bundle later. **Nothing decides anything on it.** A timestamp an attacker can set is not a version, and a reader that preferred the newer of two bundles by date would be preferring whichever one lied.

### Components

| Field | Type | Meaning |
| --- | --- | --- |
| `file` | string | The archive entry holding the bytes. A bare filename — bundle entries are flat |
| `length` | integer | The payload's length in bytes |
| `sha256` | string | 64 lowercase hex characters |
| `identity` | string | The commit the device will report once this payload is installed |
| `interface_version` | integer | The version of the interface this payload will advertise once installed |

`length` is a second, independent statement of something the archive already knows. That is deliberate: a truncated payload is then caught as a truncation, before its digest is computed, rather than surfacing as a digest mismatch that could mean anything.

`identity` is what the post-update confirmation compares against. It is the product-string commit for the firmware and the identity registers for the gateware. Its purpose is to let the application prove an update worked by *reading the device*, rather than by assuming the write it just did had the effect it intended.

`interface_version` means the USB protocol version in `bcdDevice` for the firmware, and the register-map version in register `0x01` for the gateware. One field name for two different registers because the compatibility gate does exactly one thing with it — compare it against the range the application supports — and two names would have invited two code paths for one rule.

For `gateware-provisioning-svf`, both fields describe what the board will report *once it has been provisioned and power-cycled*: the identity registers of the factory image, and its register-map version.

### Compatibility

| Field | Type | Meaning |
| --- | --- | --- |
| `minimum_application_version` | string | The oldest release of the capture application that may install this bundle |
| `minimum_register_map_version` | integer | The oldest gateware register map the firmware in this bundle can drive |
| `epcs_layout_version` | integer | The EPCS layout the gateware payload assumes |

`minimum_application_version` is **enforced, not advisory**: a bundle requiring a newer application disables the install and says so. The gate and the reasoning behind it are on the [device update mechanism](device-update-mechanism.md) page.

It is dotted decimal numbers rather than a commit because this is the one question a commit cannot answer. Missing trailing components read as zero, so `1.2` and `1.2.0` are the same version, and comparison is component by component: `1.10.0` is newer than `1.9.0`. Anything that is not a dotted sequence of decimal numbers — a commit hash, `unknown`, an empty string, `1.4.0-dirty` — cannot be ordered, and the comparison says so rather than guessing.

`epcs_layout_version` matters more than it looks. The factory image's boot logic is frozen at provisioning time and reads exactly one layout; a bundle built against a different one must not be written, because the boot block it would leave behind is the one thing a field update cannot repair.

### What a reader refuses, and why

The manifest is the only untrusted input this application will ever parse, and it is parsed before its signature can help — so the parser is strict in ways a general JSON reader is not:

- **duplicate keys** are refused. A manifest carrying two `sha256` fields has no single meaning, and a reader that quietly kept the first or the last is a reader that can disagree with the tool that signed it;
- **numbers keep their source text.** A byte count that went through a double and came back could gain a `.0` or lose a digit. `1e3` and `1000.0` are the same quantity as `1000` to a mathematician, and a sign that something other than the build script wrote the file, so they are refused where an integer is expected;
- **nesting is capped**, so a hostile file cannot exhaust the stack before the signature that would have rejected it has been checked;
- **trailing content, comments, trailing commas, single quotes and `NaN`** are not JSON and are not accepted;
- **control characters inside strings** must be escaped, so a filename cannot carry a newline.

## The signature

`manifest.minisig` is a [minisign](https://jedisct1.github.io/minisign/) detached signature over the exact bytes of `manifest.json`. Both of minisign's modes are accepted — the default, which signs the message, and the prehashed form, which signs a BLAKE2b-512 of it — because minisign produces either depending on a flag, and a verifier that understood only one would fail on a bundle signed by a maintainer who typed the other.

A signature file has four lines: an untrusted comment, the signature, a *trusted* comment, and a second signature covering the first signature concatenated with that trusted comment. **Both signatures are checked.** Without the second, the trusted comment would be attacker-editable text presented to a user as though the project had written it.

The bundle's trusted comment carries the filename, version and channel, so a mislabelled file is visible before it is opened.

Minisign rather than a format of this project's own, for the reason given above: a maintainer can verify a published bundle with a program nobody here wrote, and the application can be tested against signatures it did not produce. A bespoke container would have made both impossible in exchange for nothing.

### Channels and keys

There are two keys and no unsigned path at all.

The **release key**'s secret half lives as a CI secret and signs release manifests only, in a tag-triggered workflow. Its public half is committed to this repository and compiled into the application.

The **development key**'s secret half is [committed to this repository](https://github.com/simoninns/DomesdayDuplicator/blob/main/tools/keys/development.key) and is therefore public. A development signature proves a bundle is well formed and proves nothing whatever about where it came from. That is exactly why it is a separate key *and* a separate channel: a release build of the application pins the release key and accepts nothing else, and a build that accepts the development key does so only on an explicit per-invocation opt-in and banners every development-signed bundle prominently.

The development key **is** the unsigned path, made explicit and impossible to confuse with a release. An actually-unsigned bundle format would have needed a second code path through verification, and a second code path through verification is where the bugs live.

Rotation is an application release that pins the new key; old bundles verify against old applications, so the release notes have to say so.

## Assembling one

```bash
./tools/make-update-bundle.sh \
    --output build/domesday-duplicator-update-1.4.0.dddfw \
    --version 1.4.0 --commit "$(git rev-parse --short=8 HEAD)" \
    --channel release --secret-key "$RELEASE_KEY" \
    --firmware result-firmware/firmware.img --firmware-identity 0123abcd \
    --notes "Jumper-free firmware updates."
```

A provisioning set is the same script with one more payload:

```bash
./tools/make-update-bundle.sh \
    --output build/domesday-duplicator-provisioning-1.4.0.dddfw \
    --version 1.4.0 --commit "$(git rev-parse --short=8 HEAD)" \
    --channel release --secret-key "$RELEASE_KEY" \
    --firmware result-firmware/firmware.img --firmware-identity 0123abcd \
    --provisioning result-bitstream/provisioning/DomesdayDuplicatorProvisioning.svf \
        --provisioning-identity 0123abcd \
    --notes "Provisioning set for bringing a board up."
```

`--help` lists the rest. The script needs bash, coreutils, GNU tar and minisign, all of which are in `nix develop` and all of which are ordinary distribution packages; nothing about it is Nix-only.

Two things it does that are worth knowing about:

**It re-reads its own output.** After the archive is written, the script lists it, extracts it, verifies the signature with `minisign -V` against the public key regenerated from the secret key, and compares every payload's digest against what the manifest recorded. This is the point in the chain where a wrong payload would be bundled with a right manifest, and the check costs milliseconds against a file that is about to be published and written to somebody's hardware.

**Its output is reproducible.** Every ownership and timestamp field in the archive is pinned, so the same inputs give the same bytes — with one exception, `created`, which defaults to now. Pass `--created`, or set `SOURCE_DATE_EPOCH`, and a bundle can be rebuilt and compared byte for byte. `nix flake check` does exactly that on every commit.

Note that the bundle's filename rides in the signature's trusted comment, so a renamed bundle is a re-signed bundle. Two builds that differ only in `--output` are two different artefacts.

## The order a reader must check things in

This order is part of the format, not an implementation detail:

1. **The archive parses, and `manifest.json` is its first entry.** First, not merely present — a reader that searched for the manifest could verify one entry while an extractor that took the first match used another.
2. **`manifest.minisig` verifies over the manifest's exact bytes.** Nothing has been interpreted yet, so an unauthentic bundle is rejected before any of what it says has been believed.
3. **Only then is the manifest parsed**, and its schema version checked.
4. **Every component the manifest declares is present, is exactly the length stated, and hashes to the digest stated.**

A bundle that passes has been proved to come from the holder of the key and to carry the bytes that key signed for. It has *not* been proved installable on the device in front of the user — that is the compatibility gate, which needs the device.

The archive reader is deliberately narrow, too. Directories, symlinks, hard links, device nodes, long-name extensions, non-empty path prefixes, names containing a path separator and two entries with the same name are all refused rather than interpreted. A bundle is four files in one directory, and every feature accepted beyond that is a feature an attacker gets to use.

## Where the code is

| File | Holds |
| --- | --- |
| `tools/make-update-bundle.sh` | The producer: assembly, signing and the self-check |
| `tools/dev-bundle.sh` | The developer loop's wrapper around it, `--kind update` or `--kind provisioning` |
| `tools/fetch-bundled-provisioning.sh` | The packaging jobs' fetch-by-digest of the pinned set |
| `ddd-gui/packaging/bundled-provisioning.env` | Which published set the installers carry |
| `ddd-gui/src/gui/bundled_provisioning.{h,cpp}` | Where an installed build looks for its copy |
| `tools/keys/development.pub`, `.key` | The development keypair, deliberately public |
| `ddd-gui/src/capture/update_bundle.{h,cpp}` | The archive reader and writer, and the check order above |
| `ddd-gui/src/capture/update_manifest.{h,cpp}` | The manifest model, its parser and the version comparison |
| `ddd-gui/src/capture/json_value.{h,cpp}` | The strict JSON reader |
| `ddd-gui/src/capture/minisign_verify.{h,cpp}` | Signature verification |
| `ddd-gui/src/vendor/` | The vendored SHA-256 and Ed25519, with their provenance |
| `nix/checks.nix` | The per-commit check that all of this works |
