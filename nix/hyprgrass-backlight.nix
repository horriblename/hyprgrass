{
  lib,
  meson,
  ninja,
  pkg-config,
  hyprlandPlugins,
  glibmm,
  udev,
  tag,
  commit,
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprgrass-backlight";
  version = "${tag}+${commit}";
  src = ./..;

  nativeBuildInputs = [ninja meson pkg-config];

  buildInputs = [glibmm udev];

  mesonFlags = [
    "-Dhyprgrass=false"
    "-Dhyprgrass-backlight=true"
    "-Dtests=disabled"
  ];

  doCheck = true;

  meta = with lib; {
    homepage = "https://github.com/horriblename/hyprgrass";
    description = "Hyprgrass extension to control backlight";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
