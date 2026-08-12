{
  description = "Domesday Duplicator documentation site";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      inherit (import ../nix/lib.nix { inherit nixpkgs; }) forAllSystems;
    in
    {
      packages = forAllSystems (pkgs: rec {
        docs-site = pkgs.callPackage ./package.nix { };
        default = docs-site;
      });

      devShells = forAllSystems (pkgs: {
        default = import ./shell.nix { inherit pkgs; };
      });

      checks = forAllSystems (pkgs: {
        inherit (self.packages.${pkgs.stdenv.hostPlatform.system}) docs-site;
      });
    };
}
