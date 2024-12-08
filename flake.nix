{
  description = "Hyprland plugin for touch gestures";

  inputs.hyprland.url = "github:hyprwm/Hyprland";

  outputs = {
    self,
    hyprland,
  } @ inputs: let
    inherit (hyprland.inputs) nixpkgs;
    withPkgsFor = fn: nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system: fn system nixpkgs.legacyPackages.${system});
  in {
    packages = withPkgsFor (system: pkgs: let
      hyprgrassPackage = pkgs.callPackage ./nix/default.nix {
        inherit (hyprland.packages.${system}) hyprland;
        inherit (self.packages.${system}) wf-touch;
      };
    in {
      inherit inputs;
      default = hyprgrassPackage;
      hyprgrass = hyprgrassPackage;
      hyprgrassWithTests = hyprgrassPackage.override {runTests = true;};
      wf-touch = pkgs.callPackage ./nix/wf-touch.nix {};
    });

    devShells = withPkgsFor (system: pkgs: {
      default = pkgs.mkShell {
        shellHook = ''
          meson setup build --reconfigure
          sed -e 's/c++23/c++2b/g' ./build/compile_commands.json > ./compile_commands.json
        '';
        name = "hyprgrass-shell";
        nativeBuildInputs = with pkgs; [gcc14 meson pkg-config ninja];
        buildInputs = [hyprland.packages.${system}.hyprland];
        inputsFrom = [
          hyprland.packages.${system}.hyprland
          self.packages.${system}.default
        ];
      };
    });

    formatter = withPkgsFor (_system: pkgs: pkgs.alejandra);
  };
}
