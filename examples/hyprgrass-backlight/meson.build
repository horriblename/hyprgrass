if get_option('hyprgrass-backlight')
  giomm = dependency('giomm-2.4')
  udev = dependency('libudev')
  spdlog = dependency('spdlog')

  shared_module(
    'hyprgrass-backlight',
    'main.cpp',
    'backlight.cpp',
    'prepare_for_sleep.cpp',
    dependencies: [udev, spdlog, giomm, hyprland_headers, hyprland_deps],
    cpp_args: ['-DHAVE_LOGIN_PROXY'],
    install: true
  )
endif
