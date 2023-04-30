gestures = static_library('gestures',
  'Gestures.cpp',
  cpp_args: ['-DWLR_USE_UNSTABLE'],
  dependencies: [
    wftouch,
  ])

if get_option('tests')
  test_exe = executable('test-gestures',
    'Gestures.cpp',
    'test/MockGestureManager.cpp',
    'test/test.cpp',
    cpp_args: ['-DWLR_USE_UNSTABLE'],
    dependencies: [
      dependency('doctest'),
      wftouch,
    ])

  test('test gestures', test_exe)
endif
