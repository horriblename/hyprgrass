custom_bindm = static_library(
  'custom_bindm', 'MouseDispatcherArg.cpp',
)

if doctest.found()
  custom_bindm_test = executable('test-custom-bindm',
    'MouseDispatcherArg.cpp',
    'test.cpp',
    dependencies: [
      doctest
    ]
  )

  test('test custom bindm', custom_bindm_test)
endif
