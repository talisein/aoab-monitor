
weekly_src = [
  'parts.cpp',
  'historic_word_stats.cpp',
]

weekly_deps = [
  lib_dep,
]

partget_exe = executable('partget', weekly_src,
                         dependencies: weekly_deps,
                         install: true)


stats_test_exe = executable('test-stats', 'historic_word_stats.cpp', 'test/stats.cpp',
                            dependencies: [weekly_deps, boostut_dep],
                            install: false)

test('stats', stats_test_exe)
