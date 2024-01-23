gestures = static_library('gestures',
  'Gestures.cpp',
  'Shared.cpp',
  'Actions.cpp',
  dependencies: [
    wftouch,
  ])

if get_option('tests')
  subdir('test')
endif
