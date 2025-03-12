{
  description = "Hyprland plugin for touch gestures";

  inputs = {
    hyprland.url = "github:hyprwm/Hyprland";
    nixpkgs.follows = "hyprland/nixpkgs";
  };

  outputs = {
    self,
    nixpkgs,
    hyprland,
    ...
  }: let
    withPkgsFor = fn:
      nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system:
        fn system (import nixpkgs {
          inherit system;
          overlays = [
            hyprland.overlays.hyprland-packages
            self.overlays.default
          ];
        }));
  in {
    packages = withPkgsFor (system: pkgs: rec {
      inherit (pkgs) wf-touch;
      inherit (pkgs.hyprlandPlugins) hyprgrass hyprgrass-pulse;

      default = hyprgrass;
      hyprgrassWithTests = hyprgrass.override {runTests = true;};
    });

    overlays = {
      default = self.overlays.hyprgrass;

      hyprgrass = final: prev: let
        tag = final.lib.replaceStrings ["\n" "v"] ["" ""] (builtins.readFile ./VERSION);
        commit = self.shortRev or "dirty";
      in {
        wf-touch = final.callPackage ./nix/wf-touch.nix {};

        hyprlandPlugins =
          (prev.hyprlandPlugins or {})
          // {
            hyprgrass = final.callPackage ./nix/default.nix {inherit tag commit;};
            hyprgrass-pulse = final.callPackage ./nix/hyprgrass-pulse.nix {inherit tag commit;};
          };
      };
    };

    devShells = withPkgsFor (system: pkgs: {
      default = pkgs.mkShell.override {inherit (pkgs.hyprland) stdenv;} {
        shellHook = ''
          meson setup build -Dbuildtype=debug -Dhyprgrass-pulse=true --reconfigure
          sed -e 's/c++23/c++2b/g' ./build/compile_commands.json > ./compile_commands.json
        '';
        name = "hyprgrass-shell";
        nativeBuildInputs = with pkgs; [meson pkg-config ninja];
        buildInputs = [pkgs.hyprland pkgs.libpulseaudio];
        inputsFrom = [
          pkgs.hyprland
          pkgs.hyprlandPlugins.hyprgrass
        ];
      };
    });

    formatter = withPkgsFor (_system: pkgs: pkgs.alejandra);
  };
}
