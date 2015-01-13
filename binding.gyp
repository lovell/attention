{
  'targets': [{
    'target_name': 'attention',
    'sources': [
      'src/exoquant.c',
      'src/attention.cc',
      'src/palette.cc',
      'src/region.cc'
    ],
    'variables': {
      'PKG_CONFIG_PATH': '<!(which brew >/dev/null 2>&1 && eval $(brew --env) && echo $PKG_CONFIG_LIBDIR || true):$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig:/usr/lib/pkgconfig'
    },
    'libraries': [
      '<!(PKG_CONFIG_PATH="<(PKG_CONFIG_PATH)" pkg-config --libs vips-cpp)'
    ],
    'include_dirs': [
      '<!(PKG_CONFIG_PATH="<(PKG_CONFIG_PATH)" pkg-config --cflags vips-cpp glib-2.0)',
      '<!(node -e "require(\'nan\')")'
    ],
    'cflags': [
      '-fexceptions',
      '-Wall',
      '-march=native',
      '-Ofast',
      '-flto',
      '-funroll-loops'
    ],
    'cflags_cc': [
      '-std=c++0x',
      '-fexceptions',
      '-Wall',
      '-march=native',
      '-Ofast',
      '-flto',
      '-funroll-loops'
    ],
    'xcode_settings': {
      'OTHER_CFLAGS': [
        '-fexceptions',
        '-Wall',
        '-march=native',
        '-Ofast',
        '-funroll-loops'
      ],
      'OTHER_CPLUSPLUSFLAGS': [
        '-std=c++11',
        '-stdlib=libc++',
        '-fexceptions',
        '-Wall',
        '-march=native',
        '-Ofast',
        '-funroll-loops'
      ],
      'MACOSX_DEPLOYMENT_TARGET': '10.7'
    }
  }]
}
