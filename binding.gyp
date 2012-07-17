{
  'targets': [
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ],
      'libraries': ['-lzmq'],
      'cflags!': ['-fno-exceptions'],
      'cflags_cc!': ['-fno-exceptions'],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
          },
          # add macports include & lib dirs
          'include_dirs': [
            '/opt/local/include',
          ],
          'libraries': [
            '-L/opt/local/lib'
          ]
        }]
      ]
    }
  ]
}
