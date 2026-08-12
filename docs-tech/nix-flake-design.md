# Nix flake design

Companion to [reorganisation-plan.md](reorganisation-plan.md) §4. Sketches are illustrative,
not tested — they show the intended structure and the specific gotchas found in this
codebase.

## 1. The core pattern: thin flakes over shared `.nix` files

The trap with "a flake per component in one repo" is that the root aggregator ends up either
(a) duplicating every component definition, or (b) declaring each component as a flake
`input`, which means seven lock files, `follows` boilerplate everywhere, and a re-lock every
time any component changes.

Avoid both by keeping the *logic* out of `flake.nix`:

```
gui/
├── package.nix     # { lib, stdenv, cmake, qt6, libusb1, ... }: stdenv.mkDerivation { ... }
├── shell.nix       # { pkgs }: pkgs.mkShell { ... }
└── flake.nix       # ~20 lines, wraps the two above
```

`gui/flake.nix`:

```nix
{
  description = "Domesday Duplicator capture GUI and tools";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAll = nixpkgs.lib.genAttrs systems;
      pkgsFor = system: nixpkgs.legacyPackages.${system};
    in {
      packages = forAll (system:
        let pkgs = pkgsFor system; in {
          default = self.packages.${system}.domesday-duplicator-gui;
          domesday-duplicator-gui = pkgs.qt6Packages.callPackage ./package.nix { };
        });

      devShells = forAll (system: {
        default = import ./shell.nix { pkgs = pkgsFor system; };
      });
    };
}
```

The root `flake.nix` imports the *same* `./gui/package.nix` via `callPackage`. No cross-flake
inputs, no duplication:

```nix
# /flake.nix (sketch)
outputs = { self, nixpkgs }:
  let
    inherit (import ./nix/lib.nix { inherit nixpkgs; }) forAllSystems forLinux;
  in {
    packages = forAllSystems (pkgs: {
      gui            = pkgs.qt6Packages.callPackage ./gui/package.nix { };
      fx3-programmer = pkgs.callPackage ./fx3/programmer/package.nix { };
      docs-site      = pkgs.callPackage ./docs/package.nix { };
    }) // forLinux (pkgs: {
      fx3-firmware   = pkgs.callPackage ./fx3/firmware/package.nix { };
      # NB: fpga is deliberately NOT here — see §6
    });

    devShells = forAllSystems (pkgs: {
      default        = import ./nix/shell.nix     { inherit pkgs; };  # everything free
      gui            = import ./gui/shell.nix     { inherit pkgs; };
      docs           = import ./docs/shell.nix    { inherit pkgs; };
      hardware       = import ./hardware/shell.nix { inherit pkgs; };
      fx3            = import ./fx3/shell.nix     { inherit pkgs; };
    });

    checks = forAllSystems (pkgs:
      builtins.removeAttrs self.packages.${pkgs.system} [ "bitstream" ]);

    nixosModules.udev = ./nix/modules/udev.nix;
  };
```

Nix 2.34 (the version on this machine) does support relative path inputs
(`inputs.gui.url = "path:./gui"`), so the input-based alternative is viable — but it buys
nothing here and costs a lock file per component. Use it only if a component ever needs to be
consumed standalone by a *different* repo.

## 2. `gui/` — Qt 6 + libusb

Straightforward. Two things to fix while packaging:

- **Dedup the CMake front-ends.** `gui-app/CMakeLists.txt` and `gui-app/tools/CMakeLists.txt`
  are near-identical; keep one at `gui/CMakeLists.txt`. Delete the four qmake `.pro` files
  too (D14) — they are a third build definition, maintained only for Qt Creator.
- **Turn on `CMAKE_EXPORT_COMPILE_COMMANDS`** (D15) so clangd works in any editor, and put
  `clang-tools` in the dev shell.
- `qt6Packages.callPackage` + `wrapQtAppsHook` handles the Qt plugin/platform wrapping;
  without it the built binary will fail at runtime with a `xcb` platform plugin error.

```nix
{ lib, stdenv, cmake, pkg-config, qt6, libusb1, wrapQtAppsHook }:

stdenv.mkDerivation (finalAttrs: {
  pname = "domesday-duplicator-gui";
  version = "1.0";
  src = ./.;                      # or lib.fileset, to avoid rebuilds on README edits

  nativeBuildInputs = [ cmake pkg-config qt6.qttools wrapQtAppsHook ];
  buildInputs = [ qt6.qtbase qt6.qtserialport libusb1 ];

  meta = {
    description = "Capture GUI and tools for the Domesday Duplicator";
    mainProgram = "DomesdayDuplicator";
    platforms = lib.platforms.unix;
  };
})
```

