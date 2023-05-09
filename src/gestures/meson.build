gestures = static_library('gestures',
  'Gestures.cpp',
  dependencies: [
    wftouch,
  ])

if get_option('tests')
  test_exe = executable('test-gestures',
    'Gestures.cpp',
    'test/MockGestureManager.cpp',
    'test/test.cpp',
    dependencies: [
      dependency('doctest'),
      wftouch,
    ])

  test('test gestures', test_exe)
endif
