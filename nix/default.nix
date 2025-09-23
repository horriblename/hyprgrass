{
  lib,
  cmake,
  meson,
  ninja,
  hyprland,
  hyprlandPlugins,
  wf-touch,
  doctest,
  tag,
  commit,
  runTests ? false,
  ...
}:
hyprlandPlugins.mkHyprlandPlugin {
  pluginName = "hyprgrass";
  version = "${tag}+${commit}";
  src = ./..;

  inherit hyprland;
  nativeBuildInputs = [cmake ninja meson] ++ lib.optional runTests doctest;

  buildInputs = [wf-touch doctest];

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
