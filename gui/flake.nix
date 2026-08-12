{
  description = "Domesday Duplicator capture GUI and tools";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      inherit (import ../nix/lib.nix { inherit nixpkgs; }) forAllSystems;
    in
    {
      packages = forAllSystems (pkgs: rec {
        domesday-duplicator-gui = pkgs.qt6Packages.callPackage ./package.nix { };
        default = domesday-duplicator-gui;
      });

      devShells = forAllSystems (pkgs: {
        default = import ./shell.nix { inherit pkgs; };
      });

      checks = forAllSystems (pkgs: {
        inherit (self.packages.${pkgs.stdenv.hostPlatform.system}) domesday-duplicator-gui;
      });
    };
}
