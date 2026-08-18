# Repository-wide checks — the ones that belong to no single component.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Every other check in this repository is scoped to a component and lives beside it
# (fpga/checks.nix, or the ctest suite inside a package's buildPhase). This file is for
# checks whose subject is the whole tree, so no component can own them.
#
# `src` is the flake source, so a check sees every tracked file and only tracked files —
# which is exactly the set the licence convention applies to, and which is also why a
# newly added script has to be tracked by git before `nix flake check` can run it. It also
# means any change anywhere re-runs both checks. That is the honest cost of a whole-tree
# check, and it is cheap: one is a shell script over a few hundred headers, the other
# assembles and takes apart a bundle of a few kilobytes.

{
  runCommand,
  bash,
  coreutils,
  gnutar,
  minisign,
  src,
}:

{
  # T4 — every project-authored source file carries a copyright and a licence statement.
  # Runs the same script a developer runs, so the two cannot drift.
  licence-headers = runCommand "ddd-licence-headers" { nativeBuildInputs = [ bash ]; } ''
    bash ${src}/tools/check-licence-headers.sh ${src}
    touch $out
  '';

  # T4 — the release tooling assembles a bundle, and the bundle is what it claims to be.
  #
  # This is the per-commit tier's proof that the update packaging works end to end: every
  # commit assembles a real (firmware-only, development-signed) bundle and takes it apart
  # again. The payload is synthetic — nothing here builds firmware, and nothing here ever
  # writes to a device (AGENTS.md §4) — because what is under test is the format and the
  # tooling, not the contents.
  #
  # Everything the check does after the bundle exists is done with **stock tools**: GNU
  # tar lists it, minisign verifies the signature, sha256sum checks the digests. That is
  # deliberate. The application's own reader is covered by its unit and golden tests
  # (ddd-gui/tests), and a check that used our reader to validate our writer would only
  # be able to say that the two agree with each other.
  update-bundle =
    runCommand "ddd-update-bundle"
      {
        nativeBuildInputs = [
          bash
          coreutils
          gnutar
          minisign
        ];
      }
      ''
        export HOME="$TMPDIR"
        cd "$TMPDIR"

        printf 'not a real firmware image\n' > firmware.img

        # --created is fixed rather than left to the clock, because the second half of
        # this check is that the same inputs give the same bytes.
        make_bundle() {
          bash ${src}/tools/make-update-bundle.sh \
            --output "$1" \
            --version 1.4.0 \
            --commit 0123abcd \
            --channel development \
            --created 2026-01-01T00:00:00Z \
            --notes "A development bundle assembled by nix flake check." \
            --secret-key ${src}/tools/keys/development.key \
            --public-key ${src}/tools/keys/development.pub \
            --firmware firmware.img \
            --firmware-identity 0123abcd
        }

        # Two assemblies of the same inputs, into the same filename in different
        # directories. The name matters: it rides in the signature's trusted comment, so a
        # bundle renamed is a bundle re-signed, and comparing two differently-named files
        # would be comparing two different artefacts.
        mkdir -p first second
        make_bundle first/domesday-duplicator-update-1.4.0.dddfw
        make_bundle second/domesday-duplicator-update-1.4.0.dddfw

        cd first

        # The entry order is part of the format: the manifest must be first, so that a
        # reader cannot verify one entry while an extractor uses another.
        listed=$(tar --list --file domesday-duplicator-update-1.4.0.dddfw | tr '\n' ' ')
        if [ "$listed" != "manifest.json manifest.minisig firmware.img " ]; then
          echo "the bundle's entries came out as '$listed'" >&2
          exit 1
        fi

        # Stock minisign, against the committed development public key.
        mkdir -p extracted
        tar --extract --file domesday-duplicator-update-1.4.0.dddfw --directory extracted
        minisign -Vm extracted/manifest.json \
          -p ${src}/tools/keys/development.pub \
          -x extracted/manifest.minisig

        # Stock sha256sum, against the digest the manifest records.
        recorded=$(grep -o '"sha256": "[0-9a-f]*"' extracted/manifest.json | cut -d'"' -f4)
        actual=$(sha256sum extracted/firmware.img | cut -d' ' -f1)
        if [ "$recorded" != "$actual" ]; then
          echo "the manifest records $recorded, the payload hashes to $actual" >&2
          exit 1
        fi

        # And the archive is reproducible: two assemblies of the same inputs are the same
        # file. Without this the release could not be rebuilt and compared against its
        # published digest, which is what the reproducibility audit will do later.
        cmp domesday-duplicator-update-1.4.0.dddfw \
            ../second/domesday-duplicator-update-1.4.0.dddfw

        cd "$TMPDIR"

        # And the same file with all four payloads, which is what every firmware release
        # actually publishes: the two an ordinary update installs, and the two a bring-up
        # needs on top. Checked here because the entry order is as much a part of this
        # format as the manifest's contents are — a reader that took the first match for
        # an entry name could verify one payload and install another.
        printf '! not a real SVF\nSTATE IDLE;\n' > gateware-provisioning.svf
        printf 'not a real factory image\n' > gateware-factory.rpd
        printf 'not a real application image\n' > gateware-app.rpd
        mkdir -p complete

        bash ${src}/tools/make-update-bundle.sh \
          --output complete/domesday-duplicator-update-1.4.0.dddfw \
          --version 1.4.0 \
          --commit 0123abcd \
          --channel development \
          --created 2026-01-01T00:00:00Z \
          --notes "A development bundle assembled by nix flake check." \
          --secret-key ${src}/tools/keys/development.key \
          --public-key ${src}/tools/keys/development.pub \
          --firmware firmware.img \
          --firmware-identity 0123abcd \
          --gateware gateware-app.rpd \
          --gateware-identity 0123abcd \
          --provisioning gateware-provisioning.svf \
          --provisioning-identity 0123abcd \
          --factory-gateware gateware-factory.rpd \
          --factory-gateware-identity 0123abcd

        listed=$(tar --list --file complete/domesday-duplicator-update-1.4.0.dddfw |
          tr '\n' ' ')
        expected="manifest.json manifest.minisig firmware.img gateware-app.rpd "
        expected="$expected"'gateware-provisioning.svf gateware-factory.rpd '
        if [ "$listed" != "$expected" ]; then
          echo "the bundle's entries came out as '$listed'" >&2
          exit 1
        fi

        # The pin that decides which published bundle a packaged build carries. Its shape
        # only — fetching would need a network this sandbox does not have, and should
        # not. What this catches is a half-filled pin: a URL with no digest is an
        # unverified download and a digest with no URL is a check that never runs, and
        # both should fail on the commit that made them.
        bash ${src}/tools/fetch-bundled-update.sh \
          --check --pin ${src}/ddd-gui/packaging/bundled-update.env

        touch $out
      '';
}
