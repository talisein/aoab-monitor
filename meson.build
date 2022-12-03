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

zlib_dep = dependency('zlib', fallback: ['zlib'])
lzma_dep = dependency('liblzma', fallback: ['liblzma'])
libxml_dep = dependency('libxml-2.0', fallback: ['libxml2'])
libxmlpp_dep = dependency('libxml++-5.0',
                          version : '>=5.0.2',
                          fallback: ['libxmlpp'],
                          default_options: ['build-deprecated-api=false',
                                            'build-examples=false',
                                            'validation=false',
                                            'maintainer-mode=false'])

date_dep = dependency('date',
                      fallback: ['hinnant-date', 'date_dep'],
                      default_options: ['use_system_tzdb=true'])

subdir('src')