Note `tools/cmake_modules/FindLibUSB.cmake` is a hand-rolled finder. Under Nix, `pkg-config`
finds libusb-1.0 cleanly; the custom module should keep working but is the first suspect if
configure fails.

## 3. `fx3/programmer/` — libusb host tool

Two things must be fixed before this packages cleanly.

**(a) The secondary loader is missing (D13).** `fx3-programmer` does RAM download *and*
I2C EEPROM / SPI flash programming; the latter two first push a Cypress secondary loader,
`cyfxflashprog.img`, into RAM. That file is not in this repository —
`find_flashprog_image()` hunts for it via `$FX3_FLASH_PROG` and six CWD-relative paths,
three pointing at a sibling `cyusb_linux` checkout. CWD-relative lookup cannot work for an
installed binary, so the derivation must install the image to
`$out/share/domesday-duplicator/` and the C must gain a compiled-in
`FLASHPROG_INSTALL_PATH` candidate. Without this, `nix run .#fx3-programmer` can load RAM
but cannot program the EEPROM — i.e. it cannot do the thing users need it for.

Note `pkgs.fxload` covers only the RAM path, so it is **not** a substitute for this tool.

**(b) The udev rule install escapes the prefix.** Current `CMakeLists.txt`:

```cmake
install(FILES configs/88-cyusb.rules DESTINATION /etc/udev/rules.d)   # escapes the prefix
```

Change to `DESTINATION ${CMAKE_INSTALL_DATADIR}/../lib/udev/rules.d` (or plain
`lib/udev/rules.d`), then ship a NixOS module so users get the rule declaratively:

```nix
# nix/modules/udev.nix
{ config, lib, pkgs, ... }:
let cfg = config.hardware.domesdayDuplicator;
in {
  options.hardware.domesdayDuplicator.enable =
    lib.mkEnableOption "udev rules for the Domesday Duplicator / Cypress FX3";

  config = lib.mkIf cfg.enable {
    services.udev.packages = [ pkgs.domesday-duplicator-fx3-programmer ];
  };
}
```

This is the single biggest quality-of-life win for NixOS users of the project, and it is
independent of everything else in the plan.

## 4. `fx3/firmware/` — ARM cross build

Uses `gcc-arm-embedded` (15.2.rel1 in nixpkgs) plus the vendored SDK. Three changes to the
build are needed:

**(a) Version injection.** `CMakeLists.txt` runs `git rev-parse --short=8 HEAD` at configure
time. There is no `.git` in a Nix sandbox, so this yields `"unknown"` and the USB descriptor
loses its version stamp. Add a cache variable:

```cmake
if(NOT DEFINED FIRMWARE_VERSION)
  execute_process(COMMAND git rev-parse --short=8 HEAD ... OUTPUT_VARIABLE FIRMWARE_VERSION)
endif()
if(NOT FIRMWARE_VERSION)
  set(FIRMWARE_VERSION "unknown")
endif()
```

and pass `-DFIRMWARE_VERSION=${self.shortRev or "dirty"}` from the flake.

**(b) `elf2img` as its own derivation**, replacing `ExternalProject_Add`:

```nix
# fx3/firmware/elf2img.nix
{ stdenv, cmake }:
stdenv.mkDerivation {
  pname = "cyfx3-elf2img";
  version = "1.3.5";
  src = ../sdk/util/elf2img;
  nativeBuildInputs = [ cmake ];
}
```

Then in `package.nix`, `nativeBuildInputs = [ cmake elf2img ]` and have CMake call `elf2img`
from `PATH` rather than from `${CMAKE_CURRENT_BINARY_DIR}/tools/bin/`.

**(c) SDK path.** `CYFX3SDK_PATH` is already a cache variable defaulting to
`${CMAKE_CURRENT_SOURCE_DIR}/cyfx3sdk` — point it at `../sdk` after the re-layout, or at a
store path if the SDK ever becomes a separate derivation.

```nix
{ stdenv, cmake, gcc-arm-embedded, elf2img, firmwareVersion ? "unknown" }:

stdenv.mkDerivation {
  pname = "domesday-duplicator-fx3-firmware";
  version = firmwareVersion;
  src = ./.;

  nativeBuildInputs = [ cmake gcc-arm-embedded elf2img ];

  cmakeFlags = [
    "-DCMAKE_TOOLCHAIN_FILE=${./arm-none-eabi-toolchain.cmake}"
    "-DFIRMWARE_VERSION=${firmwareVersion}"
    "-DCYFX3SDK_PATH=${../sdk}"
  ];

  # bare-metal ARM target: stripping and the standard fixups must be off
  dontStrip = true;
  dontPatchELF = true;
}
```

