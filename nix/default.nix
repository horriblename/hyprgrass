{
  lib,
  gcc13Stdenv,
  cmake,
  hyprland,
  wf-touch,
  doctest,
  runTests ? false,
}: let
  pluginInfo = builtins.fromTOML (builtins.readFile ../hyprload.toml);
in
  gcc13Stdenv.mkDerivation {
    pname = "hyprgrass";
    inherit (pluginInfo.hyprgrass) version;
    src = ./..;

    nativeBuildInputs = hyprland.nativeBuildInputs ++ [cmake] ++ lib.optional runTests doctest;

    buildInputs = [hyprland wf-touch doctest] ++ hyprland.buildInputs;

    doCheck = true;

    # CMake is just used for finding doctest.
    dontUseCmakeConfigure = true;

    meta = with lib; {
      homepage = "https://github.com/horriblename/hyprgrass";
      description = "Hyprland plugin for touch gestures";
      license = licenses.bsd3;
      platforms = platforms.linux;
    };
  }
