{
  lib,
  meson,
  ninja,
  pkg-config,
  hyprland,
  hyprlandPlugins,
  libpulseaudio,
  tag,
  commit,
  ...
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprgrass-pulse";
  version = "${tag}+${commit}";
  src = ./..;

  inherit hyprland;
  nativeBuildInputs = [ninja meson pkg-config];

  buildInputs = [libpulseaudio];

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
