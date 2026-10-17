{
  description = "Task Tracker Nix Flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
  flake-utils.lib.eachDefaultSystem (system: 
    let 
      pkgs = import nixpkgs { inherit system; };
    in {
      packages.default = pkgs.stdenv.mkDerivation {
        pname = "task-tracker";
        version = "0.1.0";
        src = ./.;
        dontConfigure = true;
        installFlags = [ "DESTDIR=$(out)" ];
        nativeBuildInputs = with pkgs; [
          gnumake
          gcc
        ];
      };

      devShells.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          gnumake
          gcc
          clang
        ]; 
      };
    }
  );
}
