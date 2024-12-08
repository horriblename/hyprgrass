{
  lib,
  gcc14Stdenv,
  cmake,
  meson,
  ninja,
  pkg-config,
  hyprland,
  wf-touch,
  doctest,
  runTests ? false,
}: let
  version = builtins.readFile ../VERSION;
in
  gcc14Stdenv.mkDerivation {
    pname = "hyprgrass";
    inherit version;
    src = ./..;

    nativeBuildInputs =
      hyprland.nativeBuildInputs
      ++ [cmake ninja meson pkg-config]
      ++ lib.optional runTests doctest;

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
