
protoc = find_program('protoc', required: true)

lib_gen = generator(protoc, \
                output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
                arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
lib_generated_src = lib_gen.process('defs.proto')

lib_src = [
  'curl.cpp',
  'utils.cpp',
]

lib_deps = [curl_dep, protobuf_dep, date_dep, libxmlpp_dep]

lib = static_library('aoabmonitor', lib_src, lib_generated_src,
                     dependencies: lib_deps,
                     install: false)

lib_dep = declare_dependency(link_with: lib,
                             dependencies: lib_deps,
                             include_directories: include_directories('./'))


utils_test_exe = executable('test-utils', 'test/test-utils.cpp',
                            dependencies: [boostut_dep, lib_dep],
                            install: false)

test('utils', utils_test_exe)
