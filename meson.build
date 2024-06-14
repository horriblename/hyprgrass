project('hyprgrass', 'cpp', 'c',
  version: '0.7.0',
  default_options: ['buildtype=release'],
)

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.has_argument('-std=c++23')
  add_global_arguments('-std=c++23', language: 'cpp')
elif cpp_compiler.has_argument('-std=c++2b')
  add_global_arguments('-std=c++2b', language: 'cpp')
else
  error('Could not configure current C++ compiler (' + cpp_compiler.get_id() + ' ' + cpp_compiler.version() + ') with required C++ standard (C++23)')
endif

hyprland_headers = dependency('hyprland', required: false)
if not hyprland_headers.found()
  hyprland_src = run_command('printenv', 'HYPRLAND_HEADERS', check: false).stdout().strip()
  if hyprland_src == ''
    error('Hyprland not found via pkg-config and environment variable HYPRLAND_HEADERS not set')
  endif

  add_global_arguments('-I' + hyprland_src, language: 'cpp')
endif

wftouch = dependency('wftouch', required: false)

if not wftouch.found()
  wftouch = subproject('wf-touch').get_variable('wftouch')
endif

subdir('src')
