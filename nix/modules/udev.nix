# NixOS module: device permissions for the Domesday Duplicator and its Cypress FX3.
#
# Enable from a flake-based configuration:
#
#   imports = [ domesdayduplicator.nixosModules.udev ];
#   hardware.domesdayDuplicator.enable = true;
#
# Without this, the FX3 enumerates root-only and every fx3-programmer operation fails with
# LIBUSB_ERROR_ACCESS unless run as root.

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
      udev rules giving non-root access to the Domesday Duplicator and the Cypress FX3
      USB 3.0 controller it is built around
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
  };
}
