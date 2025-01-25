{
  description = "Hyprland plugin for touch gestures";

  inputs.hyprland.url = "github:hyprwm/Hyprland";

  outputs = {
    self,
    hyprland,
  } @ inputs: let
    inherit (hyprland.inputs) nixpkgs;
    withPkgsFor = fn:
      nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system:
        fn system (import nixpkgs {
          inherit system;
          overlays = [hyprland.overlays.hyprland-packages];
        }));
  in {
    # for debugging
    inherit inputs;

    packages = withPkgsFor (system: pkgs: let
      tag = pkgs.lib.replaceStrings ["\n" "v"] ["" ""] (builtins.readFile ./VERSION);
      commit = self.shortRev or "dirty";

      hyprgrassPackage = pkgs.callPackage ./nix/default.nix {
        inherit (hyprland.packages.${system}) hyprland;
        inherit (self.packages.${system}) wf-touch;
        inherit tag commit;
      };
    in {
      inherit inputs;
      default = hyprgrassPackage;
      hyprgrass = hyprgrassPackage;
      hyprgrassWithTests = hyprgrassPackage.override {runTests = true;};
      hyprgrass-pulse = pkgs.callPackage ./nix/hyprgrass-pulse.nix {inherit tag commit;};
      wf-touch = pkgs.callPackage ./nix/wf-touch.nix {};
    });

    devShells = withPkgsFor (system: pkgs: let
      hyprlandPkgs = hyprland.packages.${system};
    in {
      default = pkgs.mkShell.override {inherit (hyprlandPkgs.hyprland) stdenv;} {
        shellHook = ''
          meson setup build -Dhyprgrass-pulse=true --reconfigure
          sed -e 's/c++23/c++2b/g' ./build/compile_commands.json > ./compile_commands.json
        '';
        name = "hyprgrass-shell";
        nativeBuildInputs = with pkgs; [meson pkg-config ninja];
        buildInputs = [hyprlandPkgs.hyprland pkgs.libpulseaudio];
        inputsFrom = [
          hyprlandPkgs.hyprland
          self.packages.${system}.default
        ];
      };
    });

    formatter = withPkgsFor (_system: pkgs: pkgs.alejandra);
  };
}
