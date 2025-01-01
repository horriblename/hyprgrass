subdir('gestures')

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
  'TouchVisualizer.cpp',
  cpp_args: ['-DWLR_USE_UNSTABLE'],
  link_with: [gestures],
  dependencies: [
    wftouch,
    hyprland_deps,
    hyprland_headers
  ],
  install: true)
