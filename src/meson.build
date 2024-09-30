configure_file(
  input: 'version.h.in',
  output: 'version.h',
  configuration: {
    'HYPRGRASS_VERSION': meson.project_version()
  }
)

subdir('gestures')

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
