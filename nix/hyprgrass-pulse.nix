{
  lib,
  gcc14Stdenv,
  meson,
  ninja,
  pkg-config,
  hyprland,
  libpulseaudio,
}: let
  version = builtins.readFile ../VERSION;
in
  gcc14Stdenv.mkDerivation {
    pname = "hyprgrass-pulse";
    inherit version;
    src = ./..;

    nativeBuildInputs =
      hyprland.nativeBuildInputs
      ++ [ninja meson pkg-config];

    buildInputs = [hyprland libpulseaudio] ++ hyprland.buildInputs;

    mesonFlags = [
      "-Dhyprgrass=false"
      "-Dhyprgrass-pulse=true"
      "-Dtests=disabled"
    ];

    doCheck = true;

    meta = with lib; {
      homepage = "https://github.com/horriblename/hyprgrass";
      description = "Hyprgrass extension to control audio volume";
      license = licenses.bsd3;
      platforms = platforms.linux;
    };
  }
