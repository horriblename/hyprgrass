{
  lib,
  gcc14Stdenv,
  meson,
  ninja,
  pkg-config,
  hyprland,
  libpulseaudio,
  tag,
  commit,
  ...
}:
gcc14Stdenv.mkDerivation {
  pname = "hyprgrass-pulse";
  version = "${tag}+${commit}";
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
