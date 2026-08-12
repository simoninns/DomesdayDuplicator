{
  description = "Domesday Duplicator PCB design (KiCad)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { nixpkgs, ... }:
    let
      inherit (import ../nix/lib.nix { inherit nixpkgs; }) forAllSystems;
    in
    {
      devShells = forAllSystems (pkgs: {
        default = import ./shell.nix { inherit pkgs; };
      });
    };
}
