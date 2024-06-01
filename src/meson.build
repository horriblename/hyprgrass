subdir('gestures')
subdir('bindm')

version_header = configure_file(
  input: 'version.hpp.in',
  output: 'version.hpp',
  configuration: {
    'HYPRGRASS_VERSION': meson.project_version()
  }
)

shared_module('hyprgrass',
  'main.cpp',
  'GestureManager.cpp',
  'VecSet.cpp',
  cpp_args: ['-DWLR_USE_UNSTABLE'],
  link_with: [gestures],
  dependencies: [
    wftouch,
    dependency('pixman-1'),
    dependency('libinput'),
    dependency('wayland-server'),
    dependency('xkbcommon'),
    dependency('libdrm'),
    hyprland_headers
  ],
  install: true)
