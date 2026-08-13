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

{
  config,
  lib,
  pkgs,
  ...
}:

let
  cfg = config.hardware.domesdayDuplicator;

  # nixpkgs' quartus-prime-lite ships no udev rules, so the JTAG cable that
  # programs the FPGA has nowhere else to get them from. Carried as a package
  # rather than through services.udev.extraRules because extraRules lands in
  # 99-local.rules — far too late for the uaccess tag, which systemd consumes
  # in 73-seat-late.rules. That is exactly the bug the FX3 rule carried for
  # years — the tag was set and nothing ever acted on it — and it would be
  # silently reintroduced here.
  usbBlasterRules = pkgs.writeTextFile {
    name = "altera-usb-blaster-udev-rules";
    destination = "/lib/udev/rules.d/70-altera-usb-blaster.rules";
    text = builtins.readFile ../../fpga/configs/70-altera-usb-blaster.rules;
  };
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
      description = ''
        Whether to install udev rules for the Altera USB-Blaster, the JTAG
        cable that programs the FPGA — the DE0-Nano has one on board. Needed
        by `quartus_pgm` and `jtagconfig`, which otherwise report
        "No JTAG hardware available" to anyone but root. Turn it off on a
        machine that captures but never reprogrammes the gateware.
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
    services.udev.packages = [ cfg.package ] ++ lib.optional cfg.usbBlaster usbBlasterRules;

    environment.systemPackages = lib.mkIf cfg.installProgrammer [ cfg.package ];
  };
}