The toolchain file uses `find_program(CMAKE_C_COMPILER arm-none-eabi-gcc)`, which works as
long as `gcc-arm-embedded` is in `nativeBuildInputs`. Watch for nixpkgs' `stdenv` hardening
flags leaking into a freestanding build — if the link fails on `-nostartfiles`, set
`hardeningDisable = [ "all" ]`.

## 5. `docs/` — MkDocs Material

The site converts from Jekyll to MkDocs + Material + `mkdocs-awesome-nav`, matching
decode-orc. Rationale and the content-side task list are in
[docs-theme-migration.md](docs-theme-migration.md); this section covers only the Nix side.

This is the easiest component to package, because all three tools are ordinary nixpkgs Python
packages — `mkdocs` 1.6.1, `mkdocs-material` 9.7.6, `mkdocs-awesome-nav` 3.3.0. Nothing is
fetched at build time, so the `remote_theme` sandbox problem disappears, and there is no Ruby
gem set to author with `bundix` and then keep aligned with GitHub's `github-pages` bundle.

```nix
# docs/package.nix
{ lib, stdenvNoCC, python312 }:

let
  mkdocsEnv = python312.withPackages (ps: [
    ps.mkdocs
    ps.mkdocs-material
    ps."mkdocs-awesome-nav"
  ]);
in
stdenvNoCC.mkDerivation {
  pname = "domesday-duplicator-docs";
  version = "0";
  src = ./.;                       # docs/ only — mkdocs.yml lives here, not at the repo root

  nativeBuildInputs = [ mkdocsEnv ];

  buildPhase = ''
    runHook preBuild
    mkdocs build --strict          # broken internal links fail the build
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out
    cp -r site/* $out/
    runHook postInstall
  '';

  meta.license = lib.licenses.cc-by-sa-40;
}
```

`docs/shell.nix` exposes the same `mkdocsEnv` so `mkdocs serve` gives live preview — the
replacement for `build-local.sh`, which is deleted. Use `--strict` in the package but *not*
in the dev shell, so a work-in-progress link does not block local preview.

Two things to get right:

- **`docs_dir: content`, never `site`.** MkDocs' `site_dir` defaults to `site` relative to
  `mkdocs.yml`; if `docs_dir` has the same name the build refuses to run.
- **`mkdocs.yml` lives in `docs/`, not at the repo root** (decode-orc puts it at the root).
  That keeps `src = ./.` scoped to the docs component instead of the whole repo. Build with
  `mkdocs build -f docs/mkdocs.yml` when invoking from elsewhere.

`check-internal-linkage.sh` and `check-orphans.sh` are not ported — `--strict` covers broken
internal links, and `awesome-nav` errors when a `.nav.yml` names a missing file.

## 6. `fpga/` — Quartus, unfree, x86_64-linux only

Verified against `pkgs/by-name/qu/quartus-prime-lite/quartus.nix` in the current nixpkgs:
`platforms = [ "x86_64-linux" ]`, `license = unfree`, `redistributable = false`, and it takes
`supportedDevices` (default six families) and `withQuesta ? true`.

```nix
# fpga/flake.nix
{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      # allowUnfree is set on OUR OWN nixpkgs import, so consumers do not need --impure
      # or NIXPKGS_ALLOW_UNFREE. Evaluation stays pure.
      pkgs = import nixpkgs {
        inherit system;
        config.allowUnfree = true;
      };
      quartus = pkgs.quartus-prime-lite.override {
        supportedDevices = [ "Cyclone IV" ];   # EP4CE22F17C6 on the DE0-Nano
        withQuesta = false;                    # no simulation in this project
      };
    in {
      packages.${system}.bitstream = pkgs.callPackage ./package.nix { inherit quartus; };
      devShells.${system}.default  = pkgs.mkShell { packages = [ quartus ]; };
    };
}
```

Setting `config.allowUnfree` inside the flake's own `import nixpkgs` is the pattern that
keeps `nix build` working without `--impure` — the alternative (`nixConfig` or telling users
to export `NIXPKGS_ALLOW_UNFREE=1`) is worse UX and breaks in CI.

The headless build, which does not exist today:

```nix
# fpga/package.nix (sketch)
{ stdenvNoCC, quartus }:
stdenvNoCC.mkDerivation {
  pname = "domesday-duplicator-bitstream";
  version = "0";
  src = ./src;
  nativeBuildInputs = [ quartus ];
  buildPhase = ''
    quartus_sh --flow compile DomesdayDuplicator
    quartus_cpf -c DomesdayDuplicator.cof
  '';
  installPhase = ''
    mkdir -p $out
    cp output_files/*.sof output_files/*.jic output_files/*.rpt $out/
  '';
}
```

