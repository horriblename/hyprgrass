if get_option('hyprgrass-backlight')
  giomm = dependency('giomm-2.4')
  udev = dependency('libudev')
  spdlog = dependency('spdlog')

  mod_args = []
  if get_option('login-proxy')
    mod_args += ['-DHAVE_LOGIN_PROXY']
  endif

  shared_module(
    'hyprgrass-backlight',
    'main.cpp',
    'backlight.cpp',
    'prepare_for_sleep.cpp',
    dependencies: [udev, spdlog, giomm, hyprland_headers, hyprland_deps],
    cpp_args: mod_args,
    install: true
  )
endif
