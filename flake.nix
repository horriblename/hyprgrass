{
  # WARNING this is mainly for testing if it will work in nix
  # depending on which part of hyprland is used, it may be necessary to add
  # the dependencies of Hyprland here as well
  description = "Hyprland plugin for touch gestures";

  inputs = {
    hyprland.url = "github:horriblename/Hyprland/nix-pluginenv";
    nixpkgs.follows = "hyprland/nixpkgs";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    nixpkgs,
    hyprland,
    flake-utils,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = import nixpkgs {inherit system;};
      hyprpkgs = hyprland.packages.${system};
    in rec {
      packages.default = pkgs.callPackage ./nix/default.nix {
        stdenv = pkgs.gcc12Stdenv;
        hyprland = hyprpkgs.hyprland;
        wf-touch = packages.wf-touch;
        wlroots = hyprpkgs.wlroots-hyprland;
      };
      packages.wf-touch = pkgs.callPackage ./nix/wf-touch.nix {};

      formatter = pkgs.alejandra;
      devShells.default = pkgs.mkShell.override {stdenv = pkgs.gcc12Stdenv;} {
        name = "hyprland-plugin-shell";
        nativeBuildInputs = with pkgs; [
          meson
          ninja
          pkg-config
          doctest

          clang-tools_15
        ];

        buildInputs = with pkgs; [
          hyprpkgs.wlroots-hyprland
          libdrm
          pixman
          packages.wf-touch
        ];

        inputsFrom = [
          hyprpkgs.hyprland
          hyprpkgs.wlroots-hyprland
        ];

        #HYPRLAND_HEADERS = hyprpkgs.hyprland.src; - TODO
      };
    });
}
