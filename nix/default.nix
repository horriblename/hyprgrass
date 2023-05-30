{
  lib,
  stdenv,
  cmake,
  hyprland,
  wf-touch,
  doctest,
}:
stdenv.mkDerivation {
  pname = "hyprland-touch-gestures";
  version = "0.1";
  src = ./..;

  nativeBuildInputs = hyprland.nativeBuildInputs ++ [cmake];

  buildInputs = [hyprland wf-touch doctest] ++ hyprland.buildInputs;

  # CMake is just used for finding doctest.
  dontUseCmakeConfigure = true;

  meta = with lib; {
    homepage = "https://github.com/horriblename/hyprland-touch-gestures";
    description = "Hyprland plugin for touch gestures";
    license = licenses.bsd3;
    platforms = platforms.linux;
  };
}
