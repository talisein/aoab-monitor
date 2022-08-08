
protoc = find_program('protoc', required : false)
gen = generator(protoc, \
  output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
generated = gen.process('defs.proto')

src = [
  'main.cpp',
  'curl.cpp'
]

deps = [curl_dep, protobuf_dep, date_dep]

exe = executable('aoab-monitor', src, generated,
                 dependencies: deps,
                 install: true)