The wrapper puts **every** binary from `quartus/bin/*` on `PATH` (plus
`qsys-{generate,edit,script}`), so the dev shell gives `quartus_sh`, `quartus_map`,
`quartus_fit`, `quartus_asm`, `quartus_sta`, `quartus_cpf`, `quartus_pgm` and `jtagd` — the
GUI is one binary among them and never required. Add the free HDL tooling to the same shell
so Verilog editing needs no Quartus at all:

```nix
devShells.${system} = {
  default = pkgs.mkShell {                    # unfree: full toolchain
    packages = [ quartus pkgs.verible pkgs.verilator pkgs.iverilog ];
  };
  hdl = pkgs.mkShell {                        # free: edit, lint, simulate — no Quartus
    packages = [ pkgs.verible pkgs.verilator pkgs.iverilog ];
  };
};
```

`quartus_pgm` additionally needs `jtagd` and a USB-Blaster udev rule; the nixpkgs package
ships **no** udev rules, so that rule belongs in `nix/modules/udev.nix` next to the FX3 one.

Caveats to document in `fpga/README.md`:

- **Reproducibility, stated accurately.** Quartus *fitting* is deterministic: for a given
  Fitter seed (a fixed project setting, default 1) and `Maximum processors allowed`, the fit
  is the same run to run regardless of machine or core count. Identical results do require
  the same Quartus version, the same 32/64-bit build and the same CPU architecture. Separately,
  a compile timestamp is embedded in the bitstream header, so identical configuration content
  can still land in non-identical *files*. Treat the packaged output as reproducible in
  content but not assumed byte-identical until measured — P6-9. The dev shell
  (`nix develop ./fpga`) remains the primary deliverable.
- The download is multi-gigabyte and cannot come from a binary cache
  (`redistributable = false`), so the first build is slow.
- `quartus_sh` needs a writable `$HOME`; set `export HOME=$TMPDIR` in the build.
- The IP is **not** regenerated at build time: `IPfifo.v` and `IPpllGenerator.v` are
  committed Verilog instantiating `dcfifo`/`altpll` with explicit `defparam`s. Treat them as
  source, not as wizard output — see [ide-independence.md](ide-independence.md) §2.2.

## 7. `hardware/` — KiCad

Dev shell now, packaged export later:

```nix
{ pkgs }: pkgs.mkShell { packages = [ pkgs.kicad ]; }
```

`kicad-cli`-based gerber/PDF/BOM generation is blocked on migrating the KiCad 5 project files
to the current format — `kicad-cli` cannot read `.sch` legacy schematics. Treat that as a
separate change with its own review.

## 8. CI

Replace the three workflows with one path-filtered job set:

```yaml
- uses: DeterminateSystems/nix-installer-action@main
- uses: DeterminateSystems/magic-nix-cache-action@main
- run: nix build .#gui .#fx3-firmware .#fx3-programmer .#docs-site
```

That job runs **on every commit**, and its outputs are the artefacts a release publishes —
see [implementation-plan.md](implementation-plan.md) → *Release artefacts and provenance*.
Two consequences for the derivations sketched above:

- **Version must be injectable, not discovered.** A Nix build from a tag has no `.git`, so
  anything that shells out to `git rev-parse` silently yields `unknown`. The firmware already
  takes `-DFIRMWARE_VERSION=` (D4); the GUI needs the same treatment (**D21**, task P5-6).
  The release workflow fails if any artefact reports `unknown`.
- **Nix does not cover Windows.** The existing native build matrix stays alongside the flake
  builds — it is the only way the Windows binary gets produced. Nix is additive here.

`.#bitstream` stays out of CI: unfree, multi-gigabyte, x86_64-linux only, and — decisively —
`redistributable = false`, so it can never be served from a binary cache and every cold run
would re-fetch it from Intel. **Decided 2026-08-12 to leave the FPGA out of CI for now**; the
bitstream is built locally and attached to releases by hand, together with a provenance record
(task P6-8) and digests that make it verifiable (P6-10). Quartus fitting is deterministic
given a pinned toolchain, so a rebuild *should* agree — but the embedded compile timestamp
means the check must be made over a canonical form rather than the raw file, unless P6-9
measures the `.sof` to be byte-identical.

Pages deployment follows decode-orc's `deploy-docs.yml`: install Nix, `nix build .#docs-site`,
then `upload-pages-artifact` with `path: ./result` and `deploy-pages`. That drops
`actions/jekyll-build-pages` entirely and makes the deployed site the *same derivation output*
as the local one — which the Jekyll route could not guarantee. (An earlier draft of this
document said reimplementing Pages deployment in Nix gains nothing; with MkDocs it costs
nothing and removes a whole class of "differs on Pages" problems.)

Fix while here: the current firmware workflow's `working-directory: fx3-firmware` does not
match the actual `fx3/fx3-firmware` path.
