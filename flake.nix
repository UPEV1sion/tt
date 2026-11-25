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
      srcFiles = pkgs.lib.fileset.toSource {
        root = ./.;
        fileset = pkgs.lib.fileset.unions [
          ./Makefile
          ./src
          ./tests
        ];
      };
    in {
      packages.default = pkgs.stdenv.mkDerivation {
        pname = "task-tracker";
        version = "0.1.0";
        src = srcFiles;
        dontConfigure = true;

        nativeBuildInputs = with pkgs; [
          gnumake
          gcc
        ];

        doCheck = true;
        nativeCheckInputs = with pkgs; [
          valgrind
          (python3.withPackages (p: [ p.pytest ]))
        ];

        installFlags = [ "DESTDIR=$(out)" ];
      };

      devShells.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          fish
          gnumake
          gcc
          clang
          (python3.withPackages (p: [ p.pytest ]))
          valgrind
        ];
        shellHook = "fish";
      };
    }
  );
}
