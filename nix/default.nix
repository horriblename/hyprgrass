{
  lib,
  stdenv,
  cmake,
  hyprland,
  wf-touch,
  doctest,
}: let
  pluginInfo = builtins.fromTOML (builtins.readFile ../hyprload.toml);
in
  stdenv.mkDerivation {
    pname = "hyprgrass";
    version = pluginInfo.hyprgrass.version;
    src = ./..;

    nativeBuildInputs = hyprland.nativeBuildInputs ++ [cmake];

    buildInputs = [hyprland wf-touch doctest] ++ hyprland.buildInputs;

    # CMake is just used for finding doctest.
    dontUseCmakeConfigure = true;

    meta = with lib; {
      homepage = "https://github.com/horriblename/hyprgrass";
      description = "Hyprland plugin for touch gestures";
      license = licenses.bsd3;
      platforms = platforms.linux;
    };
  }
