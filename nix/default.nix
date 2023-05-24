{
  lib,
  stdenv,
  hyprland,
  wf-touch,
}:
stdenv.mkDerivation {
  pname = "hyprland-touch-gestures";
  version = "0.1";
  src = ./..;

  inherit (hyprland) nativeBuildInputs;

  buildInputs = [hyprland wf-touch] ++ hyprland.buildInputs;

  meta = with lib; {
    homepage = "https://github.com/horriblename/hyprland-touch-gestures";
    description = "Hyprland plugin for touch gestures";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
