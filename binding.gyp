{
  'targets' : [
    {
      'target_name': 'binding',
      'sources': [ 'binding.cc' ],
#     'libraries': ['-lzmq'],
      'cflags!': ['-fno-exceptions'],
      'cflags_cc!': ['-fno-exceptions'],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
          },
          'include_dirs': [
            '/opt/local/include',
          ],
          'libraries': [
            '-L/opt/local/lib'
          ]
        }],
        ['OS=="win"', {
          'defines': [
            'PLATFORM="win32"',
            '_LARGEFILE_SOURCE',
            '_FILE_OFFSET_BITS=64',
            '_WINDOWS',
            'Configuration=Release',
            'BUILDING_NODE_EXTENSION'
          ],
          'libraries': [ 
              'libzmq.lib',
          ],
          'include_dirs': [
            'deps\\include\\',
            'deps\\zmq\\',
          ],         
           'msvs_settings': {
            'VCLinkerTool': {
              'AdditionalLibraryDirectories': [
                '..\\deps\\zmq'
              ],
            },
          },
        }]
      ]
    }
  ]
}
