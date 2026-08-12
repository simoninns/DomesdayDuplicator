{
  description = "Programmer for the Cypress FX3 USB 3.0 controller on the Domesday Duplicator";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      inherit (import ../../nix/lib.nix { inherit nixpkgs; }) forLinux;
    in
    {
      packages = forLinux (pkgs: rec {
        fx3-programmer = pkgs.callPackage ./package.nix { };
        default = fx3-programmer;
      });

      devShells = forLinux (pkgs: {
        default = import ../shell.nix { inherit pkgs; };
      });

      checks = forLinux (pkgs: {
        inherit (self.packages.${pkgs.stdenv.hostPlatform.system}) fx3-programmer;
      });

      nixosModules.udev = ../../nix/modules/udev.nix;
      nixosModules.default = self.nixosModules.udev;
    };
}
