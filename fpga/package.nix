# The Domesday Duplicator FPGA bitstream.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Unlike every other package in this repository, this one is **not** built by CI
# and is **not** in `nix flake check`. Quartus Prime Lite is unfree,
# x86_64-linux only, and marked `redistributable = false` — so it can never be
# served from a binary cache, and every cold build would have to fetch several
# gigabytes from Altera. The decision and the three ways it could reach CI later
# are recorded in fpga/README.md, "Why this is not built by CI".
#
# What this derivation is for is making the *local* build repeatable: same
# inputs, same command line, an output directory that always contains the same
# set of files, and a provenance record generated rather than typed. The dev
# shell (`nix develop .#fpga-quartus`) remains the primary way to work on the
# gateware.
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
      ./src
      ./bitstream-provenance.py
      ./generate-version.sh
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
    # run in fpga/src — see build-local.sh, which does the same copy this does.
    cd src
    chmod -R u+w .

    # Stamp the build with the commit, which the gateware presents to the FX3
    # in its identity registers. It has to happen after the chmod, because that
    # is what makes this copy of the sources writable, and it overwrites the
    # placeholder version.vh committed beside them. bitstreamVersion is passed
    # in for the same reason the firmware's is: there is no .git here to ask.
    bash "$src/generate-version.sh" . "${bitstreamVersion}"

    echo "Compiling with $(quartus_sh --version | sed -n 2p)"
    quartus_sh --flow compile DomesdayDuplicator

    # The .cof is committed and names its own inputs and outputs, so the
    # conversion needs no arguments beyond the file itself.
    quartus_cpf -c DomesdayDuplicator.cof

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p "$out"

    # The two configuration files, and the memory map quartus_cpf emits beside
    # the .jic.
    install -Dm444 DomesdayDuplicator.sof -t "$out"
    install -Dm444 DomesdayDuplicator.jic -t "$out"
    install -Dm444 DomesdayDuplicator.map -t "$out"

    # The compilation reports. A bitstream with no timing report is not
    # something anyone should be asked to trust, and these are small.
    mkdir -p "$out/reports"
    install -Dm444 DomesdayDuplicator.*.rpt -t "$out/reports"
    install -Dm444 DomesdayDuplicator.*.summary -t "$out/reports"

    # The programming files, so the output directory is enough on its own to
    # program a board: quartus_pgm reads these and they name the files above.
    install -Dm444 DomesdayDuplicator_write_sof.cdf -t "$out"
    install -Dm444 DomesdayDuplicator_write_jic.cdf -t "$out"

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

    for required in DomesdayDuplicator.sof DomesdayDuplicator.jic bitstream-provenance.txt; do
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
      Compiles the DE0-Nano gateware with Quartus Prime Lite and converts it to
      an EPCS64 flash image, producing DomesdayDuplicator.sof for volatile JTAG
      configuration and DomesdayDuplicator.jic for permanent configuration,
      alongside a provenance record with digests for both.

      Not part of `nix flake check` and not built by CI: Quartus is unfree,
      x86_64-linux only, and non-redistributable, so it cannot come from a
      binary cache.
    '';
    homepage = "https://github.com/simoninns/DomesdayDuplicator";
    # The gateware is GPLv3. The toolchain that compiles it is not free
    # software, which is why this attribute is reachable only through the
    # flake's separate allowUnfree import of the same locked nixpkgs.
    license = lib.licenses.gpl3Plus;
    platforms = [ "x86_64-linux" ];
  };
}
