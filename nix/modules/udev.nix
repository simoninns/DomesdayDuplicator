# NixOS module: device permissions for the Domesday Duplicator, its Cypress FX3, and the
# Altera USB-Blaster used to program the FPGA.
#
# Domesday Duplicator - LaserDisc RF sampler
# SPDX-FileCopyrightText: 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Enable from a flake-based configuration:
#
#   imports = [ domesdayduplicator.nixosModules.udev ];
#   hardware.domesdayDuplicator.enable = true;
#
# Without this, the FX3 enumerates root-only and every fx3-programmer operation fails with
# LIBUSB_ERROR_ACCESS unless run as root, and quartus_pgm reports "No JTAG hardware
# available" because nixpkgs' Quartus package ships no udev rules of its own.
#
# Both are covered by the single ruleset the fx3-programmer package installs — there is no
# separate option for the JTAG cable, because the machine that never expected to need JTAG
# is exactly the one that will need it during a recovery.

{
  config,
  lib,
  pkgs,
  ...
}:

let
  cfg = config.hardware.domesdayDuplicator;
in
{
  options.hardware.domesdayDuplicator = {
    enable = lib.mkEnableOption ''
      udev rules giving non-root access to the Domesday Duplicator, the Cypress FX3
      USB 3.0 controller it is built around, and the Altera USB-Blaster that programs
      its FPGA
    '';

    package = lib.mkOption {
      type = lib.types.package;
      default =
        pkgs.domesday-duplicator-fx3-programmer or (throw ''
          hardware.domesdayDuplicator.package must be set explicitly unless the
          domesdayduplicator overlay is in nixpkgs.overlays. From a flake:

            hardware.domesdayDuplicator.package =
              domesdayduplicator.packages.''${pkgs.system}.fx3-programmer;
        '');
      defaultText = lib.literalExpression "pkgs.domesday-duplicator-fx3-programmer";
      description = ''
        Package supplying `lib/udev/rules.d/70-domesday-duplicator.rules`. This is normally the
        `fx3-programmer` package from this repository, which installs the rule as part of
        its ordinary install step.
      '';
    };

    usbBlaster = lib.mkOption {
      type = lib.types.bool;
      default = true;
      visible = false;
      description = ''
        Deprecated and ignored. The USB-Blaster rules are now part of the single ruleset
        installed with the FX3 rules, because a machine that only captures is still the
        machine that has to program the FPGA when a board needs recovering. Setting this
        to `false` no longer skips them, and warns.
      '';
    };

    installProgrammer = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = ''
        Whether to add `fx3-programmer` to `environment.systemPackages`. Turn this off to
        get only the device permissions, for example on a machine that runs the capture GUI
        but never programs the hardware.
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    services.udev.packages = [ cfg.package ];

    environment.systemPackages = lib.mkIf cfg.installProgrammer [ cfg.package ];

    warnings = lib.optional (!cfg.usbBlaster) ''
      hardware.domesdayDuplicator.usbBlaster no longer does anything and can be removed.
      The Altera USB-Blaster rules are part of 70-domesday-duplicator.rules and are always
      installed: JTAG is how a board is recovered, so the permissions have to already be in
      place on the day they are needed.
    '';
  };
}
