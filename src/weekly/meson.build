
weekly_src = [
  'parts.cpp',
  'utils.cpp',
  'historic_word_stats.cpp',
]

weekly_deps = [
  lib_dep,
  libxmlpp_dep,
]

partget_exe = executable('partget', weekly_src, lib_generated_src,
                         dependencies: weekly_deps,
                         install: true)


stats_test_exe = executable('test-stats', 'historic_word_stats.cpp', 'test/stats.cpp', 'utils.cpp',
                            dependencies: [weekly_deps, boostut_dep],
                            install: false)

test('stats', stats_test_exe)
