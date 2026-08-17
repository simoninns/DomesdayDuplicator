# The Domesday Duplicator FPGA bitstream.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Unlike every other package in this repository, this one is **not** in
# `nix flake check`. Quartus Prime Lite is unfree, x86_64-linux only, and marked
# `redistributable = false` — so it can never be served from a binary cache, and
# making it a dependency of the check every contributor runs would put a
# multi-gigabyte unfree download in front of a one-line documentation fix.
#
# It *is* built by CI, in a workflow of its own: .github/workflows/bitstream.yml
# on gateware changes and manual dispatch, and from the tag by
# release-firmware.yml, so every released bitstream is CI-built from the tagged
# commit. fpga/README.md, "How the bitstream is built", has the full model.
#
# What this derivation is for is making the build repeatable wherever it runs:
# same inputs, same command line, an output directory that always contains the
# same set of files, and a provenance record generated rather than typed. The
# dev shell (`nix develop .#fpga-quartus`) remains the primary way to work on
# the gateware.
#
# Reproducibility, measured rather than assumed (P6-9): two compiles of the same
# commit on the same toolchain produce a byte-identical .jic and a .sof that
# differs only in a compile timestamp, a per-run design hash and the checksum
# covering them. bitstream-provenance.txt carries digests for both.

{
  lib,
  stdenvNoCC,
  quartus-prime-lite,
  python3,
  # The commit this was built from. Passed in rather than read from git,
  # because the derivation's source has no .git — the same reason the firmware
  # and the GUI take theirs as a parameter (D4, D21).
  bitstreamVersion ? "unknown",
}:

