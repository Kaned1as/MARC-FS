project('marcfs', 'c', 'cpp',
  default_options : ['c_std=c11', 'cpp_std=c++17'],
  subproject_dir: 'contrib',
  version: run_command('git', 'describe', '--tags', '--always').stdout(),
  license: 'GPL-3.0-or-later'
)

# project-wide variables
use_static_linking = get_option('default_library') == 'static'
extra_cpp_flags = []
extra_link_args = []

if use_static_linking
  extra_link_args += ['-static', '-static-libstdc++', '-static-libgcc']
endif

# libraries
libcurl_dep = dependency('libcurl', static: use_static_linking)
jsoncpp_dep = dependency('jsoncpp', static: use_static_linking)
fuse3_dep = dependency('fuse3', static: use_static_linking)
extra_cpp_flags += ['-D_FILE_OFFSET_BITS=64', '-DFUSE_USE_VERSION=35']

# subprojects
cmake = import('cmake')

curlcpp_proj = cmake.subproject('curlcpp')
curlcpp_dep = curlcpp_proj.dependency('curlcpp')
googletest_proj = cmake.subproject('googletest')
googletest_main = googletest_proj.dependency('gtest_main')
googletest_lib = googletest_proj.dependency('gtest')

marcfs_lib_src = [
  'src/abstract_storage.cpp',
  'src/account.cpp',
  'src/file_storage.cpp',
  'src/fuse_hooks.cpp',
  'src/marc_api_cloudfile.cpp',
  'src/marc_api_shard.cpp',
  'src/marc_dir_node.cpp',
  'src/marc_file_node.cpp',
  'src/marc_node.cpp',
  'src/marc_rest_client.cpp',
  'src/memory_storage.cpp',
  'src/mru_cache.cpp',
  'src/object_pool.cpp',
  'src/utils.cpp'
]

marcfs_src = marcfs_lib_src + ['src/main.cpp']
executable('marcfs', marcfs_src,
  dependencies: [fuse3_dep, curlcpp_dep, libcurl_dep, jsoncpp_dep],
  cpp_args: extra_cpp_flags,
  link_args: extra_link_args,
  install: true
)

apitest_src = marcfs_lib_src + ['tests/apitest.cpp']
apitest = executable('apitest', apitest_src,
  dependencies: [fuse3_dep, curlcpp_dep, libcurl_dep, jsoncpp_dep, googletest_lib, googletest_main],
  cpp_args: extra_cpp_flags,
  link_args: extra_link_args,
  install: false
)
test('MARC-FS API test', apitest)
