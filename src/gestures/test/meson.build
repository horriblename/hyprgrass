doctest = dependency('doctest', required: get_option('tests'))
if doctest.found()
  # HACK: workaround bad lib flags in nixpkgs doctest:
  # https://github.com/NixOS/nixpkgs/issues/514722
  doctest_compile = doctest.partial_dependency(
    compile_args: true,
    includes: true,
  )

  test_exe = executable('test-gestures',
    'MockGestureManager.cpp',
    'test.cpp',
    'TouchSequence.cpp',
    link_with: gestures,
    dependencies: [
      wftouch,
      doctest_compile
    ]
  )

  test('test gestures', test_exe)
endif