stdenvNoCC.mkDerivation {
  pname = "domesday-duplicator-bitstream";
  version = "0";

  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./application
      ./common
      ./factory
      ./provisioning
      ./bitstream-provenance.py
      ./generate-version.sh
      ./make-boot-block.py
    ];
  };

  nativeBuildInputs = [
    quartus-prime-lite
    python3
  ];

  # There is no configure step, and stdenv's default would fail looking for one.
  dontConfigure = true;

  buildPhase = ''
    runHook preBuild

    # Quartus writes settings, a license cache and lock files into $HOME on
    # every invocation, and refuses to start without a writable one.
    export HOME="$TMPDIR"

    # The project files must be writable: quartus_sh rewrites the .qsf in place
    # to record LAST_QUARTUS_VERSION. That is why a local build should never be
    # run in the source directories — see build-local.sh, which does the same
    # copy this does.
    chmod -R u+w .

    # Stamp the build with the commit, which the gateware presents to the FX3
    # in its identity registers. It has to happen after the chmod, because that
    # is what makes this copy of the sources writable, and it overwrites the
    # placeholder version.vh committed beside them. bitstreamVersion is passed
    # in for the same reason the firmware's is: there is no .git here to ask.
    # Both images include the same file, so both name the same commit.
    bash "$src/generate-version.sh" common "${bitstreamVersion}"

    echo "Compiling with $(quartus_sh --version | sed -n 2p)"

    # The factory image first, because the provisioning file needs both and
    # this is the one whose failure matters most: a unit cannot be provisioned
    # without it, and it is the half that can never be repaired in the field.
    (cd factory && quartus_sh --flow compile DomesdayDuplicatorFactory)
    (cd application && quartus_sh --flow compile DomesdayDuplicator)

    # The .cof files are committed and name their own inputs and outputs, so
    # the conversions need no arguments beyond the file itself. The
    # provisioning one carries both images at the addresses the EPCS layout
    # documents; the application one exists to emit the raw image bytes, and
    # the .jic it produces alongside them is deleted because programming it
    # would write the capture gateware over the factory image.
    # The converter reads its inputs from beside the .cof, so the two images
    # are copied in and removed again once it has run.
    cp factory/DomesdayDuplicatorFactory.sof application/DomesdayDuplicator.sof provisioning/
    (cd provisioning &&
      quartus_cpf -c DomesdayDuplicatorProvisioning.cof &&
      rm -f DomesdayDuplicatorFactory.sof DomesdayDuplicator.sof)

    # The same provisioning content again, as the JTAG vectors that write it,
    # so that a board can be provisioned by ddd-jtag over the DE0-Nano's
    # on-board USB-Blaster on a machine that has never had Quartus installed.
    # Every Cyclone IV and serial flash loader decision stays here, at build
    # time, in the tool that already holds it.
    #
    # The frequency is not decoration: the converter turns every wait into a
    # count of TCK cycles at the rate named here, so the file says how long
    # its erases are meant to take only in combination with this number. The
    # player reads it back out of the file and holds the waits open for that
    # long whatever the cable's own clock does.
    (cd provisioning &&
      quartus_cpf -c -q 4.5MHz -g 3.3 -n p \
        DomesdayDuplicatorProvisioning_write_jic.cdf \
        DomesdayDuplicatorProvisioning.svf)
    (cd application && quartus_cpf -c DomesdayDuplicator.cof && rm -f DomesdayDuplicator.jic)

    # The boot block that describes the application image. It is not written
    # by this build - the update path writes it onto a device - but it is
    # published with the image it belongs to, because it is derived from those
    # exact bytes and from nothing else.
    python3 "$src/make-boot-block.py" \
      --image application/DomesdayDuplicator_auto.rpd \
      --output provisioning/boot-block.bin

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out"

    # The output keeps the layout the sources have, so that what came from
    # which image is not something anyone has to work out from a filename.
    install -Dm444 application/DomesdayDuplicator.sof -t "$out/application"
    install -Dm444 application/DomesdayDuplicator_auto.rpd -t "$out/application"
    install -Dm444 application/DomesdayDuplicator_write_sof.cdf -t "$out/application"

    install -Dm444 factory/DomesdayDuplicatorFactory.sof -t "$out/factory"
    install -Dm444 factory/DomesdayDuplicatorFactory_write_sof.cdf -t "$out/factory"

    # What a board is provisioned from: both images, the map that says where
    # the converter put them, the boot block, and the file quartus_pgm reads.
    install -Dm444 provisioning/DomesdayDuplicatorProvisioning.jic -t "$out/provisioning"
    install -Dm444 provisioning/DomesdayDuplicatorProvisioning.svf -t "$out/provisioning"
    install -Dm444 provisioning/DomesdayDuplicatorProvisioning.map -t "$out/provisioning"
    install -Dm444 provisioning/DomesdayDuplicatorProvisioning_write_jic.cdf -t "$out/provisioning"
    install -Dm444 provisioning/boot-block.bin -t "$out/provisioning"

    # The compilation reports, for both images. A bitstream with no timing
    # report is not something anyone should be asked to trust, and these are
    # small.
    mkdir -p "$out/reports/application" "$out/reports/factory"
    install -Dm444 application/DomesdayDuplicator.*.rpt -t "$out/reports/application"
    install -Dm444 application/DomesdayDuplicator.*.summary -t "$out/reports/application"
    install -Dm444 factory/DomesdayDuplicatorFactory.*.rpt -t "$out/reports/factory"
    install -Dm444 factory/DomesdayDuplicatorFactory.*.summary -t "$out/reports/factory"

    python3 "$src/bitstream-provenance.py" \
      --build-dir . \
      --commit "${bitstreamVersion}" \
      --output "$out/bitstream-provenance.txt"

    runHook postInstall
  '';

  # A compile that fails to place the design still exits zero in some Quartus
  # flows, so check the artefacts rather than the exit status.
  doInstallCheck = true;
  installCheckPhase = ''
    runHook preInstallCheck

    for required in \
      application/DomesdayDuplicator.sof \
      application/DomesdayDuplicator_auto.rpd \
      factory/DomesdayDuplicatorFactory.sof \
      provisioning/DomesdayDuplicatorProvisioning.jic \
      provisioning/DomesdayDuplicatorProvisioning.svf \
      provisioning/boot-block.bin \
      bitstream-provenance.txt; do
      if [ ! -s "$out/$required" ]; then
        echo "$required is missing or empty — the compile did not produce a bitstream" >&2
        exit 1
      fi
    done

    # An artefact that cannot be traced to a commit is the thing this whole
    # provenance exercise exists to prevent (P7-9 makes it a release gate).
    if grep -q "^  commit  *unknown\$" "$out/bitstream-provenance.txt"; then
      echo "the provenance record reports an unknown commit" >&2
      exit 1
    fi

    runHook postInstallCheck
  '';

  meta = {
    description = "FPGA bitstream for the Domesday Duplicator (Cyclone IV EP4CE22F17C6)";
    longDescription = ''
      Compiles both DE0-Nano gateware images with Quartus Prime Lite: the
      resident factory boot loader and the capture application. Produces a
      .sof per image for volatile JTAG configuration, one provisioning .jic
      carrying both at their EPCS64 addresses, the raw application image a
      device update writes, the boot block that describes it, and a
      provenance record with digests for all of them.

      Not part of `nix flake check`: Quartus is unfree, x86_64-linux only, and
      non-redistributable, so it cannot come from a binary cache and has no
      place in the check every contributor runs. It is built by the dedicated
      bitstream and release workflows instead.
    '';
    homepage = "https://github.com/simoninns/DomesdayDuplicator";
    # The gateware is GPLv3. The toolchain that compiles it is not free
    # software, which is why this attribute is reachable only through the
    # flake's separate allowUnfree import of the same locked nixpkgs.
    license = lib.licenses.gpl3Plus;
    platforms = [ "x86_64-linux" ];
  };
}
