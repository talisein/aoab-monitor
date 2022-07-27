project('dregarnuhr', ['c', 'cpp'],
        license : 'GPLv3+',
        version : '0.9.0',
        default_options : ['c_std=gnu17',
                           'cpp_std=gnu++20',
                           'buildtype=release'
                          ]
       )

meson.get_compiler('cpp').has_header_symbol('experimental/iterator', '__cpp_lib_experimental_ostream_joiner')

curl_dep = dependency('libcurl')

protobuf_dep = dependency('protobuf')

subdir('src')
