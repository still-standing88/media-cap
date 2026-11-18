doctest_inc = include_directories('third_party/doctest')
test_main = static_library('mediacap_test_main', 'support/main.cpp',
  include_directories: doctest_inc, cpp_args: platform_compile_args)
test_dep = declare_dependency(link_with: test_main, include_directories: doctest_inc)

test_internal_args = platform_compile_args + platform_cpp_only_args + ['-DMEDIACAP_STATIC']
test_internal_deps = ffmpeg_deps + platform_deps + [thread_dep, test_dep]
subdir('unit')
subdir('integration')
