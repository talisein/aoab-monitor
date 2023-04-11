project('dregarnuhr', ['c', 'cpp'],
        license : 'GPLv3+',
        version : '0.9.0',
        default_options : ['c_std=gnu17',
                           'buildtype=release'
                          ]
       )
cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.get_argument_syntax() == 'msvc'
  if cpp_compiler.has_argument('/std:c++23')
    cpp23_args = ['/std:c++23']
  else
    cpp23_args = ['/std:c++latest']
  endif
else
  if cpp_compiler.has_argument('-std=c++23')
    cpp23_args = ['-std=c++23']
  else
    cpp23_args = ['-std=c++2b']
  endif
endif
add_global_arguments(cpp23_args, language: 'cpp')

meson.get_compiler('cpp').has_header_symbol('experimental/iterator', '__cpp_lib_experimental_ostream_joiner', args: cpp23_args)

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
                      fallback: ['hinnant-date', 'tz_dep'],
                      default_options: ['use_system_tzdb=true'])

boostut_dep = dependency('ut',
                         method: 'cmake',
                         modules: 'Boost::ut',
                         fallback: ['boostut', 'boostut_dep'])

add_project_arguments('-DBOOST_UT_DISABLE_MODULE', language: 'cpp')

subdir('src')
