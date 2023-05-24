{
  # WARNING this is mainly for testing if it will work in nix
  # depending on which part of hyprland is used, it may be necessary to add
  # the dependencies of Hyprland here as well
  description = "Hyprland plugin for touch gestures";

  inputs.hyprland.url = "github:hyprwm/Hyprland";

  outputs = {
    self,
    hyprland,
  }: let
    inherit (hyprland.inputs) nixpkgs;
    withPkgsFor = fn: nixpkgs.lib.genAttrs (builtins.attrNames hyprland.packages) (system: fn system nixpkgs.legacyPackages.${system});
  in {
    packages = withPkgsFor (system: pkgs: {
      default = pkgs.callPackage ./nix/default.nix {
        inherit (hyprland.packages.${system}) hyprland;
        inherit (self.packages.${system}) wf-touch;
      };
      wf-touch = pkgs.callPackage ./nix/wf-touch.nix {};
    });

    devShells = withPkgsFor (system: pkgs: {
      default = pkgs.mkShell {
        name = "hyprland-touch-gesture-shell";
        nativeBuildInputs = with pkgs; [cpplint];
        buildInputs = [hyprland.packages.${system}.hyprland self.packages.${system}.wf-touch];
        inputsFrom = [hyprland.packages.${system}.hyprland];
      };
    });

    formatter = withPkgsFor (system: pkgs: pkgs.alejandra);
  };
}
