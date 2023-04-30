subdir('gestures')

shared_module('touch-gestures',
  'main.cpp',
  'GestureManager.cpp',
  cpp_args: ['-DWLR_USE_UNSTABLE'],
  link_with: [gestures],
  dependencies: [
    wftouch,
    dependency('pixman-1'),
    dependency('libdrm'),
    hyprland_headers
  ],
  install: true)
