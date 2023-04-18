{ lib
, stdenv
, fetchFromGitHub
, pkg-config
, meson
, cmake
, ninja
, glm
, doctest
}:
stdenv.mkDerivation {
  pname = "wf-touch";
  version = "git";
  src = fetchFromGitHub {
    owner = "WayfireWM";
    repo = "wf-touch";
    rev = "8974eb0f6a65464b63dd03b842795cb441fb6403";
    hash = "sha256-MjsYeKWL16vMKETtKM5xWXszlYUOEk3ghwYI85Lv4SE=";
  };

  nativeBuildInputs = [ meson pkg-config cmake ninja ];
  buildInputs = [ glm doctest ];

  mesonBuildType = "release";

  outputs = [ "out" "dev"];
  meta = with lib; {
    homepage = "https://github.com/WayfireWM/wf-touch";
    license = licenses.mit;
    description = "Touchscreen gesture library";
    platforms = platforms.all;
  };
}
