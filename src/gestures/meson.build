gestures = static_library('gestures',
  'Gestures.cpp',
  'Shared.cpp',
  'Actions.cpp',
  'CompletedGesture.cpp',
  'DragGesture.cpp',
  dependencies: [
    wftouch,
  ],
  install: get_option('grassroots')
)


if get_option('grassroots')
  include_subdir = 'grassroots'
  headers = run_command('find', '-name', '*.hpp').stdout().strip().split()
  install_headers(headers, subdir: include_subdir)

  pkg = import('pkgconfig')
  pkg.generate(gestures, subdirs: [include_subdir])
endif

subdir('test')
